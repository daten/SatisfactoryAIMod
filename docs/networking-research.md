# Localhost transport research (Phase 9)

Recorded 2026-08-24. Read-only research before implementing PLAN.md Phase 9
("Localhost Transport"), same evidence-based approach as
[resource-node-research.md](resource-node-research.md).

## Decision: `HTTPServer` runtime module

The engine ships a built-in `HTTPServer` module
(`Engine/Source/Runtime/Online/HTTPServer/`, UBT module name `"HTTPServer"`,
`Build.cs` at `.../HttpServer.Build.cs:5` — private deps `Core`, `HTTP`,
`Sockets`, so AIMod only needs to add `"HTTPServer"` itself, not those).
This is a Runtime module, not a plugin — no `.uplugin` enablement needed.

Two alternatives exist in this engine and were rejected:

- **`Sockets` module** (`Engine/Source/Runtime/Sockets/`) — only the base
  `FSocket`/`ISocketSubsystem`/`FInternetAddr` API
  (`Public/IPAddress.h`). Would require hand-rolling accept/read loops.
- **`Networking` module's `FTcpListener`**
  (`Engine/Source/Runtime/Networking/Public/Common/TcpListener.h`) — an
  `FRunnable` that spawns its own `FRunnableThread` and invokes
  `ConnectionAcceptedDelegate.Execute(...)` directly from that background
  thread (`TcpListener.h`, `Run()`), not the game thread. Using this would
  require exactly the manual game-thread marshaling (`AsyncTask(
  ENamedThreads::GameThread, ...)`) CLAUDE.md's Threading section warns
  about for any network-triggered game-world access.

`HTTPServer` avoids that: `FHttpServerModule` derives from
`FTSTickerObjectBase`, and its `Tick()` — which drives
`AcceptConnections()`/`TickConnections()` — asserts
`check(IsInGameThread())` (`HttpServerModule.cpp:111`, confirmed directly).
So route handler callbacks already execute on the game thread; no manual
marshaling is needed for the request-handling path itself (still marshal
for any async/deferred work kicked off *from inside* a handler).

SML has zero existing networking code (`Mods/SML/Source/SML` grepped for
Socket/TcpListener/HttpServer — one unrelated match, a Blueprint attach
socket *name* string, nothing networking-related). Nothing to build on
there.

## Critical finding: the project's default HTTPServer bind address is unsafe

`Config/DefaultEngine.ini` (already present in this repo, not something
this session added) contains:

```ini
[HTTPServer.Listeners]
DefaultBindAddress=any
```

Traced to `Engine/Source/Runtime/Online/HTTPServer/Private/HttpServerConfig.cpp:17`
(`GConfig->GetString(..., TEXT("DefaultBindAddress"), ...)`) and
`HttpListener.cpp:64-66` (`"any"` → `BindAddress->SetAnyAddress()`, i.e.
`0.0.0.0` — all interfaces). This is the opposite of CLAUDE.md's explicit
requirement to "bind only to loopback by default" / "do not expose the API
to the LAN by default." This setting is project-wide, presumably intended
for some other FactoryGame/Epic Online Services HTTP use, not something
AIMod should change globally — doing so could affect other systems that
rely on the current default, and is out of scope for a mod-specific safety
requirement.

**Fix used: a per-port override, not a global change.** The same file's
`ListenerOverrides` array (parsed in `HttpServerConfig.cpp:24-51`) lets a
specific port force `BindAddress=localhost` regardless of the global
default. Exact parse format confirmed by reading the parser directly
(the key is `Port=`, not `ListenerPort=` as initially guessed by an
automated search — verified against `HttpServerConfig.cpp:34`
`FParse::Value(*ListenerConfigStr, TEXT("Port="), ConfiguredPort)`):

```ini
[HTTPServer.Listeners]
DefaultBindAddress=any
+ListenerOverrides=(Port=51902,BindAddress=localhost)
```

This is an **additive** (`+`) entry — it does not remove or alter the
existing `DefaultBindAddress=any` line, so nothing else that may depend on
the global default is affected. Only port `51902` (AIMod's RPC port, see
below) is forced to loopback-only. Verified directly against
`HttpListener.cpp:68-71`: `BindAddress.Compare(TEXT("localhost"),
ESearchCase::IgnoreCase)` → `BindAddress->SetLoopbackAddress()`.

**This must be verified at runtime** (see
[manual-verification.md](manual-verification.md)) — e.g. `netstat -an` while
the game is running, confirming the listening socket shows `127.0.0.1:51902`
and not `0.0.0.0:51902`. Config parsing bugs or a mistyped port number would
silently defeat this and are not detectable from source alone.

## Confirmed broken live (2026-08-24): the project-level ini override doesn't reach the packaged deploy

The `netstat` check above was finally run for the first time against a
real Steam-launched session and **failed**: `0.0.0.0:51902 LISTENING`,
not `127.0.0.1:51902`. Root cause: the `+ListenerOverrides` entry above
lives in this dev workspace's project-level `Config/DefaultEngine.ini`,
which only applies to sessions run *from this project* (Development
Editor Play-In-Editor). The actual game the user plays is a separate,
already-installed Steam copy of Satisfactory
(`ExecutableName: FactoryGameSteam-Win64-Shipping.exe`, confirmed from
`FactoryGame.log`) that AIMod gets deployed *into* via Alpakit — Alpakit
packages the plugin, not this project's own top-level `Config/` folder,
so the override never reached the running game at all.

**Two-layer fix applied:**

1. **Defense-in-depth (verified as the real safety net, not
   config-dependent):** `UAIModHttpServerSubsystem::HandleRpcRequest`
   now checks `FHttpServerRequest::PeerAddress` directly and rejects
   anything that isn't `127.0.0.1`/`::1` with `403 FORBIDDEN`, before
   doing anything else with the request — regardless of what the socket
   is actually bound to. This is the fix to trust.
2. **Attempted root-cause fix, NOT yet verified:** added
   `Mods/GameFeatures/AIMod/Config/DefaultEngine.ini` with the same
   `+ListenerOverrides` entry — UE plugins can ship their own
   `Config/Default*.ini` files that get merged into the corresponding
   project config at startup, which (unlike the project-level file)
   should be part of what Alpakit packages and deploys with the plugin.
   **This is a hypothesis, not a confirmed fix** — GameFeature plugins
   specifically (AIMod is one) may have different config-merge timing
   than a regular always-on plugin, since they're designed to be
   activated/deactivated at runtime, potentially after the engine's
   normal startup ini-merging pass has already run. Needs another live
   `netstat` check after the next Alpakit deploy to confirm the socket
   itself now binds to `127.0.0.1`. Even if it works, item 1 above
   should stay in place as defense-in-depth.

## Plugin-level ini fix confirmed NOT working (2026-08-25)

Re-ran `netstat -an` (via the `grep ":51902"` filter) against a live
Steam session after deploying the plugin-level
`Mods/GameFeatures/AIMod/Config/DefaultEngine.ini` (added specifically
to test this) — **still `0.0.0.0:51902 LISTENING`**, not
`127.0.0.1:51902`. The hypothesis that a GameFeature plugin's own
`Config/DefaultEngine.ini` gets merged into the packaged build the same
way a regular always-on plugin's would has been tested and disproven, at
least for this specific Alpakit-deployed setup — the config-merge timing
concern flagged when this was added has turned out to be real.

**The application-layer fix (item 1 above,
`UAIModHttpServerSubsystem::HandleRpcRequest`'s `PeerAddress` check,
commit `870e1fdfee`) remains the actual protection** and is unaffected
by this — it doesn't depend on the socket binding at all. The raw socket
being reachable from the LAN is still not ideal (a determined actor on
the LAN could still open a TCP connection, just get rejected once a
request is sent), but it's not the same risk level as the original
finding (no requests are actually served to non-loopback peers). Root
cause of *why* the plugin ini doesn't get picked up is still unknown —
would need either further Unreal plugin-config-loading research, or
accepting the socket-level fix as out of reach for now and relying on
the application layer permanently rather than as a stopgap.

## API surface used

All confirmed directly by reading headers (not relying solely on
delegated research for anything load-bearing):

- `FHttpServerModule::Get().GetHttpRouter(uint32 Port, bool
  bFailOnBindFailure = false)` → `TSharedPtr<IHttpRouter>`
  (`HttpServerModule.h:55`).
- `IHttpRouter::BindRoute(const FHttpPath&, EHttpServerRequestVerbs,
  const FHttpRequestHandler&)` → `FHttpRouteHandle`, and `UnbindRoute(const
  FHttpRouteHandle&)` (`IHttpRouter.h:34,41`).
- `FHttpRequestHandler = TDelegate<bool(const FHttpServerRequest&, const
  FHttpResultCallback&)>` (`HttpRequestHandler.h`) — returning `true` means
  the delegate itself will (now or later) invoke `OnComplete`; `false`
  means it never will.
- `FHttpServerRequest::Body` is `TArray<uint8>` (raw bytes, not
  pre-decoded) (`HttpServerRequest.h:52`).
- `FHttpServerResponse::Create(const FString&, const FString&
  ContentType)` and `::Error(EHttpServerResponseCodes, ...)`
  (`HttpServerResponse.h:56,109`).
- `EHttpServerResponseCodes` (`HttpServerConstants.h`) — used `Ok=200`,
  `BadRequest=400`, `NotFound=404`, `RequestTooLarge=413` (not
  "RequestEntityTooLarge" as initially assumed — verified the real enum
  name directly), `ServerError=500`.
- Body bytes → `FString`: `StringCast<TCHAR>(reinterpret_cast<const
  UTF8CHAR*>(Body.GetData()), Body.Num())` — **not** `FUTF8ToTCHAR`'s
  pointer constructor, which this engine version marks deprecated
  (`StringConv.h:1017`, "please use StringCast<TCHAR>(...) instead").

## Port choice

`51902` — checked `Config/DefaultEngine.ini` for existing reserved ranges
first: Satisfactory itself uses `7777-7827` (`MinPort`/`MaxPort`, beacon/game
ports) and `443` (`ServerPort`, online services). `51902` is outside both
and not a well-known port. Configurable if it ever collides with something
else on a given machine — see `UAIModHttpServerSubsystem::ListenPort`.
