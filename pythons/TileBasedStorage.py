import torch


class TileBasedStorage:
    def __init__(self, tile_size, tile_count, tile_data):
        self.tile_size = tile_size
        self.tile_count = tile_count
        self.tile_data = tile_data


class SparseTileBasedStorage(TileBasedStorage):
    def __init__(self, tile_size, tile_count, tile_data):
        super().__init__(tile_size, tile_count, tile_data)
