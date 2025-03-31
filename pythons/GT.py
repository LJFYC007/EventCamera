import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"]="1"
import subprocess
import cv2
from glob import glob
import numpy as np
from tqdm import tqdm

work_dir = r"C:\\Users\\LJF\\Documents"
output_dir = os.path.join(work_dir, "output", "gt2048")

video_width = 1920
video_height = 1080
fps = 60
time_window_ms = 1000.0 / fps

def create_video_from_exr():
    # Change file extension from .exr to .png
    png_files = sorted(glob(os.path.join(output_dir, ".ToneMapper.dst.*.png")),
                       key=lambda x: int(x.split('.')[-2]))

    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    video_output = os.path.join(work_dir, "output", "BistroInterior_Wine_gt.mp4")
    video_writer = cv2.VideoWriter(video_output, fourcc, 60, (video_width, video_height))

    events = []
    baseline = np.zeros((video_height, video_width), dtype=np.float32)

    for png_file in tqdm(png_files, desc="Processing PNG files"):
        frame = cv2.imread(png_file, cv2.IMREAD_UNCHANGED)
        if frame is None:
            print(f"Failed to read {png_file}")
            continue
        if frame.dtype == np.float32:
            frame = cv2.normalize(frame, None, 0, 255, cv2.NORM_MINMAX)
            frame = np.uint8(frame)

        # Apply RGB coefficients for illumination calculation
        # Using standard coefficients: 0.299 for Red, 0.587 for Green, 0.114 for Blue
        illumination = frame[:,:,2] * 0.299 + frame[:,:,1] * 0.587 + frame[:,:,0] * 0.114  # Ensure correct RGB order
        bright_event = illumination > baseline * 10.0
        dark_event = illumination < baseline / 10.0

        baseline[bright_event] = illumination[bright_event]
        baseline[dark_event] = illumination[dark_event]

        event_frame = np.zeros_like(frame)
        event_frame[bright_event] = [0, 0, 255]  # Red for bright events
        event_frame[dark_event] = [255, 0, 0]  # Blue for dark events

        events.append(event_frame)

        if len(events) >= 2:
            latest_event = np.zeros((frame.shape[0], frame.shape[1]), dtype=np.uint8)

            for e in events:
                bright_event = (e[:, :, 2] >= 200)
                dark_event = (e[:, :, 0] >= 200)
                latest_event[bright_event] = 1
                latest_event[dark_event] = 2

            visualization_frame = np.ones((frame.shape[0], frame.shape[1], 3), dtype=np.uint8) * 255 # White
            visualization_frame[latest_event == 1] = [0.78 * 255, 0.49 * 255, 0.25 * 255] # Blue
            visualization_frame[latest_event == 2] = [0.25 * 255, 0.14 * 255, 0.78 * 255] # Red

            video_writer.write(visualization_frame)
            events.clear()

    video_writer.release()
    print(f"Video saved at: {video_output}")

create_video_from_exr()
