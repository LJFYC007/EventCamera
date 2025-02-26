from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': 16, 'useNRDDemodulation': False, 'fixedSeed': 0})
    g.addPass(PathTracer, "PathTracer")
    VBufferRT = createPass("VBufferRT", {'samplePattern': 'Center', 'useAlphaTest': True})
    g.addPass(VBufferRT, "VBufferRT")
    AccumulatePass = createPass("AccumulatePass", {'enabled': True, 'precisionMode': 'Single'})
    g.addPass(AccumulatePass, "AccumulatePass")
    ToneMapper = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapper, "ToneMapper")
    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")

    ErrorMeasurePass= createPass("ErrorMeasurePass")
    g.addPass(ErrorMeasurePass, "ErrorMeasurePass")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Reference")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Source")
    g.markOutput("ErrorMeasurePass.Output")

    # g.addEdge("PathTracer1.color", "AccumulatePass.input")
    # g.markOutput("AccumulatePass.output")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None
