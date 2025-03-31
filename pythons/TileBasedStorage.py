import torch
import os
import numpy as np
import struct


class TileBasedStorage:
    def __init__(self, tile_size, tile_count, directory_path):
        self.tile_size = tile_size  # [x, y, z]
        self.tile_count = tile_count  # [x, y, z]
        self.directory_path = directory_path
        self.frame_dim = [
            tile_size[0] * tile_count[0],
            tile_size[1] * tile_count[1],
            tile_size[2] * tile_count[2],
        ]
        self.cached_tiles = {}  # Cache for loaded tiles

    def _get_block_filename(self, bx, by, bz):
        return os.path.join(self.directory_path, f"block_{bx}_{by}_{bz}.bin")

    def _load_block(self, bx, by, bz):
        filename = self._get_block_filename(bx, by, bz)
        if (bx, by, bz) in self.cached_tiles:
            return self.cached_tiles[(bx, by, bz)]

        if os.path.exists(filename):
            with open(filename, "rb") as f:
                data = np.fromfile(f, dtype=np.float32)
                # Reshape to RGBA format
                data = data.reshape(
                    self.tile_size[2], self.tile_size[1], self.tile_size[0], 4
                )
                tensor = torch.from_numpy(data)
                self.cached_tiles[(bx, by, bz)] = tensor
                return tensor
        else:
            # Return zeros if block doesn't exist
            return torch.zeros(
                (self.tile_size[2], self.tile_size[1], self.tile_size[0], 4)
            )

    def get_voxel(self, x, y, z):
        """Get a single voxel at coordinates (x, y, z)"""
        if not (
            0 <= x < self.frame_dim[0]
            and 0 <= y < self.frame_dim[1]
            and 0 <= z < self.frame_dim[2]
        ):
            raise IndexError(f"Coordinates ({x}, {y}, {z}) out of bounds")

        bx, by, bz = (
            x // self.tile_size[0],
            y // self.tile_size[1],
            z // self.tile_size[2],
        )
        lx, ly, lz = x % self.tile_size[0], y % self.tile_size[1], z % self.tile_size[2]

        block = self._load_block(bx, by, bz)
        return block[lz, ly, lx]

    def get(self, x=None, y=None, z=None):
        """Dynamic accessor based on specified dimensions"""
        if x is not None and y is not None and z is not None:
            # Single voxel
            return self.get_voxel(x, y, z)

        elif x is not None and y is not None:
            # Get a column (z-line)
            result = []
            for bz in range(self.tile_count[2]):
                bx, by = x // self.tile_size[0], y // self.tile_size[1]
                lx, ly = x % self.tile_size[0], y % self.tile_size[1]
                block = self._load_block(bx, by, bz)
                column = block[:, ly, lx]
                result.append(column)
            return torch.cat(result, dim=0)

        elif x is not None and z is not None:
            # Get a row (y-line)
            result = []
            for by in range(self.tile_count[1]):
                bx, bz = x // self.tile_size[0], z // self.tile_size[2]
                lx, lz = x % self.tile_size[0], z % self.tile_size[2]
                block = self._load_block(bx, by, bz)
                row = block[lz, :, lx]
                result.append(row)
            return torch.cat(result, dim=0)

        elif y is not None and z is not None:
            # Get x-line
            result = []
            for bx in range(self.tile_count[0]):
                by, bz = y // self.tile_size[1], z // self.tile_size[2]
                ly, lz = y % self.tile_size[1], z % self.tile_size[2]
                block = self._load_block(bx, by, bz)
                line = block[lz, ly, :]
                result.append(line)
            return torch.cat(result, dim=0)

        elif x is not None:
            # Get YZ plane
            result = torch.zeros((self.frame_dim[2], self.frame_dim[1], 4))
            bx = x // self.tile_size[0]
            lx = x % self.tile_size[0]

            for by in range(self.tile_count[1]):
                for bz in range(self.tile_count[2]):
                    block = self._load_block(bx, by, bz)
                    plane_part = block[:, :, lx]

                    y_start = by * self.tile_size[1]
                    z_start = bz * self.tile_size[2]
                    y_end = min((by + 1) * self.tile_size[1], self.frame_dim[1])
                    z_end = min((bz + 1) * self.tile_size[2], self.frame_dim[2])

                    result[z_start:z_end, y_start:y_end] = plane_part[
                        : z_end - z_start, : y_end - y_start
                    ]

            return result

        elif y is not None:
            # Get XZ plane
            result = torch.zeros((self.frame_dim[2], self.frame_dim[0], 4))
            by = y // self.tile_size[1]
            ly = y % self.tile_size[1]

            for bx in range(self.tile_count[0]):
                for bz in range(self.tile_count[2]):
                    block = self._load_block(bx, by, bz)
                    plane_part = block[:, ly, :]

                    x_start = bx * self.tile_size[0]
                    z_start = bz * self.tile_size[2]
                    x_end = min((bx + 1) * self.tile_size[0], self.frame_dim[0])
                    z_end = min((bz + 1) * self.tile_size[2], self.frame_dim[2])

                    result[z_start:z_end, x_start:x_end] = plane_part[
                        : z_end - z_start, : x_end - x_start
                    ]

            return result

        elif z is not None:
            # Get XY plane (frame)
            result = torch.zeros((self.frame_dim[1], self.frame_dim[0], 4))
            bz = z // self.tile_size[2]
            lz = z % self.tile_size[2]

            for bx in range(self.tile_count[0]):
                for by in range(self.tile_count[1]):
                    block = self._load_block(bx, by, bz)
                    plane_part = block[lz, :, :]

                    x_start = bx * self.tile_size[0]
                    y_start = by * self.tile_size[1]
                    x_end = min((bx + 1) * self.tile_size[0], self.frame_dim[0])
                    y_end = min((by + 1) * self.tile_size[1], self.frame_dim[1])

                    result[y_start:y_end, x_start:x_end] = plane_part[
                        : y_end - y_start, : x_end - x_start
                    ]

            return result

        else:
            # If all are None, we would need to load the entire volume
            raise ValueError("At least one dimension must be specified")


if __name__ == "__main__":
    # Use existing directory
    existing_dir = "C:\\Users\\LJF\\Documents\\output\\4096SPP\\Output"
    tile_size = [64, 64, 64]
    tile_count = [19, 11, 18]

    # Create storage
    print("Initializing TileBasedStorage...")
    storage = TileBasedStorage(tile_size, tile_count, existing_dir)

    # Test different access methods
    print("\nTesting different access methods:")

    # Single voxel access
    x, y, z = 5, 6, 7
    voxel = storage.get(x, y, z)
    print(f"Voxel at ({x}, {y}, {z}): {voxel.numpy()}")
    # Assert voxel dimensions and values
    assert voxel.shape == torch.Size(
        [4]
    ), f"Expected voxel shape [4], got {voxel.shape}"
#    assert np.isclose(voxel[0].item(), 0.11696338), "Voxel value mismatch"
#    assert np.isclose(voxel[3].item(), 1.0), "Alpha channel mismatch"

    # Get a plane (z=3)  RGBA float
    z_plane = storage.get(z=3)
    print(f"XY plane at z=3 shape: {z_plane.shape}")
    # Assert plane dimensions
    expected_shape = torch.Size([704, 1216, 4])  # 11*64=704, 19*64=1216
    assert (
        z_plane.shape == expected_shape
    ), f"Expected plane shape {expected_shape}, got {z_plane.shape}"

    # write to an image
    import cv2

    z_plane = z_plane.numpy()
    z_plane = (z_plane * 255).astype(np.uint8)
    # OpenCV uses BGRA format, so convert from RGBA to BGRA
    z_plane = cv2.cvtColor(z_plane, cv2.COLOR_RGBA2BGRA)
    cv2.imwrite("z_plane.png", z_plane)

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
