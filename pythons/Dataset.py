import os
import yaml
import subprocess
import argparse
import TemplateInstantiate
import time
from pathlib import Path

root_dir = "F:\\EventCamera"

scenes = [
    os.path.join(root_dir, "../Scenes", "Bistro_v5_2", "BistroInterior_Wine.pyscene"),
    os.path.join(root_dir, "../Scenes", "Bistro_v5_2", "BistroExterior.pyscene"),
    os.path.join(root_dir, "../Scenes", "Bistro_v5_2", "BistroExterior_Night.pyscene"),
    os.path.join(root_dir, "../Scenes", "MEASURE_ONE", "MEASURE_ONE.pyscene"),
    os.path.join(root_dir, "../Scenes", "staircase", "staircase.pyscene"),
    os.path.join(root_dir, "../Scenes", "kitchen", "kitchen.pyscene"),
    os.path.join(root_dir, "../Scenes", "classroom", "classroom.pyscene"),
]

def run(args, scene_index):
    print(f"config file: {args.config}")
    with open(args.config, 'r') as file:
        config = yaml.safe_load(file)
    name = config.get('name', 'default')

    build_type = config.get('build', 'Release')
    assert(build_type in ['Release', 'Debug'])
    mogwai_path = os.path.join(root_dir, 'build', 'windows-ninja-msvc', 'bin', build_type, 'Mogwai')
    width = config.get('width', 0)
    height = config.get('height', 0)

    assert(scene_index in [0, 1, 2, 4, 5, 6])
    scene = scenes[scene_index]

    # Use TemplateInstantiate.py to create script based on config parameters
    script_config = config.get('script', {})
    dataset_script = os.path.join(root_dir, 'scripts', f"EventCamera.py")
    network_script = os.path.join(root_dir, 'scripts', f"Network.py")
    denoise_script = os.path.join(root_dir, 'scripts', f"Denoise.py")
    optix_denoise_script = os.path.join(root_dir, 'scripts', f"OptixDenoise.py")
    nrd_denoise_script = os.path.join(root_dir, 'scripts', f"NRD.py")
    dataset_template_path = os.path.join(root_dir, 'scripts', 'DatasetScriptTemplate.py')
    network_template_path = os.path.join(root_dir, 'scripts', 'NetworkScriptTemplate.py')
    denoise_template_path = os.path.join(root_dir, 'scripts', 'DenoiseScriptTemplate.py')
    optix_denoise_template_path = os.path.join(root_dir, 'scripts', 'OptixDenoiseScriptTemplate.py')
    nrd_denoise_template_path = os.path.join(root_dir, 'scripts', 'NRDScriptTemplate.py')

    # Extract parameters from config
    samples_per_pixel = script_config.get('samplesPerPixel', 8)
    russian_roulette = script_config.get('russianRoulette', True)
    threshold = script_config.get('threshold', 1.5)
    need_accumulated_events = script_config.get('needAccumulatedEvents', 100)
    tolerance_events = script_config.get('toleranceEvents', 0.1)
    enable_compress = script_config.get('enableCompress', True)
    enable_block_storage = script_config.get('enableBlockStorage', False)
    directory = Path(scenes[scene_index]).stem
    directory = os.path.join(root_dir, "..\\Dataset", directory)
    network_model = script_config.get('networkModel')
    batch_size = script_config.get('batchSize', 64)
    tau = script_config.get('tau')
    vThreshold = script_config.get('vThreshold')

    time_scale = script_config.get('timeScale', 10000.0)
    network_time_scale = script_config.get('networkTimeScale', 10000.0)
    exit_time = 20 if scene_index <= 2 else 10
    accumulatePass = script_config.get('accumulatePass', 1)

    if not os.path.exists(os.path.join(directory)):
        os.makedirs(os.path.join(directory))
    if not os.path.exists(os.path.join(directory, 'image')):
        os.makedirs(os.path.join(directory, 'image'))
    if not os.path.exists(os.path.join(directory, 'bin')):
        os.makedirs(os.path.join(directory, 'bin'))
    if not os.path.exists(os.path.join(directory, 'denoise')):
        os.makedirs(os.path.join(directory, 'denoise'))
    if not os.path.exists(os.path.join(directory, 'optix')):
        os.makedirs(os.path.join(directory, 'optix'))
    if not os.path.exists(os.path.join(directory, 'nrd')):
        os.makedirs(os.path.join(directory, 'nrd'))

    parameters = {
        "SAMPLES_PER_PIXEL": samples_per_pixel,
        "RUSSIAN_ROULETTE": russian_roulette,
        "THRESHOLD": threshold,
        "NEED_ACCUMULATED_EVENTS": need_accumulated_events,
        "TOLERANCE_EVENTS": tolerance_events,
        "EXIT_FRAME": exit_time * time_scale * accumulatePass,
        "ENABLED": enable_compress,
        "BLOCK_STORAGE_ENABLED": enable_block_storage,
        "TIME_SCALE": time_scale,
        "ACCUMULATE_PASS": accumulatePass,
        "DIRECTORY": directory,
        "NETWORK_MODEL": network_model,
        "BATCH_SIZE": batch_size,
        "TAU": tau,
        "VTHRESHOLD": vThreshold,
        "NETWORK_TIME_SCALE": network_time_scale,
        "NETWORK_EXIT_FRAME": exit_time * network_time_scale * accumulatePass,
        "OPTIX_NETWORK_EXIT_FRAME": exit_time * network_time_scale,
    }
    TemplateInstantiate.instantiate_template(dataset_template_path, dataset_script, parameters)
    TemplateInstantiate.instantiate_template(network_template_path, network_script, parameters)
    TemplateInstantiate.instantiate_template(denoise_template_path, denoise_script, parameters)
    TemplateInstantiate.instantiate_template(optix_denoise_template_path, optix_denoise_script, parameters)
    TemplateInstantiate.instantiate_template(nrd_denoise_template_path, nrd_denoise_script, parameters)

    verbosity = config.get('verbosity', 2)
    assert(verbosity in [0, 1, 2, 3, 4, 5])

    """
    # ------------------------- dataset -----------------------------
    cmd = [mogwai_path, f"--script={dataset_script}", f"--scene={scene}", f"--verbosity={verbosity}", "--deferred", f"--width={width}", f"--height={height}"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running Dataset: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    # ------------------------- network -----------------------------
    cmd = [mogwai_path, f"--script={network_script}", f"--scene={scene}", f"--verbosity={verbosity}", "--deferred", f"--width={width}", f"--height={height}"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running Network: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    npz_python = os.path.join(root_dir, 'pythons', 'npz.py')
    input_dir = os.path.join(directory, 'bin')
    cmd = ["python", npz_python, "--input_dir", input_dir, "--width", str(width), "--height", str(height), "--save_npz"]

    try:
        print(f"Running npz: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: npz.py not found at path: {npz_python}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    # ------------------------- denoise -----------------------------
    cmd = [mogwai_path, f"--script={denoise_script}", f"--scene={scene}", f"--verbosity={verbosity}", "--deferred", f"--width={width}", f"--height={height}"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running Denoise: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    npz_python = os.path.join(root_dir, 'pythons', 'npz.py')
    input_dir = os.path.join(directory, 'denoise')
    cmd = ["python", npz_python, "--input_dir", input_dir, "--width", str(width), "--height", str(height), "--save_npz", "--name", 'denoise']

    try:
        print(f"Running npz: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: npz.py not found at path: {npz_python}")
    except Exception as e:
        print(f"Unexpected error: {e}")
    """

    # ------------------------- optix denoise -----------------------------
    cmd = [mogwai_path, f"--script={optix_denoise_script}", f"--scene={scene}", f"--verbosity={verbosity}", "--deferred", f"--width={width}", f"--height={height}"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running OptiX Denoise: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    npz_python = os.path.join(root_dir, 'pythons', 'npz.py')
    input_dir = os.path.join(directory, 'optix')
    cmd = ["python", npz_python, "--input_dir", input_dir, "--width", str(width), "--height", str(height), "--save_npz", "--name", 'optix-temporal']

    try:
        print(f"Running npz: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: npz.py not found at path: {npz_python}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    # ------------------------- nrd denoise -----------------------------
    cmd = [mogwai_path, f"--script={nrd_denoise_script}", f"--scene={scene}", f"--verbosity={verbosity}", "--deferred", f"--width={width}", f"--height={height}"]
    if config.get('headless', False):
        cmd.append("--headless")

    try:
        print(f"Running NRD Denoise: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: Mogwai.exe not found at path: {mogwai_path}")
    except Exception as e:
        print(f"Unexpected error: {e}")

    npz_python = os.path.join(root_dir, 'pythons', 'npz.py')
    input_dir = os.path.join(directory, 'nrd')
    cmd = ["python", npz_python, "--input_dir", input_dir, "--width", str(width), "--height", str(height), "--save_npz", "--name", 'nrd']

    try:
        print(f"Running npz: {' '.join(cmd)}")
        start_time = time.time()
        process = subprocess.Popen(cmd)
        process.wait()
        execution_time = time.time() - start_time
        print(f"Execution completed successfully in {execution_time:.2f} seconds ({execution_time/60:.2f} minutes).")
    except FileNotFoundError:
        print(f"Error: npz.py not found at path: {npz_python}")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Running configuration.")
    parser.add_argument('--config', type=str, default="config/default.yaml", help='Path to the YAML configuration file.')
    parser.add_argument('--scene', type=int, default=0, help='Scene index to run.')
    args = parser.parse_args()
    run(args, args.scene)
