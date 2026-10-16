"""Explicit generated material contract: legacy FBX reimport does not own palette refresh."""
import unreal

DEST = '/Game/BlockoutGenerated/Materials'
PARAMETER = 'AuthoredColor'


def material_for(name, rgba):
    if not name.isalnum() or len(rgba) != 4:
        raise ValueError('Invalid authored palette entry: ' + name)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    master_path = DEST + '/M_BlockoutPalette'
    master = unreal.load_asset(master_path)
    if master is None:
        master = tools.create_asset('M_BlockoutPalette', DEST, unreal.Material, unreal.MaterialFactoryNew())
        expression = unreal.MaterialEditingLibrary.create_material_expression(
            master, unreal.MaterialExpressionVectorParameter, -200, 0)
        expression.set_editor_property('parameter_name', PARAMETER)
        expression.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
        unreal.MaterialEditingLibrary.connect_material_property(expression, '', unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(master)
        unreal.EditorAssetLibrary.save_asset(master_path)
    if not isinstance(master, unreal.Material):
        raise RuntimeError('Generated palette master path has incompatible asset')
    path = DEST + '/MI_' + name
    instance = unreal.load_asset(path)
    if instance is None:
        instance = tools.create_asset('MI_' + name, DEST, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    if not isinstance(instance, unreal.MaterialInstanceConstant):
        raise RuntimeError('Generated palette instance path has incompatible asset: ' + path)
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, master)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, PARAMETER, unreal.LinearColor(*rgba))
    unreal.EditorAssetLibrary.save_asset(path)
    return instance


def apply_palette(mesh, palette):
    applied = []
    for index, slot in enumerate(mesh.static_materials):
        name = str(slot.material_slot_name)
        if name not in palette:
            raise RuntimeError('Unmapped FBX material slot: ' + name)
        instance = material_for(name, palette[name])
        mesh.set_material(index, instance)
        actual = unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, PARAMETER)
        actual_rgba = [actual.r, actual.g, actual.b, actual.a]
        if any(abs(a - b) > .0001 for a, b in zip(actual_rgba, palette[name])):
            raise RuntimeError('Generated material color did not update: ' + name)
        applied.append({'slot': name, 'material': instance.get_path_name(), 'rgba': actual_rgba})
    return applied
