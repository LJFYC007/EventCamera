import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import numpy as np
import cv2
import argparse
from TileBasedStorage import TileBasedStorage, lin_log
from tqdm import *

def convert_tiles_to_exr(input_dir, output_dir, tile_size, tile_count, data_format):
    os.makedirs(output_dir, exist_ok=True)

    print(f"Initializing TileBasedStorage: {input_dir}")
    storage = TileBasedStorage(tile_size, tile_count, input_dir, data_format=data_format)

    num_timestamps = tile_size[2] * tile_count[2]
    print(f"Starting conversion of {num_timestamps} timestamps to EXR images...")

    # Create an EXR image for each timestamp
    for z in tqdm(range(num_timestamps), desc="Processing", unit="timestamp"):
        try:
            xy_plane = storage.get(z=z)
            if data_format == "rgba":
                xy_plane = lin_log(xy_plane)
            output_file = os.path.join(output_dir, f"frame_{z:06d}.exr")
            cv2.imwrite(output_file, xy_plane.numpy())
        except Exception as e:
            print(f"Error processing timestamp {z}: {str(e)}")

    print(f"Conversion complete! All EXR images saved to {output_dir}")

def main():
    parser = argparse.ArgumentParser(description='Convert tile-based storage data to EXR image sequence')
    parser.add_argument('--input', type=str, default="C:\\Users\\LJF\\Documents\\output\\4096SPP\\Output",
                        help='Input directory containing tile files')
    parser.add_argument('--output', type=str, default="C:\\Users\\LJF\\Documents\\output\\4096SPP\\EXR",
                        help='Output directory for EXR images')
    parser.add_argument('--tile_size_x', type=int, default=64, help='Tile size in x dimension')
    parser.add_argument('--tile_size_y', type=int, default=64, help='Tile size in y dimension')
    parser.add_argument('--tile_size_z', type=int, default=64, help='Tile size in z dimension')
    parser.add_argument('--tile_count_x', type=int, default=20, help='Number of tiles in x dimension')
    parser.add_argument('--tile_count_y', type=int, default=12, help='Number of tiles in y dimension')
    parser.add_argument('--tile_count_z', type=int, default=46, help='Number of tiles in z dimension')
    parser.add_argument('--data_format', type=str, default='rgba', help='Data format (rgba or int8)')

    args = parser.parse_args()

    input_dir = args.input
    output_dir = args.output

    tile_size = [args.tile_size_x, args.tile_size_y, args.tile_size_z]
    tile_count = [args.tile_count_x, args.tile_count_y, args.tile_count_z]

    convert_tiles_to_exr(input_dir, output_dir, tile_size, tile_count, args.data_format)

if __name__ == "__main__":
    main()
