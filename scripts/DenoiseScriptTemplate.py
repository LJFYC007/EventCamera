from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {
        'samplesPerPixel': 1,
        'useNRDDemodulation': False,
        'maxTransmissionBounces': 0,
    })
    g.addPass(PathTracer, "PathTracer")
    VBufferRT = createPass("VBufferRT", {'samplePattern': 'Stratified', 'sampleCount': 16, 'useAlphaTest': True})
    g.addPass(VBufferRT, "VBufferRT")
    AccumulatePass = createPass("AccumulatePass", {'enabled': True, 'precisionMode': 'Single'})
    g.addPass(AccumulatePass, "AccumulatePass")

    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")
    g.addEdge("PathTracer.color", "AccumulatePass.input")

    DenoisePass = createPass("DenoisePass", {
        'accumulatePass': $ACCUMULATE_PASS$,
        'directory': "$DIRECTORY$\\denoise",
    })
    g.addPass(DenoisePass, "DenoisePass")

    g.addEdge("AccumulatePass.output", "DenoisePass.input")
    g.markOutput("DenoisePass.output")
    g.markOutput("DenoisePass.color")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.timeScale = $NETWORK_TIME_SCALE$
m.clock.exitFrame = $NETWORK_EXIT_FRAME$
