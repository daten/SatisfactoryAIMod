"""Live integration check against a running DocMod HTTP server.

Unlike everything else in this package, this REQUIRES Satisfactory (or
the Editor in Play-In-Editor) to actually be running, with DocMod loaded
and its HTTP server up (docs/networking-research.md - default
http://127.0.0.1:51902/rpc). Not part of `python -m unittest discover`
for that reason - it isn't a repeatable, dependency-free test, it's a
diagnostic against live game state. Run it explicitly:

    python controller/live_check.py
    python controller/live_check.py --url http://127.0.0.1:51902/rpc

Prints one PASS/FAIL line per method and exits non-zero if anything
failed, so it's scriptable (e.g. for Claude to check without needing a
human to manually poke Blueprint nodes or read the Output Log).

This complements, not replaces, docs/self-test.md's in-process self-test:
that one runs INSIDE the game and can check things this can't (e.g. that
JSON round-trips through Unreal's own parser); this one runs OUTSIDE the
game and checks things the in-process test can't (that the RPC envelope
and HTTP transport genuinely work end-to-end, as an external client would
see it).
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Any, Callable, Optional

from satisfactory_ai.protocol import (
    parse_buildable_telemetry,
    parse_factory_connection_telemetry,
    parse_resource_node_telemetry,
)

DEFAULT_URL = "http://127.0.0.1:51902/rpc"
DEFAULT_TIMEOUT_SECONDS = 5.0


@dataclass
class CheckResult:
    name: str
    passed: bool
    detail: str = field(default="")


def rpc_call(
    url: str,
    method: str,
    params: Optional[dict] = None,
    request_id: str = "live-check",
    timeout: float = DEFAULT_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    """POSTs one JSON-RPC request. Raises on transport failure; the
    caller is responsible for checking response["success"]."""
    payload: dict[str, Any] = {"protocolVersion": 1, "requestId": request_id, "method": method}
    if params is not None:
        payload["params"] = params

    request = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        # DocMod returns a real JSON body with a structured error even on
        # non-2xx status (see docs/operations-protocol.md -
        # UNKNOWN_METHOD/TARGET_NOT_FOUND map to 404, etc.) - urllib
        # otherwise treats any non-2xx as a hard failure and discards the
        # body. Read it the same way a 2xx response would be read. Found
        # this bug live: the mod was correctly returning 404 with a valid
        # error body, and this function was wrongly reporting it as
        # "request failed" instead of parsing it.
        return json.loads(exc.read().decode("utf-8"))


def _check_read_method(url: str, method: str, parser: Callable[[str], Any], describe: Callable[[Any], str]) -> CheckResult:
    try:
        response = rpc_call(url, method)
    except Exception as exc:  # noqa: BLE001 - deliberately broad, this is a diagnostic script
        return CheckResult(method, False, f"request failed: {exc}")

    if not response.get("success"):
        return CheckResult(method, False, f"success=false: {response.get('error')}")

    try:
        parsed = parser(json.dumps(response["result"]))
    except (KeyError, ValueError, TypeError) as exc:
        return CheckResult(method, False, f"schema/parse error: {exc}")

    return CheckResult(method, True, describe(parsed))


def check_resource_nodes(url: str) -> CheckResult:
    return _check_read_method(
        url, "world.resourceNodes", parse_resource_node_telemetry,
        lambda t: f"{len(t.resource_nodes)} node(s)",
    )


def check_buildables(url: str) -> CheckResult:
    return _check_read_method(
        url, "world.buildables", parse_buildable_telemetry,
        lambda t: f"{len(t.buildables)} buildable(s)",
    )


def check_connections(url: str) -> CheckResult:
    return _check_read_method(
        url, "world.connections", parse_factory_connection_telemetry,
        lambda t: f"{len(t.connections)} connection point(s)",
    )


def check_manufacturers(url: str) -> CheckResult:
    # No dedicated dataclass/parser exists yet for manufacturers (Phase 10
    # added the mod-side telemetry but the controller-side model was never
    # added - only resourceNodes/buildables/connections are in
    # satisfactory_ai.models). Validate the raw shape inline rather than
    # skip the check entirely.
    try:
        response = rpc_call(url, "world.manufacturers")
    except Exception as exc:  # noqa: BLE001
        return CheckResult("world.manufacturers", False, f"request failed: {exc}")

    if not response.get("success"):
        return CheckResult("world.manufacturers", False, f"success=false: {response.get('error')}")

    result = response.get("result", {})
    if result.get("protocolVersion") != 1 or not isinstance(result.get("manufacturers"), list):
        return CheckResult("world.manufacturers", False, f"unexpected shape: {result}")

    valid_statuses = {"None", "Producing", "ProducingWithCrystal", "Standby", "Error"}
    for manufacturer in result["manufacturers"]:
        if manufacturer.get("productionStatus") not in valid_statuses:
            return CheckResult(
                "world.manufacturers", False,
                f"unexpected productionStatus {manufacturer.get('productionStatus')!r}",
            )

    return CheckResult("world.manufacturers", True, f"{len(result['manufacturers'])} manufacturer(s)")


def check_unknown_method_rejected(url: str) -> CheckResult:
    try:
        response = rpc_call(url, "world.thisMethodDoesNotExist")
    except Exception as exc:  # noqa: BLE001
        return CheckResult("rejects-unknown-method", False, f"request failed: {exc}")

    ok = response.get("success") is False and response.get("error", {}).get("code") == "UNKNOWN_METHOD"
    return CheckResult("rejects-unknown-method", ok, json.dumps(response))


def check_write_validation_rejects_bad_target(url: str) -> CheckResult:
    # Negative-path only, matches docs/self-test.md's in-process
    # equivalent - never a positive-path mutation from this script either.
    try:
        response = rpc_call(
            url, "world.setClockSpeed",
            params={"buildableId": "__live_check_nonexistent__", "clockSpeedPercent": 100.0},
        )
    except Exception as exc:  # noqa: BLE001
        return CheckResult("setClockSpeed-rejects-unknown-target", False, f"request failed: {exc}")

    ok = response.get("success") is False and response.get("error", {}).get("code") == "TARGET_NOT_FOUND"
    return CheckResult("setClockSpeed-rejects-unknown-target", ok, json.dumps(response))


CHECKS: list[Callable[[str], CheckResult]] = [
    check_resource_nodes,
    check_buildables,
    check_manufacturers,
    check_connections,
    check_unknown_method_rejected,
    check_write_validation_rejects_bad_target,
]


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default=DEFAULT_URL, help=f"DocMod /rpc endpoint (default: {DEFAULT_URL})")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS, help="Per-request timeout in seconds")
    args = parser.parse_args(argv)

    print(f"Checking {args.url} ...\n")

    results = [check(args.url) for check in CHECKS]
    for result in results:
        status = "PASS" if result.passed else "FAIL"
        suffix = f" - {result.detail}" if result.detail else ""
        print(f"[{status}] {result.name}{suffix}")

    failed = [r for r in results if not r.passed]
    print(f"\n{len(results) - len(failed)}/{len(results)} passed")

    if failed and any("request failed" in r.detail for r in failed):
        print(
            "\nNote: at least one check couldn't even connect - is Satisfactory/the "
            "Editor running with DocMod loaded? See docs/manual-verification.md item 5.",
            file=sys.stderr,
        )

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
