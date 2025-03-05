import os
import yaml
import subprocess
import argparse
import TemplateInstantiate

root_dir = "C:\\Users\\-LJF007-\\Documents\\EventCamera"

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
    threshold = script_config.get('threshold', 1.5)
    accumulate_max = script_config.get('accumulateMax', 100)
    exit_frame = script_config.get('exitFrame', 10000000)

    parameters = {
        "SAMPLES_PER_PIXEL": samples_per_pixel,
        "THRESHOLD": threshold,
        "ACCUMULATE_MAX": accumulate_max,
        "EXIT_FRAME": exit_frame,
    }
    TemplateInstantiate.instantiate_template(template_path, script_output, parameters)

    verbosity = config.get('verbosity', 2)
    assert(verbosity in [0, 1, 2, 3, 4, 5])

    cmd = [mogwai_path, f"--script={script}", f"--scene={scene}", f"--verbosity={verbosity}"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running: {' '.join(cmd)}")
        process = subprocess.Popen(cmd, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
        process.wait()
        print("Execution completed successfully.")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Running configuration.")
    parser.add_argument('--config', type=str, default="config/default.yaml", help='Path to the YAML configuration file.')
    args = parser.parse_args()
    run(args)
