"""Import the conditioned performer GLB into Blender and save an editable .blend.

Run with:
  blender --background --python import_human_performer_blender.py -- <input.glb> <output.blend>
"""

import pathlib
import sys

import bpy


arguments = sys.argv[sys.argv.index("--") + 1 :]
if len(arguments) != 2:
    raise SystemExit("expected input GLB and output BLEND paths")

source = pathlib.Path(arguments[0]).resolve()
destination = pathlib.Path(arguments[1]).resolve()
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=str(source))
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 1.0
bpy.context.scene.render.fps = 60
destination.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(destination))
