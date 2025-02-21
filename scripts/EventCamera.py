from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': 1, 'useNRDDemodulation': False, 'fixedSeed': 1})
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
    g.addEdge("PathTracer.color", "AccumulatePass.input")
    # g.addEdge("AccumulatePass.output", "ToneMapper.src")
    # g.markOutput("ToneMapper.dst")

    PathTracer1 = createPass("PathTracer", {'samplesPerPixel': 1, 'useNRDDemodulation': False, 'fixedSeed': 1})
    g.addPass(PathTracer1, "PathTracer1")
    VBufferRT1 = createPass("VBufferRT", {'samplePattern': 'Center', 'useAlphaTest': True, 'movement': float3(0.0, 0.01, 0.01)})
    g.addPass(VBufferRT1, "VBufferRT1")
    AccumulatePass1 = createPass("AccumulatePass", {'enabled': True, 'precisionMode': 'Single'})
    g.addPass(AccumulatePass1, "AccumulatePass1")
    g.addEdge("VBufferRT1.vbuffer", "PathTracer1.vbuffer")
    g.addEdge("VBufferRT1.viewW", "PathTracer1.viewW")
    g.addEdge("VBufferRT1.mvec", "PathTracer1.mvec")
    g.addEdge("PathTracer1.color", "AccumulatePass1.input")

    ErrorMeasurePass= createPass("ErrorMeasurePass")
    g.addPass(ErrorMeasurePass, "ErrorMeasurePass")
    g.addEdge("AccumulatePass.output", "ErrorMeasurePass.Reference")
    g.addEdge("AccumulatePass1.output", "ErrorMeasurePass.Source")
    # g.addEdge("PathTracer.color", "ErrorMeasurePass.Reference")
    # g.addEdge("PathTracer1.color", "ErrorMeasurePass.Source")
    g.markOutput("ErrorMeasurePass.Output")

    """
    g.addEdge("ErrorMeasurePass.Output", "AccumulatePass.input")
    g.markOutput("AccumulatePass.output")

    g.addEdge("AccumulatePass.output", "ToneMapper.src")
    g.markOutput("ToneMapper.dst")

    g.markOutput("PathTracer.color")
    g.markOutput("PathTracer1.color")
    """
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None
