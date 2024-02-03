//
//  NoiseGate.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 2/5/23.
//

#pragma once

#include <cmath>
#include <unordered_set>
#include <vector>

#include "dsp.h"

namespace dsp
{
namespace noise_gate
{
// Disclaimer: No one told me how noise gates work. I'm just going to try
// and have fun with it and see if I like what I get! :D

// "The noise floor." The loudness of anything quieter than this is bumped
// up to as if it were this loud for gating purposes (i.e. computing gain
// reduction).
const DSP_SAMPLE MINIMUM_LOUDNESS_DB = -120.0;
const DSP_SAMPLE MINIMUM_LOUDNESS_POWER = pow(10.0, MINIMUM_LOUDNESS_DB / 10.0);

// Parts 2: The gain module.
// This applies the gain reduction taht was determined by the trigger.
// It's declared first so that the trigger can define listeners without a
// forward declaration.

// The class that applies the gain reductions calculated by a trigger instance.
class Gain : public DSP
{
public:
  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const size_t numChannels, const size_t numFrames) override;

  void SetGainReductionDB(std::vector<std::vector<DSP_SAMPLE>>& gainReductionDB)
  {
    this->mGainReductionDB = gainReductionDB;
  }

private:
  std::vector<std::vector<DSP_SAMPLE>> mGainReductionDB;
};

// Part 1 of the noise gate: the trigger.
// This listens to a stream of incoming audio and determines how much gain
// to apply based on the loudness of the signal.

class TriggerParams
{
public:
  TriggerParams(const DSP_SAMPLE time, const DSP_SAMPLE threshold, const DSP_SAMPLE ratio, const DSP_SAMPLE openTime,
                const DSP_SAMPLE holdTime, const DSP_SAMPLE closeTime)
  : mTime(time)
  , mThreshold(threshold)
  , mRatio(ratio)
  , mOpenTime(openTime)
  , mHoldTime(holdTime)
  , mCloseTime(closeTime){};

  DSP_SAMPLE GetTime() const { return this->mTime; };
  DSP_SAMPLE GetThreshold() const { return this->mThreshold; };
  DSP_SAMPLE GetRatio() const { return this->mRatio; };
  DSP_SAMPLE GetOpenTime() const { return this->mOpenTime; };
  DSP_SAMPLE GetHoldTime() const { return this->mHoldTime; };
  DSP_SAMPLE GetCloseTime() const { return this->mCloseTime; };

private:
  // The time constant for quantifying the loudness of the signal.
  DSP_SAMPLE mTime;
  // The threshold at which expanssion starts
  DSP_SAMPLE mThreshold;
  // The compression ratio.
  DSP_SAMPLE mRatio;
  // How long it takes to go from maximum gain reduction to zero.
  DSP_SAMPLE mOpenTime;
  // How long to stay open before starting to close.
  DSP_SAMPLE mHoldTime;
  // How long it takes to go from open to maximum gain reduction.
  DSP_SAMPLE mCloseTime;
};

class Trigger : public DSP
{
public:
  Trigger();

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const size_t numChannels, const size_t numFrames) override;
  std::vector<std::vector<DSP_SAMPLE>> GetGainReduction() const { return this->mGainReductionDB; };
  void SetParams(const TriggerParams& params) { this->mParams = params; };
  void SetSampleRate(const DSP_SAMPLE sampleRate) { this->mSampleRate = sampleRate; }
  std::vector<std::vector<DSP_SAMPLE>> GetGainReductionDB() const { return this->mGainReductionDB; };

  void AddListener(Gain* gain)
  {
    // This might be risky dropping a raw pointer, but I don't think that the
    // gain would be destructed, so probably ok.
    this->mGainListeners.insert(gain);
  }

private:
  enum class State
  {
    MOVING = 0,
    HOLDING
  };

  DSP_SAMPLE _GetGainReduction(const DSP_SAMPLE levelDB) const
  {
    const DSP_SAMPLE threshold = this->mParams.GetThreshold();
    // Quadratic gain reduction? :)
    return levelDB < threshold ? -(this->mParams.GetRatio()) * (levelDB - threshold) * (levelDB - threshold) : 0.0;
  }
  DSP_SAMPLE _GetMaxGainReduction() const { return this->_GetGainReduction(MINIMUM_LOUDNESS_DB); }
  virtual void _PrepareBuffers(const size_t numChannels, const size_t numFrames) override;

  TriggerParams mParams;
  std::vector<State> mState; // One per channel
  std::vector<DSP_SAMPLE> mLevel;

  // Hold the vectors of gain reduction for the block, in dB.
  // These can be given to the Gain object.
  std::vector<std::vector<DSP_SAMPLE>> mGainReductionDB;
  std::vector<DSP_SAMPLE> mLastGainReductionDB;

  DSP_SAMPLE mSampleRate;
  // How long we've been holding
  std::vector<DSP_SAMPLE> mTimeHeld;

  std::unordered_set<Gain*> mGainListeners;
};

}; // namespace noise_gate
}; // namespace dsp