add_plugin(Network)

target_sources(Network PRIVATE
    Network.cpp
    Network.h
    NetworkInput.cs.slang
    NetworkOutput.cs.slang
)

target_copy_shaders(Network RenderPasses/Network)

target_source_group(Network "RenderPasses")
