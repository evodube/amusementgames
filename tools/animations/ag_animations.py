"""Amusement Games' settler throws, keyed in Blender (5.2) with PyNifly 30.2, 30 fps.

In Blender: import Meshes/Actors/Character/CharacterAssets/skeleton.hkx (Import HKX), then
MT/Neutral/PoseA_Idle1.hkx onto it (the standing pose every key starts from); run this; then for each action
export_scene.pynifly_hkx(game='FO4', reference_skel=skeleton.hkx, fps=30).
The ball leaves the hand at frame 24 (the shot) and 22 (the roll); Arcade.cpp cues them that early.
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
def legs(s,FZ):
    snaps={n+s:snap(n+s) for n in LEG}
    lo,hi=0.0,80.0
    for _ in range(18):
        ang=(lo+hi)/2
        for n in snaps: put(n,snaps[n])
        rot("Leg_Thigh."+s,X,ang); rot("Leg_Calf."+s,X,-2*ang); rot("Leg_Foot."+s,X,ang)
        if a.pose.bones["Leg_Foot."+s].head.z>FZ[s]: hi=ang
        else: lo=ang
def pose(p,FZ):
    reset(); dz=p.get('com',0.0)
    move("COM",Vector((0,0,dz)))
    sp=p.get('spine',0); rot("Spine1",X,sp*.5); rot("Spine2",X,sp*.3); rot("Chest",X,sp*.2)
    if dz<-0.05:
        legs("L",FZ); legs("R",FZ)
    for s,sg in (("R",1),("L",-1)):
        rot("Arm_UpperArm."+s,X,p.get('up'+s,0)); rot("Arm_UpperArm."+s,Z,sg*p.get('yaw'+s,0))
        rot("Arm_ForeArm1."+s,X,p.get('fore'+s,0)); rot("Arm_Hand."+s,X,p.get('hand'+s,0))
def ease(t): return 0.5-0.5*math.cos(math.pi*t)
def lk(keys,f):
    for i in range(len(keys)-1):
        f0,p0=keys[i]; f1,p1=keys[i+1]
        if f0<=f<=f1:
            t=ease((f-f0)/(f1-f0)); return {k:p0.get(k,0)*(1-t)+p1.get(k,0)*t for k in set(p0)|set(p1)}
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
                      (8,
                       {'com': -4,
                        'foreL': 95,
                        'foreR': 95,
                        'handL': 10,
                        'handR': 10,
                        'spine': -5,
                        'upL': 55,
                        'upR': 55,
                        'yawL': 25,
                        'yawR': 25}),
                      (14,
                       {'com': -10,
                        'foreL': 100,
                        'foreR': 100,
                        'handL': 10,
                        'handR': 20,
                        'spine': -8,
                        'upL': 60,
                        'upR': 60,
                        'yawL': 25,
                        'yawR': 25}),
                      (21,
                       {'com': 1,
                        'foreL': 40,
                        'foreR': 25,
                        'handR': 30,
                        'spine': 0,
                        'upL': 130,
                        'upR': 145,
                        'yawL': 20,
                        'yawR': 10}),
                      (24,
                       {'com': 1,
                        'foreL': 30,
                        'foreR': 10,
                        'handL': -10,
                        'handR': -45,
                        'spine': 2,
                        'upL': 120,
                        'upR': 150,
                        'yawL': 15,
                        'yawR': 5}),
                      (32,
                       {'foreL': 30,
                        'foreR': 15,
                        'handL': -10,
                        'handR': -50,
                        'spine': 0,
                        'upL': 110,
                        'upR': 145,
                        'yawL': 10,
                        'yawR': 5}),
                      (46, {})],
 'AGSkeeballRoll': [(0, {}),
                    (10, {'com': -4, 'foreL': 20, 'foreR': 10, 'spine': -10, 'upL': 15, 'upR': -50}),
                    (20, {'com': -14, 'foreL': 35, 'foreR': 5, 'handR': -5, 'spine': -30, 'upL': 25, 'upR': 25}),
                    (22, {'com': -14, 'foreL': 35, 'foreR': 5, 'handR': -15, 'spine': -30, 'upL': 25, 'upR': 40}),
                    (30, {'com': -10, 'foreL': 30, 'foreR': 10, 'handR': -20, 'spine': -22, 'upL': 20, 'upR': 75}),
                    (48, {})]}

for _name, _keys in KEYS.items():
    build(_name, _keys)
