import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import subprocess
import cv2
from glob import glob
import numpy as np
from tqdm import tqdm

work_dir = r"C:\Users\-LJF007-\Documents"
output_dir = os.path.join(work_dir, "output", "BistroInterior_Wine")
script_path = os.path.join(work_dir, "EventCamera-GT", "scripts", "PathTracer.py")
scene_path = os.path.join(work_dir, "Scenes", "Bistro_v5_2", "BistroInterior_Wine.pyscene")

video_width = 1280
video_height = 720

def run_mogwai():
    os.chdir(work_dir)
    cmd = f".\\EventCamera-GT\\build\\windows-ninja-msvc\\bin\\Release\\Mogwai.exe --gpu=1 --script={script_path} --scene={scene_path} --headless"
    subprocess.run(cmd, shell=True)

run_mogwai()
