#include "editor.h"
#include "pluginparamids.h"
#include "controls.h"

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "vstgui/lib/cvstguitimer.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/platform/win32/win32factory.h"

using namespace VSTGUI;

namespace WineSynth {

Editor::Editor (void* controller)
    : VSTGUIEditor (controller)
{
    setRect ({0, 0, kEditorWidth, kEditorHeight});
}

bool PLUGIN_API Editor::open (void* parent, const PlatformType& platformType)
{
    // DirectComposition is now supported via Wine's DComp DesktopDevice
    // implementation (BeginDraw/EndDraw with dirty-rect clipping + BitBlt
    // present). No need to disable it — let VSTGUI use the DComp path
    // for proper partial redraws.

    CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
    frame = new CFrame (frameSize, this);
    frame->setBackgroundColor (kBgColor);

    // --- Title ---
    auto titleLabel = new CTextLabel (CRect (20, 8, 200, 28));
    titleLabel->setText ("WineSynth");
    titleLabel->setFontColor (kWaveformColor);
    titleLabel->setBackColor (kBgColor);
    titleLabel->setFrameColor (kBgColor);
    titleLabel->setHoriAlign (kLeftText);
    frame->addView (titleLabel);

    auto versionLabel = new CTextLabel (CRect (520, 8, 600, 28));
    versionLabel->setText ("v1.0");
    versionLabel->setFontColor (CColor (80, 80, 80, 255));
    versionLabel->setBackColor (kBgColor);
    versionLabel->setFrameColor (kBgColor);
    versionLabel->setHoriAlign (kRightText);
    frame->addView (versionLabel);

    // --- Knob Row: Gain, Frequency, Fine ---
    auto makeLabel = [&](CCoord x, CCoord y, CCoord w, const char* text) {
        auto label = new CTextLabel (CRect (x, y, x + w, y + 16));
        label->setText (text);
        label->setFontColor (kLabelColor);
        label->setBackColor (kBgColor);
        label->setFrameColor (kBgColor);
        label->setHoriAlign (kCenterText);
        frame->addView (label);
    };

    // Gain
    makeLabel (15, 38, 80, "Gain");
    auto gainKnob = new SynthKnobView (CRect (25, 56, 95, 126), this, kGainId, 0.5f);
    frame->addView (gainKnob);

    // Cutoff
    makeLabel (115, 38, 80, "Cutoff");
    auto cutoffKnob = new SynthKnobView (CRect (125, 56, 195, 126), this, kCutoffId, 1.0f);
    frame->addView (cutoffKnob);

    // Resonance
    makeLabel (215, 38, 80, "Reso");
    auto resoKnob = new SynthKnobView (CRect (225, 56, 295, 126), this, kResonanceId, 0.0f);
    frame->addView (resoKnob);

    // Fine
    makeLabel (315, 38, 80, "Fine");
    auto fineKnob = new SynthKnobView (CRect (325, 56, 395, 126), this, kFineId, 0.5f);
    frame->addView (fineKnob);

    // --- Waveform Selector ---
    makeLabel (20, 140, 120, "Waveform");
    CCoord btnX = 20;
    CCoord btnY = 158;
    CCoord btnW = 55;
    CCoord btnH = 35;
    CCoord btnGap = 5;

    for (int i = 0; i < 4; i++)
    {
        waveButtons[i] = new WaveformButton (
            CRect (btnX + i * (btnW + btnGap), btnY, btnX + i * (btnW + btnGap) + btnW, btnY + btnH),
            this, i);
        frame->addView (waveButtons[i]);
    }

    // Waveform labels
    const char* waveNames[] = {"Sin", "Saw", "Sqr", "Tri"};
    for (int i = 0; i < 4; i++)
        makeLabel (btnX + i * (btnW + btnGap), btnY + btnH + 2, btnW, waveNames[i]);

    // --- Waveform Display ---
    waveDisplay = new WaveformDisplay (CRect (20, 210, 600, 310));
    frame->addView (waveDisplay);

    // --- Envelope: Attack & Release ---
    makeLabel (30, 325, 80, "Attack");
    auto attackKnob = new SynthKnobView (CRect (40, 343, 110, 413), this, kAttackId, 0.05f);
    frame->addView (attackKnob);

    makeLabel (150, 325, 80, "Release");
    auto releaseKnob = new SynthKnobView (CRect (160, 343, 230, 413), this, kReleaseId, 0.3f);
    frame->addView (releaseKnob);

    // Envelope label
    auto envLabel = new CTextLabel (CRect (250, 370, 400, 390));
    envLabel->setText ("AR Envelope");
    envLabel->setFontColor (CColor (80, 80, 80, 255));
    envLabel->setBackColor (kBgColor);
    envLabel->setFrameColor (kBgColor);
    envLabel->setHoriAlign (kLeftText);
    frame->addView (envLabel);

    // --- Live Oscilloscope ---
    makeLabel (20, 425, 160, "Live Oscilloscope");
    liveScope = new LiveOscilloscopeView (CRect (20, 443, 600, 523));
    frame->addView (liveScope);

    // --- Piano Keyboard (one octave C4-B4) ---
    makeLabel (20, 535, 100, "Keyboard");
    keyboard = new PianoKeyboardView (CRect (20, 553, 600, 650), this, kKeyboardTag);
    frame->addView (keyboard);

    frame->open (parent, platformType);

    // Fix 2: Subclass parent HWND to suppress WM_ERASEBKGND (white flash on Wine).
    // Reaper's FX panel has a white background brush that shows through before
    // VSTGUI's child window completes its first D2D1 paint.
    parentHwnd_ = (HWND)parent;
    origParentWndProc_ = (WNDPROC)GetWindowLongPtrA (parentHwnd_, GWLP_WNDPROC);
    SetPropA (parentHwnd_, "WineSynthEditor", (HANDLE)this);
    SetWindowLongPtrA (parentHwnd_, GWLP_WNDPROC, (LONG_PTR)parentSubclassProc);

    // Under Wine, the initial WM_PAINT arrives before D2D1 is fully
    // initialized, leaving framebuffer garbage visible. Schedule a
    // delayed full redraw to ensure proper rendering.
    CFrame* f = frame;
    Call::later ([f] () { f->invalid (); }, 100);

    // Start live oscilloscope animation
    if (liveScope)
        liveScope->start ();

    // Timer for deferred waveform display + MIDI keyboard highlight (~15 fps)
    displayTimer = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) {
        flushDisplayUpdate ();

        // Poll keyboard note parameter for MIDI highlight feedback
        if (keyboard && controller)
        {
            float noteVal = controller->getParamNormalized (kKeyboardNoteId);
            int noteIdx = (int)(noteVal * 12.0f + 0.5f);

            // Update all keys: set pressed state from MIDI
            for (int i = 0; i < 12; i++)
                keyboard->setNoteState (60 + i, (i + 1) == noteIdx);
        }
    }, 66);

    return true;
}

LRESULT CALLBACK Editor::parentSubclassProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* editor = (Editor*)GetPropA (hwnd, "WineSynthEditor");

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
        RemovePropA (parentHwnd_, "WineSynthEditor");
        parentHwnd_ = nullptr;
        origParentWndProc_ = nullptr;
    }

    if (displayTimer)
    {
        displayTimer->stop ();
        displayTimer = nullptr;
    }

    if (liveScope)
    {
        liveScope->stop ();
        liveScope = nullptr;
    }

    waveDisplay = nullptr;
    for (int i = 0; i < 4; i++)
        waveButtons[i] = nullptr;

    if (frame)
    {
        frame->forget ();
        frame = nullptr;
    }
}

void Editor::flushDisplayUpdate ()
{
    if (displayDirty && waveDisplay)
    {
        waveDisplay->setCutoff (pendingCutoff);
        waveDisplay->setResonance (pendingResonance);
        displayDirty = false;
    }
}

void Editor::valueChanged (CControl* pControl)
{
    if (!controller)
        return;

    int32_t tag = pControl->getTag ();

    // Waveform buttons have internal tags kWaveBtnTagBase + waveType
    if (tag >= kWaveBtnTagBase && tag < kWaveBtnTagBase + kNumWaveforms)
    {
        int waveType = tag - kWaveBtnTagBase;
        selectWaveform (waveType);
        return;
    }

    // GUI keyboard: send note parameter
    if (tag == kKeyboardTag)
    {
        float value = pControl->getValue ();
        controller->beginEdit (kKeyboardNoteId);
        controller->setParamNormalized (kKeyboardNoteId, value);
        controller->performEdit (kKeyboardNoteId, value);
        controller->endEdit (kKeyboardNoteId);
        return;
    }

    // All other controls: forward value to controller
    float value = pControl->getValue ();
    controller->setParamNormalized (tag, value);
    controller->performEdit (tag, value);

    // Update waveform display for filter parameters (delayed to avoid redraw conflicts)
    if (waveDisplay && (tag == kCutoffId || tag == kResonanceId))
    {
        pendingCutoff = (tag == kCutoffId) ? value : pendingCutoff;
        pendingResonance = (tag == kResonanceId) ? value : pendingResonance;
        displayDirty = true;
    }
}

void Editor::selectWaveform (int waveType)
{
    if (waveDisplay)
        waveDisplay->setWaveform (waveType);
    if (liveScope)
        liveScope->setWaveform (waveType);

    if (controller)
    {
        float normValue = (float)waveType / (float)(kNumWaveforms - 1);
        controller->setParamNormalized (kWaveformId, normValue);
        controller->performEdit (kWaveformId, normValue);
    }
}

} // namespace WineSynth
