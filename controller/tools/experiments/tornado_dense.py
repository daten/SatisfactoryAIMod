"""Dense belt tornado v2 - tight layer spacing (user: gap should be ~10% of
the 624/rev original). Parametrized helix built from scratch; total height
stays under ~7600 so NO perches are needed - ground teleports only.

Usage: py tornado_dense.py <wp_start> <wp_end_exclusive> <dz_per_rev>
Waypoint 0 = first pole. Same center as before.
"""
import math, sys, time
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError

WP_A, WP_B, DZ = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3])
c = RpcClient()
POLE = '/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorPole.Recipe_ConveyorPole_C'
BELT = '/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C'
FLAGS = {'ignoreGroundTrace': True, 'ignoreInvalidFloor': True, 'ignoreAimLocation': True,
         'ignorePlayerEncroachment': True, 'gridSnapSize': 0}
CX, CY = 27200.0, 280000.0
R0, Z0 = 1400.0, 400.0
DR = 100.0        # radius growth per revolution
REVS = 72
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

def tp_under(i):
    x, y, _ = xyz(i)
    try:
        g = c.call('world.groundHeight', {'x': x, 'y': y, 'z': 100})
        gz = g['z'] if g.get('found') else 80.0
        c.call('world.teleportPlayer', {'x': x, 'y': y, 'z': gz + 150})
    except RpcError:
        pass

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

pending = []
t0 = time.time()
last_tp = -999
for i in range(WP_A, end_i):
    if i - last_tp >= 24:            # re-position player every ~2 revolutions
        tp_under(i)
        last_tp = i
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

print(f'fixer pass: {len(pending)}', flush=True)
still = []
for i, s, d, err in pending:
    x, y, z = xyz(i); px, py, pz = xyz(i-1)
    mx, my, mz = (x+px)/2, (y+py)/2, (z+pz)/2
    try:
        g = c.call('world.groundHeight', {'x': mx, 'y': my, 'z': 100})
        gz = g['z'] if g.get('found') else 80.0
        c.call('world.teleportPlayer', {'x': mx, 'y': my, 'z': gz+150})
    except RpcError:
        pass
    ok = False
    for _ in range(5):
        time.sleep(1.5)
        if belt(s, d) is True or belt(d, s) is True:
            ok = True; break
    if ok:
        print(f'  wp{i} healed', flush=True)
    else:
        try:
            hyaw = math.degrees(math.atan2(py-my, px-mx))
            h = call_retry('world.placeBuilding', {'recipeClass': POLE, 'x': mx, 'y': my, 'z': mz,
                                                   'yaw': hyaw, **FLAGS})['buildableId']
            time.sleep(0.5)
            r1 = belt(s, h); time.sleep(0.5); r2 = belt(h, d)
            if r1 is True and r2 is True:
                print(f'  wp{i} healed via subdivide', flush=True)
            else:
                still.append((i, err, str(r1)[:60], str(r2)[:60]))
        except RpcError as e:
            still.append((i, err, 'helper-fail', str(e)[:60]))
print(f'CHUNK DONE {time.time()-t0:.0f}s, wp {WP_A}..{end_i-1}, unresolved: {still}', flush=True)
