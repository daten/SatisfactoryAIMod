# World-boundary hazards (map-edge damage zones)

2026-09-19. Source research + implementation notes for the four hazard RPCs:
`world.damageVolumes`, `world.probeHazard`, `world.setDamageVolumeEnabled`,
`world.despawnDamageVolume`. Status: **ALL FOUR LIVE-TESTED 2026-09-19**
(same day, post-Alpakit redeploy) — enumeration, 9 probe points, disable →
re-enable state round-trip, and a real `Destroy()` despawn (15 → 14 volumes)
all confirmed. The one thing still untested is damage actually stopping for a
player ALREADY standing inside a volume at the moment it is disabled
(unknown #3 below); everything else is verified.

## LIVE RESULTS — the real map boundary (GameLevel01, measured)

15 `AFGDamageOverTimeVolume` actors, a **two-tier design**: thin "warning"
volumes dealing 1 damage / 0.2 s where the border experience starts, backed by
1000 damage / 0.2 s instant-kill slabs further out. Every one uses
`BP_PointDamageType_WorldBounds_C` with `destroysVehicles=true` and
`playerAlwaysVulnerable=true` (god mode does not negate the border). Dot
classes name their role outright: `BP_DoTWorldPerimeter_C`,
`BP_DoTWorldBottom_C`, `BP_DoTWorldTop_C`.

Playable envelope in world units (damage begins past the warning face;
lethal = the 1000-dmg slab face):

| Direction | Warning begins | Lethal begins |
|---|---|---|
| West (−X) | −341,430 | −391,740 |
| East (+X) | +450,090 | +499,210 |
| South (−Y) | −370,000 | −430,825 |
| North (+Y) | +334,875 | +385,205 |
| Up (+Z) | +200,000 (2 km) | +250,000 (2.5 km) |
| Down (−Z) | −24,400 | −34,400 |

Plus three diagonal corner wedges (rotated brushes: Volume17 NE, Volume67 NW,
Volume18_UAID SW) — their AABBs hugely overestimate; probeHazard resolves the
true diagonal (verified: a playable-land point inside Volume17's AABB probes
`inside=false`).

Surprises worth knowing:

- **The sky above z = +450,000 (4.5 km) is hazard-free in vanilla** — the top
  warning layer spans 2–4 km and the lethal layer 2.5–4.5 km; above that,
  nothing until engine limits.
- **A free void layer exists under the map**: the bottom lethal slab ends at
  z = −434,400 (−4.34 km); from there down to `KillZ` there is no hazard.
- **`KillZ` = −1,048,575** — exactly the UE engine default
  (−`UE_OLD_HALF_WORLD_MAX1`); the map does NOT set its own. The death plane,
  not a DOT volume, is what kills below −10.49 km.
- `worldBounds2D` (minimap): x −324,698..+425,302, y −375,000..+375,000 —
  sits inside the warning faces, as expected.

**Id stability caveat**: volume ids are World Partition generated-cell paths
(`.../Persistent_Level/_Generated_/<HASH>.Persistent_Level:PersistentLevel.
FGDamageOverTimeVolume86`). Do not persist them across sessions — re-fetch
from `world.damageVolumes` each session before disabling/despawning.

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

## Flagged unknowns — live-test outcomes (2026-09-19)

1. ~~Are the edge volumes literally `AFGDamageOverTimeVolume`?~~ **YES** —
   15 found by the base-class iterator, names/dot classes explicitly
   World-Perimeter/Bottom/Top.
2. ~~Is `EncompassesPoint` brush containment?~~ **YES, brush-accurate** —
   a playable point inside rotated Volume17's AABB correctly probed
   `inside=false`; the diagonal corner wedges resolve properly.
3. Does damage stop IMMEDIATELY for a player already standing inside when
   the volume is disabled/despawned (end-overlap unregister theory)?
   **STILL UNTESTED** — needs the player physically in the border zone;
   test: stand in a warning band, disable, watch health, re-enable.
4. ~~Does `Destroy()` succeed on level brush actors?~~ **YES** — Volume61
   (the 2.5–4.5 km lethal top slab) despawned cleanly, enumeration dropped
   15 → 14. (Left despawned for that session; returns on save load.)

## Remaining live test (needs the player in the zone)

Everything RPC-side is verified; what's left is the in-zone damage-stop
confirmation (unknown #3). Recipe: save first (standing discipline); teleport
to solid ground just outside the east warning band (x ≈ 445,000, terrain
permitting), walk east past x = 450,090 until damage ticks, back out;
`world.setDamageVolumeEnabled(Volume6, false)` (+ Volume17, which overlaps
there) → walk in again → confirm no damage WHILE STANDING INSIDE when it is
toggled off; re-enable → confirm damage resumes; reload the save → confirm
volumes are live again (session-only expectation). The despawned-volume
half of that check also confirms the boundary vignette disappears.

## 2026-09-19 — Player altitude ceiling (teleport snap-down), live-found

Separate from the DOT volumes and KillZ: there is a **hard player ceiling at
z ≈ 2,440,000** (a "transparent boundary" — blocks the player but NOT
construction, and blocks visibility/ground traces which read INSIDE-solid at
2,440,000+). Found while visiting the space station (actor origin z=2,350,000;
its mesh towers ~90km-scale above that into the sky layer).

Key behavior: `world.teleportPlayer` to any z ABOVE the boundary reports
success but the engine's TeleportTo collision-sweep **snaps the pawn down to
~2,440,000** (the boundary top), which is not standable, so the player then
falls. Verified: requested z=2,445,450 → landed 2,440,304 → fell. This is the
true cause of the earlier "fell off the 2,750,000 pad" incidents — the pawn
never reached the high pad; it was snapped to the boundary and dropped.
Floating foundations/catwalks CAN be built above the boundary (construction
ignores it), but no player can ever stand on them.

Practical rule: the highest a player can be placed is ~2,439,000 (just under
the boundary). Pads at 2,365,050 and 2,432,050 worked (below); 2,750,000
failed (above). For any sky build the player must interact with, keep it below
z 2,440,000. Rescue from a fall = instant ground teleport (works at any z).
