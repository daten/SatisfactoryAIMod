"""Op-list executor with the measured reliability envelope (2026-09-02).

Phase 1c of docs/build-efficiency-plan.md. Runs router/composite
RouteOps against a live game through rpc_client, applying - once, in
code - the operational lessons that previously cost an agent turn each
time (docs/placement-lessons.md, memory project_hmf_optimization.md):

- belt and wire connects are player-proximity-sensitive: teleport near
  the midpoint (at a safe z above the work) before each, with a short
  settle wait
- one automatic retry for the known-flaky failure classes ("Surface is
  too uneven!", first-attempt snap flakes)
- the connectPower global stuck state ("Must be hooked up to a
  connection!" on everything): reset by placing and deleting a dummy
  pole, then retry
- placements pass the standard bypass flags and explicit yaw ALWAYS
  (implicit yaw is nondeterministic - live-confirmed 2026-08-30)
- per-op structured results; halt-on-error by default so a failed
  prerequisite doesn't cascade into dangling-belt cleanup

The executor runs WHAT it is given, in order. It plans nothing.
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .models import Position
from .router import RouteOp, RoutePlan
from .rpc_client import RpcClient, RpcError

BYPASS_FLAGS = {
    "ignoreAimLocation": True,
    "ignorePlayerEncroachment": True,
    "ignoreGroundTrace": True,
    "ignoreInvalidFloor": True,
    "ignoreClearance": True,
}

#: Error text fragments that are worth exactly one blind retry - each
#: was observed live to succeed on immediate retry with no change.
RETRYABLE_FRAGMENTS = (
    "Surface is too uneven",
    "identical buildable is already built",
    "Must be hooked up to a connection",
    "A creature is in the way",
)

POLE_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C"
)
DEFAULT_BELT_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"
)
LIFT_RECIPE = (
    "/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorLiftMk4.Recipe_ConveyorLiftMk4_C"
)
SETTLE_SECONDS = 0.6
TELEPORT_HOVER_Z = 250.0  # above the higher endpoint - never a deep z


@dataclass
class OpResult:
    op: RouteOp
    success: bool
    buildable_id: Optional[str] = None
    error: Optional[str] = None
    attempts: int = 1


@dataclass
class ExecutionReport:
    results: List[OpResult] = field(default_factory=list)
    halted: bool = False

    @property
    def ok(self) -> bool:
        return not self.halted and all(r.success for r in self.results)

    def placed_ids(self) -> List[str]:
        return [r.buildable_id for r in self.results if r.success and r.buildable_id]

    def describe(self) -> str:
        lines = []
        for i, r in enumerate(self.results):
            status = "OK" if r.success else f"FAIL {r.error}"
            extra = f" -> {r.buildable_id}" if r.buildable_id else ""
            retries = f" (attempts={r.attempts})" if r.attempts > 1 else ""
            lines.append(f"[{i}] {r.op.kind} {r.op.note}: {status}{extra}{retries}")
        if self.halted:
            lines.append("HALTED at first failure (remaining ops not attempted)")
        return "\n".join(lines)


@dataclass
class Executor:
    client: RpcClient
    belt_recipe: str = DEFAULT_BELT_RECIPE
    halt_on_error: bool = True
    settle_seconds: float = SETTLE_SECONDS

    def execute(self, plan: RoutePlan) -> ExecutionReport:
        report = ExecutionReport()
        placed_by_op_index: Dict[int, str] = {}
        for index, op in enumerate(plan.ops):
            result = self._run_op(op, placed_by_op_index)
            report.results.append(result)
            if result.success and op.kind == "place" and result.buildable_id:
                placed_by_op_index[index] = result.buildable_id
            if not result.success and self.halt_on_error:
                report.halted = True
                break
        return report

    # ------------------------------------------------------------------

    def _run_op(self, op: RouteOp, placed: Dict[int, str]) -> OpResult:
        if op.kind == "place":
            return self._place(op)
        if op.kind == "belt":
            return self._belt(op, placed)
        if op.kind == "wire":
            return self._wire(op, placed)
        if op.kind == "call":
            return self._call(op, placed)
        if op.kind == "lift":
            return self._lift(op, placed)
        return OpResult(op=op, success=False, error=f"unsupported op kind {op.kind!r}")

    def _lift(self, op: RouteOp, placed: Dict[int, str]) -> OpResult:
        """world.connectConveyorLift with pinned connectors - the
        live-proven pattern (arbitrary heights work since the trace-ray
        fix; connector pinning is mandatory or the hologram grabs wrong
        ports). Mk4 lift by default (Mk1's 60/min cap starved a live
        chain once)."""
        source_id = self._resolve_ref(op.source_ref, placed)
        dest_id = self._resolve_ref(op.dest_ref, placed)
        if source_id is None or dest_id is None:
            return OpResult(op=op, success=False, error="unresolved op reference")
        self._hover_near(op.source_pin, op.dest_pin)
        last_error = ""
        for attempt in (1, 2):
            try:
                self.client.call(
                    "world.connectConveyorLift",
                    {
                        "sourceBuildableId": self.client.full_id(source_id),
                        "destBuildableId": self.client.full_id(dest_id),
                        "recipeClass": LIFT_RECIPE,
                        "sourceConnectorPosition": _pos_dict(op.source_pin),
                        "destConnectorPosition": _pos_dict(op.dest_pin),
                    },
                )
                return OpResult(op=op, success=True, attempts=attempt)
            except RpcError as exc:
                last_error = str(exc)
                if not _retryable(last_error):
                    break
                time.sleep(self.settle_seconds)
        return OpResult(op=op, success=False, error=last_error, attempts=attempt)

    def _wire(self, op: RouteOp, placed: Dict[int, str]) -> OpResult:
        source_id = self._resolve_ref(op.source_ref, placed)
        dest_id = self._resolve_ref(op.dest_ref, placed)
        if source_id is None or dest_id is None:
            return OpResult(op=op, success=False, error="unresolved op reference")
        near = None
        if op.source_pin is not None and op.dest_pin is not None:
            near = Position(
                x=(op.source_pin.x + op.dest_pin.x) / 2.0,
                y=(op.source_pin.y + op.dest_pin.y) / 2.0,
                z=max(op.source_pin.z, op.dest_pin.z),
            )
        result = self.connect_power(source_id, dest_id, near=near)
        result.op = op
        return result

    def _call(self, op: RouteOp, placed: Dict[int, str]) -> OpResult:
        target = self._resolve_ref(op.dest_ref, placed)
        if target is None:
            return OpResult(op=op, success=False, error="unresolved op reference")
        params = dict(op.params or {})
        params["buildableId"] = self.client.full_id(target)
        try:
            self.client.call(op.method, params)
            return OpResult(op=op, success=True)
        except RpcError as exc:
            return OpResult(op=op, success=False, error=str(exc))

    def _place(self, op: RouteOp) -> OpResult:
        params = {
            "recipeClass": op.recipe_class,
            "x": op.position.x,
            "y": op.position.y,
            "z": op.position.z,
            "yaw": float(op.yaw if op.yaw is not None else 0.0),
            **BYPASS_FLAGS,
        }
        last_error = ""
        for attempt in (1, 2):
            try:
                result = self.client.call("world.placeBuilding", params)
                return OpResult(
                    op=op,
                    success=True,
                    buildable_id=result.get("buildableId"),
                    attempts=attempt,
                )
            except RpcError as exc:
                last_error = str(exc)
                if not _retryable(last_error):
                    break
                time.sleep(self.settle_seconds)
        return OpResult(op=op, success=False, error=last_error, attempts=attempt)

    def _belt(self, op: RouteOp, placed: Dict[int, str]) -> OpResult:
        source_id = self._resolve_ref(op.source_ref, placed)
        dest_id = self._resolve_ref(op.dest_ref, placed)
        if source_id is None or dest_id is None:
            return OpResult(
                op=op, success=False, error="unresolved op reference (prerequisite place failed?)"
            )
        self._hover_near(op.source_pin, op.dest_pin)
        base = {
            "sourceBuildableId": self.client.full_id(source_id),
            "destBuildableId": self.client.full_id(dest_id),
            "recipeClass": self.belt_recipe,
            "instigatorStrategy": "RealCharacter",
            "sourceConnectorPosition": _pos_dict(op.source_pin),
            "destConnectorPosition": _pos_dict(op.dest_pin),
        }
        last_error = ""
        attempts = 0
        for route_mode in ("Straight", "Curve", "Auto"):
            attempts += 1
            try:
                self.client.call("world.connectConveyor", {**base, "routeMode": route_mode})
                return OpResult(op=op, success=True, attempts=attempts)
            except RpcError as exc:
                last_error = str(exc)
        # One flaky-class retry across all modes, after a settle.
        if _retryable(last_error):
            time.sleep(self.settle_seconds * 2)
            for route_mode in ("Straight", "Auto"):
                attempts += 1
                try:
                    self.client.call("world.connectConveyor", {**base, "routeMode": route_mode})
                    return OpResult(op=op, success=True, attempts=attempts)
                except RpcError as exc:
                    last_error = str(exc)
        return OpResult(op=op, success=False, error=last_error, attempts=attempts)

    def execute_and_verify(self, plan: RoutePlan, repair_rounds: int = 1) -> ExecutionReport:
        """execute() plus the post-build endpoint verification and
        auto-repair the first live composite test proved necessary
        (2026-09-02): world.connectConveyor can report success while the
        belt's far end never attached (the documented dangling-belt
        class). After execution, ONE filtered world.connections query
        checks every belt op's two pinned endpoints; any belt with an
        unattached end is repaired by the documented pattern - delete
        the dangling belt actor (found via the attached end's
        connectedBuildableId), rebuild, re-verify - up to repair_rounds
        times. Wire/call/place ops are not re-verified here (place
        failures already fail loudly; wire verification needs power-graph
        telemetry the mod doesn't expose yet)."""
        report = self.execute(plan)
        if report.halted:
            return report
        placed = {
            f"op:{i}": r.buildable_id
            for i, r in enumerate(report.results)
            if r.success and r.buildable_id
        }
        for _ in range(max(0, repair_rounds) + 1):
            broken = self._find_broken_belts(plan, placed)
            if not broken:
                break
            if _ == repair_rounds:  # rounds exhausted; report what's left
                for op, missing_end, dangling in broken:
                    report.results.append(OpResult(
                        op=op, success=False,
                        error=f"verify: {missing_end} end unattached after repairs",
                    ))
                break
            for op, missing_end, dangling in broken:
                # Only ever delete a conveyor-belt actor - if the
                # attached end's peer is anything else, the pin was
                # taken by a different structure and deleting it would
                # be destructive; rebuild alone and let it fail loudly.
                if dangling and "ConveyorBelt" not in dangling:
                    dangling = ""
                if dangling:
                    try:
                        self.client.call(
                            "world.deleteBuilding",
                            {"buildableId": self.client.full_id(dangling)},
                        )
                        time.sleep(self.settle_seconds)
                    except RpcError:
                        pass
                repair = self._belt(op, {int(k[3:]): v for k, v in placed.items() if k.startswith("op:")})
                repair.op = op
                repair.error = (repair.error or "") if repair.success else repair.error
                report.results.append(OpResult(
                    op=op, success=repair.success,
                    error=None if repair.success else repair.error,
                    attempts=repair.attempts,
                ))
        return report

    def _find_broken_belts(self, plan: RoutePlan, placed: Dict[str, str]):
        """(op, which_end, dangling_belt_id) for every belt op whose
        pinned endpoint connector reads unattached. One filtered
        world.connections query for the whole plan."""
        belt_ops = [op for op in plan.ops if op.kind == "belt"]
        if not belt_ops:
            return []
        ids = set()
        resolved = {}
        for op in belt_ops:
            for ref in (op.source_ref, op.dest_ref):
                rid = placed.get(ref, ref) if ref else None
                if rid:
                    resolved[ref] = rid
                    ids.add(rid.rsplit(".", 1)[-1])
        try:
            rows = self.client.call(
                "world.connections", {"ids": sorted(ids)}, timeout_seconds=120
            )["connections"]
        except RpcError:
            return []

        def state_at(owner_id: str, pin: Position):
            short = owner_id.rsplit(".", 1)[-1]
            for c in rows:
                p = c["position"]
                if short in c["ownerBuildableId"] and (
                    abs(p["x"] - pin.x) <= 5 and abs(p["y"] - pin.y) <= 5 and abs(p["z"] - pin.z) <= 5
                ):
                    return c["connected"], c.get("connectedBuildableId", "")
            return None, ""

        broken = []
        for op in belt_ops:
            src = resolved.get(op.source_ref)
            dst = resolved.get(op.dest_ref)
            if not src or not dst:
                continue
            src_ok, src_peer = state_at(src, op.source_pin)
            dst_ok, dst_peer = state_at(dst, op.dest_pin)
            if src_ok is True and dst_ok is False:
                broken.append((op, "dest", src_peer))  # belt hangs off the source
            elif dst_ok is True and src_ok is False:
                broken.append((op, "source", dst_peer))
            elif src_ok is False and dst_ok is False:
                broken.append((op, "both", ""))
        return broken

    def validate_plan(self, plan: RoutePlan) -> List[OpResult]:
        """Bulk DRY-RUN of a plan's belt ops via world.testConveyorBelt -
        nothing is built. Belt ops that reference not-yet-placed
        attachments (op:<i>) cannot be dry-run (their endpoint doesn't
        exist) and are reported as skipped-successes with a note.

        Calibration note (2026-09-02, live): the game's dry-run is the
        AUTHORITATIVE geometry gate - it accepts some routes the
        router's conservative pre-filter rejects (long-range S-curves,
        steep-but-long climbs). Use the router to plan cheaply, this to
        validate before building, and treat a dry-run failure as a
        planning input (reroute), not a retry candidate.
        """
        results: List[OpResult] = []
        for op in plan.ops:
            if op.kind != "belt":
                continue
            if (op.source_ref or "").startswith("op:") or (op.dest_ref or "").startswith("op:"):
                results.append(
                    OpResult(op=op, success=True, error="skipped: endpoint not yet placed")
                )
                continue
            self._hover_near(op.source_pin, op.dest_pin)
            try:
                self.client.call(
                    "world.testConveyorBelt",
                    {
                        "sourceBuildableId": self.client.full_id(op.source_ref),
                        "destBuildableId": self.client.full_id(op.dest_ref),
                        "recipeClass": self.belt_recipe,
                        "routeMode": "Auto",
                        "instigatorStrategy": "RealCharacter",
                        "sourceConnectorPosition": _pos_dict(op.source_pin),
                        "destConnectorPosition": _pos_dict(op.dest_pin),
                    },
                )
                results.append(OpResult(op=op, success=True))
            except RpcError as exc:
                results.append(OpResult(op=op, success=False, error=str(exc)))
        return results

    # -- power (used by composites, not RouteOps yet) -------------------

    def connect_power(self, id_a: str, id_b: str, near: Optional[Position] = None) -> OpResult:
        """connectPower with the full live-derived recovery ladder:
        proximity teleport, one plain retry, then the dummy-pole
        dismantle-cycle reset for the global stuck state."""
        op = RouteOp(kind="wire", source_ref=id_a, dest_ref=id_b, note="power")
        params = {
            "buildableIdA": self.client.full_id(id_a),
            "buildableIdB": self.client.full_id(id_b),
            "ignoreAimLocation": True,
        }
        if near is not None:
            # Best-effort, like _hover_near - a blocked teleport must not
            # kill the wire attempt.
            try:
                self.client.teleport(near.x, near.y, near.z + TELEPORT_HOVER_Z)
            except RpcError:
                pass
            time.sleep(self.settle_seconds)
        last_error = ""
        for attempt in (1, 2, 3):
            try:
                self.client.call("world.connectPower", params)
                return OpResult(op=op, success=True, attempts=attempt)
            except RpcError as exc:
                last_error = str(exc)
                if "Must be hooked up" in last_error:
                    self._reset_wire_state(near)
                time.sleep(self.settle_seconds)
        return OpResult(op=op, success=False, error=last_error, attempts=attempt)

    # ------------------------------------------------------------------

    def _hover_near(self, a: Position, b: Position) -> None:
        """Teleport above the midpoint of a connect - never AT a deep z
        (feedback_no_deep_teleport): hover at the HIGHER endpoint's z
        plus a margin, which live testing proved keeps the player in
        range of deep work while it descends toward it.

        BEST-EFFORT: TeleportTo needs a clear destination and can refuse
        (TELEPORT_BLOCKED) over open air near new structures - found on
        the first live composite run, where it crashed the whole
        execution. A hover failure must never kill the plan: try a
        higher hover, then proceed from wherever the player is (the
        connect's own retries/error handling take it from there)."""
        mx, my = (a.x + b.x) / 2.0, (a.y + b.y) / 2.0
        mz = max(a.z, b.z, 200.0) + TELEPORT_HOVER_Z
        for attempt_z in (mz, mz + 500.0):
            try:
                self.client.teleport(mx, my, attempt_z)
                break
            except RpcError:
                continue
        time.sleep(self.settle_seconds)

    def _reset_wire_state(self, near: Optional[Position]) -> None:
        """The dismantle-cycle that clears connectPower's global stuck
        state (live-confirmed fix, 2026-09-02)."""
        spot = near or Position(x=0.0, y=0.0, z=400.0)
        try:
            result = self.client.call(
                "world.placeBuilding",
                {
                    "recipeClass": POLE_RECIPE,
                    "x": spot.x + 600.0,
                    "y": spot.y + 600.0,
                    "z": spot.z + 200.0,
                    "yaw": 0.0,
                    **BYPASS_FLAGS,
                },
            )
            dummy = result.get("buildableId")
            if dummy:
                self.client.call("world.deleteBuilding", {"buildableId": self.client.full_id(dummy)})
        except RpcError:
            pass  # best-effort reset; the retry ladder continues regardless
        time.sleep(self.settle_seconds)

    @staticmethod
    def _resolve_ref(ref: Optional[str], placed: Dict[int, str]) -> Optional[str]:
        if ref is None:
            return None
        if ref.startswith("op:"):
            return placed.get(int(ref[3:]))
        return ref


def _retryable(error_text: str) -> bool:
    return any(fragment in error_text for fragment in RETRYABLE_FRAGMENTS)


def _pos_dict(p: Position) -> Dict[str, float]:
    return {"x": p.x, "y": p.y, "z": p.z}
