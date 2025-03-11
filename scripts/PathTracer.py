from falcor import *

samplesPerPixel = 16
accumulateFrames = 1
totalFrames = 1 * 10000

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': samplesPerPixel, 'maxTransmissionBounces': 0, 'useNRDDemodulation': False})
    g.addPass(PathTracer, "PathTracer")
    VBufferRT = createPass("VBufferRT", {'samplePattern': 'Stratified', 'sampleCount': 16, 'useAlphaTest': True, 'maxTransmissionBounces': 0})
    g.addPass(VBufferRT, "VBufferRT")
    AccumulatePass = createPass("AccumulatePass", {'enabled': True, 'precisionMode': 'Single'})
    g.addPass(AccumulatePass, "AccumulatePass")
    g.addEdge("VBufferRT.vbuffer", "PathTracer.vbuffer")
    g.addEdge("VBufferRT.viewW", "PathTracer.viewW")
    g.addEdge("VBufferRT.mvec", "PathTracer.mvec")
    g.addEdge("PathTracer.color", "AccumulatePass.input")
    g.markOutput("PathTracer.color")
    # g.markOutput("AccumulatePass.output")
    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.exitFrame = accumulateFrames * totalFrames
m.frameCapture.outputDir = f"C:\\Users\\-LJF007-\\Documents\\output\\gt{samplesPerPixel * accumulateFrames}"
m.frameCapture.baseFilename = ""

frames = [(accumulateFrames - 1) + accumulateFrames * i for i in range(totalFrames)]
m.frameCapture.addFrames(m.activeGraph, frames)
