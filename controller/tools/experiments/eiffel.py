"""Eiffel Tower (detailed) from I-beams (Recipe_Beam). Beams take two 3D endpoints
and need no player proximity, so the whole lattice builds at exact coords with the
player safe. Curved splayed legs, full X-braced faces, the signature base arches,
platform decks and a spire. Base is placed ABOVE terrain (EIF_BASE_Z) so nothing
is underground."""
import sys, math, os, time
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError
c = RpcClient()
BEAM='/Game/FactoryGame/Prototype/Buildable/Beams/Recipe_Beam.Recipe_Beam_C'
CX=float(os.environ.get('EIF_CX',27200.0)); CY=float(os.environ.get('EIF_CY',280000.0))
BASE_Z=float(os.environ.get('EIF_BASE_Z',3000.0))
HB=float(os.environ.get('EIF_HB',3000.0))     # base half-width (60 m base)
H =float(os.environ.get('EIF_H',15000.0))      # height to top ring (150 m)
SPIRE=3500.0
HT=80.0                                         # top half-width
def hw(f): return max(HT, HB*(1.0-f)**1.6)

# rings up the height (denser low, where the legs curve hardest)
FR=[0.0,0.04,0.09,0.15,0.22,0.30,0.39,0.48,0.57,0.66,0.75,0.83,0.90,0.95,1.0]
SIGNS=[(1,1),(1,-1),(-1,-1),(-1,1)]   # NE,SE,SW,NW - consecutive share a face
def corner(k,f):
    w=hw(f); s=SIGNS[k]; return (CX+s[0]*w, CY+s[1]*w, BASE_Z+H*f)
rings=[[corner(k,f) for k in range(4)] for f in FR]

n=[0]; fail=[0]
t0=time.time()
def beam(a,b):
    for _ in range(3):
        try:
            c.call('world.constructBeam',{'recipeClass':BEAM,'startX':a[0],'startY':a[1],'startZ':a[2],
                                          'endX':b[0],'endY':b[1],'endZ':b[2],'ignoreGroundTrace':True})
            n[0]+=1; return True
        except (RpcError,RpcTransportError): time.sleep(0.3)
    n[0]+=1; fail[0]+=1; return False

# 1) legs (corner across consecutive rings)
for i in range(len(rings)-1):
    for k in range(4): beam(rings[i][k],rings[i+1][k])
# 2) horizontal frame at each ring
for r in rings:
    for k in range(4): beam(r[k],r[(k+1)%4])
# 3) X cross-braces on every face, every gap
for i in range(len(rings)-1):
    for k in range(4):
        a0,b0=rings[i][k],rings[i][(k+1)%4]; a1,b1=rings[i+1][k],rings[i+1][(k+1)%4]
        beam(a0,b1); beam(b0,a1)
    print(f'lattice through ring {i+1}/{len(rings)-1}, beams={n[0]} [{time.time()-t0:.0f}s]',flush=True)
# 4) base arches on each face (the Eiffel's signature): spring low on the two legs,
#    arc up to a peak at the face centre just under the 1st platform
F_LOW,F_HIGH=0.05,0.14
NP=8
for k in range(4):
    s1,s2=SIGNS[k],SIGNS[(k+1)%4]
    lo1=(CX+s1[0]*hw(F_LOW),CY+s1[1]*hw(F_LOW),BASE_Z+H*F_LOW)
    lo2=(CX+s2[0]*hw(F_LOW),CY+s2[1]*hw(F_LOW),BASE_Z+H*F_LOW)
    zpk=BASE_Z+H*F_HIGH
    pts=[]
    for t in range(NP+1):
        u=t/NP
        x=lo1[0]+(lo2[0]-lo1[0])*u; y=lo1[1]+(lo2[1]-lo1[1])*u
        z=zpk-(zpk-lo1[2])*(2*u-1)**2      # parabola peaking at the centre
        pts.append((x,y,z))
    for t in range(NP): beam(pts[t],pts[t+1])
# 5) platform decks: diagonals + center cross at the 1st (f~0.15) and 2nd (f~0.39) rings
for ri in (3,6):
    r=rings[ri]; cen=((r[0][0]+r[2][0])/2,(r[0][1]+r[2][1])/2,r[0][2])
    beam(r[0],r[2]); beam(r[1],r[3])                 # diagonals
    for k in range(4): beam(r[k],cen)                # spokes to centre
# 6) spire: taper the top ring to an apex, plus a little mast
top=rings[-1]; tcx=sum(p[0] for p in top)/4; tcy=sum(p[1] for p in top)/4; tz=top[0][2]
apex=(tcx,tcy,tz+SPIRE); mid=(tcx,tcy,tz+SPIRE*0.45)
for p in top: beam(p,mid)
beam(mid,apex)
print(f'EIFFEL DONE {time.time()-t0:.0f}s: {n[0]-fail[0]}/{n[0]} beams, {fail[0]} fails. base z={BASE_Z:.0f} apex z={tz+SPIRE:.0f}',flush=True)
