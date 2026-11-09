"""render_clip.py anim.blend clip out_prefix [angle_deg] [size] : renders one clip's frames (Workbench, textured) -> out_prefix_fNN.png"""
import bpy, sys, math, mathutils
src, clip, out = sys.argv[1], sys.argv[2], sys.argv[3]
ang = float(sys.argv[4]) if len(sys.argv) > 4 else 35; size = int(sys.argv[5]) if len(sys.argv) > 5 else 480
bpy.ops.wm.open_mainfile(filepath=src)
sc = bpy.context.scene; arm = bpy.data.objects['DragonRig']; mesh = bpy.data.objects['Dragon']
act = bpy.data.actions[clip]; arm.animation_data.action = act
try: arm.animation_data.action_slot = act.slots[0]
except Exception: pass
sc.frame_start, sc.frame_end = int(act.frame_start), int(act.frame_end)
cam_d = bpy.data.cameras.new('c'); cam = bpy.data.objects.new('c', cam_d); sc.collection.objects.link(cam); sc.camera = cam
tgt = bpy.data.objects.new('t', None); sc.collection.objects.link(tgt)
bb = [mesh.matrix_world @ mathutils.Vector(c) for c in mesh.bound_box]; ctr = sum(bb, mathutils.Vector()) / 8; R = max(mesh.dimensions)
tgt.location = ctr; a = math.radians(ang)
cam.location = (ctr.x + R * 1.5 * math.sin(a), ctr.y - R * 1.5 * math.cos(a), ctr.z + R * 0.6)
c = cam.constraints.new('TRACK_TO'); c.target = tgt; c.track_axis = 'TRACK_NEGATIVE_Z'; c.up_axis = 'UP_Y'; cam_d.lens = 40
# ground plane for shadow
bpy.ops.mesh.primitive_plane_add(size=R * 40, location=(ctr.x, ctr.y, min(v.z for v in bb)))
sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'STUDIO'; sc.display.shading.color_type = 'TEXTURE'
sc.display.shading.show_shadows = True; sc.display.shading.show_cavity = True
sc.render.resolution_x = size; sc.render.resolution_y = size
sc.world = bpy.data.worlds.new('w'); sc.world.color = (0.82, 0.82, 0.8)
sc.render.filepath = out + '_f'; sc.render.image_settings.file_format = 'PNG'
bpy.ops.render.render(animation=True)
print('frames', sc.frame_start, sc.frame_end)
