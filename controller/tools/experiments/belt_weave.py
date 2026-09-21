"""Woven conveyor grid: N E/W belts crossed with N N/S belts, alternating
over/under at every intersection in a checkerboard (basket weave). Built high in
the air on a fixed plane; the player rides a floating platform-perch (see
tornado_dense.py) so terrain is irrelevant.

At crossing (row i, col j): the E/W belt is HIGH when (i+j) is even else LOW; the
N/S belt is the opposite - so at every crossing one passes over the other, and
each row/column alternates. Each belt is a chain of pole-to-pole segments that
ramp between HIGH and LOW, which is what produces the weave.

Usage: py belt_weave.py <N> [pitch] [delta]   e.g. py belt_weave.py 20 300 200
Center via env WEAVE_CX / WEAVE_CY (default beside the gap-free tornado).
"""
import math, os, sys, time
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError

N     = int(sys.argv[1]) if len(sys.argv) > 1 else 20
PITCH = float(sys.argv[2]) if len(sys.argv) > 2 else 300.0
DELTA = float(sys.argv[3]) if len(sys.argv) > 3 else 200.0
CX = float(os.environ.get('WEAVE_CX', 47200.0))
CY = float(os.environ.get('WEAVE_CY', 294000.0))
LOW = 3000.0
HIGH = LOW + DELTA
c = RpcClient()
POLE = '/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorPole.Recipe_ConveyorPole_C'
BELT = '/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C'
FND  = '/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C'
F  = {'ignoreGroundTrace': True, 'ignoreInvalidFloor': True, 'ignoreAimLocation': True,
      'ignorePlayerEncroachment': True, 'gridSnapSize': 0}
PF = dict(F, ignoreClearance=True)
HALF = (N - 1) * PITCH / 2.0

OFF = 90.0   # shift the N/S pole grid off the E/W grid: co-located poles (same
             # x,y, different z) fail belt connection on clearance overlap.
def X(j): return CX - HALF + j * PITCH
def Y(i): return CY - HALF + i * PITCH
def z_ew(i, j): return HIGH if (i + j) % 2 == 0 else LOW
def z_ns(i, j): return LOW if (i + j) % 2 == 0 else HIGH

# ---- platform-perch (terrain-proof); re-perch by distance ----
_perch = {'id': None, 'xy': None}
def _player_ok():
    try:
        p = c.call('world.player').get('position', {})
        return p if p and (p.get('x'), p.get('y'), p.get('z')) != (0, 0, 0) else None
    except (RpcError, RpcTransportError):
        return None
def reperch(x, y):
    """Ensure the player is perched within reach of (x,y). Perch plane sits below
    the weave so it never fouls belt validation."""
    if _perch['xy'] and math.hypot(x - _perch['xy'][0], y - _perch['xy'][1]) < 3500:
        return
    pz = LOW - 600.0
    try:
        nid = c.call('world.placeBuilding', {'recipeClass': FND, 'x': x, 'y': y, 'z': pz, 'yaw': 0, **PF})['buildableId']
    except (RpcError, RpcTransportError):
        return
    try:
        c.call('world.teleportPlayer', {'x': x, 'y': y, 'z': pz + 50 + 170})
    except (RpcError, RpcTransportError):
        pass
    time.sleep(0.4)
    old = _perch['id']
    _perch['id'] = nid; _perch['xy'] = (x, y)
    if old:
        try: c.call('world.deleteBuilding', {'buildableId': old})
        except (RpcError, RpcTransportError): pass

def place(x, y, z, yaw):
    reperch(x, y)
    for _ in range(3):
        try:
            return c.call('world.placeBuilding', {'recipeClass': POLE, 'x': x, 'y': y, 'z': z, 'yaw': yaw, **F})['buildableId']
        except RpcError as e:
            if 'NO_PLAYER' in str(e):
                raise SystemExit('ABORT: player died')
            time.sleep(0.4)
        except RpcTransportError:
            time.sleep(0.6)
    return None

def belt(s, d):
    try:
        c.call('world.connectConveyor', {'recipeClass': BELT, 'sourceBuildableId': s, 'destBuildableId': d}); return True
    except (RpcError, RpcTransportError):
        return False

if _player_ok() is None:
    raise SystemExit('NO_PLAYER: respawn first')

t0 = time.time()
# Poles: ewp[i][j] carries the E/W belt (yaw 180, faces -X); nsp[i][j] the N/S belt (yaw 270, faces -Y)
ewp = [[None]*N for _ in range(N)]
nsp = [[None]*N for _ in range(N)]
for i in range(N):
    for j in range(N):
        ewp[i][j] = place(X(j),       Y(i),       z_ew(i, j), 180.0)
        nsp[i][j] = place(X(j) + OFF, Y(i) + OFF, z_ns(i, j), 270.0)
    print(f'poles row {i+1}/{N} [{time.time()-t0:.0f}s]', flush=True)

# Belts: E/W along +X per row, N/S along +Y per col. Retry both directions.
gaps = 0; total = 0
def link(a, b):
    global gaps, total
    total += 1
    if not a or not b:
        gaps += 1; return
    for _ in range(4):                   # belt connect is flaky near crossings; retry both dirs
        if belt(a, b) or belt(b, a):
            return
        time.sleep(0.6)
    gaps += 1
for i in range(N):
    for j in range(N-1):
        reperch((X(j)+X(j+1))/2, Y(i)); link(ewp[i][j], ewp[i][j+1])
    print(f'E/W row {i+1}/{N} belts, gaps so far {gaps} [{time.time()-t0:.0f}s]', flush=True)
for j in range(N):
    for i in range(N-1):
        reperch(X(j)+OFF, (Y(i)+Y(i+1))/2 + OFF); link(nsp[i][j], nsp[i+1][j])
    print(f'N/S col {j+1}/{N} belts, gaps so far {gaps} [{time.time()-t0:.0f}s]', flush=True)

print(f'WEAVE DONE {time.time()-t0:.0f}s: {N}x{N}, belts {total-gaps}/{total} ({100*(total-gaps)/max(1,total):.0f}%), gaps {gaps}', flush=True)
print(f'final perch id (player standing on it): {_perch["id"]}', flush=True)
