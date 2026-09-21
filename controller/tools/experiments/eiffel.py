"""Eiffel Tower (v3, faithful) from I-beams. Fixes the "flat spike" problem: a
sectioned profile that KEEPS A SQUARE SHAFT (never converges to a point until the
spire), fine X-bracing on ALL FOUR faces, overhanging square platforms at Level 1
and Level 2, and base arches on all four faces. Beams take two 3D endpoints and
need no player proximity, so it all builds at exact coords with the player safe.
Base placed ABOVE terrain (EIF_BASE_Z)."""
import sys, math, os, time
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError
c = RpcClient()
BEAM='/Game/FactoryGame/Prototype/Buildable/Beams/Recipe_Beam.Recipe_Beam_C'
CX=float(os.environ.get('EIF_CX',27200.0)); CY=float(os.environ.get('EIF_CY',280000.0))
BASE_Z=float(os.environ.get('EIF_BASE_Z',3000.0))
HB=float(os.environ.get('EIF_HB',4000.0))    # base half-width (80 m base)
H =float(os.environ.get('EIF_H',22000.0))     # ground -> spire base (~220 m)
SPIRE=3000.0

# KEY profile points (height fraction f, half-width fraction of HB). Note it holds
# a slender-but-real square shaft up high (0.05) instead of going to a point.
KEY=[(0.00,1.00),(0.04,0.82),(0.09,0.66),(0.15,0.52),   # base legs -> 1st platform
     (0.22,0.44),(0.30,0.37),(0.38,0.31),               # middle -> 2nd platform
     (0.48,0.245),(0.58,0.185),(0.68,0.14),(0.78,0.105),
     (0.86,0.08),(0.92,0.06),(1.00,0.05)]               # upper tower -> shaft top
def wfrac(f):
    for i in range(len(KEY)-1):
        f0,w0=KEY[i]; f1,w1=KEY[i+1]
        if f0<=f<=f1:
            t=(f-f0)/(f1-f0) if f1>f0 else 0.0
            return w0+(w1-w0)*t
    return KEY[-1][1]
# rings: interpolate to N levels for fine bracing
N=22
FR=[i/(N-1) for i in range(N)]
def halfw(f): return HB*wfrac(f)
SIGNS=[(1,1),(1,-1),(-1,-1),(-1,1)]  # NE,SE,SW,NW (consecutive share a face)
def corner(k,f,extra=0.0):
    w=halfw(f)+extra; s=SIGNS[k]; return (CX+s[0]*w, CY+s[1]*w, BASE_Z+H*f)
rings=[[corner(k,f) for k in range(4)] for f in FR]

n=[0]; fail=[0]; t0=time.time()
def beam(a,b):
    for _ in range(3):
        try:
            c.call('world.constructBeam',{'recipeClass':BEAM,'startX':a[0],'startY':a[1],'startZ':a[2],
                                          'endX':b[0],'endY':b[1],'endZ':b[2],'ignoreGroundTrace':True})
            n[0]+=1; return True
        except (RpcError,RpcTransportError): time.sleep(0.3)
    n[0]+=1; fail[0]+=1; return False

# 1) legs across consecutive rings (all 4 corners)
for i in range(len(rings)-1):
    for k in range(4): beam(rings[i][k],rings[i+1][k])
# 2) horizontal frame at every ring (square outline)
for r in rings:
    for k in range(4): beam(r[k],r[(k+1)%4])
# 3) X cross-braces on ALL FOUR faces, every gap (this is what makes it read 3D)
for i in range(len(rings)-1):
    for k in range(4):
        a0,b0=rings[i][k],rings[i][(k+1)%4]; a1,b1=rings[i+1][k],rings[i+1][(k+1)%4]
        beam(a0,b1); beam(b0,a1)
    if i%4==0: print(f'lattice ring {i+1}/{len(rings)-1} beams={n[0]} [{time.time()-t0:.0f}s]',flush=True)
# 4) overhanging square platforms at Level 1 (f~0.15) and Level 2 (f~0.38):
#    a second frame just above, pushed out past the legs, plus deck diagonals
def platform(f, over):
    zi=min(range(len(FR)), key=lambda i:abs(FR[i]-f))
    for dz in (0.0, 0.012):                              # double frame (thickness)
        ring=[corner(k, FR[zi]+dz, over) for k in range(4)]
        for k in range(4): beam(ring[k],ring[(k+1)%4])
    base=[corner(k, FR[zi], over) for k in range(4)]
    cen=(CX,CY,BASE_Z+H*FR[zi])
    beam(base[0],base[2]); beam(base[1],base[3])         # deck diagonals
    for k in range(4): beam(base[k],cen)                 # spokes
platform(0.15, 500.0)
platform(0.38, 350.0)
# 5) base arches on all 4 faces: spring low on the two legs, arc up to a peak
#    at the face centre just under the 1st platform
F_LOW,F_HIGH,NP=0.03,0.14,9
for k in range(4):
    s1,s2=SIGNS[k],SIGNS[(k+1)%4]
    lo1=(CX+s1[0]*halfw(F_LOW),CY+s1[1]*halfw(F_LOW),BASE_Z+H*F_LOW)
    lo2=(CX+s2[0]*halfw(F_LOW),CY+s2[1]*halfw(F_LOW),BASE_Z+H*F_LOW)
    zpk=BASE_Z+H*F_HIGH; pts=[]
    for t in range(NP+1):
        u=t/NP
        x=lo1[0]+(lo2[0]-lo1[0])*u; y=lo1[1]+(lo2[1]-lo1[1])*u
        z=zpk-(zpk-lo1[2])*(2*u-1)**2
        pts.append((x,y,z))
    for t in range(NP): beam(pts[t],pts[t+1])
# 6) spire: taper the shaft top to a point through a couple of steps + a mast
top=rings[-1]; tcx=CX; tcy=CY; tz=top[0][2]
s1=[(tcx+SIGNS[k][0]*30, tcy+SIGNS[k][1]*30, tz+SPIRE*0.5) for k in range(4)]
for k in range(4): beam(top[k],s1[k])
for k in range(4): beam(s1[k],s1[(k+1)%4])
apex=(tcx,tcy,tz+SPIRE)
for p in s1: beam(p,apex)
print(f'EIFFEL v3 DONE {time.time()-t0:.0f}s: {n[0]-fail[0]}/{n[0]} beams, {fail[0]} fails. base z={BASE_Z:.0f} apex z={tz+SPIRE:.0f}',flush=True)
