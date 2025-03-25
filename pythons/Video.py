import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import subprocess
import cv2
from glob import glob
import numpy as np
from tqdm import tqdm
import argparse
import yaml
import struct
from Run import run

work_dir = r"C:\\Users\\pengfei\\workspace"
video_width = 1280
video_height = 720

def create_video_from_bin():
    with open(args.config, 'r') as file:
        config = yaml.safe_load(file)
    script_config = config.get('script', {})
    output_dir = os.path.join(work_dir, "output", script_config.get('directory', 'Temp'))
    video_FPS = config.get('videoFPS', 60)
    time_window_ms = script_config.get('timeScale', 10000) / 60 # it should be the video fps
    # need_accumulated_events = script_config.get('needAccumulatedEvents', 100)

    bin_files = sorted(glob(os.path.join(output_dir, "data-*.bin")),
                       key=lambda x: int(x.split('-')[-1].split('.')[0]))

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    video_output = os.path.join(work_dir, "output", config.get('outputFile') + ".mp4")
    video_writer = cv2.VideoWriter(video_output, fourcc, video_FPS, (video_width, video_height))

    combined_data = []

    events = []
    event_frame = np.zeros((video_height, video_width), dtype=np.uint8)
    for frame_idx, bin_file in enumerate(tqdm(bin_files, desc="Processing bin files")):
        data = np.fromfile(bin_file, dtype=np.uint32)
        for value in data:
            pol = value % 2
            value //= 2
            x = value % video_width
            y = value // video_width
            if 0 <= x < video_width and 0 <= y < video_height:
                event_frame[y, x] = pol + 1
                combined_data.append([x, y, pol, frame_idx])

        events.append(event_frame)

        if len(events) >= time_window_ms:
            visualization_frame = np.ones((video_height, video_width, 3), dtype=np.uint8) * 255  # White background
            bright_event = (event_frame == 1)
            visualization_frame[bright_event] = [0.78 * 255, 0.49 * 255, 0.25 * 255]  # Blue
            dark_event = (event_frame == 2)
            visualization_frame[dark_event] = [0.25 * 255, 0.14 * 255, 0.78 * 255] # Red
            video_writer.write(visualization_frame)

            event_frame = np.zeros((video_height, video_width), dtype=np.uint8)
            events.clear()

    video_writer.release()
    print(f"Video saved at: {video_output}")

    # Save combined data to a new file
    combined_data = np.array(combined_data, dtype=np.uint32)
    combined_output_file = os.path.join(work_dir, "output", config.get('outputFile') + ".npy")
    combined_data.tofile(combined_output_file)
    print(f"Combined data saved at: {combined_output_file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Running configuration.")
    parser.add_argument('--config', type=str, default="config/default.yaml", help='Path to the YAML configuration file.')
    args = parser.parse_args()

    # Run the main process using Run.py
    run(args)

    # Create video from bin files
    create_video_from_bin()
