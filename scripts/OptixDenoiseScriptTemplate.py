from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {
        'samplesPerPixel': 64,
        'useNRDDemodulation': False,
        'maxTransmissionBounces': 0,
    })
    g.addPass(PathTracer, "PathTracer")
    VBufferRT = createPass("VBufferRT", {'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True})
    g.addPass(VBufferRT, "VBufferRT")

    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")

    OptixDenoiser = createPass("OptixDenoiser", {"model": "Temporal"})
    g.addPass(OptixDenoiser, "OptixDenoiser")
    g.addEdge("PathTracer.color", "OptixDenoiser.color")
    g.addEdge("PathTracer.albedo", "OptixDenoiser.albedo")
    g.addEdge("PathTracer.guideNormal", "OptixDenoiser.normal")
    g.addEdge("VBufferRT.mvec", "OptixDenoiser.mvec")

    DenoisePass = createPass("DenoisePass", {
        'accumulatePass': 1,
        'directory': "$DIRECTORY$\\optix",
    })
    g.addPass(DenoisePass, "DenoisePass")

    g.addEdge("OptixDenoiser.output", "DenoisePass.input")
    g.markOutput("DenoisePass.output")
    g.markOutput("DenoisePass.color")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.timeScale = $NETWORK_TIME_SCALE$
m.clock.exitFrame = $OPTIX_NETWORK_EXIT_FRAME$
