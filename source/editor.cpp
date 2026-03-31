#include "editor.h"
#include "pluginparamids.h"
#include "controls.h"

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"

#include <cstdio>

using namespace VSTGUI;

namespace TooltipTest {

Editor::Editor (void* controller)
    : VSTGUIEditor (controller)
{
    setRect ({0, 0, kEditorWidth, kEditorHeight});
}

bool PLUGIN_API Editor::open (void* parent, const PlatformType& platformType)
{
    CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
    frame = new CFrame (frameSize, this);
    frame->setBackgroundColor (kBgColor);

    // --- Title ---
    auto titleLabel = new CTextLabel (CRect (20, 12, 200, 32));
    titleLabel->setText ("Tooltip Test");
    titleLabel->setFontColor (CColor (0, 200, 220, 255));
    titleLabel->setBackColor (kBgColor);
    titleLabel->setFrameColor (kBgColor);
    titleLabel->setHoriAlign (kLeftText);
    frame->addView (titleLabel);

    auto versionLabel = new CTextLabel (CRect (200, 12, 280, 32));
    versionLabel->setText ("v1.0");
    versionLabel->setFontColor (CColor (80, 80, 80, 255));
    versionLabel->setBackColor (kBgColor);
    versionLabel->setFrameColor (kBgColor);
    versionLabel->setHoriAlign (kRightText);
    frame->addView (versionLabel);

    // --- Instruction text ---
    auto instrLabel = new CTextLabel (CRect (20, 42, 280, 58));
    instrLabel->setText ("Drag the knob to show tooltip");
    instrLabel->setFontColor (CColor (100, 100, 110, 255));
    instrLabel->setBackColor (kBgColor);
    instrLabel->setFrameColor (kBgColor);
    instrLabel->setHoriAlign (kCenterText);
    frame->addView (instrLabel);

    // --- Knob label ---
    auto knobLabel = new CTextLabel (CRect (110, 72, 190, 88));
    knobLabel->setText ("Value");
    knobLabel->setFontColor (kLabelColor);
    knobLabel->setBackColor (kBgColor);
    knobLabel->setFrameColor (kBgColor);
    knobLabel->setHoriAlign (kCenterText);
    frame->addView (knobLabel);

    // --- Single knob, centered ---
    valueKnob = new SynthKnobView (CRect (115, 92, 185, 162), this, kGainId, 0.5f);
    frame->addView (valueKnob);

    // --- Value readout below knob ---
    auto readoutLabel = new CTextLabel (CRect (110, 168, 190, 184));
    readoutLabel->setText ("0.50");
    readoutLabel->setFontColor (CColor (160, 160, 170, 255));
    readoutLabel->setBackColor (kBgColor);
    readoutLabel->setFrameColor (kBgColor);
    readoutLabel->setHoriAlign (kCenterText);
    frame->addView (readoutLabel);
    readout = readoutLabel;

    // --- Tooltip overlay ---
    // The overlay covers the entire frame so that dirty-rect invalidation
    // from the knob can overlap with the tooltip bounds, triggering the
    // triple-layer compositing path in Wine's D2D1/DComp pipeline.
    tooltipOverlay = new TooltipOverlay (CRect (0, 0, kEditorWidth, kEditorHeight));
    frame->addView (tooltipOverlay);

    // Link knob to tooltip overlay
    valueKnob->setTooltipOverlay (tooltipOverlay);

    frame->open (parent, platformType);

    // Fix: Subclass parent HWND to suppress WM_ERASEBKGND (white flash on Wine).
    parentHwnd_ = (HWND)parent;
    origParentWndProc_ = (WNDPROC)GetWindowLongPtrA (parentHwnd_, GWLP_WNDPROC);
    SetPropA (parentHwnd_, "TooltipTestEditor", (HANDLE)this);
    SetWindowLongPtrA (parentHwnd_, GWLP_WNDPROC, (LONG_PTR)parentSubclassProc);

    // Under Wine, the initial WM_PAINT arrives before D2D1 is fully
    // initialized, leaving framebuffer garbage visible. Schedule a
    // delayed full redraw to ensure proper rendering.
    CFrame* f = frame;
    Call::later ([f] () { f->invalid (); }, 100);

    return true;
}

LRESULT CALLBACK Editor::parentSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* editor = (Editor*)GetPropA (hwnd, "TooltipTestEditor");

    if (msg == WM_ERASEBKGND)
        return 1;

    if (editor && editor->origParentWndProc_)
        return CallWindowProcA (editor->origParentWndProc_, hwnd, msg, wParam, lParam);

    return DefWindowProcA (hwnd, msg, wParam, lParam);
}

void PLUGIN_API Editor::close ()
{
    // Restore original WndProc before tearing down the frame
    if (parentHwnd_ && origParentWndProc_)
    {
        SetWindowLongPtrA (parentHwnd_, GWLP_WNDPROC, (LONG_PTR)origParentWndProc_);
        RemovePropA (parentHwnd_, "TooltipTestEditor");
        parentHwnd_ = nullptr;
        origParentWndProc_ = nullptr;
    }

    tooltipOverlay = nullptr;
    valueKnob = nullptr;
    readout = nullptr;

    if (frame)
    {
        frame->forget ();
        frame = nullptr;
    }
}

void Editor::valueChanged (CControl* pControl)
{
    if (!controller)
        return;

    int32_t tag = pControl->getTag ();
    float value = pControl->getValue ();

    controller->setParamNormalized (tag, value);
    controller->performEdit (tag, value);

    // Update readout label
    if (tag == kGainId && readout)
    {
        char buf[32];
        snprintf (buf, sizeof (buf), "%.2f", value);
        readout->setText (buf);
        readout->invalid ();
    }
}

} // namespace TooltipTest