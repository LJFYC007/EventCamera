from falcor import *

def render_graph_PathTracer():
    g = RenderGraph("PathTracer")
    PathTracer = createPass("PathTracer", {
        'samplesPerPixel': $SAMPLES_PER_PIXEL$,
        'useNRDDemodulation': False,
        'maxTransmissionBounces': 0,
        'useRussianRoulette': $RUSSIAN_ROULETTE$,
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

    """
    ErrorMeasurePass = createPass("ErrorMeasurePass", {
        'threshold': $THRESHOLD$,
        'needAccumulatedEvents': $NEED_ACCUMULATED_EVENTS$,
        'toleranceEvents': $TOLERANCE_EVENTS$,
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
    """

    AccumulatePass = createPass("AccumulatePass", {})
    AccumulatePassDI = createPass("AccumulatePass", {})
    AccumulatePassGI = createPass("AccumulatePass", {})
    g.addPass(AccumulatePass, "AccumulatePass")
    g.addPass(AccumulatePassDI, "AccumulatePassDI")
    # g.addPass(AccumulatePassGI, "AccumulatePassGI")
    g.addEdge("PathTracer.color", "AccumulatePass.input")
    g.addEdge("PathTracer.DI", "AccumulatePassDI.input")
    # g.addEdge("PathTracer.GI", "AccumulatePassGI.input")

    g.markOutput("AccumulatePass.output")

    SceneDebuggerID = createPass('SceneDebugger', {'mode': 'InstanceID'})
    g.addPass(SceneDebuggerID, 'SceneDebuggerID')
    SceneDebuggerNormal = createPass('SceneDebugger', {'mode': 'FaceNormal'})
    g.addPass(SceneDebuggerNormal, 'SceneDebuggerNormal')

    BlockStoragePass = createPass("BlockStoragePass", {
        'enabled': $BLOCK_STORAGE_ENABLED$,
        'accumulatePass': $ACCUMULATE_PASS$,
        'directory': "$DIRECTORY$/Output",
    })
    g.addPass(BlockStoragePass, "BlockStoragePass")
    g.addEdge("AccumulatePass.output", "BlockStoragePass.input")

    BlockStoragePassDI = createPass("BlockStoragePass", {
        'enabled': $BLOCK_STORAGE_ENABLED$,
        'accumulatePass': $ACCUMULATE_PASS$,
        'directory': "$DIRECTORY$/OutputDI",
    })
    g.addPass(BlockStoragePassDI, "BlockStoragePassDI")
    g.addEdge("AccumulatePassDI.output", "BlockStoragePassDI.input")

    BlockStoragePassID = createPass("BlockStoragePass", {
        'enabled': $BLOCK_STORAGE_ENABLED$,
        'accumulatePass': $ACCUMULATE_PASS$,
        'directory': "$DIRECTORY$/OutputID",
    })
    g.addPass(BlockStoragePassID, "BlockStoragePassID")
    g.addEdge("SceneDebuggerID.output", "BlockStoragePassID.input")

    BlockStoragePassNormal = createPass("BlockStoragePass", {
        'enabled': $BLOCK_STORAGE_ENABLED$,
        'accumulatePass': $ACCUMULATE_PASS$,
        'directory': "$DIRECTORY$/OutputNormal",
    })
    g.addPass(BlockStoragePassNormal, "BlockStoragePassNormal")
    g.addEdge("SceneDebuggerNormal.output", "BlockStoragePassNormal.input")

    """
    BlockStoragePassGI = createPass("BlockStoragePass", {
        'enabled': $BLOCK_STORAGE_ENABLED$,
        'accumulatePass': $ACCUMULATE_PASS$,
        'directory': "$DIRECTORY$/OutputGI",
    })
    g.addPass(BlockStoragePassGI, "BlockStoragePassGI")
    g.addEdge("AccumulatePassGI.output", "BlockStoragePassGI.input")
    """


    # g.markOutput("CompressPass.output")
    g.markOutput("BlockStoragePass.output")
    # g.markOutput("BlockStoragePassDI.output")
    # g.markOutput("BlockStoragePassGI.output")
    # g.markOutput("BlockStoragePassID.output")
    # g.markOutput("BlockStoragePassNormal.output")

    return g

PathTracer = render_graph_PathTracer()
try: m.addGraph(PathTracer)
except NameError: None

m.clock.exitFrame = $EXIT_FRAME$
m.clock.timeScale = $TIME_SCALE$
