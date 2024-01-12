#include <emscripten/webaudio.h>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <utility>
#include <cassert>
#include <string>

#include <NAM/activations.h>
#include <NAM/dsp.h>

#define SMOOTH_EPSILON .0001f

float inputLevel = 0;
float outputLevel = 0;
// default input level
float input_level = 0;
// default output level
float output_level = 0;

std::unique_ptr<nam::DSP> currentModel = nullptr;
float prevDCInput = 0;
float prevDCOutput = 0;

void setDsp() {
  // loads wavenet based profile
	currentModel = nam::get_dsp("evh.nam");
  // loads lstm based profile
	// currentModel = nam::get_dsp("BossLSTM-1x16.nam");
}

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
		currentModel->process(audio_out, audio_out, n_samples);
		currentModel->finalize_(n_samples);

		if (currentModel->HasLoudness())
		{
			// Normalize model to -18dB
			modelLoudnessAdjustmentDB = -18 - currentModel->GetLoudness();
		}
	}

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

uint8_t audioThreadStack[4096];

EM_BOOL NamProcessor(int numInputs, const AudioSampleFrame *inputs,
                      int numOutputs, AudioSampleFrame *outputs,
                      int numParams, const AudioParamFrame *params,
                      void *userData)
{
	process(inputs[0].data, outputs[0].data, 128);
  
  return EM_TRUE; // Keep the graph output going
}

EM_BOOL OnCanvasClick(int eventType, const EmscriptenMouseEvent *mouseEvent, void *userData)
{
  EMSCRIPTEN_WEBAUDIO_T audioContext = (EMSCRIPTEN_WEBAUDIO_T)userData;
  if (emscripten_audio_context_state(audioContext) != AUDIO_CONTEXT_STATE_RUNNING) {
    emscripten_resume_audio_context_sync(audioContext);
  }
  return EM_FALSE;
}

void AudioWorkletProcessorCreated(EMSCRIPTEN_WEBAUDIO_T audioContext, EM_BOOL success, void *userData)
{
  if (!success) return; // Check browser console in a debug build for detailed errors

  int outputChannelCounts[1] = { 1 };
  EmscriptenAudioWorkletNodeCreateOptions options = {
    .numberOfInputs = 1,
    .numberOfOutputs = 1,
    .outputChannelCounts = outputChannelCounts
  };

  // Create node
  EMSCRIPTEN_AUDIO_WORKLET_NODE_T wasmAudioWorklet = emscripten_create_wasm_audio_worklet_node(audioContext,
                                                            "nam-worklet", &options, &NamProcessor, 0);

  // Connect it to audio context destination
  EM_ASM(
		{
			// emscriptenGetAudioObject($0).connect(emscriptenGetAudioObject($1).destination);
			window.audioCtx1 = emscriptenGetAudioObject($0);
			window.audioCtx2 = emscriptenGetAudioObject($1);
		},
    wasmAudioWorklet, audioContext
	);

  // Resume context on mouse click
  emscripten_set_click_callback("canvas", (void*)audioContext, 0, OnCanvasClick);
}

void AudioThreadInitialized(EMSCRIPTEN_WEBAUDIO_T audioContext, EM_BOOL success, void *userData)
{
  if (!success) return; // Check browser console in a debug build for detailed errors
  WebAudioWorkletProcessorCreateOptions opts = {
    .name = "nam-worklet",
  };
  emscripten_create_wasm_audio_worklet_processor_async(audioContext, &opts, &AudioWorkletProcessorCreated, 0);
}

int main()
{
	setDsp();

  EMSCRIPTEN_WEBAUDIO_T context = emscripten_create_audio_context(0);

  emscripten_start_wasm_audio_worklet_thread_async(context, audioThreadStack, sizeof(audioThreadStack),
                                                   &AudioThreadInitialized, 0);
}

