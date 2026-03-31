#include "controller.h"
#include "pluginparamids.h"
#include "editor.h"

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

#include <cstring>

namespace TooltipTest {

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

} // namespace TooltipTest