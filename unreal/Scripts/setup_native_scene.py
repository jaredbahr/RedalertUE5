"""One commandlet: original blockout import followed by isolated gameplay map."""
from pathlib import Path
import runpy
import unreal

scripts = Path(__file__).resolve().parent
runpy.run_path(str(scripts / 'import_scene.py'), run_name='__main__')
runpy.run_path(str(scripts / 'create_gameplay_map.py'), run_name='__main__')
unreal.log('NATIVE_SETUP_OK')
