import TileBasedStorage
import torch
import matplotlib.pyplot as plt
import numpy as np
import random
import os

tile_size = [64, 64, 64]
tile_count = [19, 11, 18]
rendered_8_spp = TileBasedStorage.TileBasedStorage(
    tile_size, tile_count, "../output/Temp"
)

# Create a batch ID for this run
batch_id = f"batch_{random.randint(1000, 9999)}"

# Create the base output directory
base_output_dir = "../output/testplot"
os.makedirs(base_output_dir, exist_ok=True)

# Create subdirectories for this batch and different types
batch_dir = os.path.join(base_output_dir, batch_id)
raw_dir = os.path.join(batch_dir, "raw")
smoothed_dir = os.path.join(batch_dir, "smoothed")
combined_dir = os.path.join(batch_dir, "combined")

# Create all required directories
for directory in [raw_dir, smoothed_dir, combined_dir]:
    os.makedirs(directory, exist_ok=True)

# Get multiple slices from random positions
num_samples = 5
slices = []
positions = []

for _ in range(num_samples):
    # Generate random positions within valid range
    x = random.randint(0, tile_size[0] * tile_count[0] - 1)
    y = random.randint(0, tile_size[1] * tile_count[1] - 1)
    positions.append((x, y))

    # Get the slice
    slice_data = rendered_8_spp.get(x=x, y=y)

    # Convert RGB to luma
    luma = torch.tensor([0.299, 0.587, 0.114])
    luma_slice = torch.matmul(slice_data[:, :3], luma)
    slices.append(torch.log(luma_slice+0.01))

# Function to apply sliding window average
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
    padded_data = np.pad(data_np, (window_size//2, window_size//2), mode='edge')
    result = np.convolve(padded_data, kernel, mode='valid')
    
    # Ensure the result has the same length as the input
    return result[:len(data_np)]

# Create a 1D plot for all raw slices
plt.figure(figsize=(10, 6))
for i, (luma_slice, (x, y)) in enumerate(zip(slices, positions)):
    plt.plot(luma_slice, label=f"Slice at x={x}, y={y}")

plt.legend()
plt.title("Multiple Random Slices")
plt.xlabel("Z Position")
plt.ylabel("Luma Value")
plt.savefig(os.path.join(combined_dir, "multiple_slices_raw.png"))
plt.close()

# Save individual raw slice plots
for i, (luma_slice, (x, y)) in enumerate(zip(slices, positions)):
    plt.figure(figsize=(8, 4))
    plt.plot(luma_slice)
    plt.title(f"Slice at x={x}, y={y}")
    plt.xlabel("Z Position")
    plt.ylabel("Luma Value")
    plt.savefig(os.path.join(raw_dir, f"slice_{i}_x{x}_y{y}.png"))
    plt.close()

# Create plots with sliding window averaging
window_size = 30  # Adjust as needed

# Create a 1D plot for all smoothed slices
plt.figure(figsize=(10, 6))
for i, (luma_slice, (x, y)) in enumerate(zip(slices, positions)):
    smoothed_data = sliding_window_average(luma_slice, window_size)
    plt.plot(smoothed_data, label=f"Smoothed slice at x={x}, y={y}")

plt.legend()
plt.title(f"Multiple Random Slices (Window Size: {window_size})")
plt.xlabel("Z Position")
plt.ylabel("Smoothed Luma Value")
plt.savefig(os.path.join(combined_dir, f"multiple_slices_smoothed_w{window_size}.png"))
plt.close()

# Save individual smoothed slice plots
for i, (luma_slice, (x, y)) in enumerate(zip(slices, positions)):
    plt.figure(figsize=(8, 4))
    smoothed_data = sliding_window_average(luma_slice, window_size)
    plt.plot(smoothed_data)
    plt.title(f"Smoothed Slice at x={x}, y={y} (Window Size: {window_size})")
    plt.xlabel("Z Position")
    plt.ylabel("Smoothed Luma Value")
    plt.savefig(os.path.join(smoothed_dir, f"slice_{i}_x{x}_y{y}_smoothed_w{window_size}.png"))
    plt.close()

print(f"Results saved to {batch_dir}")
