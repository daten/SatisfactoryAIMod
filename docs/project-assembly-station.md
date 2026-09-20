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

- **Width (horizontal): half-width ≥ 955,415 units (≥9.55 km) EAST, edge not yet reached.**
  With the station at origin 50,000, the player drove a straight line EAST from center
  (−61,300, 210,500). Waypoints, all still on the top surface at z=1,008,044:
  - r = **628,670** (x=567,369) — open flat top (renders invisibly).
  - r = **955,415** (x=894,038, y=222,632) — outer edge of a **raised opaque section**
    (a visible structural ring); its top is at the SAME z=1,008,044, i.e. the walkable
    collision plane is uniform, only the rendering differs. A **lower ring extends
    further** and the collision may continue past here.
  The collision top has been **dead flat at z=1,008,044 across the whole 9.5 km run**.
  955,415 is already ~2.5× the entire play area's radius (~375,000).

  **TIERED STRUCTURE (found 2026-09-19):** the upper tier's outer edge is at ~r=989,525
  (drove the upper collision from the opaque ring at 955k out to ~990k, then fell off).
  Below it is a **wider lower tier** with collision top at **z≈1,004,458** (~3,600 units
  below the upper top), which extends further out. So the station is stepped, not a single
  flat disc. Falling off the upper edge = a short ~1,700u drop onto the lower tier, not a
  plunge to the ground.

  **ANGLED RAMPS / SLOPED OUTER EDGE (found 2026-09-19):** beyond the lower tier the
  collision **slopes down and outward** via invisible angled ramps toward a visible outer
  ring (drivable/standable on the slope). Waypoint on a ramp: r=**999,205**, z=**994,896**
  (13,148 below the upper top). So the station's rim is a sculpted descending disc, not a
  sheer edge. Radial profile so far (EAST): flat top z=1,008,044 out to the opaque ring
  r~955k → upper edge r~990k → lower tier z~1,004,458 → sloping ramps (r~999k, z~994,896)
  → visible outer ring (further out, lower).

  **OUTER EDGE REACHED (2026-09-19):** the visible edge is at radial **r ≈ 1,229,138
  units (12.29 km)**, z=**765,448** — the rim slopes down **242,596 units** from the top
  (1,008,044) over the outer descent (~41° average slope; steep enough to slide, needs
  bracing). So, station dimensions (EAST radius; assumed roughly radial):
  - **Outer radius ≈ 1,229,138 (12.29 km); diameter ≈ 2.46M units (~24.6 km)** — ~3.3× the
    play area's ~750k width.
  - **Height ≈ 958,044 above origin (~9.6 km)**; collision top flat at 1,008,044 (with
    origin at 50,000).
  Full radial profile (from center, east): flat top z=1,008,044 out to opaque ring r~955k
  → upper edge r~990k → lower tier z~1,004,458 → steep sloped rim (ramp waypoint r~999k
  z~994,896) descending to the visible outer edge r~1,229,138 z~765,448.
  A genuine small-moon-scale sculpted disc. (Only the EAST radius was walked/driven;
  symmetry assumed but not verified on other headings.)

## Gotchas

- `world.player` reads **(0,0,0)** while the player is DRIVING a vehicle (the pawn is
  possessed by the vehicle) — read the vehicle's position instead, or dismount to measure.
- Player altitude is **separately** hard-capped at ~z=2,440,000 (teleport destinations
  above it snap down) — see `world-boundary-hazards.md`. This is why the station at its
  default 2,350,000 could never be climbed above on foot; lowering it via
  `setProjectAssemblyHeight` is the way to view/reach it.
