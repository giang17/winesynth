#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/gui/iplugview.h"

namespace WineSynth {

class Controller : public Steinberg::Vst::EditControllerEx1
{
public:
    static Steinberg::FUnknown* createInstance (void*)
    {
        return (Steinberg::Vst::IEditController*)new Controller;
    }

    Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
    Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
    Steinberg::IPlugView* PLUGIN_API createView (const char* name) SMTG_OVERRIDE;
};

} // namespace WineSynth
