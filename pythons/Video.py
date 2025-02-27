import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import subprocess
import cv2
from glob import glob
import numpy as np
from tqdm import tqdm

work_dir = r"C:\Users\-LJF007-\Documents"
output_dir = os.path.join(work_dir, "output", "BistroInterior_Wine")
script_path = os.path.join(work_dir, "EventCamera", "scripts", "EventCamera.py")
scene_path = os.path.join(work_dir, "Scenes", "Bistro_v5_2", "BistroInterior_Wine.pyscene")

video_width = 1280
video_height = 720
fps = 60
time_window_ms = 1000.0 / fps

def run_mogwai():
    os.chdir(work_dir)
    cmd = f".\\EventCamera\\build\\windows-ninja-msvc\\bin\\Release\\Mogwai.exe --headless --script={script_path} --scene={scene_path}"
    subprocess.run(cmd, shell=True)

def create_video_from_exr():
    exr_files = sorted(glob(os.path.join(output_dir, ".ErrorMeasurePass.Output.*.exr")),
                       key=lambda x: int(x.split('.')[-2]))

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    video_output = os.path.join(work_dir, "output", "BistroInterior_Wine.mp4")
    video_writer = cv2.VideoWriter(video_output, fourcc, fps, (video_width, video_height))

    events = []
    for exr_file in tqdm(exr_files, desc="Processing EXR files"):
        frame = cv2.imread(exr_file, cv2.IMREAD_UNCHANGED)
        if frame is None:
            print(f"Failed to read {exr_file}")
            continue
        if frame.dtype == np.float32:
            frame = cv2.normalize(frame, None, 0, 255, cv2.NORM_MINMAX)
            frame = np.uint8(frame)
        events.append(frame)

        if len(events) * 1 >= time_window_ms:
            latest_event = np.zeros((frame.shape[0], frame.shape[1]), dtype=np.uint8)

            for e in events:
                bright_event = (e[:, :, 2] >= 200)
                dark_event = (e[:, :, 1] >= 200)
                latest_event[bright_event] = 1
                latest_event[dark_event] = 2

            visualization_frame = np.ones((frame.shape[0], frame.shape[1], 3), dtype=np.uint8) * 255 # White
            visualization_frame[latest_event == 1] = [0.78 * 255, 0.49 * 255, 0.25 * 255] # Blue
            visualization_frame[latest_event == 2] = [0.20 * 255, 0.14 * 255, 0.11 * 255] # Black

            video_writer.write(visualization_frame)
            events.clear()

    video_writer.release()
    print(f"Video saved at: {video_output}")

# run_mogwai()
create_video_from_exr()
