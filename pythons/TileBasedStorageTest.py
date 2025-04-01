import torch
import os
os.environ["OPENCV_IO_ENABLE_OPENEXR"] = "1"  # Enable OpenEXR support in OpenCV
from TileBasedStorage import TileBasedStorage

if __name__ == "__main__":
    # Use existing directory
    existing_dir = "C:\\Users\\LJF\\Documents\\output\\4096SPP\\events"
    tile_size = [64, 64, 64]
    tile_count = [19, 11, 1]

    # Create storage
    print("Initializing TileBasedStorage...")
    storage = TileBasedStorage(tile_size, tile_count, existing_dir, data_format="int8")

    # Test different access methods
    print("\nTesting different access methods:")

    # Single voxel access
    x, y, z = 5, 6, 7
    voxel = storage.get(x, y, z)
    print(f"Voxel at ({x}, {y}, {z}): {voxel.numpy()}")
    # Assert voxel dimensions and values
    assert voxel.shape == torch.Size(
        [1]
    ), f"Expected voxel shape [4], got {voxel.shape}"
#    assert np.isclose(voxel[0].item(), 0.11696338), "Voxel value mismatch"
#    assert np.isclose(voxel[3].item(), 1.0), "Alpha channel mismatch"

    # Get a plane (z=3)  RGBA float
    z_plane = storage.get(z=500)
    print(f"XY plane at z=3 shape: {z_plane.shape}")
    # Assert plane dimensions
    expected_shape = torch.Size([704, 1216, 1])  # 11*64=704, 19*64=1216
    assert (
        z_plane.shape == expected_shape
    ), f"Expected plane shape {expected_shape}, got {z_plane.shape}"

    # write to an image
    import cv2

    # Keep the original floating-point values for EXR
    z_plane = z_plane.numpy()
    # OpenCV expects the EXR data in BGRA format for writing
    z_plane_bgra = cv2.cvtColor(z_plane, cv2.COLOR_RGBA2BGRA)
    # Write as EXR file
    cv2.imwrite("z_plane.exr", z_plane_bgra)

    # Get a column
    x, y = 10, 10
    column = storage.get(x=x, y=y)
    print(f"Z column at x={x}, y={y} shape: {column.shape}")
    # Assert column dimensions
    expected_column_shape = torch.Size([1152, 4])  # 18*64=1152
    assert (
        column.shape == expected_column_shape
    ), f"Expected column shape {expected_column_shape}, got {column.shape}"

    # Get a row
    x, z = 10, 5
    row = storage.get(x=x, z=z)
    print(f"Y row at x={x}, z={z} shape: {row.shape}")
    # Assert row dimensions
    expected_row_shape = torch.Size([704, 4])  # 11*64=704
    assert (
        row.shape == expected_row_shape
    ), f"Expected row shape {expected_row_shape}, got {row.shape}"

    print("\nTest completed!")
