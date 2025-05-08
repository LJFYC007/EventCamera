import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"
import numpy as np
import cv2
import argparse
from TileBasedStorage import TileBasedStorage, lin_log
from tqdm import tqdm

def convert_tiles(input_dir, output_dir, tile_size, tile_count, data_format,
                  apply_lin_log=True, output_format='exr', frame_mod=1):
    os.makedirs(output_dir, exist_ok=True)

    print(f"Initializing TileBasedStorage: {input_dir}")
    storage = TileBasedStorage(tile_size, tile_count, input_dir, data_format=data_format)

    num_timestamps = tile_size[2] * tile_count[2]
    print(f"Starting conversion of {num_timestamps} timestamps...")

    for z in tqdm(range(num_timestamps), desc="Processing", unit="timestamp"):
        if z % frame_mod != 0:
            continue  # Skip frames not satisfying the frame_mod condition

        try:
            xy_plane = storage.get(z=z)

            if apply_lin_log:
                xy_plane = lin_log(xy_plane)
            else:
                xy_plane = xy_plane[..., [2, 1, 0]]  # Swap channels

            # Format output filename
            output_ext = output_format.lower()
            output_file = os.path.join(output_dir, f"frame_{int(z/frame_mod):010d}.{output_ext}")

            if output_ext == 'exr':
                cv2.imwrite(output_file, xy_plane.numpy())
            elif output_ext == 'png':
                img = np.clip(xy_plane.numpy(), 0.0, 1.0)
                img = (img * 255.0).astype(np.uint8)
                cv2.imwrite(output_file, img)
            else:
                raise ValueError(f"Unsupported output format: {output_format}")

        except Exception as e:
            print(f"Error processing timestamp {z}: {str(e)}")

    print(f"Conversion complete! All images saved to {output_dir}")

def main():
    parser = argparse.ArgumentParser(description='Convert tile-based storage data to image sequence')
    parser.add_argument('--input', type=str, default="C:\\Users\\LJF\\Documents\\output\\2048SPP_2000FPS\\Output",
                        help='Input directory containing tile files')
    parser.add_argument('--output', type=str, default="C:\\Users\\LJF\\Documents\\output\\2048SPP_2000FPS\\EXR_30",
                        help='Output directory for output images')
    parser.add_argument('--tile_size_x', type=int, default=64, help='Tile size in x dimension')
    parser.add_argument('--tile_size_y', type=int, default=64, help='Tile size in y dimension')
    parser.add_argument('--tile_size_z', type=int, default=64, help='Tile size in z dimension')
    parser.add_argument('--tile_count_x', type=int, default=20, help='Number of tiles in x dimension')
    parser.add_argument('--tile_count_y', type=int, default=12, help='Number of tiles in y dimension')
    parser.add_argument('--tile_count_z', type=int, default=46, help='Number of tiles in z dimension')
    parser.add_argument('--data_format', type=str, default='rgba', help='Data format (rgba or int8)')
    parser.add_argument('--no_lin_log', action='store_true', default=False,
                        help='Apply lin_log processing (default: True)')
    parser.add_argument('--output_format', type=str, default='exr', choices=['exr', 'png'],
                        help='Output format: exr or png (default: exr)')
    parser.add_argument('--frame_mod', type=int, default=1,
                        help='Only output frames where frame_idx %% frame_mod == 0 (default: 1)')

    args = parser.parse_args()

    tile_size = [args.tile_size_x, args.tile_size_y, args.tile_size_z]
    tile_count = [args.tile_count_x, args.tile_count_y, args.tile_count_z]

    convert_tiles(args.input, args.output, tile_size, tile_count,
                  args.data_format, not args.no_lin_log, args.output_format, args.frame_mod)

if __name__ == "__main__":
    main()
