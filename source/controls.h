#pragma once

#include "pluginparamids.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/ccolor.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace VSTGUI;

namespace WineSynth {

// Color palette
static const CColor kBgColor        (30, 30, 30, 255);
static const CColor kKnobFill       (70, 70, 70, 255);
static const CColor kKnobStroke     (180, 180, 180, 255);
static const CColor kKnobIndicator  (255, 255, 255, 255);
static const CColor kLabelColor     (180, 180, 180, 255);
static const CColor kDisplayBg      (20, 25, 20, 255);
static const CColor kWaveformColor  (100, 255, 100, 255);
static const CColor kButtonBg       (50, 50, 50, 255);
static const CColor kButtonStroke   (100, 100, 100, 255);
static const CColor kActiveStroke   (100, 255, 100, 255);

//------------------------------------------------------------------------
// SynthKnobView — vector-drawn knob with arc value indicator
//------------------------------------------------------------------------
class SynthKnobView : public CControl
{
public:
    SynthKnobView (const CRect& r, IControlListener* listener, int32_t tag,
                   float defaultVal = 0.5f)
        : CControl (r, listener, tag)
    {
        setMin (0.f);
        setMax (1.f);
        setValue (defaultVal);
        setDefaultValue (defaultVal);
    }

    void draw (CDrawContext* context) override
    {
        context->setDrawMode (kAntiAliasing);
        auto r = getViewSize ();

        auto cx = r.getCenter ().x;
        auto cy = r.getCenter ().y;
        auto radius = std::min (r.getWidth (), r.getHeight ()) * 0.38;

        // Knob circle
        CRect knobRect (cx - radius, cy - radius, cx + radius, cy + radius);
        context->setFillColor (kKnobFill);
        context->drawEllipse (knobRect, kDrawFilled);
        context->setFrameColor (kKnobStroke);
        context->setLineWidth (1.5);
        context->drawEllipse (knobRect, kDrawStroked);

        // Arc track (background, full range)
        auto arcRadius = radius + 6;
        if (auto path = owned (context->createGraphicsPath ()))
        {
            CRect arcRect (cx - arcRadius, cy - arcRadius, cx + arcRadius, cy + arcRadius);
            path->addArc (arcRect, 135.0, 405.0, true);
            context->setFrameColor (CColor (50, 50, 50, 255));
            context->setLineWidth (3.0);
            context->drawGraphicsPath (path, CDrawContext::kPathStroked);
        }

        // Arc value indicator
        if (getValue () > 0.001f)
        {
            if (auto path = owned (context->createGraphicsPath ()))
            {
                double endAngle = 135.0 + getValue () * 270.0;
                CRect arcRect (cx - arcRadius, cy - arcRadius, cx + arcRadius, cy + arcRadius);
                path->addArc (arcRect, 135.0, endAngle, true);
                context->setFrameColor (kWaveformColor);
                context->setLineWidth (3.0);
                context->drawGraphicsPath (path, CDrawContext::kPathStroked);
            }
        }

        // Value indicator line
        double angle = (0.75 + getValue () * 1.5) * M_PI;
        auto ix = cx + radius * 0.55 * cos (angle);
        auto iy = cy + radius * 0.55 * sin (angle);
        auto ox = cx + radius * 0.85 * cos (angle);
        auto oy = cy + radius * 0.85 * sin (angle);
        context->setFrameColor (kKnobIndicator);
        context->setLineWidth (2.0);
        context->drawLine (CPoint (ix, iy), CPoint (ox, oy));

        setDirty (false);
    }

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override
    {
        if (buttons.isLeftButton ())
        {
            beginEdit ();
            lastY = where.y;
            return kMouseEventHandled;
        }
        return kMouseEventNotHandled;
    }

    CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override
    {
        if (buttons.isLeftButton ())
        {
            float delta = (float)(lastY - where.y) * 0.005f;
            float newVal = getValue () + delta;
            if (newVal < 0.f) newVal = 0.f;
            if (newVal > 1.f) newVal = 1.f;
            setValue (newVal);
            valueChanged ();
            invalid ();
            lastY = where.y;
            return kMouseEventHandled;
        }
        return kMouseEventNotHandled;
    }

    CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override
    {
        endEdit ();
        return kMouseEventHandled;
    }

    CLASS_METHODS (SynthKnobView, CControl)
private:
    CCoord lastY = 0;
};

//------------------------------------------------------------------------
// WaveformButton — simple clickable waveform selector
// Each button has a unique tag (kWaveBtnTagBase + waveType).
// Selection state is managed entirely by the Editor.
//------------------------------------------------------------------------
enum { kWaveBtnTagBase = 1000 };

class WaveformButton : public CControl
{
public:
    WaveformButton (const CRect& r, IControlListener* listener, int waveType)
        : CControl (r, listener, kWaveBtnTagBase + waveType)
        , waveType (waveType)
    {
    }

    int getWaveType () const { return waveType; }

    void draw (CDrawContext* context) override
    {
        context->setDrawMode (kAntiAliasing);
        auto r = getViewSize ();

        // Always draw the same: dark bg + gray border + gray waveform
        context->setFillColor (kButtonBg);
        context->drawRect (r, kDrawFilled);
        context->setFrameColor (kButtonStroke);
        context->setLineWidth (1.0);
        context->drawRect (r, kDrawStroked);

        // Waveform preview
        if (auto path = owned (context->createGraphicsPath ()))
        {
            auto inset = 6.0;
            auto left = r.left + inset;
            auto right = r.right - inset;
            auto top = r.top + inset;
            auto bottom = r.bottom - inset;
            auto cy = (top + bottom) * 0.5;
            auto amp = (bottom - top) * 0.4;
            auto w = right - left;
            int segs = 32;

            path->beginSubpath (CPoint (left, cy));
            for (int i = 1; i <= segs; i++)
            {
                double t = (double)i / segs;
                double phase = t * 2.0 * M_PI;
                double sample = 0.0;

                switch (waveType)
                {
                    case kWaveSine:    sample = sin (phase); break;
                    case kWaveSaw:     sample = 2.0 * (t - 0.5); break;
                    case kWaveSquare:  sample = t < 0.5 ? 1.0 : -1.0; break;
                    case kWaveTriangle: sample = 4.0 * fabs (t - 0.5) - 1.0; break;
                }

                path->addLine (CPoint (left + t * w, cy - sample * amp));
            }

            context->setFrameColor (kLabelColor);
            context->setLineWidth (1.5);
            context->drawGraphicsPath (path, CDrawContext::kPathStroked);
        }

        setDirty (false);
    }

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override
    {
        if (buttons.isLeftButton ())
        {
            // Toggle between 0/1 so valueChanged always fires
            beginEdit ();
            setValue (getValue () < 0.5f ? 1.f : 0.f);
            valueChanged ();
            endEdit ();
            return kMouseEventHandled;
        }
        return kMouseEventNotHandled;
    }

    CLASS_METHODS (WaveformButton, CControl)
private:
    int waveType;
};

//------------------------------------------------------------------------
// WaveformDisplay — live waveform visualization
//------------------------------------------------------------------------
class WaveformDisplay : public CView
{
public:
    WaveformDisplay (const CRect& size)
        : CView (size) {}

    void setWaveform (int type)
    {
        if (waveType != type)
        {
            waveType = type;
            invalid ();
        }
    }

    void draw (CDrawContext* context) override
    {
        context->setDrawMode (kAntiAliasing);
        auto r = getViewSize ();

        // Background
        context->setFillColor (kDisplayBg);
        context->drawRect (r, kDrawFilled);
        context->setFrameColor (CColor (40, 50, 40, 255));
        context->setLineWidth (1.0);
        context->drawRect (r, kDrawStroked);

        // Center line (dim)
        auto cy = r.getCenter ().y;
        context->setFrameColor (CColor (40, 60, 40, 255));
        context->setLineWidth (0.5);
        context->drawLine (CPoint (r.left + 5, cy), CPoint (r.right - 5, cy));

        // Waveform
        if (auto path = owned (context->createGraphicsPath ()))
        {
            auto inset = 10.0;
            auto left = r.left + inset;
            auto right = r.right - inset;
            auto amp = r.getHeight () * 0.38;
            auto w = right - left;
            int segs = 200;
            double periods = 3.0;

            path->beginSubpath (CPoint (left, cy));
            for (int i = 1; i <= segs; i++)
            {
                double t = (double)i / segs;
                double phase = t * periods * 2.0 * M_PI;
                double tmod = fmod (t * periods, 1.0);
                double sample = 0.0;

                switch (waveType)
                {
                    case kWaveSine:
                        sample = sin (phase);
                        break;
                    case kWaveSaw:
                        sample = 2.0 * tmod - 1.0;
                        break;
                    case kWaveSquare:
                        sample = tmod < 0.5 ? 1.0 : -1.0;
                        break;
                    case kWaveTriangle:
                        sample = 4.0 * fabs (tmod - 0.5) - 1.0;
                        break;
                }

                path->addLine (CPoint (left + t * w, cy - sample * amp));
            }

            context->setFrameColor (kWaveformColor);
            context->setLineWidth (2.0);
            context->drawGraphicsPath (path, CDrawContext::kPathStroked);
        }

        setDirty (false);
    }

private:
    int waveType = kWaveSine;
};

} // namespace WineSynth
