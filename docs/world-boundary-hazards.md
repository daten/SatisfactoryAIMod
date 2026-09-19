# World-boundary hazards (map-edge damage zones)

2026-09-19. Source research + implementation notes for the four hazard RPCs:
`world.damageVolumes`, `world.probeHazard`, `world.setDamageVolumeEnabled`,
`world.despawnDamageVolume`. Status: **compiled, NOT yet live-tested** —
update this line after the next redeploy's live pass.

## What the boundary actually is

The harmful map border is **not a wall or a build restriction**. It is a set of
level-placed `AFGDamageOverTimeVolume` actors (`FGDamageOverTimeVolume.h`) —
plain `AVolume` brushes each carrying:

- a `UFGDotComponent` that applies a `UFGDamageOverTime`
  (damage types + `mDamageInterval`) to overlapping actors, and
- an optional post-process settings class (the red boundary vignette).

Damage is purely overlap-driven (`OnActorBeginOverlap` →
`RegisterDOTForActor`). Construction/holograms never consult these volumes,
which is exactly why RPC builds succeed outside the border while the player
dies there.

Direct source confirmation that this mechanism IS the world perimeter:
`UFGDamageType::mDestroyVehicles` (`DamageTypes/FGDamageType.h`) is
documented *"True if this damage type should immediately destroy vehicles upon
contact (e.g. world perimeter damage)"*.

Poison-gas areas use the same DOT mechanism, so `world.damageVolumes` returns
those too — edge volumes are distinguishable by enormous size and perimeter
placement. Two other boundary mechanisms exist and are reported alongside:

- **`KillZ`** (`AWorldSettings`) — instant death below a global Z (the void).
- **`UFGMapFunctionLibrary::GetWorldBounds`** — the minimap's 2D extent.
  Informational only; it is NOT the damage line.

## RPC behavior notes

- **Ids** are actor path names, same convention as `world.waterVolumes`.
- **AABBs overestimate** rotated/non-box brushes. `world.probeHazard` does the
  brush-accurate point test (`EncompassesPoint`) — bisect with it when the
  exact surface matters. It also reports the distance to the nearest
  non-containing volume to guide the bisection.
- **Disable ordering** (`SetDamageVolumeEnabled(false)`): collision is turned
  off FIRST so the resulting end-overlap events unregister the DOT from anyone
  currently inside while the `UFGDotComponent` is still active; only then is
  the component deactivated. Enabling reverses the order.
- **Despawn vs disable**: `despawnDamageVolume` = `Destroy()`, which also
  tears down the volume's post-process vignette
  (`PostUnregisterAllComponents`). A disabled-but-alive volume keeps its
  vignette registered — harmless but visible.
- **Session-only, by design**: runtime changes to map-placed level actors are
  not written to the save. Disabled/despawned volumes are back (and lethal)
  after any save load. Treat "the border is off" as valid only within the
  current session.
- `mDotClass` on `UFGDotComponent` is a protected UPROPERTY with no public
  getter — the mod reads it via `FindFProperty`/`GetPropertyValue_InContainer`
  (internal telemetry sourcing only; no generic property access is exposed
  through the protocol).

## Flagged unknowns for the live test

1. Whether the map-edge volumes are literally `AFGDamageOverTimeVolume` or a
   Blueprint subclass of it (`TActorIterator` on the base class catches
   subclasses either way — but an EMPTY result means the boundary uses some
   other mechanism entirely and this doc's premise needs revisiting).
2. Whether `EncompassesPoint`'s real (stub-sourced here) implementation is
   brush containment — cross-check `probeHazard` against a volume's own AABB
   interior before trusting bisection results.
3. Whether damage stops IMMEDIATELY for a player already standing inside a
   volume when it is disabled/despawned (the end-overlap unregister theory).
   Test exactly that: stand in the border zone, disable, watch health.
4. Whether `Destroy()` succeeds on these level brush actors (it should — the
   failure path returns `OPERATION_FAILED` and leaves the volume inert with
   collision off, which is a safe residual state).

## Live-test recipe (safe)

1. `world.damageVolumes` → pick the giant perimeter volume nearest a map edge;
   note its AABB.
2. `world.probeHazard` at a point just inside that AABB and just outside it —
   sanity-check containment agrees with geometry.
3. Save the game first (standing discipline). Teleport to solid ground just
   OUTSIDE the volume, walk in briefly to confirm damage ticks, back out.
4. `world.setDamageVolumeEnabled(enabled=false)` → walk in → confirm no
   damage → re-enable → confirm damage returns.
5. `world.despawnDamageVolume` → confirm volume gone from `world.damageVolumes`
   and the boundary vignette no longer appears; walk the same spot unharmed.
6. Reload the save → confirm the volume is back (session-only expectation).
