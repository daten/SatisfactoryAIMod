"""Machine-free belt helix ("tornado"). Parametrized helix built from scratch.
Keep total height under ~7600 so NO perches are needed - ground teleports only
(reach also self-corrects where terrain rises with radius).

Usage: py tornado_dense.py <wp_start> <wp_end_exclusive> <dz_per_rev>
Waypoint 0 = first pole. Center and revolution count can be overridden with the
TORNADO_CX / TORNADO_CY / TORNADO_REVS env vars (default = the original site).
DZ (vertical rise per revolution) is the density knob: ~100 = pole-height-tight
(hits a ~5-9% clearance-overlap gap floor); ~150+ builds essentially gap-free.
"""
import math, os, sys, time
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError

WP_A, WP_B, DZ = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
c = RpcClient()
POLE = '/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorPole.Recipe_ConveyorPole_C'
BELT = '/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C'
FLAGS = {'ignoreGroundTrace': True, 'ignoreInvalidFloor': True, 'ignoreAimLocation': True,
         'ignorePlayerEncroachment': True, 'gridSnapSize': 0}
CX = float(os.environ.get('TORNADO_CX', 27200.0))
CY = float(os.environ.get('TORNADO_CY', 280000.0))
R0, Z0 = 1400.0, 400.0
DR = 100.0        # radius growth per revolution
REVS = int(os.environ.get('TORNADO_REVS', 72))
CHORD = 2400.0

def rev_points(n):
    r_avg = R0 + DR * (n + 0.5)
    step = 2.0 * math.degrees(math.asin(min(0.9, CHORD / (2.0 * r_avg))))
    return max(12, math.ceil(360.0 / step))

WAYPTS = []
for n in range(REVS):
    pts = rev_points(n)
    for j in range(pts):
        f = j / pts
        WAYPTS.append((n * 360.0 + 360.0 * f, R0 + DR * (n + f), Z0 + DZ * (n + f)))
WAYPTS.append((REVS * 360.0, R0 + DR * REVS, Z0 + DZ * REVS))  # closing top point
TOTAL = len(WAYPTS)

def xyz(i):
    a, r, z = WAYPTS[i]
    ar = math.radians(a)
    return (CX + r * math.cos(ar), CY + r * math.sin(ar), z)

def yaw_back(i):
    x, y, _ = xyz(i)
    px, py, _ = xyz(i - 1) if i > 0 else xyz(1)
    return math.degrees(math.atan2(py - y, px - x))

def call_retry(method, params, tries=4, pause=0.6):
    # Catch transport errors too: a single connection blip (WinError 10054 - the
    # game HTTP server dropping the socket during a frame hitch) must not kill a
    # long build. Back off and retry; the server usually recovers.
    last = None
    for _ in range(tries):
        try:
            return c.call(method, params)
        except (RpcError, RpcTransportError) as e:
            last = e
            time.sleep(pause)
    raise last

def find_pole_at(i):
    x, y, z = xyz(i)
    b = c.call('world.buildables', {'minX': x-140, 'minY': y-140, 'maxX': x+140, 'maxY': y+140})
    for r in b.get('buildables', []):
        if 'Build_ConveyorPole_C' in r['id'] and abs(r['position']['z'] - z) < 60:
            return r['id']
    return None

# Safe teleport via a FLOATING PLATFORM. Landing the player on raw terrain is a
# player-killer: over uneven/deep ground the pawn falls to its death or dies
# underground (both leave NO_PLAYER, and there is no respawn RPC), and terrain
# reads are unreliable to boot. Instead we float an 8x1 foundation at a chosen
# altitude and stand the player on IT - terrain below becomes irrelevant. The
# perch moves with the work (place the new one, land on it, THEN delete the old,
# so the player always has ground). Calibrated live: foundation top = placement
# z + 50; teleporting to top+170 lands the pawn stably (no fall-through).
FND = '/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C'
PERCH_FLAGS = dict(FLAGS, ignoreClearance=True)
PERCH_DROP = 550.0   # place the perch this far BELOW the belt plane so its
                     # clearance never interferes with belt validation
FND_HALF = 50.0      # 8x1 foundation half-thickness (top = placement z + FND_HALF)
_perch = {'id': None}

def _player_pos():
    try:
        p = c.call('world.player').get('position', {})
        if p and (p.get('x'), p.get('y'), p.get('z')) != (0, 0, 0):
            return p
    except (RpcError, RpcTransportError):
        pass
    return None

def _perch_at(x, y, z):
    """Float a foundation whose top sits at ~z and stand the player on it."""
    pz = z - FND_HALF
    try:
        new_id = call_retry('world.placeBuilding',
                            {'recipeClass': FND, 'x': x, 'y': y, 'z': pz, 'yaw': 0, **PERCH_FLAGS})['buildableId']
    except (RpcError, RpcTransportError):
        return False
    try:
        c.call('world.teleportPlayer', {'x': x, 'y': y, 'z': pz + FND_HALF + 170})
    except (RpcError, RpcTransportError):
        pass
    time.sleep(0.4)
    old = _perch['id']
    _perch['id'] = new_id
    if old:                      # delete the previous perch only after the player is on the new one
        try:
            c.call('world.deleteBuilding', {'buildableId': old})
        except (RpcError, RpcTransportError):
            pass
    return True

def _safe_tp(x, y, z):
    # perch a bit below the work: clear of the belt plane, player still within reach
    _perch_at(x, y, z - PERCH_DROP)

def walk_in(tx, ty, tz, hop=3000.0):
    """Hop the player to (tx,ty) on floating perches at a fixed high altitude
    (above any terrain, incl. canyons) so crossing to a far site never risks a
    terrain death; the final perch drops to the build altitude."""
    p = _player_pos()
    if not p:
        return
    px, py = p['x'], p['y']
    alt = max(p['z'] + 300.0, 3000.0)   # cruising altitude, clear of terrain
    dist = math.hypot(tx - px, ty - py)
    steps = max(1, int(dist // hop))
    for s in range(1, steps + 1):
        _perch_at(px + (tx - px) * s / steps, py + (ty - py) * s / steps, alt)

def tp_under(i):
    x, y, z = xyz(i)
    _safe_tp(x, y, z)

def cleanup_perch():
    if _perch['id']:
        try:
            c.call('world.deleteBuilding', {'buildableId': _perch['id']})
        except (RpcError, RpcTransportError):
            pass
        _perch['id'] = None

def belt(src, dst):
    try:
        c.call('world.connectConveyor', {'recipeClass': BELT, 'sourceBuildableId': src, 'destBuildableId': dst})
        return True
    except (RpcError, RpcTransportError) as e:
        return str(e)   # transport blip -> treated as a gap, retried in the fixer

end_i = min(WP_B, TOTAL)
prev_id = find_pole_at(WP_A - 1) if WP_A > 0 else None
if WP_A > 0 and not prev_id:
    raise SystemExit(f'cannot resolve previous pole at wp {WP_A-1}')
print(f'dense build: wp {WP_A}..{end_i-1} of {TOTAL}, dz/rev={DZ}', flush=True)

# Bail loudly if there is no live player - every placeBuilding/teleport would
# just NO_PLAYER, and there is no RPC to respawn (a manual respawn/reload is
# required). Then walk the player safely to the first waypoint.
if _player_pos() is None:
    raise SystemExit('NO_PLAYER: respawn or reload a save before building')
_fx, _fy, _fz = xyz(WP_A)
walk_in(_fx, _fy, _fz)

pending = []
t0 = time.time()
# Re-position by DISTANCE, not waypoint count: conveyor validation fails when the
# player is beyond ~5000 units of the connection, and at large radius a fixed
# waypoint stride spans a whole loop (>5000 across). Teleport whenever the work
# moves TP_DIST horizontally so the player stays close the entire climb.
last_txy = None
TP_DIST = 2000.0
for i in range(WP_A, end_i):
    x_i, y_i, _ = xyz(i)
    if last_txy is None or math.hypot(x_i - last_txy[0], y_i - last_txy[1]) > TP_DIST:
        tp_under(i)
        last_txy = (x_i, y_i)
        print(f'wp {i}: r={WAYPTS[i][1]:.0f} z={WAYPTS[i][2]:.0f} pending={len(pending)} '
              f'[{time.time()-t0:.0f}s]', flush=True)
    try:
        pid = call_retry('world.placeBuilding', {'recipeClass': POLE, **dict(zip(('x','y','z'), xyz(i))),
                                                 'yaw': yaw_back(i), **FLAGS})['buildableId']
    except RpcError as e:
        if 'NO_PLAYER' in str(e):
            raise SystemExit(f'ABORT at wp{i}: player died (NO_PLAYER) - respawn/'
                             f'reload and restart from this waypoint. Built {i-WP_A} poles.')
        print(f'  POLE FAIL wp{i}: {str(e)[:90]}', flush=True)
        prev_id = None
        continue
    if prev_id:
        r = belt(prev_id, pid)
        if r is not True:
            r2 = belt(pid, prev_id)
            if r2 is not True:
                pending.append((i, prev_id, pid, str(r)[:80]))
    prev_id = pid

# Lean healer: re-teleport ON TOP of the gap and retry both directions a few
# times (belt validation sometimes needs a settle tick). No mid-segment helper
# poles - subdivision left clutter and half-built belts. A miss is a clean gap
# between two real poles, which reads fine in a dense mass and is trivial to
# sweep later.
print(f'fixer pass: {len(pending)}', flush=True)
still = []
for i, s, d, err in pending:
    x, y, z = xyz(i); px, py, pz = xyz(i-1)
    mx, my = (x+px)/2, (y+py)/2
    _safe_tp(mx, my, (z+pz)/2)
    ok = False
    for _ in range(3):
        time.sleep(1.0)
        if belt(s, d) is True or belt(d, s) is True:
            ok = True; break
    if ok:
        print(f'  wp{i} healed', flush=True)
    else:
        still.append((i, str(err)[:70]))
gaps = len(still)
span = max(1, end_i - WP_A)
print(f'CHUNK DONE {time.time()-t0:.0f}s, wp {WP_A}..{end_i-1}, '
      f'gaps: {gaps}/{span} ({100*gaps/span:.0f}%)', flush=True)
if still:
    print('  gap wps:', [i for i, _ in still], flush=True)
# Leave the player standing on the final perch (deleting it would drop the pawn).
# The single leftover foundation is cleaned up separately once the player has been
# moved to safe ground.
print(f'final perch id (player is standing on it): {_perch["id"]}', flush=True)
