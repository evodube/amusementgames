"""Amusement Games' settler throws, keyed in Blender (5.2) with PyNifly 30.2, 30 fps.

In Blender: import Meshes/Actors/Character/CharacterAssets/skeleton.hkx (Import HKX), then
MT/Neutral/PoseA_Idle1.hkx onto it (the standing pose every key starts from); run this; then for each action
export_scene.pynifly_hkx(game='FO4', reference_skel=skeleton.hkx, fps=30).
The ball leaves the hand at key 24 (the shot) and 22 (the roll); Arcade.cpp cues them that early.
"""

import bpy, math
from mathutils import Matrix, Vector
a=bpy.data.objects["Root:ARMATURE"]; sc=bpy.context.scene; VL=bpy.context.view_layer
a.animation_data.action=bpy.data.actions['PoseA_Idle1']; sc.frame_set(1); VL.update()
base={pb.name:(pb.location.copy(),pb.rotation_quaternion.copy(),pb.rotation_mode) for pb in a.pose.bones}; X=Vector((1,0,0)); Z=Vector((0,0,1))
def reset():
    for pb in a.pose.bones:
        l,q,m=base[pb.name]; pb.location=l; pb.rotation_quaternion=q; pb.scale=(1,1,1)
    VL.update()
def rot(n,ax,d):
    if abs(d)<1e-6: return
    pb=a.pose.bones[n]; M=pb.matrix.copy(); h=M.translation.copy()
    pb.matrix=Matrix.Translation(h)@Matrix.Rotation(math.radians(d),4,ax)@Matrix.Translation(-h)@M; VL.update()
def move(n,v):
    pb=a.pose.bones[n]; pb.matrix=Matrix.Translation(v)@pb.matrix; VL.update()
def snap(n): pb=a.pose.bones[n]; return (pb.location.copy(), pb.rotation_quaternion.copy())
def put(n,s): pb=a.pose.bones[n]; pb.location=s[0]; pb.rotation_quaternion=s[1]; VL.update()
LEG=["Leg_Thigh.","Leg_Calf.","Leg_Foot."]
def legik(s,target):
    # the foot's ankle onto target (armature space), foot kept flat: thigh +a, calf -k, foot +(k-a) about X (world)
    snaps={n+s:snap(n+s) for n in LEG}
    def at(a,k):
        for n in snaps: put(n,snaps[n])
        rot("Leg_Thigh."+s,X,a); rot("Leg_Calf."+s,X,-k); rot("Leg_Foot."+s,X,k-a)
        p=bpy.data.objects["Root:ARMATURE"].pose.bones["Leg_Foot."+s].head
        return (p.y-target.y)**2+(p.z-target.z)**2
    best=(1e18,0,0)
    for ga in range(-40,81,10):
        for gk in range(0,131,10):
            e=at(ga,gk)
            if e<best[0]: best=(e,ga,gk)
    step=5.0
    _,ba,bk=best
    while step>0.05:
        moved=False
        for da,dk in ((step,0),(-step,0),(0,step),(0,-step)):
            e=at(ba+da,bk+dk)
            if e<best[0]-1e-9:
                best=(e,ba+da,bk+dk); ba,bk=ba+da,bk+dk; moved=True
        if not moved: step/=2
    at(ba,bk)
def pose(p,FZ):
    reset()
    feet={s:a.pose.bones["Leg_Foot."+s].head.copy() for s in "LR"}
    dz=p.get('com',0.0); dy=p.get('comy',0.0)
    move("COM",Vector((0,dy,dz)))
    sp=p.get('spine',0); rot("Spine1",X,sp*.5); rot("Spine2",X,sp*.3); rot("Chest",X,sp*.2)
    stepL=p.get('stepL',0.0)
    if abs(dz)>0.05 or abs(dy)>0.05 or abs(stepL)>0.05:
        for s in "LR":
            tgt=feet[s].copy()
            if s=="L":
                tgt.y+=stepL; tgt.z+=p.get('liftL',0.0)
            legik(s,tgt)
    for s,sg in (("R",1),("L",-1)):
        rot("Arm_UpperArm."+s,X,p.get('up'+s,0)); rot("Arm_UpperArm."+s,Z,sg*p.get('yaw'+s,0))
        rot("Arm_ForeArm1."+s,X,p.get('fore'+s,0)); rot("Arm_Hand."+s,X,p.get('hand'+s,0))
def ease(t): return 0.5-0.5*math.cos(math.pi*t)
def lk(keys,f):
    # through every key with no stop at it (Catmull-Rom, timed): the speed carries on through a key, still at the
    # first and last
    ts=[k[0] for k in keys]; ps=[k[1] for k in keys]; names=set().union(*ps)
    for i in range(len(keys)-1):
        f0,f1=ts[i],ts[i+1]
        if f0<=f<=f1:
            h=f1-f0; u=(f-f0)/h
            h00=2*u**3-3*u**2+1; h10=u**3-2*u**2+u; h01=-2*u**3+3*u**2; h11=u**3-u**2
            def tan(j,k):
                if j==0 or j==len(keys)-1: return 0.0
                return (ps[j+1].get(k,0)-ps[j-1].get(k,0))/(ts[j+1]-ts[j-1])
            return {k:h00*ps[i].get(k,0)+h10*h*tan(i,k)+h01*ps[i+1].get(k,0)+h11*h*tan(i+1,k) for k in names}
    return keys[-1][1]
def build(name,keys):
    a.animation_data.action=None
    reset(); FZ={s:a.pose.bones["Leg_Foot."+s].head.z for s in "LR"}
    act=bpy.data.actions.get(name)
    if act: bpy.data.actions.remove(act)
    n=keys[-1][0]; frames=[]
    for f in range(n+1):
        pose(lk(keys,f),FZ)
        frames.append({pb.name:snap(pb.name) for pb in a.pose.bones})
    act=bpy.data.actions.new(name); a.animation_data.action=act
    for f,fr in enumerate(frames):
        for pb in a.pose.bones:
            pb.location=fr[pb.name][0]; pb.rotation_quaternion=fr[pb.name][1]; pb.scale=(1,1,1)
            pb.keyframe_insert("rotation_quaternion",frame=f+1); pb.keyframe_insert("location",frame=f+1); pb.keyframe_insert("scale",frame=f+1)
    sc.frame_start=1; sc.frame_end=n+1
    return FZ


KEYS = {'AGBasketballShot': [(0, {}),
                      (6,
                       {'com': -3,
                        'foreL': 65,
                        'foreR': 60,
                        'handL': 10,
                        'handR': 10,
                        'spine': -4,
                        'upL': 15,
                        'upR': 20,
                        'yawL': 30,
                        'yawR': 25}),
                      (14,
                       {'com': -10,
                        'foreL': 75,
                        'foreR': 75,
                        'handL': 10,
                        'handR': 20,
                        'spine': -8,
                        'upL': 40,
                        'upR': 40,
                        'yawL': 15,
                        'yawR': 15}),
                      (20,
                       {'com': -2,
                        'foreL': 45,
                        'foreR': 40,
                        'handR': 35,
                        'spine': 0,
                        'upL': 105,
                        'upR': 110,
                        'yawL': 13,
                        'yawR': 8}),
                      (24,
                       {'com': 2,
                        'foreL': 10,
                        'foreR': 0,
                        'handL': -10,
                        'handR': -50,
                        'spine': 4,
                        'upL': 140,
                        'upR': 150,
                        'yawL': 25,
                        'yawR': 15}),
                      (33,
                       {'com': 1,
                        'foreL': 20,
                        'foreR': 0,
                        'handL': -20,
                        'handR': -65,
                        'spine': 3,
                        'upL': 125,
                        'upR': 155,
                        'yawL': 20,
                        'yawR': 12}),
                      (50, {})],
 'AGSkeeballRoll': [(0, {}),
                    (8,
                     {'com': -4,
                      'foreL': 40,
                      'foreR': 55,
                      'liftL': 4,
                      'spine': -8,
                      'stepL': 4,
                      'upL': 20,
                      'upR': 25,
                      'yawR': 5}),
                    (14,
                     {'com': -14,
                      'comy': 4,
                      'foreL': 35,
                      'foreR': 5,
                      'handR': 5,
                      'spine': -24,
                      'stepL': 16,
                      'upL': 30,
                      'upR': -35}),
                    (22,
                     {'com': -22,
                      'comy': 8,
                      'foreL': 35,
                      'foreR': 5,
                      'handR': -15,
                      'spine': -36,
                      'stepL': 16,
                      'upL': 30,
                      'upR': 60}),
                    (30,
                     {'com': -16,
                      'comy': 7,
                      'foreL': 30,
                      'foreR': 8,
                      'handR': -10,
                      'spine': -28,
                      'stepL': 16,
                      'upL': 25,
                      'upR': 95}),
                    (38,
                     {'com': -12,
                      'comy': 5,
                      'foreL': 30,
                      'foreR': 15,
                      'spine': -22,
                      'stepL': 16,
                      'upL': 20,
                      'upR': 90}),
                    (46, {'com': -4, 'comy': 2, 'foreR': 20, 'liftL': 3, 'spine': -8, 'stepL': 8, 'upR': 20}),
                    (54, {})]}

for _name, _keys in KEYS.items():
    build(_name, _keys)
