#!/usr/bin/env python
# Script to process event camera data using TileToEXR and v2e

import os
import subprocess
import sys
import platform
import argparse

# Define v2e directory
v2e_dir = r"C:\Users\LJF\Documents\v2e\v2e.py"

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description="Process event camera data using TileToEXR and v2e")
    parser.add_argument("--home_dir", required=True, help="Home directory for input/output")
    parser.add_argument('--tile_count_z', type=int, default=46, help='Number of tiles in z dimension')
    parser.add_argument("--dvs_params", choices=["noisy", "clean", "none"], default="none",
                      help="DVS parameters to use: 'noisy', 'clean', or 'none' (default)")
    return parser.parse_args()

def run_tile_to_exr(home_dir, tile_count_z):
    """Run TileToEXR.py to convert tiles to EXR format"""
    print("Step 1: Running TileToEXR.py...")

    input_dir = os.path.join(home_dir, "Output")
    output_exr_dir = os.path.join(home_dir, "EXR")

    # Create output directory if it doesn't exist
    os.makedirs(output_exr_dir, exist_ok=True)

    # Build and execute the command
    cmd = [
        "python",
        os.path.join(os.path.dirname(__file__), "TileToEXR.py"),
        "--input", input_dir,
        "--output", output_exr_dir,
        "--tile_count_z", str(tile_count_z),
    ]

    print(f"Executing: {' '.join(cmd)}")
    result = subprocess.run(cmd, check=True)

    if result.returncode == 0:
        print("TileToEXR.py completed successfully")
    else:
        print("Error running TileToEXR.py")
        sys.exit(1)

    return output_exr_dir

def run_v2e(input_dir, output_dir, dvs_params):
    """Run v2e.py with the specified parameters"""
    print("\nStep 2: Running v2e.py...")

    # Build and execute the command
    cmd = [
        "python",
        v2e_dir,
        "-i", input_dir,
        "--overwrite",
        f"--output_folder={output_dir}",
        "--pos_thres=.2",
        "--neg_thres=.2",
        "--sigma_thres=0.03",
        "--cutoff_hz=15",
        "--leak_rate_hz=0",
        "--leak_jitter_fraction=0",
        "--noise_rate_cov_decades=0",
        "--shot_noise_rate_hz=0",
        "--refractory_period=0.005",
        "--output_width=1280",
        "--output_height=720",
        "--no_preview",
        "--input_frame_rate", "1000",
        "--disable_slomo",
        "--dvs_text", "events_data",
        "--hdr",
    ]

    # Add dvs_params if not none
    if dvs_params.lower() != "none":
        cmd.append("--dvs_params")
        cmd.append(dvs_params)

    print(f"Executing: {' '.join(cmd)}")
    result = subprocess.run(cmd, check=True)

    if result.returncode == 0:
        print("v2e.py completed successfully")
    else:
        print("Error running v2e.py")
        sys.exit(1)

def run_event_to_tile(data_path, output_path, tile_count_z):
    """Run EventToTile.py to convert events to tile format"""
    print("\nStep 3: Running EventToTile.py...")

    # Create output directory if it doesn't exist
    os.makedirs(output_path, exist_ok=True)

    # Build and execute the command
    cmd = [
        "python",
        os.path.join(os.path.dirname(__file__), "EventToTile.py"),
        "--data_path", data_path,
        "--output_path", output_path,
        "--tile_count_z", str(tile_count_z)
    ]

    print(f"Executing: {' '.join(cmd)}")
    result = subprocess.run(cmd, check=True)

    if result.returncode == 0:
        print("EventToTile.py completed successfully")
    else:
        print("Error running EventToTile.py")
        sys.exit(1)

def main():
    """Main function to execute the entire pipeline"""
    # Parse command line arguments
    args = parse_arguments()

    # Check if we're running in the conda environment
    if "CONDA_DEFAULT_ENV" not in os.environ or os.environ["CONDA_DEFAULT_ENV"] != "v2e":
        print("Not running in v2e conda environment.")
        return

    print("Starting event camera processing pipeline...")
    print(f"Home directory: {args.home_dir}")
    print(f"DVS parameters: {args.dvs_params}")

    exr_output_dir = run_tile_to_exr(args.home_dir, args.tile_count_z)

    exr_output_dir = os.path.join(args.home_dir, "EXR")
    v2e_output_dir = os.path.join(args.home_dir, "v2e")
    run_v2e(exr_output_dir, v2e_output_dir, args.dvs_params)

    # Add EventToTile step
    events_output_dir = os.path.join(args.home_dir, "Events")
    run_event_to_tile(v2e_output_dir, events_output_dir, args.tile_count_z)

    print("\nProcessing pipeline completed successfully!")

if __name__ == "__main__":
    main()
