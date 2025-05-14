add_plugin(Network)

target_sources(Network PRIVATE
    Network.cpp
    Network.h
)

target_source_group(Network "RenderPasses")
