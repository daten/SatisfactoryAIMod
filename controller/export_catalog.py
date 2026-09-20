"""Export the live recipe/item/buildable catalog to a local JSON cache.

Like `live_check.py`, this REQUIRES Satisfactory (or the Editor in
Play-In-Editor) to actually be running, with AIMod loaded and its HTTP
server up (default http://127.0.0.1:51902/rpc). It is deliberately a
snapshot-on-demand tool, not documentation: `world.recipeCatalog`/
`world.itemCatalog`/`world.buildableCatalog` (docs/telemetry-protocol.md)
are the actual source of truth, and can change whenever the game patches
or AIMod's catalog logic changes. Re-run this whenever you want a fresh
snapshot rather than trusting an old one - the output file's `exportedAt`
field makes staleness visible at a glance.

Why a cache file instead of a static markdown reference: the combined
catalog is large (roughly 2,000 recipes/items/buildables as of
2026-08-27) and would make a poor manually-maintained doc - it's real
game data, not a lesson or a how-to, and belongs regenerated from the
live game rather than hand-edited. A markdown export would also silently
drift out of date the moment the game or the mod's catalog logic changes,
which cuts against this whole project's "the live game is the source of
truth" principle (see CLAUDE.md).

Usage:

    python controller/export_catalog.py
    python controller/export_catalog.py --url http://127.0.0.1:51902/rpc --output my_catalog.json

Output defaults to controller/catalog_cache.json (gitignored - it's a
snapshot, not source, and should never be committed).
"""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

from live_check import DEFAULT_TIMEOUT_SECONDS, DEFAULT_URL, rpc_call

DEFAULT_OUTPUT = Path(__file__).parent / "catalog_cache.json"

CATALOG_METHODS = {
    "recipes": "world.recipeCatalog",
    "items": "world.itemCatalog",
    "buildables": "world.buildableCatalog",
}


def fetch_catalog(url: str, timeout: float) -> dict[str, Any]:
    """Fetches all three catalog methods, raising on any failure - a
    partial/half-fetched catalog would be worse than no cache at all."""
    export: dict[str, Any] = {
        "exportedAt": datetime.now(timezone.utc).isoformat(),
        "sourceUrl": url,
    }

    for key, method in CATALOG_METHODS.items():
        response = rpc_call(url, method, timeout=timeout)
        if not response.get("success"):
            raise RuntimeError(f"{method} failed: {response.get('error')}")
        result = response["result"]
        # AIMod's LogXAsJson methods return their JSON already-decoded
        # as the "result" object (not a further-nested string) - see
        # docs/telemetry-protocol.md's examples for each method's shape.
        items = result.get(key)
        if not isinstance(items, list):
            raise RuntimeError(f"{method}: expected result.{key!r} to be a list, got {type(items).__name__}")
        export[key] = items

    return export


def main(argv: Optional[list[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--url", default=DEFAULT_URL, help=f"AIMod /rpc endpoint (default: {DEFAULT_URL})")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_SECONDS, help="Per-request timeout in seconds")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help=f"Output path (default: {DEFAULT_OUTPUT})")
    args = parser.parse_args(argv)

    print(f"Fetching catalog from {args.url} ...")
    try:
        export = fetch_catalog(args.url, args.timeout)
    except Exception as exc:  # noqa: BLE001 - deliberately broad, this is a CLI tool
        print(f"FAILED: {exc}", file=sys.stderr)
        print(
            "Is Satisfactory/the Editor running with AIMod loaded? "
            "See docs/manual-verification.md item 5.",
            file=sys.stderr,
        )
        return 1

    args.output.write_text(json.dumps(export, indent=2), encoding="utf-8")

    print(f"  {len(export['recipes'])} recipe(s)")
    print(f"  {len(export['items'])} item(s)")
    print(f"  {len(export['buildables'])} buildable(s)")
    print(f"Wrote {args.output}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
