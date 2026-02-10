#include "editor.h"
#include "pluginparamids.h"
#include "controls.h"

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ctextlabel.h"
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
    // Disable DirectComposition (not implemented in Wine)
    if (auto win32Factory = getPlatformFactory ().asWin32Factory ())
        win32Factory->disableDirectComposition ();

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
    makeLabel (30, 38, 80, "Gain");
    auto gainKnob = new SynthKnobView (CRect (40, 56, 110, 126), this, kGainId, 0.5f);
    frame->addView (gainKnob);

    // Frequency
    makeLabel (150, 38, 100, "Frequency");
    auto freqKnob = new SynthKnobView (CRect (165, 56, 235, 126), this, kFrequencyId, 0.5f);
    frame->addView (freqKnob);

    // Fine
    makeLabel (280, 38, 80, "Fine");
    auto fineKnob = new SynthKnobView (CRect (290, 56, 360, 126), this, kFineId, 0.5f);
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
            this, kWaveformId, i, i == 0);
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

    frame->open (parent, platformType);
    return true;
}

void PLUGIN_API Editor::close ()
{
    waveDisplay = nullptr;
    for (int i = 0; i < 4; i++)
        waveButtons[i] = nullptr;

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

    // Update waveform display and button selection when waveform changes
    if (tag == kWaveformId)
    {
        int waveType = (int)(value * (kNumWaveforms - 1) + 0.5f);
        updateWaveformSelection (waveType);
    }
}

void Editor::updateWaveformSelection (int waveType)
{
    for (int i = 0; i < 4; i++)
    {
        if (waveButtons[i])
            waveButtons[i]->setSelected (i == waveType);
    }
    if (waveDisplay)
        waveDisplay->setWaveform (waveType);
}

} // namespace WineSynth
