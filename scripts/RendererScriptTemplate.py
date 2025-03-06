from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {
        'samplesPerPixel': $SAMPLES_PER_PIXEL$,
        'useNRDDemodulation': False,
        'fixedSeed': 0,
        'maxTransmissionBounces': 0,
        'useRussianRoulette': True
    })
    g.addPass(PathTracer, "PathTracer")

    VBufferRT = createPass("VBufferRT", {
        'samplePattern': 'Stratified',
        'useAlphaTest': True
    })
    g.addPass(VBufferRT, "VBufferRT")
    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")

    ErrorMeasurePass = createPass("ErrorMeasurePass", {
        'threshold': $THRESHOLD$,
        'accumulateMax': $ACCUMULATE_MAX$
    })
    g.addPass(ErrorMeasurePass, "ErrorMeasurePass")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Reference")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Source")

    g.markOutput("ErrorMeasurePass.Output")

    CompressPass = createPass("CompressPass", {
        'enabled': $ENABLED$,
        'directory': "$DIRECTORY$",
    })
    g.addPass(CompressPass, "CompressPass")
    g.addEdge("ErrorMeasurePass.Output", "CompressPass.input")

    g.markOutput("CompressPass.output")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.exitFrame = $EXIT_FRAME$
m.clock.timeScale = $TIME_SCALE$
