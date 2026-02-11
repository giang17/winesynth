#include "controller.h"
#include "pluginparamids.h"
#include "editor.h"

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

#include <cstring>
#include <cmath>

namespace WineSynth {

using namespace Steinberg;
using namespace Steinberg::Vst;

tresult PLUGIN_API Controller::initialize (FUnknown* context)
{
    tresult result = EditControllerEx1::initialize (context);
    if (result != kResultOk)
        return result;

    // Gain (0..1, default 0.5)
    parameters.addParameter (STR16 ("Gain"), nullptr, 0, 0.5,
                             ParameterInfo::kCanAutomate, kGainId);

    // Cutoff (0..1 → 20..20000 Hz, default 1.0 = fully open)
    parameters.addParameter (STR16 ("Cutoff"), STR16 ("Hz"), 0, 1.0,
                             ParameterInfo::kCanAutomate, kCutoffId);

    // Fine tuning (0..1 → -100..+100 cent, default 0.5 = 0 cent)
    parameters.addParameter (STR16 ("Fine"), STR16 ("ct"), 0, 0.5,
                             ParameterInfo::kCanAutomate, kFineId);

    // Resonance (0..1, default 0.0 = no resonance)
    parameters.addParameter (STR16 ("Resonance"), nullptr, 0, 0.0,
                             ParameterInfo::kCanAutomate, kResonanceId);

    // Waveform (list: Sine, Saw, Square, Triangle)
    auto* waveformParam = new StringListParameter (STR16 ("Waveform"), kWaveformId);
    waveformParam->appendString (STR16 ("Sine"));
    waveformParam->appendString (STR16 ("Saw"));
    waveformParam->appendString (STR16 ("Square"));
    waveformParam->appendString (STR16 ("Triangle"));
    parameters.addParameter (waveformParam);

    // Attack (0..1, default 0.05)
    parameters.addParameter (STR16 ("Attack"), STR16 ("ms"), 0, 0.05,
                             ParameterInfo::kCanAutomate, kAttackId);

    // Release (0..1, default 0.3)
    parameters.addParameter (STR16 ("Release"), STR16 ("ms"), 0, 0.3,
                             ParameterInfo::kCanAutomate, kReleaseId);

    // Bypass
    parameters.addParameter (STR16 ("Bypass"), nullptr, 1, 0,
                             ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass, kBypassId);

    return result;
}

tresult PLUGIN_API Controller::setComponentState (IBStream* state)
{
    if (!state)
        return kResultFalse;

    IBStreamer streamer (state, kLittleEndian);
    float f; int32 i;

    if (!streamer.readFloat (f)) return kResultFalse;
    setParamNormalized (kGainId, f);

    if (!streamer.readFloat (f)) return kResultFalse;
    setParamNormalized (kCutoffId, f);

    if (!streamer.readFloat (f)) return kResultFalse;
    setParamNormalized (kFineId, f);

    if (!streamer.readFloat (f)) return kResultFalse;
    setParamNormalized (kResonanceId, f);

    if (!streamer.readInt32 (i)) return kResultFalse;
    setParamNormalized (kWaveformId, (float)i / (float)(kNumWaveforms - 1));

    if (!streamer.readFloat (f)) return kResultFalse;
    setParamNormalized (kAttackId, f);

    if (!streamer.readFloat (f)) return kResultFalse;
    setParamNormalized (kReleaseId, f);

    if (!streamer.readInt32 (i)) return kResultFalse;
    setParamNormalized (kBypassId, i > 0 ? 1.0 : 0.0);

    return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView (const char* name)
{
    if (strcmp (name, ViewType::kEditor) == 0)
        return new Editor (this);
    return nullptr;
}

} // namespace WineSynth
