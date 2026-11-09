"""pose_test.py rig.blend out_prefix : renders rest + 4 test poses with Workbench (textured) to check the skinning."""
import bpy, sys, math
from mathutils import Euler
src, out = sys.argv[1], sys.argv[2]
bpy.ops.wm.open_mainfile(filepath=src)
sc = bpy.context.scene; arm = bpy.data.objects['DragonRig']; mesh = bpy.data.objects['Dragon']
# camera + light
cam_d = bpy.data.cameras.new('c'); cam = bpy.data.objects.new('c', cam_d); sc.collection.objects.link(cam); sc.camera = cam
tgt = bpy.data.objects.new('t', None); sc.collection.objects.link(tgt)
dims = mesh.dimensions; R = max(dims)
import mathutils; bb=[mesh.matrix_world @ mathutils.Vector(c) for c in mesh.bound_box]; ctr=sum(bb, mathutils.Vector())/8; tgt.location = ctr
cam.location = (ctr.x + R * 0.9, ctr.y - R * 1.0, ctr.z + R * 0.55); c = cam.constraints.new('TRACK_TO'); c.target = tgt; c.track_axis = 'TRACK_NEGATIVE_Z'; c.up_axis = 'UP_Y'
cam_d.lens = 50
sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'TEXTURE'
sc.display.shading.show_shadows = True; sc.display.shading.show_cavity = True
sc.render.resolution_x = 640; sc.render.resolution_y = 640; sc.render.film_transparent = False
sc.world = bpy.data.worlds.new('w'); sc.world.color = (0.8, 0.8, 0.78)
pb = arm.pose.bones
def rot(name, x=0, y=0, z=0):
    b = pb[name]; b.rotation_mode = 'XYZ'; b.rotation_euler = Euler((math.radians(x), math.radians(y), math.radians(z)), 'XYZ')
def reset():
    for b in pb: b.rotation_mode = 'XYZ'; b.rotation_euler = (0, 0, 0); b.location = (0, 0, 0)
poses = {
 'rest': lambda: None,
 'look': lambda: (rot('neck1', 0, 0, 35), rot('neck2', 0, 0, 25), rot('head', -15, 0, 10), rot('jaw', 25)),
 'step': lambda: (rot('legFL_upper', -45), rot('legFL_lower', 40), rot('legBR_upper', 35), rot('legBR_lower', -30), rot('spine2', 0, 0, 8)),
 'wings': lambda: (rot('wingL_1', 0, 0, 50), rot('wingR_1', 0, 0, -50), rot('wingL_2', 0, 0, 20), rot('wingR_2', 0, 0, -20), rot('tail1', 0, 0, 25), rot('tail2', 0, 0, 25), rot('tail3', 0, 0, 25)),
 'wingsX': lambda: (rot('wingL_1', 50, 0, 0), rot('wingR_1', 50, 0, 0), rot('wingL_2', 20, 0, 0), rot('wingR_2', 20, 0, 0)),
 'sit': lambda: (rot('legBL_upper', 60), rot('legBR_upper', 60), rot('legBL_lower', -70), rot('legBR_lower', -70), rot('hips', -20), rot('neck1', 20), rot('tail2', 20), rot('tail3', 20), rot('tail4', 20)),
}
for name, fn in poses.items():
    reset(); fn()
    sc.render.filepath = f'{out}_{name}.png'; bpy.ops.render.render(write_still=True)
print('rendered', list(poses))
