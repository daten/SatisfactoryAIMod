"""Eiffel Tower (v4) from I-beams. KEY FIX: beams max out at 4000 units, so every
structural member is SUBDIVIDED into <=3800-unit segments that actually span (a
single constructBeam call clamps longer spans to a 4000 stub -> gaps). Sectioned
profile keeps a real square shaft; full X-bracing on all four faces; interior
cross-bracing; overhanging Level 1/2 platforms; base arches on all four faces;
spire. Beams need no player proximity; base placed above terrain (EIF_BASE_Z)."""
import sys, math, os, time
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError
c = RpcClient()
BEAM='/Game/FactoryGame/Prototype/Buildable/Beams/Recipe_Beam.Recipe_Beam_C'
CX=float(os.environ.get('EIF_CX',27200.0)); CY=float(os.environ.get('EIF_CY',280000.0))
BASE_Z=float(os.environ.get('EIF_BASE_Z',3000.0))
HB=float(os.environ.get('EIF_HB',4000.0)); H=float(os.environ.get('EIF_H',22000.0))
SPIRE=3000.0; MAXSEG=3800.0

KEY=[(0.00,1.00),(0.04,0.82),(0.09,0.66),(0.15,0.52),
     (0.22,0.44),(0.30,0.37),(0.38,0.31),
     (0.48,0.245),(0.58,0.185),(0.68,0.14),(0.78,0.105),
     (0.86,0.08),(0.92,0.06),(1.00,0.05)]
def wfrac(f):
    for i in range(len(KEY)-1):
        f0,w0=KEY[i]; f1,w1=KEY[i+1]
        if f0<=f<=f1:
            t=(f-f0)/(f1-f0) if f1>f0 else 0.0
            return w0+(w1-w0)*t
    return KEY[-1][1]
N=26
FR=[i/(N-1) for i in range(N)]
def halfw(f): return HB*wfrac(f)
SIGNS=[(1,1),(1,-1),(-1,-1),(-1,1)]
def corner(k,f,extra=0.0):
    w=halfw(f)+extra; s=SIGNS[k]; return (CX+s[0]*w, CY+s[1]*w, BASE_Z+H*f)
rings=[[corner(k,f) for k in range(4)] for f in FR]

n=[0]; fail=[0]; t0=time.time()
def _beam(a,b):
    for _ in range(3):
        try:
            c.call('world.constructBeam',{'recipeClass':BEAM,'startX':a[0],'startY':a[1],'startZ':a[2],
                                          'endX':b[0],'endY':b[1],'endZ':b[2],'ignoreGroundTrace':True})
            n[0]+=1; return
        except (RpcError,RpcTransportError): time.sleep(0.25)
    n[0]+=1; fail[0]+=1
def strut(a,b):
    d=math.dist(a,b)
    m=max(1,math.ceil(d/MAXSEG))
    for i in range(m):
        p=tuple(a[k]+(b[k]-a[k])*i/m for k in range(3))
        q=tuple(a[k]+(b[k]-a[k])*(i+1)/m for k in range(3))
        _beam(p,q)

# 1) legs
for i in range(len(rings)-1):
    for k in range(4): strut(rings[i][k],rings[i+1][k])
# 2) horizontal frame at each ring (subdivided edges)
for r in rings:
    for k in range(4): strut(r[k],r[(k+1)%4])
    if FR[rings.index(r)] < 1: pass
# 3) full X cross-braces on ALL FOUR faces, every gap (subdivided)
for i in range(len(rings)-1):
    for k in range(4):
        a0,b0=rings[i][k],rings[i][(k+1)%4]; a1,b1=rings[i+1][k],rings[i+1][(k+1)%4]
        strut(a0,b1); strut(b0,a1)
    if i%5==0: print(f'faces through ring {i+1}/{len(rings)-1} beams={n[0]} [{time.time()-t0:.0f}s]',flush=True)
# 4) INTERIOR cross-bracing: horizontal X across the square section at each ring
for r in rings:
    strut(r[0],r[2]); strut(r[1],r[3])
# 5) overhanging square platforms at Level 1 (f~0.15) and Level 2 (f~0.38)
def platform(f, over):
    zi=min(range(len(FR)), key=lambda i:abs(FR[i]-f))
    for dz in (0.0,0.012):
        ring=[corner(k, FR[zi]+dz, over) for k in range(4)]
        for k in range(4): strut(ring[k],ring[(k+1)%4])
    base=[corner(k, FR[zi], over) for k in range(4)]
    cen=(CX,CY,BASE_Z+H*FR[zi])
    strut(base[0],base[2]); strut(base[1],base[3])
    for k in range(4): strut(base[k],cen)
platform(0.15,500.0); platform(0.38,350.0)
print(f'platforms done beams={n[0]} [{time.time()-t0:.0f}s]',flush=True)
# 6) base arches on all 4 faces
F_LOW,F_HIGH,NP=0.03,0.14,10
for k in range(4):
    s1,s2=SIGNS[k],SIGNS[(k+1)%4]
    lo1=(CX+s1[0]*halfw(F_LOW),CY+s1[1]*halfw(F_LOW),BASE_Z+H*F_LOW)
    lo2=(CX+s2[0]*halfw(F_LOW),CY+s2[1]*halfw(F_LOW),BASE_Z+H*F_LOW)
    zpk=BASE_Z+H*F_HIGH; pts=[]
    for t in range(NP+1):
        u=t/NP; x=lo1[0]+(lo2[0]-lo1[0])*u; y=lo1[1]+(lo2[1]-lo1[1])*u
        z=zpk-(zpk-lo1[2])*(2*u-1)**2; pts.append((x,y,z))
    for t in range(NP): strut(pts[t],pts[t+1])
# 7) spire
top=rings[-1]; tz=top[0][2]
s1=[(CX+SIGNS[k][0]*30, CY+SIGNS[k][1]*30, tz+SPIRE*0.5) for k in range(4)]
for k in range(4): strut(top[k],s1[k])
for k in range(4): strut(s1[k],s1[(k+1)%4])
apex=(CX,CY,tz+SPIRE)
for p in s1: strut(p,apex)
print(f'EIFFEL v4 DONE {time.time()-t0:.0f}s: {n[0]-fail[0]}/{n[0]} beams, {fail[0]} fails. base z={BASE_Z:.0f} apex z={tz+SPIRE:.0f}',flush=True)
