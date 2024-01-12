# NeuralAmpModelerCore WASM building project
The project compiles the code to WASM using emscripten.
It even generates glue JavaScript code to set a Web Audio Worklet and an Audio Context, so you can easily consume them in your webpage.

### Building:
You will need to install [Emscripten](https://emscripten.org/docs/getting_started/downloads.html "Emscripten") to build this project, along with [CMake](https://cmake.org/ "CMake") (I personally use [Ninja](https://ninja-build.org/ "Ninja") generator with it on Windows 11)
Run the following script in your script:
```
cd NeuralAmpModelerCore_WASM/build
emcmake cmake .. -DCMAKE_BUILD_TYPE="Release"
cmake --build . --config=release -j4
```
Or if you use Windows just double click on the **build_wasm.bat** in the **scripts** folder.

### Example:
1) Import the main JS file: `<script type="text/javascript" src="loadwebmodel.js"></script>`
2) Execute the following code in your JS once the WASM is loaded (wait for some time, add the code to some DOM event handler, etc.):
```
const stream = await navigator.mediaDevices.getUserMedia({
      audio: {
        autoGainControl: false,
        echoCancellation: false,
        noiseSuppression: false,
        sampleRate: 48000,
      }
    });
window.audioCtx2.resume();
const microphoneStreamNode = window.audioCtx2.createMediaStreamSource(stream);
microphoneStreamNode.connect(window.audioCtx1); 
window.audioCtx1.connect(window.audioCtx2.destination);
	```
3) Add IR or other effects with your flavor

*The project uses a tiny bit of code from the [unofficial NAM LV2 plugin](https://github.com/mikeoliphant/neural-amp-modeler-lv2 "unofficial NAM LV2 plugin").*

# NeuralAmpModelerCore
Core DSP library for NAM plugins.

For an example how to use, see [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin).

_The general Audio DSP tools from version 0.0 have been moved to [AudioDSPTools](https://github.com/sdatkinson/AudioDSPTools)._

## Sharp edges
This library uses [Eigen](http://eigen.tuxfamily.org) to do the linear algebra routines that its neural networks require. Since these models hold their parameters as eigen object members, there is a risk with certain compilers and compiler optimizations that their memory is not aligned properly. This can be worked around by providing two preprocessor macros: `EIGEN_MAX_ALIGN_BYTES 0` and `EIGEN_DONT_VECTORIZE`, though this will probably harm performance. See [Structs Having Eigen Members](http://eigen.tuxfamily.org/dox-3.2/group__TopicStructHavingEigenMembers.html) for more information. This is being tracked as [Issue 67](https://github.com/sdatkinson/NeuralAmpModelerCore/issues/67).
