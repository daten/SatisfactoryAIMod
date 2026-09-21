"""Hypertube cannon: N pairs of (entrance -> short hypertube -> exit) in a dense
row, so the player accelerates through the sequence. Built in the air on moving
platform-perches (terrain-proof). Powers every entrance if the buildable has a
power connection (hypertubes may not need power - reported either way)."""
import sys, math, time, json
sys.path.insert(0, r"F:\Claude\SatisfactoryModLoader\controller")
from satisfactory_ai.rpc_client import RpcClient, RpcError, RpcTransportError
c = RpcClient()
START='/Game/FactoryGame/Recipe_PipeHyperStart.Recipe_PipeHyperStart_C'
POLE ='/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C'
FND  ='/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C'
F={'ignoreGroundTrace':True,'ignoreInvalidFloor':True,'ignoreAimLocation':True,'ignorePlayerEncroachment':True,'gridSnapSize':0}
PF=dict(F,ignoreClearance=True)
N=8; TUBE=600.0; GAP=200.0; PITCH=TUBE+GAP
X0,Y0,Z0=44000.0,288000.0,3550.0

_perch={'id':None,'xy':None}
def ppos():
    try:
        p=c.call('world.player').get('position',{})
        return p if p and tuple(round(v) for v in p.values())!=(0,0,0) else None
    except (RpcError,RpcTransportError): return None
def reperch(x,y):
    if _perch['xy'] and math.hypot(x-_perch['xy'][0],y-_perch['xy'][1])<3200: return
    pz=Z0-600.0
    try: nid=c.call('world.placeBuilding',{'recipeClass':FND,'x':x,'y':y,'z':pz,'yaw':0,**PF})['buildableId']
    except (RpcError,RpcTransportError): return
    try: c.call('world.teleportPlayer',{'x':x,'y':y,'z':pz+50+170})
    except (RpcError,RpcTransportError): pass
    time.sleep(0.4)
    old=_perch['id']; _perch['id']=nid; _perch['xy']=(x,y)
    if old:
        try: c.call('world.deleteBuilding',{'buildableId':old})
        except (RpcError,RpcTransportError): pass
def place(x,y,z,yaw):
    reperch(x,y)
    for _ in range(3):
        try: return c.call('world.placeBuilding',{'recipeClass':START,'x':x,'y':y,'z':z,'yaw':yaw,**F})['buildableId']
        except RpcError as e:
            if 'NO_PLAYER' in str(e): raise SystemExit('player died')
            time.sleep(0.4)
        except RpcTransportError: time.sleep(0.6)
    return None

if ppos() is None: raise SystemExit('NO_PLAYER: respawn first')
ent=[]; ext=[]; tubes=0
# INTERLEAVE place+connect per pair: connect each pair's tube while no adjacent
# pair exists yet, so FindFreeHyperPipeConnection can't grab a neighbour's connector.
for i in range(N):
    ex=X0+i*PITCH
    e=place(ex,Y0,Z0,0.0)          # entrance, funnel faces -X (player enters going +X)
    x=place(ex+TUBE,Y0,Z0,180.0)   # exit, funnel faces +X (ejects going +X)
    ent.append(e); ext.append(x)
    ok=False
    if e and x:
        reperch(ex+TUBE/2,Y0)
        for _ in range(4):
            try:
                c.call('world.connectHypertube',{'sourceBuildableId':e,'destBuildableId':x}); ok=True; tubes+=1; break
            except (RpcError,RpcTransportError):
                time.sleep(0.6)
    print(f'pair {i+1}/{N}: entrance={e is not None} exit={x is not None} tube={ok} (tubes {tubes})',flush=True)
# POWER: does an entrance even have a power connection? place a pole and try connectPower
print('--- power test ---',flush=True)
reperch(X0,Y0)
pole=None
try: pole=c.call('world.placeBuilding',{'recipeClass':POLE,'x':X0-300,'y':Y0-300,'z':Z0,'yaw':0,**F})['buildableId']
except RpcError as e: print('pole place err',e)
powered=0; power_supported=True
if pole and ent[0]:
    try:
        c.call('world.connectPower',{'buildableIdA':ent[0],'buildableIdB':pole}); powered+=1; print('entrance 0 powered OK -> entrances DO take power')
    except RpcError as e:
        print('connectPower failed on entrance:',str(e)[:120]); power_supported=False
if power_supported and pole:
    for b in ent+ext:
        if b is None: continue
        for _ in range(2):
            try: c.call('world.connectPower',{'buildableIdA':b,'buildableIdB':pole}); powered+=1; break
            except (RpcError,RpcTransportError): time.sleep(0.3)
print(f'CANNON DONE: {sum(1 for e in ent if e)} entrances, {sum(1 for x in ext if x)} exits, {tubes} tubes; powered={powered} power_supported={power_supported}',flush=True)
print('first entrance id:',ent[0],flush=True)
print('entrance row start (jump in here):',X0,Y0,Z0,flush=True)
print('final perch id:',_perch['id'],flush=True)
