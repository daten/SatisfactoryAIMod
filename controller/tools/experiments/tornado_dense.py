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
from satisfactory_ai.rpc_client import RpcClient, RpcError

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
    last = None
    for _ in range(tries):
        try:
            return c.call(method, params)
        except RpcError as e:
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

# Safe teleport. A hardcoded fallback z is a player-killer: teleporting far
# ABOVE local ground drops the pawn to its death (fatal fall), and far BELOW
# ground kills it underground - both leave NO_PLAYER with no RPC to respawn.
# So: prefer live groundHeight (valid once the player is local), else the last
# confirmed landing z, else the player's CURRENT z (a no-drop move). Only ever
# land ~150 above the reference. Read back to keep the ground estimate current
# as terrain changes with radius.
_last_gz = {'z': None}

def _player_pos():
    try:
        p = c.call('world.player').get('position', {})
        if p and (p.get('x'), p.get('y'), p.get('z')) != (0, 0, 0):
            return p
    except RpcError:
        pass
    return None

def _safe_tp(x, y):
    g = None
    try:
        g = c.call('world.groundHeight', {'x': x, 'y': y, 'z': 100})
    except RpcError:
        pass
    if g and g.get('found'):
        gz = g['z']
    elif _last_gz['z'] is not None:
        gz = _last_gz['z']
    else:
        cur = _player_pos()
        gz = (cur['z'] - 150.0) if cur else 80.0   # gz+150 == current z: no drop
    try:
        c.call('world.teleportPlayer', {'x': x, 'y': y, 'z': gz + 150})
    except RpcError:
        return
    time.sleep(0.4)
    p = _player_pos()
    if p:
        _last_gz['z'] = p['z']

def walk_in(tx, ty, hop=2500.0):
    """Hop the player to (tx,ty) in small steps so groundHeight stays valid and
    every teleport is a short, survivable move - avoids the fatal first-teleport
    fall when the build site is far from where the player currently stands."""
    p = _player_pos()
    if not p:
        return
    px, py = p['x'], p['y']
    dist = math.hypot(tx - px, ty - py)
    steps = max(1, int(dist // hop))
    for s in range(1, steps + 1):
        _safe_tp(px + (tx - px) * s / steps, py + (ty - py) * s / steps)

def tp_under(i):
    x, y, _ = xyz(i)
    _safe_tp(x, y)

def belt(src, dst):
    try:
        c.call('world.connectConveyor', {'recipeClass': BELT, 'sourceBuildableId': src, 'destBuildableId': dst})
        return True
    except RpcError as e:
        return str(e)

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
_fx, _fy, _ = xyz(WP_A)
walk_in(_fx, _fy)

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
    try:
        g = c.call('world.groundHeight', {'x': mx, 'y': my, 'z': 100})
        gz = g['z'] if g.get('found') else 80.0
        c.call('world.teleportPlayer', {'x': mx, 'y': my, 'z': gz+150})
    except RpcError:
        pass
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
