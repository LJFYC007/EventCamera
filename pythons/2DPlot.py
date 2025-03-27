import TileBasedStorage
import torch
import numpy as np
import random
import os
import argparse
import OpenEXR
import Imath
import array

# filepath: C:/Users/Pengfei/WorkSpace/EventCamera/pythons/2DPlot.py
import matplotlib.pyplot as plt


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Generate 2D plots from TileBasedStorage data as EXR images"
    )

    # Data source arguments
    parser.add_argument(
        "--input_dir",
        default="../output/Temp",
        help="Input directory for TileBasedStorage",
    )
    parser.add_argument(
        "--tile_size",
        nargs=3,
        type=int,
        default=[64, 64, 64],
        help="Size of tiles (x, y, z)",
    )
    parser.add_argument(
        "--tile_count",
        nargs=3,
        type=int,
        default=[19, 11, 18],
        help="Count of tiles (x, y, z)",
    )

    # Output configuration
    parser.add_argument(
        "--output_dir", default="../output/test2dplot", help="Base output directory"
    )
    parser.add_argument(
        "--batch_id",
        default=None,
        help="Custom batch ID, if not provided a random one will be generated",
    )

    # Sampling parameters
    parser.add_argument(
        "--num_samples", type=int, default=5, help="Number of random slices to sample"
    )
    parser.add_argument(
        "--slice_axis",
        default="z",
        choices=["x", "y", "z"],
        help="Axis to slice along (x, y, or z)",
    )
    parser.add_argument(
        "--fixed_positions",
        nargs="+",
        type=int,
        default=[],
        help="Fixed positions along the slice axis",
    )
    parser.add_argument(
        "--layers",
        nargs="+",
        type=int,
        default=None,
        help="Specific layers to plot (overrides num_samples and fixed_positions)",
    )

    # Processing parameters
    parser.add_argument(
        "--log_transform", action="store_true", help="Apply log transform to data"
    )
    parser.add_argument(
        "--rgb_only", action="store_true", help="Save only RGB channels, ignore alpha"
    )

    return parser.parse_args()


def setup_directories(base_output_dir, batch_id=None):
    """Create directory structure for output"""
    if batch_id is None:
        batch_id = f"batch_{random.randint(1000, 9999)}"

    batch_dir = os.path.join(base_output_dir, batch_id)
    rgb_dir = os.path.join(batch_dir, "rgb")
    exr_dir = os.path.join(batch_dir, "exr")

    for directory in [rgb_dir, exr_dir]:
        os.makedirs(directory, exist_ok=True)

    return batch_dir, rgb_dir, exr_dir


def get_slices(
    storage, axis, positions=None, num_samples=5, apply_log=False, layers=None
):
    """Extract 2D slices from specified positions or random positions"""
    slices = []
    positions_list = []

    # Get dimensions based on the axis
    if axis == "x":
        max_pos = storage.tile_size[0] * storage.tile_count[0] - 1
    elif axis == "y":
        max_pos = storage.tile_size[1] * storage.tile_count[1] - 1
    else:  # z
        max_pos = storage.tile_size[2] * storage.tile_count[2] - 1

    # If layers are specified, use those instead of positions or random samples
    if layers is not None:
        positions_list = [layer for layer in layers if 0 <= layer <= max_pos]
        if len(positions_list) < len(layers):
            out_of_bounds = [layer for layer in layers if layer < 0 or layer > max_pos]
            print(
                f"Warning: Layers {out_of_bounds} are out of bounds (0-{max_pos}) and will be skipped."
            )
        if not positions_list:
            print(f"No valid layers specified. Using random samples instead.")
            positions_list = [random.randint(0, max_pos) for _ in range(num_samples)]
    # Generate random positions if none provided
    elif not positions:
        for _ in range(num_samples):
            pos = random.randint(0, max_pos)
            positions_list.append(pos)
    else:
        positions_list = positions

    # Get the slices
    for pos in positions_list:
        if axis == "x":
            slice_data = storage.get(x=pos)
        elif axis == "y":
            slice_data = storage.get(y=pos)
        else:  # z
            slice_data = storage.get(z=pos)

        if apply_log:
            # Apply log transform while avoiding log(0)
            slice_data = torch.log(slice_data + 0.01)

        slices.append(slice_data)

    return slices, positions_list


def save_as_exr(data, filepath, rgb_only=False):
    """Save tensor data as OpenEXR file"""
    # Convert to numpy and ensure proper shape
    if isinstance(data, torch.Tensor):
        data_np = data.detach().cpu().numpy()
    else:
        data_np = np.array(data)

    height, width, channels = data_np.shape

    # Prepare header
    header = OpenEXR.Header(width, height)
    pixel_type = Imath.PixelType(Imath.PixelType.FLOAT)

    # Set channel names based on whether we're saving RGB only or RGBA
    if rgb_only or channels == 3:
        channel_names = ["R", "G", "B"]
    else:
        channel_names = ["R", "G", "B", "A"]

    header["channels"] = {c: Imath.Channel(pixel_type) for c in channel_names}

    # Create output file
    out_file = OpenEXR.OutputFile(filepath, header)

    # Prepare data for each channel
    channel_data = {}
    for i, c in enumerate(channel_names):
        if i < channels:
            # Convert the data to float32 and then to a binary string
            channel_data[c] = array.array(
                "f", data_np[:, :, i].astype(np.float32).flatten()
            ).tobytes()

    # Write the data
    out_file.writePixels(channel_data)
    out_file.close()


def plot_slice(slice_data, output_path, title=None, apply_gamma=True):
    """Plot a 2D slice as an image"""
    plt.figure(figsize=(10, 10))

    # Handle RGB vs RGBA data
    channels = slice_data.shape[-1]

    if channels >= 3:
        # Get RGB data
        rgb_data = slice_data[:, :, :3]

        # Apply simple gamma correction for display
        if apply_gamma:
            rgb_data = rgb_data ** (1 / 2.2)

        # Clip to valid range
        rgb_data = np.clip(rgb_data, 0, 1)

        plt.imshow(rgb_data)
    else:
        # For grayscale
        plt.imshow(slice_data, cmap="gray")

    if title:
        plt.title(title)

    plt.axis("off")
    plt.tight_layout()
    plt.savefig(output_path)
    plt.close()


def main():
    args = parse_arguments()

    # Load storage
    storage = TileBasedStorage.TileBasedStorage(
        args.tile_size, args.tile_count, args.input_dir
    )

    # Setup directories
    batch_dir, rgb_dir, exr_dir = setup_directories(args.output_dir, args.batch_id)

    # Parse positions if provided
    positions = args.fixed_positions if args.fixed_positions else None

    # Get slices
    slices, positions = get_slices(
        storage,
        args.slice_axis,
        positions=positions,
        num_samples=args.num_samples,
        apply_log=args.log_transform,
        layers=args.layers,
    )

    # Process and save each slice
    for i, (slice_data, pos) in enumerate(zip(slices, positions)):
        # Create filenames
        base_filename = f"{args.slice_axis}{pos}"
        exr_path = os.path.join(exr_dir, f"{base_filename}.exr")
        png_path = os.path.join(rgb_dir, f"{base_filename}.png")

        # Save as EXR
        save_as_exr(slice_data, exr_path, rgb_only=args.rgb_only)

        # Save as PNG for preview
        transform_text = "_log" if args.log_transform else ""
        title = f"Slice at {args.slice_axis}={pos}{transform_text}"
        plot_slice(slice_data, png_path, title=title)

    print(f"Results saved to {batch_dir}")
    print(f"Generated {len(slices)} slices along the {args.slice_axis}-axis")


if __name__ == "__main__":
    main()
