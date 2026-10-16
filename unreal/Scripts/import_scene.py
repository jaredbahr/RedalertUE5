"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Rebuilds ONLY /Game/BlockoutGenerated/Maps/VisualProof and imports/replaces
assets within /Game/BlockoutGenerated. Never touches another content folder.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[2]
DEST = "/Game/BlockoutGenerated"
ASSETS = ["SM_TeslaReactor_Blockout", "SM_RhinoTank_Blockout", "SM_Conscript_Blockout"]


def run():
    files = [ROOT / "generated" / "assets" / (name + ".fbx") for name in ASSETS]
    missing = [str(p) for p in files if not p.is_file()]
    if missing:
        raise RuntimeError("Run the Blender generator first. Missing FBX: " + ", ".join(missing))
    tasks = []
    for name, source in zip(ASSETS, files):
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_animations", False)
        options.set_editor_property("import_materials", True)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("automated_import_should_detect_type", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("convert_scene", True)
        data.set_editor_property("convert_scene_unit", True)
        data.set_editor_property("auto_generate_collision", False)
        # Explicit legacy factory bypasses Interchange in UE 5.7 AssetTools.
        task = unreal.AssetImportTask()
        task.set_editor_property("factory", unreal.FbxFactory())
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", DEST + "/Meshes")
        task.set_editor_property("destination_name", name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        task.set_editor_property("options", options)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    meshes = []
    for name, task in zip(ASSETS, tasks):
        package_path = DEST + "/Meshes/" + name
        object_path = package_path + "." + name
        imported_paths = [str(path) for path in task.get_editor_property("imported_object_paths")]
        if object_path not in imported_paths:
            raise RuntimeError("Task did not freshly import expected mesh: " + object_path)
        mesh = unreal.load_asset(object_path)
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError("Imported object is not a StaticMesh: " + object_path)
        meshes.append(mesh)

    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    map_path = DEST + "/Maps/VisualProof"
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        if not level.load_level(map_path):
            raise RuntimeError("Cannot load generated proof level")
        for existing in actors.get_all_level_actors():
            if existing.actor_has_tag("RedalertProofGenerated"):
                actors.destroy_actor(existing)
    elif not level.new_level(map_path):
        raise RuntimeError("Cannot create generated proof level")

    def spawn(actor_class, location, rotation=unreal.Rotator()):
        actor = actors.spawn_actor_from_class(actor_class, location, rotation)
        actor.set_editor_property("tags", ["RedalertProofGenerated"])
        return actor
    report = {"engine": unreal.SystemLibrary.get_engine_version(), "assets": [],
              "warning": "Unapproved silhouettes; no gameplay, multiplayer, bot or scale proof."}
    for name, mesh, x in zip(ASSETS, meshes, [-750.0, 100.0, 550.0]):
        actor = spawn(unreal.StaticMeshActor, unreal.Vector(x, 0, 0))
        actor.set_actor_label(name)
        actor.static_mesh_component.set_static_mesh(mesh)
        origin, extent = actor.get_actor_bounds(False)
        report["assets"].append({"name": name, "full_bounds_cm": [extent.x * 2, extent.y * 2, extent.z * 2]})
        if extent.z < 10 or extent.z > 5000:
            raise RuntimeError("Unexpected asset scale for " + name + ": " + str(extent))
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -15))
    floor.set_actor_label("Proof ground")
    floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    floor.set_actor_scale3d(unreal.Vector(45, 35, 0.3))
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(-45, -35, 0))
    sun.set_actor_label("Proof sun")
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 800))
    sky.set_actor_label("Proof fill")
    # Fixed world transforms make successive silhouette comparisons repeatable.
    for label, width in [("RTS_Normal", 2500.0), ("RTS_ZoomOut", 4500.0)]:
        camera = spawn(unreal.CameraActor, unreal.Vector(0, -2200, 2200), unreal.Rotator(-45, 90, 0))
        camera.set_actor_label(label)
        camera.camera_component.set_editor_property("projection_mode", unreal.CameraProjectionMode.ORTHOGRAPHIC)
        camera.camera_component.set_editor_property("ortho_width", width)
        camera.camera_component.set_editor_property("aspect_ratio", 16.0 / 9.0)
    generated_actors = [actor for actor in actors.get_all_level_actors()
                        if actor.actor_has_tag("RedalertProofGenerated")]
    expected_labels = set(ASSETS + ["Proof ground", "Proof sun", "Proof fill", "RTS_Normal", "RTS_ZoomOut"])
    labels = [actor.get_actor_label() for actor in generated_actors]
    if len(labels) != len(expected_labels) or set(labels) != expected_labels:
        raise RuntimeError("Generated actor count/labels do not match: " + str(labels))
    report["generated_actor_count"] = len(labels)
    report["generated_actor_labels"] = sorted(labels)
    if not level.save_current_level():
        raise RuntimeError("Cannot save generated proof level")
    unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=False, recursive=True)
    evidence = ROOT / "generated" / "ue_import_report.json"
    evidence.write_text(json.dumps(report, indent=2), encoding="utf-8")
    unreal.log("REDALERT_PROOF_OK " + str(evidence))


run()
