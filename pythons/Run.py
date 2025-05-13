import os
import yaml
import subprocess
import argparse
import TemplateInstantiate
import time

root_dir = "C:\\Users\\LJF\\Documents\\EventCamera"

# 0 is "..\Scenes\Bistro_v5_2\BistroInterior_Wine.pyscene"
# 1 is "..\Scenes\Bistro_v5_2\BistroExterior.pyscene"
# 2 is "..\Scenes\MEASURE_ONE\MEASURE_ONE.pyscene"
scenes = [
    os.path.join(root_dir, "..\\Scenes", "Bistro_v5_2", "BistroInterior_Wine.pyscene"),
    os.path.join(root_dir, "..\\Scenes", "Bistro_v5_2", "BistroExterior.pyscene"),
    os.path.join(root_dir, "..\\Scenes", "MEASURE_ONE", "MEASURE_ONE.pyscene")
]

def run(args):
    print(f"config file: {args.config}")
    with open(args.config, 'r') as file:
        config = yaml.safe_load(file)
    name = config.get('name', 'default')

    build_type = config.get('build', 'Release')
    assert(build_type in ['Release', 'Debug'])
    mogwai_path = os.path.join(root_dir, 'build', 'windows-ninja-msvc', 'bin', build_type, 'Mogwai.exe')

    scene_index = config.get('scene', 0)
    assert(scene_index in [0, 1, 2])
    scene = scenes[scene_index]

    # Use TemplateInstantiate.py to create script based on config parameters
    script_config = config.get('script', {})
    script_name = script_config.get('name', 'EventCamera')
    assert(script_name in ['EventCamera'])
    script = os.path.join(root_dir, 'scripts', f"{script_name}.py")
    template_path = os.path.join(root_dir, 'scripts', 'RendererScriptTemplate.py')
    script_output = os.path.join(root_dir, 'scripts', f"{script_name}.py")

    # Extract parameters from config
    samples_per_pixel = script_config.get('samplesPerPixel', 8)
    russian_roulette = script_config.get('russianRoulette', True)
    threshold = script_config.get('threshold', 1.5)
    need_accumulated_events = script_config.get('needAccumulatedEvents', 100)
    tolerance_events = script_config.get('toleranceEvents', 0.1)
    enable_compress = script_config.get('enableCompress', True)
    enable_block_storage = script_config.get('enableBlockStorage', False)
    directory = script_config.get('directory', 'Temp')

    time_scale = script_config.get('timeScale', 10000.0)
    exit_frame = script_config.get('exitFrame', 0)
    accumulatePass = script_config.get('accumulatePass', 1)

    if not os.path.exists(os.path.join(root_dir, '..\\output', directory)):
        os.makedirs(os.path.join(root_dir, '..\\output', directory))
    if not os.path.exists(os.path.join(root_dir, '..\\output', directory, 'Output')):
        os.makedirs(os.path.join(root_dir, '..\\output', directory, 'Output'))
    if not os.path.exists(os.path.join(root_dir, '..\\output', directory, 'OutputDI')):
        os.makedirs(os.path.join(root_dir, '..\\output', directory, 'OutputDI'))
    if not os.path.exists(os.path.join(root_dir, '..\\output', directory, 'OutputGI')):
        os.makedirs(os.path.join(root_dir, '..\\output', directory, 'OutputGI'))
    if not os.path.exists(os.path.join(root_dir, '..\\output', directory, 'OutputID')):
        os.makedirs(os.path.join(root_dir, '..\\output', directory, 'OutputID'))
    if not os.path.exists(os.path.join(root_dir, '..\\output', directory, 'OutputNormal')):
        os.makedirs(os.path.join(root_dir, '..\\output', directory, 'OutputNormal'))

    parameters = {
        "SAMPLES_PER_PIXEL": samples_per_pixel,
        "RUSSIAN_ROULETTE": russian_roulette,
        "THRESHOLD": threshold,
        "NEED_ACCUMULATED_EVENTS": need_accumulated_events,
        "TOLERANCE_EVENTS": tolerance_events,
        "EXIT_FRAME": exit_frame * accumulatePass,
        "ENABLED": enable_compress,
        "BLOCK_STORAGE_ENABLED": enable_block_storage,
        "TIME_SCALE": time_scale,
        "ACCUMULATE_PASS": accumulatePass,
        "DIRECTORY": f"C:\\\\Users\\\\LJF\\\\Documents\\\\EventCamera\\\\..\\\\output\\\\{directory}"
    }
    TemplateInstantiate.instantiate_template(template_path, script_output, parameters)

    verbosity = config.get('verbosity', 2)
    assert(verbosity in [0, 1, 2, 3, 4, 5])

    cmd = [mogwai_path, f"--script={script}", f"--scene={scene}", f"--verbosity={verbosity}", "--deferred"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Running configuration.")
    parser.add_argument('--config', type=str, default="config/default.yaml", help='Path to the YAML configuration file.')
    args = parser.parse_args()
    run(args)
