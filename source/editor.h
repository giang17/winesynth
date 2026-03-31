#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/controls/icontrollistener.h"
#include "vstgui/lib/controls/ctextlabel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace TooltipTest {

class TooltipOverlay;
class SynthKnobView;

class Editor : public Steinberg::Vst::VSTGUIEditor, public VSTGUI::IControlListener
{
public:
    Editor (void* controller);

    bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
    void PLUGIN_API close () SMTG_OVERRIDE;

    // IControlListener
    void valueChanged (VSTGUI::CControl* pControl) SMTG_OVERRIDE;

private:
    // WM_ERASEBKGND subclass for parent HWND (Wine white-on-open fix)
    static LRESULT CALLBACK parentSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static const int kEditorWidth = 300;
    static const int kEditorHeight = 250;

    SynthKnobView* valueKnob = nullptr;
    TooltipOverlay* tooltipOverlay = nullptr;
    VSTGUI::CTextLabel* readout = nullptr;

    // Parent HWND subclass state
    HWND parentHwnd_ = nullptr;
    WNDPROC origParentWndProc_ = nullptr;
};

} // namespace TooltipTest