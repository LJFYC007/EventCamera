from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {
        'samplesPerPixel': 8,
        'useNRDDemodulation': False,
        'fixedSeed': 0,
        'maxTransmissionBounces': 0
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
        'threshold': 1.5,
        'accumulateMax': 100
    })
    g.addPass(ErrorMeasurePass, "ErrorMeasurePass")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Reference")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Source")

    g.markOutput("ErrorMeasurePass.Output")

    CompressPass = createPass("CompressPass", {
        'enabled': True,
        'directory': "C:\\Users\\-LJF007-\\Documents\\EventCamera\\..\\output\\Temp",
    })
    g.addPass(CompressPass, "CompressPass")
    g.addEdge("ErrorMeasurePass.Output", "CompressPass.input")

    g.markOutput("CompressPass.output")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.exitFrame = 5000
m.clock.timeScale = 10000
