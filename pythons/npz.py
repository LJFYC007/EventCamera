import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
import argparse
from tqdm import tqdm
import re

def load_events(file_path, width):
    data = np.fromfile(file_path, dtype=np.uint32)
    events = data.reshape(-1, 2)
    timestamps = events[:, 0].astype(np.uint32)
    addr = events[:, 1].astype(np.uint32)
    polarity = addr & 1
    addr >>= 1
    y = addr // width
    x = addr % width
    return x, y, timestamps, polarity

def process_files(input_dir, pattern, width, height, save_npz, show, name):
    files = sorted(Path(input_dir).glob(pattern), key=lambda f: int(re.findall(r'\d+', f.stem)[0]))

    all_x = []
    all_y = []
    all_t = []
    all_p = []

    for f in tqdm(files, desc="Processing files"):
        x, y, t, p = load_events(f, width)

        all_x.append(x)
        all_y.append(y)
        all_t.append(t)
        all_p.append(p)

        if show:
            canvas = np.ones((height, width, 3), dtype=np.float32)
            for xi, yi, pi in zip(x, y, p):
                if pi == 0:
                    canvas[yi, xi] = [1, 0, 0]
                else:
                    canvas[yi, xi] = [0, 0, 1]

            plt.figure(figsize=(10, 6))
            plt.imshow(canvas)
            plt.title(f.name)
            plt.axis('off')
            plt.show()

    if save_npz:
        x_all = np.concatenate(all_x)
        y_all = np.concatenate(all_y)
        t_all = np.concatenate(all_t)
        p_all = np.concatenate(all_p)

        output_file = Path(input_dir).parent / f'events-{name}.npz'
        np.savez(output_file, t=t_all, x=x_all, y=y_all, p=p_all)
        print(f"Saved merged events to {output_file}")



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input_dir', type=str, default=r'F:\output', help='Directory containing .bin files')
    parser.add_argument('--pattern', type=str, default='data-*.bin', help='Filename pattern to match')
    parser.add_argument('--width', type=int, default=1280, help='Sensor width in pixels')
    parser.add_argument('--height', type=int, default=720, help='Sensor height in pixels')
    parser.add_argument('--save_npz', action='store_true', help='Flag to save extracted events as .npz')
    parser.add_argument('--show', action='store_true', help='Flag to show images')
    parser.add_argument('--name', type=str, default='default', help='Name for the output .npz file')
    args = parser.parse_args()
    process_files(args.input_dir, args.pattern, args.width, args.height, args.save_npz, args.show, args.name)

if __name__ == '__main__':
    main()
