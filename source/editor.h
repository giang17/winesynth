#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/controls/icontrollistener.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace WineSynth {

class WaveformButton;
class WaveformDisplay;
class LiveOscilloscopeView;
class PianoKeyboardView;

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

    // WM_ERASEBKGND subclass for parent HWND (Wine white-on-open fix)
    static LRESULT CALLBACK parentSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static const int kEditorWidth = 620;
    static const int kEditorHeight = 670;

    WaveformButton* waveButtons[4] = {};
    WaveformDisplay* waveDisplay = nullptr;
    LiveOscilloscopeView* liveScope = nullptr;
    PianoKeyboardView* keyboard = nullptr;

    // Parent HWND subclass state
    HWND parentHwnd_ = nullptr;
    WNDPROC origParentWndProc_ = nullptr;

    // Deferred display update (avoid redraw conflicts while dragging knobs)
    float pendingCutoff = 1.0f;
    float pendingResonance = 0.0f;
    bool displayDirty = false;
    VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> displayTimer;
};

} // namespace WineSynth