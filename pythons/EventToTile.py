import os
from tqdm import *
import numpy as np
import OpenEXR
import Imath
import array
import sys
import math
import argparse

def save_events_as_tiles(events_data, output_path, tile_size):
    time, height, width = events_data.shape

    # 计算瓦片数量
    tile_count_x = math.ceil(width / tile_size[0])
    tile_count_y = math.ceil(height / tile_size[1])
    tile_count_z = math.ceil(time / tile_size[2])

    print(f"Tiles Number: {tile_count_x} x {tile_count_y} x {tile_count_z}")

    # 遍历所有瓦片
    for bz in tqdm(range(tile_count_z), desc="Processing tiles", unit="tile"):
        for bx in range(tile_count_x):
            for by in range(tile_count_y):
                x_start = bx * tile_size[0]
                y_start = by * tile_size[1]
                z_start = bz * tile_size[2]

                x_end = min((bx + 1) * tile_size[0], width)
                y_end = min((by + 1) * tile_size[1], height)
                z_end = min((bz + 1) * tile_size[2], time)

                tile_data = events_data[z_start:z_end, y_start:y_end, x_start:x_end]

                rgba_data = np.zeros((64, 64, 64), dtype=np.int8)
                rgba_data[:z_end - z_start, :y_end - y_start, :x_end - x_start] = tile_data

                tile_filename = os.path.join(output_path, f"block_{bx}_{by}_{bz}.bin")
                rgba_data.tofile(tile_filename)

    return (tile_size, (tile_count_x, tile_count_y, tile_count_z))

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description="Convert event data to tile format")
    parser.add_argument("--data_path", required=True, help="Path to the event data directory")
    parser.add_argument("--output_path", required=True, help="Output directory for tiles")
    parser.add_argument("--tile_count_z", type=int, default=46, help="Number of tiles in z dimension")
    return parser.parse_args()

if __name__ == "__main__":
    args = parse_arguments()
    data_path = args.data_path
    output_path = args.output_path
    tile_count_z = args.tile_count_z

    if not os.path.exists(output_path):
        os.makedirs(output_path)

    events_data_path = os.path.join(data_path, 'events_data.txt')
    if not os.path.exists(events_data_path):
        raise ValueError(f"File {events_data_path} does not exist")

    print("Reading event data...")
    # 修改硬编码的长度为 64 * tile_count_z
    time_length = 64 * tile_count_z
    events_data = np.zeros((time_length, 720, 1280), dtype=int)

    with open(events_data_path, 'r') as f:
        # Skip the first 6 lines
        for _ in range(6):
            next(f, None)

        for line in tqdm(f, desc="Processing events", unit="event"):
            timestamp, x, y, polarity = line.strip().split()
            timestamp = int(float(timestamp) * 1000)
            x, y, polarity = int(x), int(y), int(polarity)
            if timestamp < time_length:  # 确保timestamp在有效范围内
                events_data[timestamp, y, x] = 1 if polarity == 1 else -1

    tile_size = (64, 64, 64)
    tile_info = save_events_as_tiles(events_data, output_path, tile_size)

