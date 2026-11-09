# ui_blender.py - renders the 3D parts of the menu art with Blender (bpy):
#   logo.png  : "DRAGONS BEYOND" in carved gold letters
#   panel.png : a dark carved-stone slab with a raised gold rim and jewelled corners (cut into 9 pieces by the game)
#   gem.png   : an emerald in a gold claw setting (the menu cursor)
# usage: python3 tools/ui_blender.py <font.ttf> <outdir>
import bpy, math, sys, os
font_path, outdir = sys.argv[1], sys.argv[2]
os.makedirs(outdir, exist_ok=True)

def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.device = 'CPU'; sc.cycles.samples = 96; sc.cycles.use_denoising = True
    sc.render.film_transparent = True; sc.render.image_settings.file_format = 'PNG'; sc.render.image_settings.color_mode = 'RGBA'
    sc.view_settings.view_transform = 'Standard'; sc.view_settings.look = 'None'
    w = bpy.data.worlds.new('w'); sc.world = w; w.use_nodes = True
    nt = w.node_tree; bg = nt.nodes['Background']
    # a warm sky over dark ground, so the gold has something to reflect
    tc = nt.nodes.new('ShaderNodeTexCoord'); sep = nt.nodes.new('ShaderNodeSeparateXYZ'); ramp = nt.nodes.new('ShaderNodeValToRGB')
    nt.links.new(tc.outputs['Generated'], sep.inputs[0]); nt.links.new(sep.outputs['Z'], ramp.inputs['Fac'])
    r = ramp.color_ramp; r.elements[0].position = 0.45; r.elements[0].color = (0.02, 0.015, 0.01, 1); r.elements[1].position = 0.6; r.elements[1].color = (1.0, 0.85, 0.6, 1)
    e = r.elements.new(0.52); e.color = (0.35, 0.22, 0.1, 1)
    nt.links.new(ramp.outputs['Color'], bg.inputs['Color']); bg.inputs['Strength'].default_value = 1.2
    return sc

def mat_gold(name='gold', base=(1.0, 0.70, 0.28), rough=0.28, worn=True):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*base, 1); p.inputs['Metallic'].default_value = 1.0; p.inputs['Roughness'].default_value = rough
    if worn:   # darker, rougher in the hollows: old gold
        n = nt.nodes.new('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = 5; n.inputs['Detail'].default_value = 8
        cr = nt.nodes.new('ShaderNodeValToRGB'); cr.color_ramp.elements[0].color = (base[0] * 0.55, base[1] * 0.42, base[2] * 0.3, 1); cr.color_ramp.elements[1].color = (*base, 1)
        cr.color_ramp.elements[0].position = 0.35; cr.color_ramp.elements[1].position = 0.65
        nt.links.new(n.outputs['Fac'], cr.inputs['Fac']); nt.links.new(cr.outputs['Color'], p.inputs['Base Color'])
        rr = nt.nodes.new('ShaderNodeMapRange'); rr.inputs['To Min'].default_value = rough * 0.6; rr.inputs['To Max'].default_value = rough * 1.6
        nt.links.new(n.outputs['Fac'], rr.inputs['Value']); nt.links.new(rr.outputs['Result'], p.inputs['Roughness'])
    return m

def mat_stone(name='stone'):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; p = nt.nodes['Principled BSDF']
    p.inputs['Roughness'].default_value = 0.85
    vor = nt.nodes.new('ShaderNodeTexVoronoi'); vor.inputs['Scale'].default_value = 7
    noi = nt.nodes.new('ShaderNodeTexNoise'); noi.inputs['Scale'].default_value = 40; noi.inputs['Detail'].default_value = 10
    mix = nt.nodes.new('ShaderNodeMath'); mix.operation = 'MULTIPLY_ADD'; mix.inputs[1].default_value = 0.5
    nt.links.new(noi.outputs['Fac'], mix.inputs[0]); nt.links.new(vor.outputs['Distance'], mix.inputs[2])
    cr = nt.nodes.new('ShaderNodeValToRGB'); cr.color_ramp.elements[0].color = (0.004, 0.005, 0.005, 1); cr.color_ramp.elements[1].color = (0.022, 0.026, 0.024, 1)
    nt.links.new(mix.outputs['Value'], cr.inputs['Fac']); nt.links.new(cr.outputs['Color'], p.inputs['Base Color'])
    bump = nt.nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = 0.6; bump.inputs['Distance'].default_value = 0.05
    nt.links.new(mix.outputs['Value'], bump.inputs['Height']); nt.links.new(bump.outputs['Normal'], p.inputs['Normal'])
    return m

def mat_gem(color=(0.0, 0.45, 0.12)):
    m = bpy.data.materials.new('gem'); m.use_nodes = True; p = m.node_tree.nodes['Principled BSDF']
    p.inputs['Base Color'].default_value = (*color, 1); p.inputs['Roughness'].default_value = 0.02
    p.inputs['Transmission Weight'].default_value = 0.85; p.inputs['IOR'].default_value = 1.6
    p.inputs['Emission Color'].default_value = (*color, 1); p.inputs['Emission Strength'].default_value = 0.25
    return m

def lights(sc, target=(0, 0, 0)):
    for name, loc, en, size, col in (('key', (-4, -6, 7), 900, 3, (1, 0.95, 0.85)), ('rim', (5, 4, 3), 500, 2, (1, 0.8, 0.55)), ('fill', (4, -6, 2), 150, 4, (0.7, 0.8, 1.0))):
        d = bpy.data.lights.new(name, 'AREA'); d.energy = en; d.size = size; d.color = col
        o = bpy.data.objects.new(name, d); sc.collection.objects.link(o); o.location = loc
        c = o.constraints.new('TRACK_TO'); e = bpy.data.objects.new(name + '_t', None); sc.collection.objects.link(e); e.location = target; c.target = e

def ortho_cam(sc, scale, loc=(0, 0, 10), rot=(0, 0, 0)):
    cd = bpy.data.cameras.new('cam'); cd.type = 'ORTHO'; cd.ortho_scale = scale
    cam = bpy.data.objects.new('cam', cd); sc.collection.objects.link(cam); cam.location = loc; cam.rotation_euler = rot; sc.camera = cam

def render(sc, w, h, name):
    sc.render.resolution_x = w; sc.render.resolution_y = h; sc.render.filepath = os.path.join(outdir, name)
    bpy.ops.render.render(write_still=True)

def text_obj(sc, body, size, extrude, bevel, mat, z=0):
    cu = bpy.data.curves.new('t', 'FONT'); cu.body = body; cu.font = bpy.data.fonts.load(font_path); cu.size = size
    cu.align_x = 'CENTER'; cu.align_y = 'CENTER'; cu.extrude = extrude; cu.bevel_depth = bevel; cu.bevel_resolution = 4; cu.space_character = 1.04
    o = bpy.data.objects.new('t', cu); sc.collection.objects.link(o); o.data.materials.append(mat); o.location.z = z
    return o

# ---------- logo ----------
sc = reset()
gold = mat_gold('gold', (1.0, 0.72, 0.3), 0.22)
bronze = mat_gold('bronze', (0.28, 0.13, 0.05), 0.5, worn=False)
text_obj(sc, 'DRAGONS BEYOND', 1.0, 0.12, 0.035, gold, 0.1)
back = text_obj(sc, 'DRAGONS BEYOND', 1.0, 0.06, 0.0, bronze, -0.05); back.data.offset = 0.06   # dark rim behind the gold
lights(sc)
ortho_cam(sc, 11.2, (0, -0.9, 10), (math.radians(5), 0, 0))
render(sc, 2048, 384, 'logo.png')

# ---------- panel: stone slab, gold rim, jewelled corners (seen from above) ----------
sc = reset()
gold = mat_gold('gold', (1.0, 0.70, 0.28), 0.3); stone = mat_stone(); gem = mat_gem()
bpy.ops.mesh.primitive_plane_add(size=3.7); slab = bpy.context.object; slab.data.materials.append(stone)
def rim(half, depth, mat):
    cu = bpy.data.curves.new('rim', 'CURVE'); cu.dimensions = '3D'; cu.bevel_depth = depth; cu.bevel_resolution = 5
    sp = cu.splines.new('POLY'); pts = [(-half, -half), (half, -half), (half, half), (-half, half)]
    sp.points.add(3); [sp.points[i].co.__setitem__(slice(0, 4), (x, y, 0.02, 1)) for i, (x, y) in enumerate(pts)]; sp.use_cyclic_u = True
    o = bpy.data.objects.new('rim', cu); sc.collection.objects.link(o); o.data.materials.append(mat); return o
rim(1.80, 0.075, gold); rim(1.62, 0.03, gold); rim(1.93, 0.02, bronze if 'bronze' in bpy.data.materials else gold)
for sx in (-1, 1):
    for sy in (-1, 1):
        cx, cy = sx * 1.71, sy * 1.71
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.2, depth=0.08, location=(cx, cy, 0.04)); o = bpy.context.object; o.data.materials.append(gold)
        bpy.ops.object.modifier_add(type='BEVEL'); o.modifiers[-1].width = 0.03; o.modifiers[-1].segments = 3
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.12, location=(cx, cy, 0.12), segments=24, ring_count=12); g = bpy.context.object; g.scale.z = 0.6; g.data.materials.append(gem); bpy.ops.object.shade_smooth()
        for k in range(4):   # little gold studs around each corner jewel
            a = math.pi / 4 + k * math.pi / 2
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.04, location=(cx + math.cos(a) * 0.27, cy + math.sin(a) * 0.27, 0.05)); s = bpy.context.object; s.data.materials.append(gold); bpy.ops.object.shade_smooth()
lights(sc)
ortho_cam(sc, 4.15)
render(sc, 512, 512, 'panel.png')

# ---------- gem cursor ----------
sc = reset()
gold = mat_gold('gold', (1.0, 0.72, 0.3), 0.25); gem = mat_gem()
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, segments=8, ring_count=4); g = bpy.context.object; g.scale = (1, 1.35, 0.7); g.data.materials.append(gem)
bpy.ops.mesh.primitive_torus_add(major_radius=0.62, minor_radius=0.07); t = bpy.context.object; t.scale = (1, 1.3, 1); t.data.materials.append(gold); bpy.ops.object.shade_smooth()
for k in range(4):   # claws holding the stone
    a = k * math.pi / 2 + math.pi / 4
    bpy.ops.mesh.primitive_cone_add(radius1=0.09, depth=0.45, location=(math.cos(a) * 0.55, math.sin(a) * 0.7, 0.15)); c = bpy.context.object
    c.rotation_euler = (math.sin(a) * 1.0, -math.cos(a) * 1.0, 0); c.data.materials.append(gold); bpy.ops.object.shade_smooth()
lights(sc)
ortho_cam(sc, 1.9)
render(sc, 128, 128, 'gem.png')
r = bpy.data.materials['gem'].node_tree.nodes['Principled BSDF']
for k in ('Base Color', 'Emission Color'): r.inputs[k].default_value = (0.75, 0.02, 0.06, 1)
render(sc, 128, 128, 'ruby.png')   # the same jewel as a ruby: a heart
print('UI renders done')
