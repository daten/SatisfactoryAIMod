# Orbital Space Station (Project Assembly) — dimensions & control

2026-09-19. What we know about the end-game orbital station, the `AFGProjectAssembly`
actor that assembles above the Space Elevator as project phases complete.

## Control RPCs

- **`world.projectAssembly`** (read): position, launch-sequence/movement state,
  phase→visual-stage map, all game-phase assets, current/target phase.
- **`world.setProjectAssemblyVisualPhase`** (write, commit da4f7a39b0 era): drives
  the visual build phase without touching real progression (session-only).
- **`world.setProjectAssemblyHeight(z)`** (write, commit da4f7a39b0): moves the
  station to any world Z. **Session-only** — `mProjectAssemblyHeight` is not a
  SaveGame field, so a reload restores the default. Holds against the BP tick (no
  snap-back); live-verified at z = 50,000 / 2,000 / 2,342,000.

## Position

- **Default height: z = 2,350,000** (absolute world Z, ~23.5 km up).
- **NOT relative to the Space Elevator's ground Z**: elevator sat at z=3,380, station
  at exactly 2,350,000. Height is a fixed constant.
- **XY tracks the Space Elevator exactly** — the station hovers directly above it
  (both at x=−61,300, y=210,500 in the test save). Move/rebuild the elevator and the
  station moves horizontally with it, but never changes altitude.

## Measured dimensions

The station is **moon-scale**. Trace/clearance probes are USELESS for measuring it —
they sample its interior and report false "sky/atmosphere INSIDE" bands. The only
reliable method is **putting the player on its surface and reading the pawn position**.

- **Height (vertical): collision top ≈ 958,044 units (~9.6 km) above the origin.**
  Measured by lowering the station to origin z=50,000 and free-falling the player onto
  it — the pawn landed on solid collision at **z=1,008,044**. The top:
  - has real collision and is survivable to land on (barely — terminal velocity ~4,050 u/s nearly killed the player),
  - renders **invisibly** up close (an "invisible boundary" you stand on),
  - is **flat and drivable** — a wheeled vehicle traverses it.

- **Width (horizontal): half-width ≥ 628,670 units (≥6.29 km), edge not yet reached.**
  With the station at origin 50,000, the player drove a straight line EAST from center
  and at radial distance **628,670** (x=567,369) was still on the top surface. That is
  already ~1.7× the entire play area's radius (~375,000). User visual estimate: roughly
  halfway, so the east radius may be ~1.26M units (~12.6 km). **Measurement in progress
  / paused** — finish by continuing east until the collision surface ends, or by
  edge-sampling (walk probes outward from center, find where z=1,008,044 collision stops).

## Gotchas

- `world.player` reads **(0,0,0)** while the player is DRIVING a vehicle (the pawn is
  possessed by the vehicle) — read the vehicle's position instead, or dismount to measure.
- Player altitude is **separately** hard-capped at ~z=2,440,000 (teleport destinations
  above it snap down) — see `world-boundary-hazards.md`. This is why the station at its
  default 2,350,000 could never be climbed above on foot; lowering it via
  `setProjectAssemblyHeight` is the way to view/reach it.
