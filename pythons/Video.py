import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import subprocess
import cv2
from glob import glob
import numpy as np

work_dir = r"C:\Users\-LJF007-\Documents"
output_dir = os.path.join(work_dir, "output", "BistroInterior_Wine")
script_path = os.path.join(work_dir, "EventCamera", "scripts", "EventCamera.py")
scene_path = os.path.join(work_dir, "Scenes", "Bistro_v5_2", "BistroInterior_Wine.pyscene")

video_width = 1280
video_height = 720

def run_mogwai():
    os.chdir(work_dir)
    cmd = f".\\EventCamera\\build\\windows-ninja-msvc\\bin\\Release\\Mogwai.exe --headless --script={script_path} --scene={scene_path}"
    subprocess.run(cmd, shell=True)

def create_video_from_exr():
    exr_files = sorted(glob(os.path.join(output_dir, ".ErrorMeasurePass.Output.*.exr")),
                       key=lambda x: int(x.split('.')[-2]))
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    video_output = os.path.join(work_dir, "output", "BistroInterior_Wine.mp4")
    video_writer = cv2.VideoWriter(video_output, fourcc, 500, (video_width, video_height))

    for exr_file in exr_files:
        frame = cv2.imread(exr_file, cv2.IMREAD_UNCHANGED)

        if frame is None:
            print(f"Failed to read {exr_file}")
            continue

        if frame.dtype == np.float32:
            frame = cv2.normalize(frame, None, 0, 255, cv2.NORM_MINMAX)
            frame = np.uint8(frame)

        frame_resized = cv2.resize(frame, (video_width, video_height))
        video_writer.write(frame_resized)

    video_writer.release()

# run_mogwai()
create_video_from_exr()
