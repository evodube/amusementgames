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
LAST={}
def legik(s,target):
    # the foot's ankle onto target (armature space), foot kept flat: thigh +a, calf -k, foot +(k-a) about X (world).
    # Each frame starts from the last one's angles (LAST), so the leg moves on smoothly instead of jumping between
    # nearby answers
    snaps={n+s:snap(n+s) for n in LEG}
    arm=bpy.data.objects["Root:ARMATURE"]
    def at(ta,tk):
        for n in snaps: put(n,snaps[n])
        rot("Leg_Thigh."+s,X,ta); rot("Leg_Calf."+s,X,-tk); rot("Leg_Foot."+s,X,tk-ta)
        p=arm.pose.bones["Leg_Foot."+s].head
        return (p.y-target.y)**2+(p.z-target.z)**2
    ba,bk=LAST.get(s,(0.0,0.0))
    best=at(ba,bk)
    step=2.0
    while step>0.005:
        moved=False
        for da,dk in ((step,0),(-step,0),(0,step),(0,-step),(step,step),(-step,-step),(step,2*step),(-step,-2*step)):
            if bk+dk<0: continue
            e=at(ba+da,bk+dk)
            if e<best-1e-12:
                best=e; ba,bk=ba+da,bk+dk; moved=True
                break
        if not moved: step/=2
    at(ba,bk)
    LAST[s]=(ba,bk)
def pose(p,FZ):
    reset()
    feet={s:a.pose.bones["Leg_Foot."+s].head.copy() for s in "LR"}
    dz=p.get('com',0.0); dy=p.get('comy',0.0)
    move("COM",Vector((0,dy,dz)))
    sp=p.get('spine',0); rot("Spine1",X,sp*.5); rot("Spine2",X,sp*.3); rot("Chest",X,sp*.2)
    tw=p.get('twist',0); rot("Spine1",Z,tw*.4); rot("Spine2",Z,tw*.3); rot("Chest",Z,tw*.3)  # (the torso turned)
    sd=p.get('side',0); rot("Spine1",Vector((0,1,0)),sd*.6); rot("Spine2",Vector((0,1,0)),sd*.4)  # (leant to a side)
    stepL=p.get('stepL',0.0)
    if True:
        for s in "LR":
            tgt=feet[s].copy()
            if s=="L":
                tgt.y+=stepL; tgt.z+=p.get('liftL',0.0)
            legik(s,tgt)
    for s,sg in (("R",1),("L",-1)):
        if ('wx'+s) in p:
            armik(s,(p['wx'+s],p['wy'+s],p['wz'+s]),(p['px'+s],p['py'+s],p['pz'+s]),(p['ax'+s],p['ay'+s],p['az'+s]),(p['nx'+s],p['ny'+s],p['nz'+s]))
            continue
        arm(s,sg,p.get('up'+s,0),p.get('fore'+s,0),p.get('yaw'+s,0),p.get('twist'+s,0),p.get('hand'+s,0),p.get('abd'+s,0))
def arm(s,sg,up,fore,yaw,twist,hand,abd=0):
    rot("Arm_UpperArm."+s,X,up); rot("Arm_UpperArm."+s,Z,sg*yaw)
    rot("Arm_UpperArm."+s,Vector((0,1,0)),sg*abd)  # (out to the side: the elbow away from the body)
    rot("Arm_ForeArm1."+s,X,fore)
    if abs(twist)>1e-6:
        # the forearm turned about its own length (the palm turned over), the turn shared down the twist bones
        f1=a.pose.bones["Arm_ForeArm1."+s]; h=a.pose.bones["Arm_Hand."+s]
        axis=(h.head-f1.head).normalized()
        rot("Arm_ForeArm1."+s,axis,twist)
    rot("Arm_Hand."+s,X,hand)

def armik(s,wrist,pole,along,normal):
    # two-bone IK to put the wrist (the Hand bone's head) on `wrist`, the elbow bent toward `pole`; the hand then turned
    # so its fingers run along `along` and its palm faces `normal` (hand space: fingers +X, palm -Y)
    up=a.pose.bones["Arm_UpperArm."+s]; f1=a.pose.bones["Arm_ForeArm1."+s]; hd=a.pose.bones["Arm_Hand."+s]
    S=up.head.copy(); E=f1.head.copy(); W=hd.head.copy()
    L1=(E-S).length; L2=(W-E).length
    T=Vector(wrist); d=T-S; dn=d.normalized()
    # (reach eased off near full stretch: a hard stop there snapped the elbow straight)
    m=L1+L2; k=0.92*m; dist=max(d.length,abs(L1-L2)+1e-3)
    if dist>k: dist=k+(m-k)*(1-math.exp(-(dist-k)/(m-k)))
    # the elbow: on the circle about S->T, toward the pole
    cosA=(L1*L1+dist*dist-L2*L2)/(2*L1*dist); cosA=max(-1,min(1,cosA)); sinA=math.sqrt(1-cosA*cosA)
    pv=Vector(pole)-S; pv=pv-dn*pv.dot(dn)
    if pv.length<1e-6: pv=Vector((0,0,-1))-dn*(-dn.z)
    pv.normalize()
    Enew=S+dn*(L1*cosA)+pv*(L1*sinA)
    def aim(bone,frm,to):
        q=(frm).rotation_difference(to)
        M=bone.matrix.copy(); h=M.translation.copy()
        bone.matrix=Matrix.Translation(h)@q.to_matrix().to_4x4()@Matrix.Translation(-h)@M; VL.update()
    aim(up,(E-S),(Enew-S))
    E2=f1.head.copy(); W2=hd.head.copy()
    aim(f1,(W2-E2),(S+dn*dist-E2))
    # the hand: fingers along `along`, palm to `normal`
    A=Vector(along).normalized(); N=Vector(normal); N=(N-A*N.dot(A)).normalized()
    Y=-N; Zv=A.cross(Y)
    R=Matrix((A,Y,Zv)).transposed()
    M=hd.matrix.copy(); hd.matrix=Matrix.Translation(M.translation)@R.to_4x4()@Matrix.Diagonal(M.to_scale()).to_4x4(); VL.update()

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
    LAST.clear()
    a.animation_data.action=None
    reset(); FZ={s:a.pose.bones["Leg_Foot."+s].head.z for s in "LR"}
    act=bpy.data.actions.get(name)
    if act: bpy.data.actions.remove(act)
    n=keys[-1][0]; frames=[]
    base=globals().get('HANDS_FROM',{}).get(name)
    for f in range(n+1):
        p=lk(keys,f)
        if base:
            p.update({k:v for k,v in lk(KEYS[base],f).items() if k not in BODY})
        pose(p,FZ)
        frames.append({pb.name:snap(pb.name) for pb in a.pose.bones})
    # each bone's rotation kept on one side from key to key (q and -q are the same turn, but in the game a key on the
    # other side from the one before spun the bone the long way round between them: the left thigh did, at the start
    # and the end of every throw)
    for pb in a.pose.bones:
        for f in range(1,len(frames)):
            if frames[f][pb.name][1].dot(frames[f-1][pb.name][1])<0:
                frames[f][pb.name]=(frames[f][pb.name][0],-frames[f][pb.name][1])+tuple(frames[f][pb.name][2:])
    act=bpy.data.actions.new(name); a.animation_data.action=act
    for f,fr in enumerate(frames):
        for pb in a.pose.bones:
            pb.location=fr[pb.name][0]; pb.rotation_quaternion=fr[pb.name][1]; pb.scale=(1,1,1)
            pb.keyframe_insert("rotation_quaternion",frame=f+1); pb.keyframe_insert("location",frame=f+1); pb.keyframe_insert("scale",frame=f+1)
    sc.frame_start=1; sc.frame_end=n+1
    return FZ


KEYS = {'AGBasketballShot': [(0,
                       {'axL': -0.1930101066827774,
                        'axR': 0.09922963380813599,
                        'ayL': 0.3476814329624176,
                        'ayR': 0.25116434693336487,
                        'azL': -0.9175318479537964,
                        'azR': -0.9628446698188782,
                        'nxL': 0.9736315608024597,
                        'nxR': -0.9887716770172119,
                        'nyL': -0.048032887279987335,
                        'nyR': -0.08376280963420868,
                        'nzL': -0.2230122685432434,
                        'nzR': -0.12375172972679138,
                        'pxL': -45,
                        'pxR': 45,
                        'pyL': -10,
                        'pyR': -10,
                        'pzL': 70,
                        'pzR': 70,
                        'wxL': -16.81695556640625,
                        'wxR': 21.443939208984375,
                        'wyL': 4.916363716125488,
                        'wyR': 0.6680952310562134,
                        'wzL': 70.69135284423828,
                        'wzR': 70.35301208496094}),
                      (7,
                       {'axL': 0.0,
                        'axR': 0.6273874044418335,
                        'ayL': 0.2873478829860687,
                        'ayR': -0.1731272041797638,
                        'azL': 0.9578262567520142,
                        'azR': 0.759218156337738,
                        'com': -3,
                        'nxL': 1.0,
                        'nxR': -0.5234239101409912,
                        'nyL': 0.0,
                        'nyR': 0.6281087398529053,
                        'nzL': 0.0,
                        'nzR': 0.5757663249969482,
                        'pxL': -38,
                        'pxR': 38,
                        'pyL': 5,
                        'pyR': 5,
                        'pzL': 58,
                        'pzR': 58,
                        'spine': -4,
                        'wxL': -13.0,
                        'wxR': 7.798674583435059,
                        'wyL': 28.362117767333984,
                        'wyR': 20.93708610534668,
                        'wzL': 84.54039001464844,
                        'wzR': 76.46018981933594}),
                      (14,
                       {'axL': 0.0,
                        'axR': 0.6273874044418335,
                        'ayL': 0.2873478829860687,
                        'ayR': -0.1731272041797638,
                        'azL': 0.9578262567520142,
                        'azR': 0.759218156337738,
                        'com': -10,
                        'nxL': 1.0,
                        'nxR': -0.5234239101409912,
                        'nyL': 0.0,
                        'nyR': 0.6281087398529053,
                        'nzL': 0.0,
                        'nzR': 0.5757663249969482,
                        'pxL': -38,
                        'pxR': 38,
                        'pyL': 5,
                        'pyR': 5,
                        'pzL': 58,
                        'pzR': 58,
                        'spine': -8,
                        'wxL': -13.0,
                        'wxR': 7.798674583435059,
                        'wyL': 28.362117767333984,
                        'wyR': 20.93708610534668,
                        'wzL': 86.54039001464844,
                        'wzR': 78.46018981933594}),
                      (17,
                       {'axL': 0.0,
                        'axR': 0.40371617674827576,
                        'ayL': 0.2873478829860687,
                        'ayR': -0.5413315892219543,
                        'azL': 0.9578262567520142,
                        'azR': 0.7375455498695374,
                        'com': -3,
                        'nxL': 1.0,
                        'nxR': -0.46725672483444214,
                        'nyL': 0.0,
                        'nyR': 0.5710915923118591,
                        'nzL': 0.0,
                        'nzR': 0.6749264001846313,
                        'pxL': -45,
                        'pxR': 45,
                        'pyL': 8,
                        'pyR': 8,
                        'pzL': 72,
                        'pzR': 72,
                        'spine': -3,
                        'wxL': -13.0,
                        'wxR': 8.174925804138184,
                        'wyL': 27.362117767333984,
                        'wyR': 22.948123931884766,
                        'wzL': 102.54039001464844,
                        'wzR': 92.9971694946289}),
                      (20,
                       {'axL': 0.0,
                        'axR': 0.32538697123527527,
                        'ayL': 0.2873478829860687,
                        'ayR': -0.6849671602249146,
                        'azL': 0.9578262567520142,
                        'azR': 0.6518767476081848,
                        'com': -3,
                        'nxL': 1.0,
                        'nxR': -0.4466625154018402,
                        'nyL': 0.0,
                        'nyR': 0.4962916970252991,
                        'nzL': 0.0,
                        'nzR': 0.744437575340271,
                        'pxL': -55.0,
                        'pxR': 55.0,
                        'pyL': 15.0,
                        'pyR': 15.0,
                        'pzL': 102.0,
                        'pzR': 102.0,
                        'spine': 0,
                        'wxL': -13.0,
                        'wxR': 8.291894912719727,
                        'wyL': 23.362117767333984,
                        'wyR': 20.963645935058594,
                        'wzL': 127.54039001464844,
                        'wzR': 117.37329864501953}),
                      (24,
                       {'axL': 0.0,
                        'axR': 0.47667649388313293,
                        'ayL': 0.2873478829860687,
                        'ayR': -0.5835177898406982,
                        'azL': 0.9578262567520142,
                        'azR': 0.6574848890304565,
                        'com': -0.5,
                        'nxL': 1.0,
                        'nxR': -0.2981424033641815,
                        'nyL': 0.0,
                        'nyR': 0.596284806728363,
                        'nzL': 0.0,
                        'nzR': 0.7453559637069702,
                        'pxL': -50.0,
                        'pxR': 50.0,
                        'pyL': 20.0,
                        'pyR': 20.0,
                        'pzL': 112.0,
                        'pzR': 112.0,
                        'spine': 4,
                        'wxL': -13.0,
                        'wxR': 5.05322265625,
                        'wyL': 22.362117767333984,
                        'wyR': 17.785493850708008,
                        'wzL': 138.54039001464844,
                        'wzR': 128.32664489746094}),
                      (33,
                       {'axL': 0.0,
                        'axR': 0.10052846465607779,
                        'ayL': 0.2873478829860687,
                        'ayR': 0.6440819229011265,
                        'azL': 0.9578262567520142,
                        'azR': 0.7583221639816939,
                        'com': -0.3,
                        'nxL': 1.0,
                        'nxR': -0.37426169637245454,
                        'nyL': 0.0,
                        'nyR': 0.7306825350264612,
                        'nzL': 0.0,
                        'nzR': -0.5709914321911648,
                        'pxL': -50,
                        'pxR': 50,
                        'pyL': 10,
                        'pyR': 10,
                        'pzL': 95,
                        'pzR': 95,
                        'spine': 3,
                        'wxL': -17.0,
                        'wxR': 5.05322265625,
                        'wyL': 20.362117767333984,
                        'wyR': 23.785493850708008,
                        'wzL': 134.54039001464844,
                        'wzR': 129.32664489746094}),
                      (50,
                       {'axL': -0.1930101066827774,
                        'axR': 0.09922963380813599,
                        'ayL': 0.3476814329624176,
                        'ayR': 0.25116434693336487,
                        'azL': -0.9175318479537964,
                        'azR': -0.9628446698188782,
                        'nxL': 0.9736315608024597,
                        'nxR': -0.9887716770172119,
                        'nyL': -0.048032887279987335,
                        'nyR': -0.08376280963420868,
                        'nzL': -0.2230122685432434,
                        'nzR': -0.12375172972679138,
                        'pxL': -45,
                        'pxR': 45,
                        'pyL': -10,
                        'pyR': -10,
                        'pzL': 70,
                        'pzR': 70,
                        'wxL': -16.81695556640625,
                        'wxR': 21.443939208984375,
                        'wyL': 4.916363716125488,
                        'wyR': 0.6680952310562134,
                        'wzL': 70.69135284423828,
                        'wzR': 70.35301208496094})],
 'AGBasketballShot2': [(0, {'com': 0, 'comy': 0, 'liftL': 0, 'spine': 0, 'stepL': 0}),
                       (6, {'com': -4, 'comy': 0, 'liftL': 2.5, 'spine': -6, 'stepL': 4}),
                       (12, {'com': -14, 'comy': 0, 'liftL': 0, 'spine': -11, 'stepL': 7}),
                       (17, {'com': -6, 'comy': 0, 'liftL': 0, 'spine': -5, 'stepL': 7}),
                       (20, {'com': -2.5, 'comy': 0, 'liftL': 0, 'spine': 0, 'stepL': 7}),
                       (24, {'com': -0.5, 'comy': 0, 'liftL': 0, 'spine': 5, 'stepL': 7}),
                       (33, {'com': -0.3, 'comy': 0, 'liftL': 0, 'spine': 3, 'stepL': 7}),
                       (42, {'com': -1.0, 'comy': 0, 'liftL': 3, 'spine': 3, 'stepL': 3.5}),
                       (50, {'com': 0, 'comy': 0, 'liftL': 0, 'spine': 0, 'stepL': 0})],
 'AGBasketballShot3': [(0, {'com': 0, 'comy': 0, 'liftL': 0, 'side': 0, 'spine': 0, 'stepL': 0, 'twist': 0}),
                       (6, {'com': -5, 'comy': 0, 'liftL': 3, 'side': -3, 'spine': -4, 'stepL': -4, 'twist': 8}),
                       (14, {'com': -20, 'comy': 0, 'liftL': 0, 'side': -5, 'spine': -12, 'stepL': -8, 'twist': 14}),
                       (18, {'com': -9, 'comy': 0, 'liftL': 0, 'side': -3, 'spine': -6, 'stepL': -8, 'twist': 9}),
                       (21, {'com': -2, 'comy': 0, 'liftL': 0, 'side': -1, 'spine': 1, 'stepL': -8, 'twist': 3}),
                       (24, {'com': -0.5, 'comy': 0, 'liftL': 0, 'side': 0, 'spine': 4, 'stepL': -8, 'twist': 0}),
                       (33, {'com': -0.3, 'comy': 0, 'liftL': 0, 'side': 0, 'spine': 3, 'stepL': -8, 'twist': -2}),
                       (42, {'com': -3, 'comy': 0, 'liftL': 3, 'side': 0, 'spine': 1, 'stepL': -4, 'twist': -1}),
                       (50, {'com': 0, 'comy': 0, 'liftL': 0, 'side': 0, 'spine': 0, 'stepL': 0, 'twist': 0})],
 'AGSkeeballRoll': [(0, {}),
                    (8,
                     {'com': -4,
                      'foreL': 40,
                      'foreR': 55,
                      'handR': 0,
                      'liftL': 4,
                      'spine': -8,
                      'stepL': 4,
                      'twistR': 96,
                      'upL': 20,
                      'upR': 25,
                      'yawR': 5}),
                    (14,
                     {'com': -14,
                      'comy': 4,
                      'foreL': 35,
                      'foreR': 1.0,
                      'handR': 40.0,
                      'spine': -24,
                      'stepL': 16,
                      'twistR': 96.0,
                      'upL': 30,
                      'upR': -32.5,
                      'yawR': -2.5}),
                    (22,
                     {'com': -22,
                      'comy': 8,
                      'foreL': 35,
                      'foreR': 14.0,
                      'handR': -40.0,
                      'spine': -36,
                      'stepL': 16,
                      'twistR': 98.5,
                      'upL': 30,
                      'upR': 57.0,
                      'yawR': 3.0}),
                    (30,
                     {'com': -16,
                      'comy': 7,
                      'foreL': 30,
                      'foreR': 32.0,
                      'handR': -20.5,
                      'spine': -28,
                      'stepL': 16,
                      'twistR': 96.5,
                      'upL': 25,
                      'upR': 85.5,
                      'yawR': 2.0}),
                    (38,
                     {'com': -12,
                      'comy': 5,
                      'foreL': 30,
                      'foreR': 30,
                      'handR': -15,
                      'spine': -22,
                      'stepL': 16,
                      'twistR': 96,
                      'upL': 20,
                      'upR': 88,
                      'yawR': 2}),
                    (46,
                     {'com': -4, 'comy': 2, 'foreR': 20, 'liftL': 3, 'spine': -8, 'stepL': 8, 'twistR': 45, 'upR': 20}),
                    (54, {})],
 'AGSkeeballRoll2': [(0, {'com': 0, 'comy': 0, 'liftL': 0, 'spine': 0, 'stepL': 0}),
                     (8, {'com': -6, 'comy': 1, 'liftL': 6, 'spine': -12, 'stepL': 10}),
                     (14, {'com': -19, 'comy': 5, 'liftL': 0, 'spine': -30, 'stepL': 16}),
                     (22, {'com': -22, 'comy': 8, 'liftL': 0, 'spine': -36, 'stepL': 16}),
                     (30, {'com': -21, 'comy': 8, 'liftL': 0, 'spine': -35, 'stepL': 16}),
                     (38, {'com': -17, 'comy': 6, 'liftL': 0, 'spine': -29, 'stepL': 16}),
                     (46, {'com': -7, 'comy': 3, 'liftL': 4, 'spine': -13, 'stepL': 9}),
                     (54, {'com': 0, 'comy': 0, 'liftL': 0, 'spine': 0, 'stepL': 0})],
 'AGSkeeballRoll3': [(0, {'com': 0, 'comy': 0, 'liftL': 0, 'side': 0, 'spine': 0, 'stepL': 0, 'twist': 0}),
                     (8, {'com': -8, 'comy': 2, 'liftL': 7, 'side': 0, 'spine': -14, 'stepL': 12, 'twist': -6}),
                     (14, {'com': -24, 'comy': 6, 'liftL': 0, 'side': -4, 'spine': -32, 'stepL': 19, 'twist': -16}),
                     (22, {'com': -22, 'comy': 8, 'liftL': 0, 'side': 0, 'spine': -36, 'stepL': 16, 'twist': 0}),
                     (30, {'com': -20, 'comy': 8, 'liftL': 0, 'side': 3, 'spine': -33, 'stepL': 16, 'twist': 10}),
                     (38, {'com': -14, 'comy': 5, 'liftL': 0, 'side': 3, 'spine': -24, 'stepL': 16, 'twist': 12}),
                     (46, {'com': -5, 'comy': 2, 'liftL': 5, 'side': 0, 'spine': -9, 'stepL': 8, 'twist': 5}),
                     (54, {'com': 0, 'comy': 0, 'liftL': 0, 'side': 0, 'spine': 0, 'stepL': 0, 'twist': 0})]}

# a second take of each throw: its own body (stance, crouch, lean), its hands the first's, frame by frame (the ball
# held and let go just as it is in the first: the same places, the same frame)
HANDS_FROM = {'AGBasketballShot2': 'AGBasketballShot', 'AGSkeeballRoll2': 'AGSkeeballRoll',
              'AGBasketballShot3': 'AGBasketballShot', 'AGSkeeballRoll3': 'AGSkeeballRoll'}
BODY = ('com', 'comy', 'spine', 'stepL', 'liftL', 'twist', 'side')


for _name, _keys in KEYS.items():
    build(_name, _keys)
