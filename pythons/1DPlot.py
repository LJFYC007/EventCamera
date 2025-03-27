import TileBasedStorage
import torch
import matplotlib.pyplot as plt
import numpy as np
import random
import os
import argparse


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Generate 1D plots from TileBasedStorage data"
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
        "--output_dir", default="../output/testplot", help="Base output directory"
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
        "--fixed_positions",
        nargs="+",
        type=str,
        default=[],
        help="Fixed x,y positions to sample (format: 'x,y')",
    )

    # Processing parameters
    parser.add_argument(
        "--window_size",
        type=int,
        default=30,
        help="Window size for sliding window averaging",
    )
    parser.add_argument(
        "--log_transform", action="store_true", help="Apply log transform to data"
    )

    return parser.parse_args()


def setup_directories(base_output_dir, batch_id=None):
    """Create directory structure for output"""
    if batch_id is None:
        batch_id = f"batch_{random.randint(1000, 9999)}"

    batch_dir = os.path.join(base_output_dir, batch_id)
    raw_dir = os.path.join(batch_dir, "raw")
    smoothed_dir = os.path.join(batch_dir, "smoothed")
    combined_dir = os.path.join(batch_dir, "combined")

    for directory in [raw_dir, smoothed_dir, combined_dir]:
        os.makedirs(directory, exist_ok=True)

    return batch_dir, raw_dir, smoothed_dir, combined_dir


def get_luma_slices(storage, positions=None, num_samples=5, apply_log=True):
    """Extract slices from specified positions or random positions"""
    slices = []
    positions_list = []

    # Generate random positions if none provided
    if not positions:
        for _ in range(num_samples):
            x = random.randint(0, storage.tile_size[0] * storage.tile_count[0] - 1)
            y = random.randint(0, storage.tile_size[1] * storage.tile_count[1] - 1)
            positions_list.append((x, y))
    else:
        positions_list = positions

    # Get the slices
    for x, y in positions_list:
        slice_data = storage.get(x=x, y=y)

        # Convert RGB to luma
        luma = torch.tensor([0.299, 0.587, 0.114])
        luma_slice = torch.matmul(slice_data[:, :3], luma)

        if apply_log:
            luma_slice = torch.log(luma_slice + 0.01)

        slices.append(luma_slice)

    return slices, positions_list


def sliding_window_average(data, window_size=5):
    """Apply sliding window average to 1D data"""
    # Ensure data is in numpy format
    if isinstance(data, torch.Tensor):
        data_np = data.detach().cpu().numpy()
    else:
        data_np = np.array(data)

    # Create a centered kernel (odd window size is better for centering)
    window_size = window_size if window_size % 2 == 1 else window_size + 1
    kernel = np.ones(window_size) / window_size

    # Use np.pad to handle edge effects
    padded_data = np.pad(data_np, (window_size // 2, window_size // 2), mode="edge")
    result = np.convolve(padded_data, kernel, mode="valid")

    # Ensure the result has the same length as the input
    return result[: len(data_np)]


def plot_combined_slices(
    slices, positions, output_path, title, window_size=None, smoothed=False
):
    """Plot multiple slices on a single figure"""
    plt.figure(figsize=(10, 6))

    for i, (luma_slice, (x, y)) in enumerate(zip(slices, positions)):
        data = (
            sliding_window_average(luma_slice, window_size) if smoothed else luma_slice
        )
        plt.plot(data, label=f"Slice at x={x}, y={y}")

    plt.legend()
    plt.title(title)
    plt.xlabel("Z Position")
    ylabel = "Smoothed Luma Value" if smoothed else "Luma Value"
    plt.ylabel(ylabel)
    plt.savefig(output_path)
    plt.close()


def plot_individual_slices(
    slices, positions, output_dir, window_size=None, smoothed=False
):
    """Plot individual slices as separate figures"""
    for i, (luma_slice, (x, y)) in enumerate(zip(slices, positions)):
        plt.figure(figsize=(8, 4))

        data = (
            sliding_window_average(luma_slice, window_size) if smoothed else luma_slice
        )
        plt.plot(data)

        title_prefix = "Smoothed " if smoothed else ""
        window_suffix = f" (Window Size: {window_size})" if smoothed else ""
        plt.title(f"{title_prefix}Slice at x={x}, y={y}{window_suffix}")

        plt.xlabel("Z Position")
        ylabel = "Smoothed Luma Value" if smoothed else "Luma Value"
        plt.ylabel(ylabel)

        filename = f"slice_{i}_x{x}_y{y}"
        if smoothed:
            filename += f"_smoothed_w{window_size}"
        plt.savefig(os.path.join(output_dir, f"{filename}.png"))
        plt.close()


def main():
    args = parse_arguments()

    # Load storage
    storage = TileBasedStorage.TileBasedStorage(
        args.tile_size, args.tile_count, args.input_dir
    )

    # Setup directories
    batch_dir, raw_dir, smoothed_dir, combined_dir = setup_directories(
        args.output_dir, args.batch_id
    )

    # Parse positions if provided
    positions = []
    if args.fixed_positions:
        for pos_str in args.fixed_positions:
            x, y = map(int, pos_str.split(","))
            positions.append((x, y))

    # Get slices
    slices, positions = get_luma_slices(
        storage,
        positions=positions if positions else None,
        num_samples=args.num_samples,
        apply_log=args.log_transform,
    )

    # Plot raw data
    plot_combined_slices(
        slices,
        positions,
        os.path.join(combined_dir, "multiple_slices_raw.png"),
        "Multiple Random Slices",
    )
    plot_individual_slices(slices, positions, raw_dir)

    # Plot smoothed data
    plot_combined_slices(
        slices,
        positions,
        os.path.join(combined_dir, f"multiple_slices_smoothed_w{args.window_size}.png"),
        f"Multiple Random Slices (Window Size: {args.window_size})",
        window_size=args.window_size,
        smoothed=True,
    )
    plot_individual_slices(
        slices, positions, smoothed_dir, window_size=args.window_size, smoothed=True
    )

    print(f"Results saved to {batch_dir}")


if __name__ == "__main__":
    main()
