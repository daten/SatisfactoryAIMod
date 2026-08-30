"""Deterministic splitter/merger port identification and selection
toolkit (2026-08-30).

Direct response to a specific, explicit user requirement: prove the mod
has deterministic control over splitter port selection - by world-facing
cardinal direction, never by component index, array position, "nearest",
or "first match" - before any manifold/production-layout code is allowed
to depend on it again. See docs/splitter-port-control-test.md for the
full test plan and its live results.

Same "toolkit, not solver" posture as satisfactory_ai.layout/conveyors:
every function here is a pure transform over ALREADY-FETCHED
satisfactory_ai.models.FactoryConnection records (from a
world.connections call the caller already made) - nothing here calls an
RPC or decides a layout. The caller is responsible for fetching
world.connections, filtering/parsing it into FactoryConnection objects,
and calling world.connectConveyor with the exact ids this module
resolves.

Cardinal convention (verified against real live game data, session
2026-08-30, NOT assumed): a connector's real `normal` vector (from
world.connections) has its dominant component on the horizontal (X/Y)
plane. This module defines +Y=NORTH, -Y=SOUTH, +X=EAST, -X=WEST -
consistent with this project's own established framing in prior
sessions (docs/placement-lessons.md's manifold notes: the Miner sat
"north" of the platform at lower Y, foundations extended "south" at
higher Y). This is an arbitrary but INTERNALLY CONSISTENT labeling - the
game world has no real compass - what matters is that the same
convention is used everywhere a direction is requested or reported.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import List

from .models import FactoryConnection, Position

CARDINAL_DIRECTIONS = ("NORTH", "EAST", "SOUTH", "WEST")


class PortSelectionError(Exception):
    """Raised by every function in this module on any ambiguous or
    invalid request - callers must handle this explicitly, there is no
    fallback/guessed selection anywhere in this module. `code` matches
    the error codes named in the user's test spec
    (ERROR_NO_OUTPUT_IN_DIRECTION / INVALID_DIRECTION / etc.) so a caller
    can branch on it without string-matching the message.
    """

    def __init__(self, code: str, message: str):
        super().__init__(f"{code}: {message}")
        self.code = code


def cardinal_from_normal(normal: Position, tolerance: float = 1e-3) -> str:
    """Classify a connector's real world-facing normal vector into one
    of NORTH/EAST/SOUTH/WEST using the dominant horizontal axis - see
    this module's docstring for the exact convention. `tolerance` guards
    against a degenerate all-zero (or near-zero-horizontal, e.g. a
    straight-up/down) normal, which should never happen for a real
    splitter/merger connector but is checked explicitly rather than
    silently defaulting to a direction.
    """
    x, y = normal.x, normal.y
    if abs(x) < tolerance and abs(y) < tolerance:
        raise PortSelectionError(
            "DEGENERATE_NORMAL",
            f"normal ({x}, {y}) has no meaningful horizontal component - cannot classify a cardinal direction",
        )
    if abs(x) > abs(y):
        return "EAST" if x > 0 else "WEST"
    return "NORTH" if y > 0 else "SOUTH"


@dataclass(frozen=True)
class SplitterPort:
    """One connector on a splitter/merger, with its cardinal direction
    already resolved - the normalized unit this module's selection
    functions operate on. `raw` is the underlying FactoryConnection this
    was derived from, kept for diagnostics/logging (component identity,
    exact position/normal) - selection logic itself only ever looks at
    `connection_type`/`cardinal`.
    """

    owner_buildable_id: str
    connection_type: str  # "Input" or "Output" (FactoryConnection.direction, unchanged)
    cardinal: str  # "NORTH" / "EAST" / "SOUTH" / "WEST"
    connected: bool
    connected_buildable_id: str
    raw: FactoryConnection


def classify_ports(connections: List[FactoryConnection], owner_buildable_id: str) -> List[SplitterPort]:
    """Filters `connections` (an already-fetched world.connections
    result, parsed into FactoryConnection objects by the caller) down to
    the ones owned by `owner_buildable_id`, and resolves each one's real
    cardinal direction from its real normal. Pure - does not call an RPC.
    """
    ports = []
    for c in connections:
        if c.owner_buildable_id != owner_buildable_id:
            continue
        ports.append(
            SplitterPort(
                owner_buildable_id=owner_buildable_id,
                connection_type=c.direction,
                cardinal=cardinal_from_normal(c.normal),
                connected=c.connected,
                connected_buildable_id=c.connected_buildable_id,
                raw=c,
            )
        )
    return ports


def get_splitter_input(ports: List[SplitterPort]) -> SplitterPort:
    """Returns the splitter's single Input port. Raises PortSelectionError
    (code WRONG_INPUT_COUNT) if there isn't exactly one - a real splitter/
    merger should always have exactly one, so more or fewer means the
    caller passed the wrong buildable's ports, or the game's connector
    layout differs from what this module assumes.
    """
    inputs = [p for p in ports if p.connection_type == "Input"]
    if len(inputs) != 1:
        raise PortSelectionError(
            "WRONG_INPUT_COUNT",
            f"expected exactly 1 Input port, found {len(inputs)} (ports={ports!r})",
        )
    return inputs[0]


def get_splitter_output_facing(ports: List[SplitterPort], direction: str) -> SplitterPort:
    """Returns the Output port facing `direction` (case-insensitive
    NORTH/EAST/SOUTH/WEST). Never falls back to "nearest"/"first"/any
    other output:
    - unknown direction string -> PortSelectionError("INVALID_DIRECTION")
    - no connector at all faces that direction -> PortSelectionError("NO_PORT_IN_DIRECTION")
    - the connector facing that direction IS the Input, not an Output
      (i.e. the caller asked for the splitter's input side as if it had
      an output there) -> PortSelectionError("ERROR_NO_OUTPUT_IN_DIRECTION"),
      exactly the negative-test behavior the user's test spec requires -
      never silently substitute a different output.
    """
    direction = direction.upper()
    if direction not in CARDINAL_DIRECTIONS:
        raise PortSelectionError(
            "INVALID_DIRECTION", f"'{direction}' is not one of {CARDINAL_DIRECTIONS}"
        )
    matches = [p for p in ports if p.cardinal == direction]
    if not matches:
        raise PortSelectionError("NO_PORT_IN_DIRECTION", f"no connector faces {direction}")
    if len(matches) > 1:
        raise PortSelectionError(
            "AMBIGUOUS_DIRECTION",
            f"{len(matches)} connectors face {direction} - cannot select unambiguously ({matches!r})",
        )
    port = matches[0]
    if port.connection_type != "Output":
        raise PortSelectionError(
            "ERROR_NO_OUTPUT_IN_DIRECTION",
            f"the {direction} connector is the splitter's {port.connection_type}, not an Output",
        )
    return port


def build_rotation_table(readings: dict) -> str:
    """Formats a {yaw: List[SplitterPort]} mapping (one entry per tested
    rotation) into the plain-text table the test spec asks for - a pure
    presentation helper, not analysis. `readings` keys are yaw degrees
    (int), values are the SplitterPort list for that rotation.
    """
    lines = ["Yaw | Input | Output | Output | Output", "----|-------|--------|--------|-------"]
    for yaw in sorted(readings):
        ports = readings[yaw]
        input_port = next((p for p in ports if p.connection_type == "Input"), None)
        output_ports = [p for p in ports if p.connection_type == "Output"]
        cells = [input_port.cardinal if input_port else "?"]
        cells += [p.cardinal for p in output_ports] + ["?"] * max(0, 3 - len(output_ports))
        lines.append(f"{yaw:<3} | " + " | ".join(f"{c:<6}" for c in cells))
    return "\n".join(lines)
