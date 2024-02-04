#include <emscripten/webaudio.h>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <utility>
#include <cassert>
#include <string>

#include <NAM/activations.h>
#include <NAM/dsp.h>
#include <AudioDSPTools/NoiseGate.h>
#include <AudioDSPTools/dsp.h>

#define SMOOTH_EPSILON .0001f

float inputLevel = 0;
float outputLevel = 0;
// default input level
float input_level = 0;
// default output level
float output_level = 0;
bool loading = false;
const int bufferSize = 128;
const int numberOfChannels = 1;

// naming conflict on just "time"
const float gateTime = 0.01;
float threshold = -80; // GetParam... a value between -100 and 0, knob step 0.1
const float ratio = 0.1; // Quadratic...
const float openTime = 0.005;
const float holdTime = 0.01;
const float closeTime = 0.05;
bool noiseGateActive = true;
unsigned int sampleRate = 48000;

std::unique_ptr<nam::DSP> currentModel = nullptr;
float prevDCInput = 0;
float prevDCOutput = 0;

dsp::noise_gate::Trigger mNoiseGateTrigger;
dsp::noise_gate::Gain mNoiseGateGain;

void process(float* audio_in, float* audio_out, int n_samples)
{
  float level;

  // convert input level from db
  float desiredInputLevel = powf(10, input_level * 0.05f);

  if (fabs(desiredInputLevel - inputLevel) > SMOOTH_EPSILON)
  {
    level = inputLevel;
    for (int i = 0; i < n_samples; i++)
    {
      // do very basic smoothing
      level = (.99f * level) + (.01f * desiredInputLevel);

      audio_out[i] = audio_in[i] * level;
    }

    inputLevel = level;
  }
  else
  {
    level = inputLevel = desiredInputLevel;

    for (int i = 0; i < n_samples; i++)
    {
      audio_out[i] = audio_in[i] * level;
    }
  }

  float modelLoudnessAdjustmentDB = 0;

  if (currentModel != nullptr)
  {
    float** triggerOutput = new float*[numberOfChannels];
    triggerOutput[0] = audio_out;
    float** audio_in_arr = new float*[numberOfChannels];
    audio_in_arr[0] = audio_in;
    float** audio_out_arr = new float*[numberOfChannels];
    audio_out_arr[0] = audio_out;

    if (noiseGateActive)
    {
      const dsp::noise_gate::TriggerParams triggerParams(gateTime, threshold, ratio, openTime, holdTime, closeTime);
      mNoiseGateTrigger.SetParams(triggerParams);
      mNoiseGateTrigger.SetSampleRate(sampleRate);
      triggerOutput = mNoiseGateTrigger.Process(audio_in_arr, 1, bufferSize);
    }

    currentModel->process(triggerOutput[0], audio_out, n_samples);
    currentModel->finalize_(n_samples);

    if (currentModel->HasLoudness())
    {
      // Normalize model to -18dB
      modelLoudnessAdjustmentDB = -18 - currentModel->GetLoudness();
    }

    float* gateGainOutput =
      noiseGateActive ? mNoiseGateGain.Process(audio_out_arr, numberOfChannels, bufferSize)[0] : audio_out;

    for (int i = 0; i < n_samples; i++)
    {
      audio_out[i] = gateGainOutput[i];
    }
  }
  // Maybe implement fallbackdsp bypass function in the else block just like in plugin

  // Convert output level from db
  float desiredOutputLevel = powf(10, (output_level + modelLoudnessAdjustmentDB) * 0.05f);

  if (fabs(desiredOutputLevel - outputLevel) > SMOOTH_EPSILON)
  {
    level = outputLevel;

    for (int i = 0; i < n_samples; i++)
    {
      // do very basic smoothing
      level = (.99f * level) + (.01f * desiredOutputLevel);

      audio_out[i] = audio_out[i] * outputLevel;
    }

    outputLevel = level;
  }
  else
  {
    level = outputLevel = desiredOutputLevel;

    for (int i = 0; i < n_samples; i++)
    {
      audio_out[i] = audio_out[i] * level;
    }
  }

  for (int i = 0; i < n_samples; i++)
  {
    float dcInput = audio_out[i];

    // dc blocker
    audio_out[i] = audio_out[i] - prevDCInput + 0.995 * prevDCOutput;

    prevDCInput = dcInput;
    prevDCOutput = audio_out[i];
  }
}

uint8_t audioThreadStack[16384 * 32];

EM_BOOL NamProcessor(int numInputs, const AudioSampleFrame* inputs, int numOutputs, AudioSampleFrame* outputs,
                     int numParams, const AudioParamFrame* params, void* userData)
{
  if (loading == false)
  {
    process(inputs[0].data, outputs[0].data, bufferSize);

    return EM_TRUE; // Keep the graph output going
  }
  else
  {
    return EM_FALSE;
  }
}

EM_BOOL OnElementClick(int eventType, const EmscriptenMouseEvent* mouseEvent, void* userData)
{
  EMSCRIPTEN_WEBAUDIO_T audioContext = (EMSCRIPTEN_WEBAUDIO_T)userData;
  if (emscripten_audio_context_state(audioContext) != AUDIO_CONTEXT_STATE_RUNNING)
  {
    emscripten_resume_audio_context_sync(audioContext);
  }
  return EM_FALSE;
}

void AudioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T audioContext, EM_BOOL success, void* userData)
{
  if (!success)
    return; // Check browser console in a debug build for detailed errors

  int outputChannelCounts[1] = {1};
  EmscriptenAudioWorkletNodeCreateOptions options = {
    .numberOfInputs = 1, .numberOfOutputs = 1, .outputChannelCounts = outputChannelCounts};

  // Create node
  EMSCRIPTEN_AUDIO_WORKLET_NODE_T wasmAudioWorklet =
    emscripten_create_wasm_audio_worklet_node(audioContext, "nam-worklet", &options, &NamProcessor, 0);

  EM_ASM(
    {
      // create global callback in your code
      // the first argument is audioContext, the second one - worklet node
      if (window.wasmAudioWorkletCreated)
      {
        window.wasmAudioWorkletCreated(emscriptenGetAudioObject($0), emscriptenGetAudioObject($1));
      }
    },
    wasmAudioWorklet, audioContext);

  // Resume context on mouse click on a specific element created in your html file
  emscripten_set_click_callback("#audio-worklet-resumer", (void*)audioContext, 0, OnElementClick);
}

void AudioThreadInitialized(EMSCRIPTEN_WEBAUDIO_T audioContext, EM_BOOL success, void* userData)
{
  if (!success)
    return; // Check browser console in a debug build for detailed errors
  WebAudioWorkletProcessorCreateOptions opts = {
    .name = "nam-worklet",
  };
  emscripten_create_wasm_audio_worklet_processor_async(audioContext, &opts, &AudioWorkletProcessorCreated, 0);
}

unsigned query_sample_rate_of_audiocontexts()
{
  return EM_ASM_INT({
    var AudioContext = window.AudioContext || window.webkitAudioContext;
    var ctx = new AudioContext();
    var sr = ctx.sampleRate;
    ctx.close();
    return sr;
  });
}

extern "C" {
void setDsp(const char* jsonStr)
{
  std::unique_ptr<nam::DSP> tmp = nullptr;

  loading = true;
  tmp = nam::get_dsp(jsonStr);

  // initializations
  if (currentModel == nullptr)
  {
    // Turn on fast tanh approximation
    nam::activations::Activation::enable_fast_tanh();

    mNoiseGateTrigger.AddListener(&mNoiseGateGain);
    currentModel = std::move(tmp);

    sampleRate = query_sample_rate_of_audiocontexts();

    EmscriptenWebAudioCreateAttributes attrs = {.latencyHint = "interactive", .sampleRate = sampleRate};

    EMSCRIPTEN_WEBAUDIO_T context = emscripten_create_audio_context(&attrs);

    emscripten_start_wasm_audio_worklet_thread_async(
      context, audioThreadStack, sizeof(audioThreadStack), &AudioThreadInitialized, 0);
  }
  else
  {
    currentModel.reset();
    currentModel = std::move(tmp);
  }

  loading = false;
}

void setNoiseGateState(bool active)
{
  noiseGateActive = active;
}

void setNoiseGateThreshold(float _threshold)
{
  threshold = _threshold;
}
}
