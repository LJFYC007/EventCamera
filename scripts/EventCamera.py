from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': 16, 'useNRDDemodulation': False, 'fixedSeed': 0})
    g.addPass(PathTracer, "PathTracer")
    VBufferRT = createPass("VBufferRT", {'samplePattern': 'Stratified', 'useAlphaTest': True})
    g.addPass(VBufferRT, "VBufferRT")
    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")

    ErrorMeasurePass= createPass("ErrorMeasurePass", {'Threshold': 1.5, 'AccumulateMax': 100})
    g.addPass(ErrorMeasurePass, "ErrorMeasurePass")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Reference")
    g.addEdge("PathTracer.color", "ErrorMeasurePass.Source")

    g.markOutput("ErrorMeasurePass.Output")
    # g.markOutput("PathTracer.color")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.exitFrame = 5001
m.frameCapture.outputDir = "C:\\Users\\-LJF007-\\Documents\\output\\BistroInterior_Wine"
m.frameCapture.baseFilename = ""
m.frameCapture.addFrames(m.activeGraph, range(200, 4999))
