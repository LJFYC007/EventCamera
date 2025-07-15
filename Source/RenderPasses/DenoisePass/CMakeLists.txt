add_plugin(DenoisePass)

target_sources(DenoisePass PRIVATE
    DenoisePass.cpp
    DenoisePass.h
    Denoise.slang
)

target_copy_shaders(DenoisePass RenderPasses/DenoisePass)

target_source_group(DenoisePass "RenderPasses")
