add_plugin(BlockStoragePass)

target_sources(BlockStoragePass PRIVATE
    BlockStoragePass.cpp
    BlockStoragePass.h
    BlockStoragePass.cs.slang
)

target_copy_shaders(BlockStoragePass RenderPasses/BlockStoragePass)

target_source_group(BlockStoragePass "RenderPasses")
