#include "processor.h"
#include "plugincids.h"
#include "pluginparamids.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "base/source/fstreamer.h"

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WineSynth {

using namespace Steinberg;
using namespace Steinberg::Vst;

Processor::Processor ()
{
    setControllerClass (ControllerUID);
}

tresult PLUGIN_API Processor::initialize (FUnknown* context)
{
    tresult result = AudioEffect::initialize (context);
    if (result != kResultOk)
        return result;

    addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);
    addEventInput (STR16 ("Event In"));

    return kResultOk;
}

tresult PLUGIN_API Processor::setActive (TBool state)
{
    if (state)
    {
        phase = 0.0;
        envState = kIdle;
        envLevel = 0.0;
        noteOn = false;
    }
    return AudioEffect::setActive (state);
}

tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& newSetup)
{
    sampleRate = newSetup.sampleRate;
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    if (symbolicSampleSize == kSample32)
        return kResultTrue;
    return kResultFalse;
}

double Processor::generateSample (double ph, int waveform)
{
    double t = ph / (2.0 * M_PI);
    switch (waveform)
    {
        case kWaveSine:
            return sin (ph);
        case kWaveSaw:
            return 2.0 * t - 1.0;
        case kWaveSquare:
            return t < 0.5 ? 1.0 : -1.0;
        case kWaveTriangle:
            return 4.0 * fabs (t - 0.5) - 1.0;
        default:
            return sin (ph);
    }
}

tresult PLUGIN_API Processor::process (ProcessData& data)
{
    // Read parameter changes
    if (IParameterChanges* paramChanges = data.inputParameterChanges)
    {
        int32 numParamsChanged = paramChanges->getParameterCount ();
        for (int32 i = 0; i < numParamsChanged; i++)
        {
            if (IParamValueQueue* paramQueue = paramChanges->getParameterData (i))
            {
                ParamValue value;
                int32 sampleOffset;
                int32 numPoints = paramQueue->getPointCount ();
                if (paramQueue->getPoint (numPoints - 1, sampleOffset, value) == kResultTrue)
                {
                    switch (paramQueue->getParameterId ())
                    {
                        case kGainId:      fGain = (float)value; break;
                        case kFrequencyId: fFrequency = (float)value; break;
                        case kFineId:      fFine = (float)value; break;
                        case kWaveformId:  iWaveform = std::min ((int32)(value * kNumWaveforms), (int32)(kNumWaveforms - 1)); break;
                        case kAttackId:    fAttack = (float)value; break;
                        case kReleaseId:   fRelease = (float)value; break;
                        case kBypassId:    bBypass = (value > 0.5f); break;
                    }
                }
            }
        }
    }

    // Process MIDI events
    if (IEventList* events = data.inputEvents)
    {
        int32 numEvents = events->getEventCount ();
        for (int32 i = 0; i < numEvents; i++)
        {
            Event event;
            if (events->getEvent (i, event) == kResultOk)
            {
                if (event.type == Event::kNoteOnEvent)
                {
                    // MIDI note to frequency
                    noteFrequency = 440.0f * powf (2.0f, ((float)event.noteOn.pitch - 69.0f) / 12.0f);
                    noteOn = true;
                    envState = kAttack;

                    // Calculate attack rate: 1..1000 ms (exponential)
                    double attackMs = 1.0 + 999.0 * fAttack * fAttack;
                    double attackSamples = attackMs * 0.001 * sampleRate;
                    attackRate = 1.0 / std::max (attackSamples, 1.0);
                }
                else if (event.type == Event::kNoteOffEvent)
                {
                    noteOn = false;
                    envState = kRelease;

                    // Calculate release rate: 10..3000 ms (exponential)
                    double releaseMs = 10.0 + 2990.0 * fRelease * fRelease;
                    double releaseSamples = releaseMs * 0.001 * sampleRate;
                    releaseRate = envLevel / std::max (releaseSamples, 1.0);
                }
            }
        }
    }

    if (data.numOutputs == 0)
        return kResultOk;

    int32 numChannels = data.outputs[0].numChannels;
    int32 numSamples = data.numSamples;
    float** out = data.outputs[0].channelBuffers32;

    if (bBypass || numSamples == 0)
    {
        for (int32 ch = 0; ch < numChannels; ch++)
            memset (out[ch], 0, numSamples * sizeof (float));
        data.outputs[0].silenceFlags = (1ULL << numChannels) - 1;
        return kResultOk;
    }

    // Calculate final frequency (knob 20..20000 Hz exponential + fine tuning)
    // MIDI notes only trigger the envelope, pitch comes from the knob
    float baseFreq = 20.0f * powf (1000.0f, fFrequency);

    float fineOffset = (fFine - 0.5f) * 200.0f;  // -100..+100 cent
    float finalFreq = baseFreq * powf (2.0f, fineOffset / 1200.0f);

    double phaseInc = 2.0 * M_PI * finalFreq / sampleRate;

    for (int32 s = 0; s < numSamples; s++)
    {
        // Envelope
        switch (envState)
        {
            case kAttack:
                envLevel += attackRate;
                if (envLevel >= 1.0)
                {
                    envLevel = 1.0;
                    envState = noteOn ? kAttack : kRelease;
                    if (noteOn) envState = kAttack; // stay at 1.0
                }
                break;
            case kRelease:
                envLevel -= releaseRate;
                if (envLevel <= 0.0)
                {
                    envLevel = 0.0;
                    envState = kIdle;
                }
                break;
            case kIdle:
            default:
                break;
        }

        float sample = 0.f;
        if (envLevel > 0.0)
        {
            sample = (float)(generateSample (phase, iWaveform) * fGain * envLevel);
            phase += phaseInc;
            if (phase >= 2.0 * M_PI)
                phase -= 2.0 * M_PI;
        }

        for (int32 ch = 0; ch < numChannels; ch++)
            out[ch][s] = sample;
    }

    data.outputs[0].silenceFlags = (envState == kIdle) ? ((1ULL << numChannels) - 1) : 0;
    return kResultOk;
}

tresult PLUGIN_API Processor::setState (IBStream* state)
{
    IBStreamer streamer (state, kLittleEndian);
    float f; int32 i;

    if (!streamer.readFloat (f)) return kResultFalse; fGain = f;
    if (!streamer.readFloat (f)) return kResultFalse; fFrequency = f;
    if (!streamer.readFloat (f)) return kResultFalse; fFine = f;
    if (!streamer.readInt32 (i)) return kResultFalse; iWaveform = i;
    if (!streamer.readFloat (f)) return kResultFalse; fAttack = f;
    if (!streamer.readFloat (f)) return kResultFalse; fRelease = f;
    if (!streamer.readInt32 (i)) return kResultFalse; bBypass = i > 0;

    return kResultOk;
}

tresult PLUGIN_API Processor::getState (IBStream* state)
{
    IBStreamer streamer (state, kLittleEndian);

    streamer.writeFloat (fGain);
    streamer.writeFloat (fFrequency);
    streamer.writeFloat (fFine);
    streamer.writeInt32 (iWaveform);
    streamer.writeFloat (fAttack);
    streamer.writeFloat (fRelease);
    streamer.writeInt32 (bBypass ? 1 : 0);

    return kResultOk;
}

} // namespace WineSynth
