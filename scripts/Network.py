from falcor import *

model_path = "F:\\EventCamera\\config\\final_v2.onnx"

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

    Network = createPass("Network", {
        'accumulatePass': 64,
        'model_path': model_path,
        'batchSize': 4096,
        'networkInputLength': 43,
        'directory': "F:\EventCamera\..\Dataset\classroom\\bin",
        'tau': 1000.0,
        'threshold': 1.0,
    })
    g.addPass(Network, "Network")

    g.addEdge("AccumulatePass.output", "Network.input")
    g.markOutput("Network.output")

    # g.markOutput("AccumulatePass.output")
    # g.markOutput("PathTracer.DI")
    # g.markOutput("PathTracer.GI")

    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.timeScale = 1000
m.clock.exitFrame = 640000
