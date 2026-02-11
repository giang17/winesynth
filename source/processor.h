#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

namespace WineSynth {

enum EnvState { kIdle, kAttack, kRelease };

class Processor : public Steinberg::Vst::AudioEffect
{
public:
    Processor ();

    static Steinberg::FUnknown* createInstance (void*)
    {
        return (Steinberg::Vst::IAudioProcessor*)new Processor;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& newSetup) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

private:
    double generateSample (double phase, int waveform);

    // Parameters
    float fGain = 0.5f;
    float fCutoff = 1.0f;      // normalized (1.0 = fully open)
    float fFine = 0.5f;        // normalized
    float fResonance = 0.0f;   // normalized
    int32_t iWaveform = 0;
    float fAttack = 0.05f;     // normalized
    float fRelease = 0.3f;     // normalized
    bool bBypass = false;

    // DSP state
    double phase = 0.0;
    double sampleRate = 44100.0;

    // SVF filter state (Cytomic TPT)
    double ic1eq = 0.0;
    double ic2eq = 0.0;

    // Envelope
    EnvState envState = kIdle;
    double envLevel = 0.0;
    double attackRate = 0.0;
    double releaseRate = 0.0;

    // MIDI
    float noteFrequency = 440.0f;
    bool noteOn = false;
};

} // namespace WineSynth
