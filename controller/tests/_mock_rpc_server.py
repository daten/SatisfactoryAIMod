"""Throwaway mock of DocMod's /rpc endpoint, used only to sanity-check
live_check.py's HTTP client logic without needing the real game running.
Not part of the package - deleted after use, or kept as a dev utility if
useful later for offline testing of the live_check script itself.
"""

import json
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class MockRpcHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):  # noqa: A002 - silence default logging
        pass

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length))
        method = body.get("method")
        request_id = body.get("requestId", "")

        if method == "world.resourceNodes":
            result = {
                "success": True,
                "response": {
                    "protocolVersion": 1,
                    "requestId": request_id,
                    "success": True,
                    "result": {
                        "protocolVersion": 1,
                        "resourceNodes": [
                            {
                                "id": "mock1", "resource": "Iron Ore", "resourceClass": "/Game/Mock.Mock_C",
                                "purity": "Pure", "position": {"x": 0, "y": 0, "z": 0}, "occupied": False,
                            }
                        ],
                    },
                },
            }
        elif method == "world.buildables":
            result = {"response": {"protocolVersion": 1, "requestId": request_id, "success": True,
                       "result": {"protocolVersion": 1, "buildables": []}}}
        elif method == "world.manufacturers":
            result = {"response": {"protocolVersion": 1, "requestId": request_id, "success": True,
                       "result": {"protocolVersion": 1, "manufacturers": [
                           {"id": "m1", "buildableClass": "/Game/Mock.Mock_C", "position": {"x": 0, "y": 0, "z": 0},
                            "recipe": "Iron Plate", "clockSpeedPercent": 100.0, "productionStatus": "Producing",
                            "productionProgress": 0.5, "productivity": 1.0, "inputInventory": [], "outputInventory": []}
                       ]}}}
        elif method == "world.connections":
            result = {"response": {"protocolVersion": 1, "requestId": request_id, "success": True,
                       "result": {"protocolVersion": 1, "connections": []}}}
        elif method == "world.setClockSpeed":
            params = body.get("params", {})
            if params.get("buildableId") == "__live_check_nonexistent__":
                result = {"response": {"protocolVersion": 1, "requestId": request_id, "success": False,
                           "error": {"code": "TARGET_NOT_FOUND", "message": "no such buildable"}}}
            else:
                result = {"response": {"protocolVersion": 1, "requestId": request_id, "success": True, "result": {}}}
        else:
            result = {"response": {"protocolVersion": 1, "requestId": request_id, "success": False,
                       "error": {"code": "UNKNOWN_METHOD", "message": f"unknown method {method}"}}}

        response_body = json.dumps(result["response"]).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(response_body)))
        self.end_headers()
        self.wfile.write(response_body)


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 51902
    server = HTTPServer(("127.0.0.1", port), MockRpcHandler)
    print(f"Mock RPC server listening on http://127.0.0.1:{port}/rpc")
    server.serve_forever()
