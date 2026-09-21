"""Eiffel Tower approximation from I-beams (Recipe_Beam). Beams take two 3D
endpoints and DON'T need the player nearby, so the whole lattice builds at exact
coords while the player stays safe on the ground. Curved splayed legs + X-braced
faces + platform rings + a shaft and spire."""
import sys, math, time, json
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError
c = RpcClient()
BEAM='/Game/FactoryGame/Prototype/Buildable/Beams/Recipe_Beam.Recipe_Beam_C'
import os
CX=float(os.environ.get('EIF_CX',27200.0)); CY=float(os.environ.get('EIF_CY',280000.0))
BASE_Z=200.0
HB=900.0          # base half-width (corner radius 1273 < tornado hollow 1400)
H=5200.0          # tower height to the top ring (~52 m)
SPIRE=1200.0      # antenna spire above the top ring

# height fractions of the rings (denser low where the legs curve) and the
# half-width at each (Eiffel-ish inward curve)
FR=[0.0,0.07,0.15,0.25,0.37,0.50,0.64,0.79,0.92,1.0]
def hw(f): return max(45.0, HB*(1.0-f)**1.6)
rings=[]
for f in FR:
    w=hw(f); z=BASE_Z+H*f
    # 4 corners NE,SE,SW,NW (go around so consecutive corners share a face)
    rings.append([(CX+w,CY+w,z),(CX+w,CY-w,z),(CX-w,CY-w,z),(CX-w,CY+w,z)])

def beam(a,b):
    for _ in range(3):
        try:
            c.call('world.constructBeam',{'recipeClass':BEAM,'startX':a[0],'startY':a[1],'startZ':a[2],
                                          'endX':b[0],'endY':b[1],'endZ':b[2],'ignoreGroundTrace':True})
            return True
        except (RpcError,RpcTransportError):
            time.sleep(0.4)
    return False

n=0; fail=0
t0=time.time()
# 1) legs: connect each corner across consecutive rings
for i in range(len(rings)-1):
    for k in range(4):
        n+=1; fail+= 0 if beam(rings[i][k],rings[i+1][k]) else 1
# 2) horizontal frame at every ring (the square edges = lattice horizontals/platforms)
for r in rings:
    for k in range(4):
        n+=1; fail+= 0 if beam(r[k],r[(k+1)%4]) else 1
    print(f'ring z={r[0][2]:.0f} done, beams={n} fails={fail} [{time.time()-t0:.0f}s]',flush=True)
# 3) X cross-braces on each of the 4 faces between consecutive rings
for i in range(len(rings)-1):
    for k in range(4):
        a0,b0=rings[i][k],rings[i][(k+1)%4]
        a1,b1=rings[i+1][k],rings[i+1][(k+1)%4]
        n+=1; fail+= 0 if beam(a0,b1) else 1
        n+=1; fail+= 0 if beam(b0,a1) else 1
# 4) spire from the top ring center upward
top=rings[-1]; tcx=sum(p[0] for p in top)/4; tcy=sum(p[1] for p in top)/4; tz=top[0][2]
apex=(tcx,tcy,tz+SPIRE)
for p in top:
    n+=1; fail+= 0 if beam(p,apex) else 1
print(f'EIFFEL DONE {time.time()-t0:.0f}s: {n-fail}/{n} beams, {fail} fails. apex z={tz+SPIRE:.0f}',flush=True)
