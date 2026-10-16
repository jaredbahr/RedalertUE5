"""Original primitive-only review blockouts; run with Blender, never system Python."""
import argparse
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--render-stills', action='store_true')
    parser.add_argument('--motion', action='store_true')
    parser.add_argument('--motion-frames', type=int, default=16)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    if args.output.resolve().drive.upper() != 'D:':
        parser.error('Generated output must be on D:.')
    if not 2 <= args.motion_frames <= 48:
        parser.error('--motion-frames must be between 2 and 48')
    return args


def material(name, color):
    value = bpy.data.materials.new(name)
    value.diffuse_color = (*color, 1)
    return value


def finish(obj, name, mat, parent):
    obj.name = name
    obj.data.materials.append(mat)
    obj.parent = parent
    return obj


def cube(name, location, scale, mat, parent, bevel=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=location)
    obj = finish(bpy.context.object, name, mat, parent)
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = obj.modifiers.new('Blockout edge', 'BEVEL')
        mod.width, mod.segments = bevel, 1
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def cylinder(name, location, radius, depth, mat, parent, vertices=12):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=location)
    return finish(bpy.context.object, name, mat, parent)


def sphere(name, location, scale, mat, parent):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=1, location=location)
    obj = finish(bpy.context.object, name, mat, parent)
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return obj


def rod(name, start, end, radius, mat, parent):
    vector = Vector(end) - Vector(start)
    obj = cylinder(name, (Vector(start) + Vector(end)) / 2, radius, vector.length, mat, parent)
    obj.rotation_euler = vector.to_track_quat('Z', 'Y').to_euler()
    return obj


def root(name):
    obj = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(obj)
    return obj


def reactor(m):
    obj = root('TeslaReactor_Blockout')
    cube('Foundation', (0, 0, .2), (6.8, 5.6, .4), m['concrete'], obj, .12)
    cube('Service building', (0, -.9, 1.05), (5.6, 2.7, 1.7), m['steel'], obj, .18)
    cube('Red roof', (0, -.9, 1.98), (5.8, 2.9, .3), m['red'], obj, .12)
    cube('Door', (0, -2.27, .88), (1, .08, 1.35), m['dark'], obj)
    for x in (-1.75, 1.75):
        cylinder('Tower plinth', (x, .9, .55), 1.3, .5, m['dark'], obj)
        cylinder('Power drum', (x, .9, 2.5), 1.02, 3.7, m['steel'], obj)
        for z in (1.0, 2.0, 3.0, 4.0):
            cylinder('Drum reinforcement', (x, .9, z), 1.11, .16, m['dark'], obj)
        sphere('Red tower dome', (x, .9, 4.25), (1.08, 1.08, .56), m['red'], obj)
        cylinder('Tower collar', (x, .9, 4.82), .38, .23, m['steel'], obj)
        rod('Terminal', (x, .9, 4.9), (x, .9, 5.45), .12, m['dark'], obj)
        cube('Power stripe', (x, -2.33, 1.2), (.5, .06, .5), m['red'], obj)
    rod('Cross conduit', (-1.75, .9, 3.7), (1.75, .9, 3.7), .17, m['steel'], obj)
    return obj


def tank(m):
    obj = root('RhinoTank_Blockout')
    # Local forward is -Y; the gun extends beyond the hull.
    for x in (-1.25, 1.25):
        cube('Track housing', (x, .25, .525), (.7, 4.6, 1.05), m['track'], obj, .28)
        for y in (-1.35, -.35, .65, 1.65):
            wheel = cylinder('Road wheel', (x * 1.035, y, .58), .34, .75, m['dark'], obj)
            wheel.rotation_euler.y = math.pi / 2
        cube('Track shoulder', (x, .25, 1.14), (.75, 4.3, .16), m['red'], obj, .08)
    cube('Angular armored hull', (0, .2, 1.02), (2.4, 4.25, .85), m['olive'], obj, .30)
    cube('Front glacis', (0, -1.62, 1.25), (2.3, .7, .45), m['olive'], obj, .12)
    cylinder('Turret ring', (0, .1, 1.49), .92, .22, m['dark'], obj)
    cube('Heavy squat turret', (0, .05, 1.89), (2.05, 2.0, .8), m['olive'], obj, .26)
    cube('Red turret recognition', (0, .86, 1.95), (1.45, .18, .5), m['red'], obj)
    rod('Main gun', (0, -.8, 1.93), (0, -3.35, 1.93), .13, m['dark'], obj)
    cube('Muzzle', (0, -3.4, 1.93), (.35, .5, .32), m['dark'], obj, .05)
    cylinder('Hatch', (0, .25, 2.34), .36, .12, m['red'], obj)
    for x in (-.78, .78):
        cube('Engine deck', (x, 1.55, 1.5), (.5, .75, .12), m['dark'], obj)
    return obj


def infantry(m):
    obj = root('Conscript_Blockout')
    # Deliberately broad helmet, coat and rifle for RTS readability; no finished rig.
    for x, y in ((-.16, -.12), (.16, .12)):
        cube('Boot', (x, y - .08, .10), (.25, .43, .20), m['dark'], obj, .04)
        rod('Trouser leg', (x, y, .20), (x, 0, .86), .13, m['cloth'], obj)
    cube('Coat skirt', (0, 0, .84), (.61, .39, .36), m['cloth'], obj, .05)
    cube('Broad coat', (0, 0, 1.20), (.61, .39, .5), m['cloth'], obj, .07)
    cube('Belt', (0, -.01, .99), (.64, .42, .09), m['dark'], obj)
    cylinder('Neck', (0, 0, 1.51), .105, .12, m['skin'], obj)
    sphere('Head', (0, -.01, 1.65), (.17, .15, .21), m['skin'], obj)
    sphere('Steel helmet', (0, .015, 1.79), (.24, .22, .16), m['olive'], obj)
    cylinder('Helmet brim', (0, .005, 1.75), .25, .055, m['olive'], obj)
    cube('Helmet red mark', (0, -.215, 1.79), (.075, .025, .075), m['red'], obj)
    for side in (-1, 1):
        rod('Coat upper arm', (side * .30, 0, 1.38), (side * .37, -.18, 1.16), .12, m['cloth'], obj)
        rod('Coat forearm', (side * .37, -.18, 1.16), (side * .15, -.38, 1.21), .1, m['cloth'], obj)
        sphere('Hand', (side * .15, -.38, 1.21), (.10, .09, .09), m['skin'], obj)
    rod('Rifle stock', (.23, -.36, .93), (-.04, -.41, 1.35), .065, m['wood'], obj)
    rod('Rifle barrel', (-.04, -.41, 1.35), (-.32, -.46, 1.79), .03, m['dark'], obj)
    return obj


def look_at(obj, target):
    obj.rotation_euler = (Vector(target) - obj.location).to_track_quat('-Z', 'Y').to_euler()


def write_motion_player(directory, frames):
    page = '''<!doctype html>
<meta charset="utf-8"><title>Unapproved blockout motion review</title>
<style>body{background:#202829;color:#e9ebe8;font:16px system-ui;margin:2rem auto;max-width:960px}
img{display:block;width:100%;height:auto}button{padding:.6rem 1rem;margin:1rem 1rem 0 0}
p{line-height:1.5}a{color:#a9d6ed}</style>
<h1>Original blockout review</h1><p>Unapproved silhouettes and scale. Placeholder tank turning,
infantry translation/body bob and camera pan/zoom. No walking rig, combat or gameplay simulation.</p>
<img id="view" src="motion_001.png" alt="Animated RTS camera blockout review">
<button id="toggle">Pause</button><span id="frame">1 / FRAME_COUNT</span>
<p><a href="rts_normal.png">Normal still</a> | <a href="rts_wide.png">Zoomed-out still</a></p>
<script>
let frame=1, running=true;const total=FRAME_COUNT;
document.getElementById('toggle').onclick=function(){running=!running;this.textContent=running?'Pause':'Play'};
setInterval(()=>{if(!running)return;frame=frame%total+1;
document.getElementById('view').src='motion_'+String(frame).padStart(3,'0')+'.png';
document.getElementById('frame').textContent=frame+' / '+total},125);
</script>'''
    (directory / 'review.html').write_text(page.replace('FRAME_COUNT', str(frames)), encoding='utf-8')


def export_asset(obj, directory):
    bpy.ops.object.select_all(action='DESELECT')
    for child in obj.children:
        child.select_set(True)
    bpy.context.view_layer.objects.active = obj.children[0]
    # Export duplicate joined geometry at asset-local ground origin, never the placed preview.
    bpy.ops.object.duplicate()
    bpy.ops.object.join()
    joined = bpy.context.object
    joined.name = 'SM_' + obj.name
    world_transform = joined.matrix_world.copy()
    joined.parent = None
    joined.matrix_world = world_transform
    # Joining retains the active mesh origin; explicitly reset origin without moving geometry.
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    # The active source primitive may be rotated (for example, a rifle rod).
    # Bake its inherited rotation so export axes and dimensions use world XYZ.
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.export_scene.fbx(filepath=str(directory / (joined.name + '.fbx')),
                             use_selection=True, object_types={'MESH'},
                             axis_forward='-Y', axis_up='Z', apply_unit_scale=True,
                             bake_anim=False, add_leaf_bones=False)
    dimensions = list(joined.dimensions)
    vertices = len(joined.data.vertices)
    bpy.data.objects.remove(joined, do_unlink=True)
    return {'file': 'assets/SM_' + obj.name + '.fbx', 'dimensions_m': dimensions,
            'vertices': vertices, 'origin': 'ground center', 'forward': '-Y', 'up': '+Z'}


def main():
    args = arguments()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    assets = output / 'assets'
    previews = output / 'previews'
    assets.mkdir(exist_ok=True)
    previews.mkdir(exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    scene.render.threads_mode, scene.render.threads = 'FIXED', 4
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.cavity_type = 'BOTH'
    scene.display.shading.background_type = 'WORLD'
    scene.world = bpy.data.worlds.new('Neutral review world')
    scene.world.color = (.14, .16, .18)
    scene.render.resolution_x, scene.render.resolution_y = 960, 640
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.view_settings.view_transform = 'Standard'
    colors = {'red': (.53, .035, .025), 'steel': (.42, .45, .44),
              'dark': (.10, .12, .12), 'track': (.13, .14, .13),
              'olive': (.31, .33, .20), 'cloth': (.43, .35, .22),
              'skin': (.67, .46, .31), 'wood': (.24, .12, .055),
              'concrete': (.30, .32, .31), 'ground': (.23, .26, .24)}
    mats = {name: material(name.title(), value) for name, value in colors.items()}
    objects = [reactor(mats), tank(mats), infantry(mats)]
    bpy.context.view_layer.update()
    manifest = {'status': 'unapproved original primitive blockouts',
                'blender_version': bpy.app.version_string, 'units': 'meters; FBX stores unit conversion metadata',
                'assets': [export_asset(obj, assets) for obj in objects],
                'reference': 'User brief and authored interpretation; no game assets or images imported. Visual fidelity unapproved.',
                'camera': {'normal_ortho_width_m': 24, 'wide_ortho_width_m': 36,
                           'location_m': [18, -24, 22], 'target_m': [0, 0, 1.3]},
                'motion': {'frames': args.motion_frames, 'fps': 8,
                           'description': 'Placeholder translation, tank turning, infantry bob, camera pan/zoom; no gameplay or network simulation'}}
    objects[0].location = (-4.1, 2.2, 0)
    objects[1].location = (3.0, -.6, 0)
    objects[2].location = (5.5, -3.5, 0)
    cube('Review ground', (0, 0, -.1), (40, 36, .2), mats['ground'], None)
    bpy.ops.object.camera_add(location=(18, -24, 22))
    camera = bpy.context.object
    camera.name = 'RTS_Review_Camera'
    camera.data.type, camera.data.ortho_scale = 'ORTHO', 24
    look_at(camera, (0, 0, 1.3))
    scene.camera = camera
    scene.render.fps = 8
    scene.frame_start, scene.frame_end = 1, args.motion_frames
    for frame, progress in ((1, 0.0), (args.motion_frames, 1.0)):
        objects[1].location = (3 - 1.6 * progress, -.6 - 1.7 * progress, 0)
        objects[1].rotation_euler.z = -.45 * progress
        objects[1].keyframe_insert(data_path='location', frame=frame)
        objects[1].keyframe_insert(data_path='rotation_euler', frame=frame)
        objects[2].location = (5.5 - 1.2 * progress, -3.5 - 1.2 * progress, 0)
        objects[2].keyframe_insert(data_path='location', frame=frame)
        camera.location = (18 + 2 * progress, -24 + 2 * progress, 22)
        look_at(camera, (progress, 0, 1.3))
        camera.keyframe_insert(data_path='location', frame=frame)
        camera.keyframe_insert(data_path='rotation_euler', frame=frame)
        camera.data.ortho_scale = 24 + 5 * progress
        camera.data.keyframe_insert(data_path='ortho_scale', frame=frame)
    # A simple body bob communicates motion without claiming a walking animation or rig.
    for frame in range(1, args.motion_frames + 1):
        objects[2].location.z = .045 * (1 - math.cos((frame - 1) * math.pi))
        objects[2].keyframe_insert(data_path='location', index=2, frame=frame)
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / 'RA2_Blockout_Review.blend'))
    (output / 'blockout_manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    if args.render_stills:
        scene.render.filepath = str(previews / 'rts_normal.png')
        bpy.ops.render.render(write_still=True)
        # Rendering evaluates animation: a direct edit to the keyed camera's scale
        # would be overwritten at frame 1. Use a separate unanimated wide camera.
        wide = camera.copy()
        wide.data = camera.data.copy()
        wide.animation_data_clear()
        wide.data.animation_data_clear()
        bpy.context.collection.objects.link(wide)
        wide.data.ortho_scale = 36
        scene.camera = wide
        scene.render.filepath = str(previews / 'rts_wide.png')
        bpy.ops.render.render(write_still=True)
        scene.camera = camera
        bpy.data.objects.remove(wide, do_unlink=True)
        scene.frame_set(1)
    if args.motion:
        scene.render.resolution_percentage = 67
        for frame in range(1, args.motion_frames + 1):
            scene.frame_set(frame)
            scene.render.filepath = str(previews / ('motion_%03d.png' % frame))
            bpy.ops.render.render(write_still=True)
        write_motion_player(previews, args.motion_frames)
    print('BLOCKOUT_OUTPUT=' + str(output))


if __name__ == '__main__':
    main()
