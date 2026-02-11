#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/controls/icontrollistener.h"

namespace WineSynth {

class WaveformButton;
class WaveformDisplay;

class Editor : public Steinberg::Vst::VSTGUIEditor, public VSTGUI::IControlListener
{
public:
    Editor (void* controller);

    bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
    void PLUGIN_API close () SMTG_OVERRIDE;

    // IControlListener
    void valueChanged (VSTGUI::CControl* pControl) SMTG_OVERRIDE;

private:
    void selectWaveform (int waveType);
    void flushDisplayUpdate ();

    static const int kEditorWidth = 620;
    static const int kEditorHeight = 420;

    WaveformButton* waveButtons[4] = {};
    WaveformDisplay* waveDisplay = nullptr;

    // Deferred display update (avoid redraw conflicts while dragging knobs)
    float pendingCutoff = 1.0f;
    float pendingResonance = 0.0f;
    bool displayDirty = false;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> displayTimer;
};

} // namespace WineSynth
