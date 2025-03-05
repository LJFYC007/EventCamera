add_plugin(CompressPass)

target_sources(CompressPass PRIVATE
    CompressPass.cpp
    CompressPass.h
    BufferPass.cs.slang
)

target_copy_shaders(CompressPass RenderPasses/CompressPass)

target_source_group(CompressPass "RenderPasses")
