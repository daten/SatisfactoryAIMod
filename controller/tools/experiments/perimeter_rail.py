"""Perimeter railway stress-test tooling (2026-09-23).

Companion to docs/perimeter-railway-experiment.md. Encodes the primitives found
to WORK against the running AIMod build for large-scale rail, plus the hard
limits discovered (see the doc):

  * Terrain can't be scanned (groundHeight/terrainHeightGrid only hit placed
    buildables). Elevation is ESTIMATED from a ground-truth point cloud
    (resource nodes + low base buildables) via IDW.
  * Only TIGHT arcs build (R <= ~9k, turn >= ~45deg). Straights and gentle
    curves fail 'too long'. Long distance => serpentine of sharp arcs.
  * Free-end arcs (land on a floating pad) need a sharp offset (>=~45deg) and
    return no resultBuildableId (find the new track by a buildables diff).
  * Self-driving needs a CLOSED loop; a linear dead-end reports
    StationUnreachable. Drivable-joint reliability drops with more/sharper
    joints (4x90deg drives; 8x45deg does not).

Reliable primitives here: serpentine corridor (build_corridor), conn-conn arc
(conn_arc), 4-station drivable square (build_square). Run from the repo root:
    py -m controller.tools.experiments.perimeter_rail --help
"""
from __future__ import annotations
import argparse, math, time
from controller.satisfactory_ai.rpc_client import RpcClient, RpcError

FND = '/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C'
TRACK = '/Game/FactoryGame/Recipes/Buildings/Recipe_RailroadTrack.Recipe_RailroadTrack_C'
STATION = '/Game/FactoryGame/Buildable/Factory/Train/Station/Recipe_TrainStation.Recipe_TrainStation_C'
LOCO = '/Game/FactoryGame/Recipes/Vehicle/Train/Recipe_Locomotive.Recipe_Locomotive_C'
_BYP = dict(ignoreGroundTrace=True, ignoreAimLocation=True, ignorePlayerEncroachment=True,
            ignoreClearance=True, ignoreInvalidFloor=True)

c = RpcClient()

# ---- placement / telemetry ------------------------------------------------
def place_pad(x, y, z, yaw=0):
    r = c.call('world.placeBuilding', dict(recipeClass=FND, x=float(x), y=float(y), z=float(z),
               yaw=yaw, gridSnapSize=0, **_BYP))
    return r.get('resultBuildableId') or r.get('buildableId')

def place_station(x, y, z, yaw):
    r = c.call('world.placeBuilding', dict(recipeClass=STATION, x=float(x), y=float(y), z=float(z),
               yaw=yaw, gridSnapSize=0, **_BYP))
    return r.get('resultBuildableId') or r.get('buildableId')

def buildables():
    return c.call('world.buildables', {}, timeout_seconds=120).get('buildables', [])

def integ_near(x, y, blds=None):
    best, bd = None, 1e18
    for b in (blds or buildables()):
        if 'RailroadTrackIntegrated' in b.get('buildableClass', ''):
            p = b.get('position') or {}
            d = (p.get('x', 9e9)-x)**2 + (p.get('y', 9e9)-y)**2
            if d < bd:
                bd, best = d, b
    return best

def spline(bid):
    return c.call('world.splineGeometry', {'buildableId': bid}, timeout_seconds=60)

def ends(tid):
    return [(p['location']['x'], p['location']['y'], p['location']['z']) for p in spline(tid)['points']]

def rail_ids():
    return {b['id'] for b in buildables() if b.get('buildableClass', '').endswith('Build_RailroadTrack_C')}

def probe_ok(x, y, z):
    try:
        r = c.call('world.probeHazard', {'x': float(x), 'y': float(y), 'z': float(z)})
        return (not r.get('insideDamageVolume')) and r.get('insideWorldBounds2D', True) and not r.get('belowKillZ')
    except RpcError:
        return True

# ---- terrain estimate (the build can't scan real terrain) -----------------
def terrain_points():
    """Ground-truth (x,y,z) from resource nodes + low base buildables."""
    pts = []
    for n in c.call('world.resourceNodes', {}, timeout_seconds=120).get('resourceNodes', []):
        p = n.get('position')
        if p:
            pts.append((p['x'], p['y'], p['z']))
    for b in buildables():
        p = b.get('position') or {}
        if -6000 < p.get('z', 9e9) < 40000:
            pts.append((p['x'], p['y'], p['z']))
    return pts

def idw(pts, x, y, k=8, power=2.0):
    d2 = sorted(((px-x)**2+(py-y)**2, pz) for px, py, pz in pts)[:k]
    if not d2:
        return None
    if d2[0][0] < 1.0:
        return d2[0][1]
    num = den = 0.0
    for dd, pz in d2:
        w = 1.0/(dd**(power/2)); num += w*pz; den += w
    return num/den

# ---- arc primitives -------------------------------------------------------
def build_free_arc(source_id, pin, land, tries=8):
    """Free-end tight arc; returns (new_track_id, free_end) or (None, err).
    constructRailroadTrack returns no id, so the new track is a buildables diff."""
    before = rail_ids(); last = ''
    for _ in range(tries):
        try:
            c.call('world.constructRailroadTrack', dict(sourceBuildableId=source_id, recipeClass=TRACK,
                   destBuildableId='', sourceConnectorPosition=dict(x=pin[0], y=pin[1], z=pin[2]),
                   destConnectorPosition=dict(x=land[0], y=land[1], z=land[2])), timeout_seconds=90)
            new = list(rail_ids() - before)
            if not new:
                time.sleep(1.0); new = list(rail_ids() - before)
            if new:
                fe = max(ends(new[0]), key=lambda p: (p[0]-pin[0])**2+(p[1]-pin[1])**2)
                return new[0], fe
        except RpcError as e:
            last = ';'.join(s for s in str(e).split('CANNOT_CONSTRUCT:')[-1].split(';') if 'ignored' not in s).strip()[:32]
            time.sleep(1.5)
    return None, last

def conn_arc(src_id, dst_id, near_src, near_dst, tries=8):
    """conn-conn arc pinned at the endpoints nearest the given points."""
    sa = min(ends(src_id), key=lambda p: (p[0]-near_src[0])**2+(p[1]-near_src[1])**2)
    sb = min(ends(dst_id), key=lambda p: (p[0]-near_dst[0])**2+(p[1]-near_dst[1])**2)
    last = ''
    for _ in range(tries):
        try:
            c.call('world.constructRailroadTrack', dict(sourceBuildableId=src_id, destBuildableId=dst_id,
                   recipeClass=TRACK, sourceConnectorPosition=dict(x=sa[0], y=sa[1], z=sa[2]),
                   destConnectorPosition=dict(x=sb[0], y=sb[1], z=sb[2])), timeout_seconds=90)
            return True, ''
        except RpcError as e:
            last = ';'.join(s for s in str(e).split('CANNOT_CONSTRUCT:')[-1].split(';') if 'ignored' not in s).strip()[:32]
            time.sleep(1.5)
    return False, last

# ---- composites -----------------------------------------------------------
def build_corridor(start_xy, travel_deg, n_arcs, zfun, chord=5000, off=50, max_grade=0.06):
    """Serpentine corridor: alternating +/-off sharp free-end arcs; net-straight.
    Places a start station (arrow along travel) then chains free-end arcs on pads
    at zfun(x,y). Returns dict(station, tracks, path, end, fails)."""
    sx, sy = start_xy; z0 = zfun(sx, sy)
    st = place_station(sx, sy, z0, int(travel_deg) % 360); time.sleep(6)
    integ = integ_near(sx, sy)
    td = (math.cos(math.radians(travel_deg)), math.sin(math.radians(travel_deg)))
    ie = max(ends(integ['id']), key=lambda p: p[0]*td[0]+p[1]*td[1])
    cur, cx, cy, cz = integ['id'], ie[0], ie[1], ie[2]
    path, tracks, fails = [(cx, cy, cz)], [], []
    for i in range(n_arcs):
        aim = math.radians(travel_deg + (off if i % 2 == 0 else -off))
        lx, ly = cx + chord*math.cos(aim), cy + chord*math.sin(aim)
        lz = zfun(lx, ly); dz = lz - cz; m = max_grade*chord
        if abs(dz) > m:
            lz = cz + math.copysign(m, dz)
        if not probe_ok(lx, ly, lz):
            fails.append((i, 'UNSAFE')); break
        place_pad(lx, ly, lz); time.sleep(0.25)
        pin = (cx + (lx-cx)*0.05, cy + (ly-cy)*0.05, cz)
        tid, fe = build_free_arc(cur, pin, (lx, ly, lz))
        if not tid:
            fails.append((i, fe)); break
        tracks.append(tid); cx, cy, cz = fe; path.append(fe); cur = tid
    return dict(station=st, tracks=tracks, path=path, end=(cx, cy, cz), fails=fails)

def build_square(cx, cy, z, R=8000):
    """4-station CW square + 4x90deg arcs = the reliably DRIVABLE loop.
    Returns dict of station ids by N/E/S/W (or a failure marker)."""
    plan = {'N': (cx, cy+R, 0), 'E': (cx+R, cy, 270), 'S': (cx, cy-R, 180), 'W': (cx-R, cy, 90)}
    sid = {k: place_station(x, y, z, yaw) for k, (x, y, yaw) in plan.items()}
    time.sleep(8)
    integ = {k: integ_near(plan[k][0], plan[k][1]) for k in plan}
    for a, b in [('N', 'E'), ('E', 'S'), ('S', 'W'), ('W', 'N')]:
        ok, err = conn_arc(integ[a]['id'], integ[b]['id'], (plan[b][0], plan[b][1]), (plan[a][0], plan[a][1]))
        if not ok:
            return dict(fail=f'arc {a}->{b}: {err}')
    return dict(stations=sid, plan=plan)

if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('cmd', choices=['square', 'corridor', 'terrain'])
    ap.add_argument('--cx', type=float, default=250000)
    ap.add_argument('--cy', type=float, default=-30000)
    ap.add_argument('--z', type=float, default=10270)
    ap.add_argument('--arcs', type=int, default=20)
    a = ap.parse_args()
    if a.cmd == 'square':
        print(build_square(a.cx, a.cy, a.z))
    elif a.cmd == 'terrain':
        pts = terrain_points()
        print('points', len(pts), 'est z@center(m)=', round(idw(pts, a.cx, a.cy)/100, 1))
    elif a.cmd == 'corridor':
        pts = terrain_points()
        r = build_corridor((a.cx, a.cy), 90, a.arcs, lambda x, y: idw(pts, x, y)+300.0)
        print('built', len(r['tracks']), 'arcs; end', r['end'], 'fails', r['fails'])
