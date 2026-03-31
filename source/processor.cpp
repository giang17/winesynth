#include "processor.h"
#include "plugincids.h"
#include "pluginparamids.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "base/source/fstreamer.h"

#include <cmath>
#include <algorithm>
#include <cstring>

namespace TooltipTest {

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

    addAudioInput (STR16 ("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);

    return kResultOk;
}

tresult PLUGIN_API Processor::setActive (TBool /*state*/)
{
    return kResultOk;
}

tresult PLUGIN_API Processor::setupProcessing (ProcessSetup& newSetup)
{
    return AudioEffect::setupProcessing (newSetup);
}

tresult PLUGIN_API Processor::canProcessSampleSize (int32 symbolicSampleSize)
{
    if (symbolicSampleSize == kSample32)
        return kResultTrue;
    return kResultFalse;
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
                        case kGainId:   fGain = (float)value; break;
                        case kBypassId: bBypass = (value > 0.5f); break;
                    }
                }
            }
        }
    }

    // No outputs? Nothing to do.
    if (data.numOutputs == 0 || data.numSamples == 0)
        return kResultOk;

    int32 numChannels = data.outputs[0].numChannels;
    int32 numSamples = data.numSamples;
    float** out = data.outputs[0].channelBuffers32;

    // Bypass: silence output
    if (bBypass)
    {
        for (int32 ch = 0; ch < numChannels; ch++)
            memset (out[ch], 0, numSamples * sizeof (float));
        data.outputs[0].silenceFlags = (1ULL << numChannels) - 1;
        return kResultOk;
    }

    // Pass-through with gain
    bool hasInput = (data.numInputs > 0 && data.inputs[0].channelBuffers32 != nullptr);

    for (int32 ch = 0; ch < numChannels; ch++)
    {
        float* dst = out[ch];
        if (hasInput && ch < data.inputs[0].numChannels)
        {
            float* src = data.inputs[0].channelBuffers32[ch];
            for (int32 s = 0; s < numSamples; s++)
                dst[s] = src[s] * fGain;
        }
        else
        {
            memset (dst, 0, numSamples * sizeof (float));
        }
    }

    data.outputs[0].silenceFlags = 0;
    return kResultOk;
}

tresult PLUGIN_API Processor::setState (IBStream* state)
{
    IBStreamer streamer (state, kLittleEndian);
    float f;
    int32 i;

    if (!streamer.readFloat (f)) return kResultFalse;
    fGain = f;

    if (!streamer.readInt32 (i)) return kResultFalse;
    bBypass = i > 0;

    return kResultOk;
}

tresult PLUGIN_API Processor::getState (IBStream* state)
{
    IBStreamer streamer (state, kLittleEndian);

    streamer.writeFloat (fGain);
    streamer.writeInt32 (bBypass ? 1 : 0);

    return kResultOk;
}

} // namespace TooltipTest