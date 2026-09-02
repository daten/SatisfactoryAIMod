"""Minimal JSON-RPC client for the AIMod loopback endpoint (2026-09-02).

Phase 1c of docs/build-efficiency-plan.md - the controller package's
first network client. controller/README.md deliberately deferred one
("no network client until there's a reason to add one"); executing
router/composite op-lists is that reason: the live sessions drove every
primitive through ad-hoc PowerShell, which is fine for one call and
miserable for a 40-op belt phase.

Standard library only (urllib) - no new dependency, matching the
package's existing zero-dependency posture. Loopback by default,
mirroring the mod's own default bind.
"""

from __future__ import annotations

import json
import urllib.request
from dataclasses import dataclass
from typing import Any, Dict, Optional
from uuid import uuid4

DEFAULT_ENDPOINT = "http://127.0.0.1:51902/rpc"
LEVEL_PREFIX = (
    "/Game/FactoryGame/Map/GameLevel01/Persistent_Level."
    "Persistent_Level:PersistentLevel."
)


class RpcError(Exception):
    """A structured error response from the mod (success=false)."""

    def __init__(self, code: str, message: str, method: str):
        super().__init__(f"{method}: {code}: {message}")
        self.code = code
        self.message = message
        self.method = method


class RpcTransportError(Exception):
    """The HTTP call itself failed (endpoint down, timeout)."""


@dataclass
class RpcClient:
    endpoint: str = DEFAULT_ENDPOINT
    timeout_seconds: float = 90.0

    def call(self, method: str, params: Optional[Dict[str, Any]] = None,
             timeout_seconds: Optional[float] = None) -> Dict[str, Any]:
        """One request. Returns the parsed `result` dict on success;
        raises RpcError on a structured failure, RpcTransportError on a
        transport failure. Raising (rather than returning an envelope)
        keeps executor retry logic linear."""
        body: Dict[str, Any] = {
            "protocolVersion": 1,
            "requestId": uuid4().hex[:8],
            "method": method,
        }
        if params:
            body["params"] = params
        data = json.dumps(body).encode("utf-8")
        request = urllib.request.Request(
            self.endpoint, data=data, headers={"Content-Type": "application/json"}
        )
        try:
            with urllib.request.urlopen(
                request, timeout=timeout_seconds or self.timeout_seconds
            ) as response:
                payload = json.loads(response.read().decode("utf-8"))
        except (OSError, ValueError) as exc:  # URLError subclasses OSError
            raise RpcTransportError(f"{method}: {exc}") from exc
        if not payload.get("success", False):
            error = payload.get("error") or {}
            raise RpcError(
                str(error.get("code", "UNKNOWN")),
                str(error.get("message", "")),
                method,
            )
        return payload.get("result", {})

    # -- tiny conveniences the live sessions used constantly ------------

    def full_id(self, short_or_full_id: str) -> str:
        """Accepts 'Build_X_C_123' or an already-full path id."""
        if short_or_full_id.startswith("/"):
            return short_or_full_id
        return LEVEL_PREFIX + short_or_full_id

    def teleport(self, x: float, y: float, z: float) -> None:
        self.call(
            "world.teleportPlayer",
            {"x": float(x), "y": float(y), "z": float(z), "ignoreGroundTrace": True},
        )
