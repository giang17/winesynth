#pragma once

#include "vstgui/lib/cview.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/controls/icontrollistener.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace TooltipTest {

using namespace VSTGUI;

// --- Color constants ---
static const CColor kBgColor        (25, 25, 30, 255);
static const CColor kKnobFill       (45, 45, 55, 255);
static const CColor kKnobStroke     (80, 80, 100, 255);
static const CColor kKnobIndicator  (0, 200, 220, 255);
static const CColor kLabelColor     (180, 180, 190, 255);

// Tooltip colors — semi-transparent background (alpha 245 ≈ 0.96)
static const CColor kTooltipBg      (30, 30, 35, 245);
static const CColor kTooltipBorder  (0, 200, 220, 200);
static const CColor kTooltipText    (255, 255, 255, 255);

// Forward declaration
class TooltipOverlay;

//------------------------------------------------------------------------
// TooltipOverlay — Offscreen-bitmap-cached tooltip (mimics Serum2/VSTGUI path)
//
// This reproduces the rendering path that triggers the Wine triple-layer
// alpha compositing bug:
//
// 1. Tooltip content is rendered into a COffscreenContext (which internally
//    creates a WIC-backed D2D1 bitmap via CreateBitmapFromWicBitmap).
// 2. The resulting CBitmap is CACHED across frames (not recreated each draw).
// 3. In draw(), the cached bitmap is blitted via context->drawBitmap()
//    (which maps to ID2D1DeviceContext::DrawBitmap internally).
// 4. When VSTGUI's dirty-rect system splits the repaint into multiple
//    rects that overlap the tooltip, DrawBitmap is called multiple times
//    with the SAME cached source bitmap.
// 5. On Wine, the source bitmap and the render target share the same
//    WIC IWICBitmap backing store. After EndDraw, the GPU readback writes
//    accumulated content back into the WIC buffer. The cached source bitmap
//    then contains stale accumulated content on the next frame.
//
// Key differences from the vector-drawing version:
// - Uses COffscreenContext::create() + getBitmap() (WIC-backed)
// - Bitmap is cached in cachedTipBitmap_ (persists across frames)
// - Only re-rendered when text/position changes (not every frame)
// - draw() uses context->drawBitmap() instead of drawGraphicsPath()
//------------------------------------------------------------------------
class TooltipOverlay : public CView
{
public:
    TooltipOverlay (const CRect& frameSize)
        : CView (frameSize)
    {
        setVisible (false);
        setMouseEnabled (false);
        tipWidth_ = 130;
        tipHeight_ = 30;
        memset (text_, 0, sizeof (text_));
    }

    void show (const CPoint& knobCenter, const char* tipText)
    {
        bool textChanged = (strcmp (text_, tipText) != 0);
        strncpy (text_, tipText, sizeof (text_) - 1);
        text_[sizeof (text_) - 1] = '\0';

        // Position tooltip centered above the knob
        tipX_ = knobCenter.x - tipWidth_ * 0.5;
        tipY_ = knobCenter.y - tipHeight_ - 8;

        // Clamp to frame bounds
        auto vs = getViewSize ();
        if (tipX_ < vs.left + 2) tipX_ = vs.left + 2;
        if (tipX_ + tipWidth_ > vs.right - 2) tipX_ = vs.right - 2 - tipWidth_;
        if (tipY_ < vs.top + 2) tipY_ = vs.top + 2;

        // Re-render the offscreen bitmap when text changes.
        // IMPORTANT: We do NOT invalidate the cached bitmap every frame —
        // this is the key to reproducing the bug. The bitmap persists and
        // if Wine's WIC backing store gets mutated by GPU readback, the
        // cached bitmap will contain accumulated content.
        if (textChanged || !cachedTipBitmap_)
            renderOffscreen ();

        setVisible (true);
        invalid ();
    }

    void hide ()
    {
        if (isVisible ())
        {
            setVisible (false);
            invalid ();
            // NOTE: We intentionally do NOT clear cachedTipBitmap_ here.
            // The bitmap stays cached so that when the tooltip reappears,
            // it reuses the same WIC-backed bitmap — maximizing the chance
            // of hitting the accumulation bug.
        }
    }

    void draw (CDrawContext* context) override
    {
        if (!isVisible () || !cachedTipBitmap_)
            return;

        // Blit the cached offscreen bitmap at the tooltip position.
        // This is the equivalent of Serum2's DrawBitmap call path.
        // Under Wine's dirty-rect system, this draw() may be called
        // multiple times per frame (once per overlapping dirty rect),
        // each time with a different clip rect but the SAME source bitmap.
        CRect destRect (tipX_, tipY_, tipX_ + tipWidth_, tipY_ + tipHeight_);
        context->drawBitmap (cachedTipBitmap_, destRect, CPoint (0, 0), 1.0f);
    }

    CLASS_METHODS (TooltipOverlay, CView)

private:
    void renderOffscreen ()
    {
        // Create an offscreen context (WIC-backed D2D1 bitmap internally).
        // This mimics VSTGUI's CBitmapPixelAccess / D2DBitmapCache path
        // that Serum2 uses for its 9-slice tooltip background.
        auto offscreen = COffscreenContext::create (CPoint (tipWidth_, tipHeight_), 1.0);
        if (!offscreen)
            return;

        offscreen->beginDraw ();

        // Clear to fully transparent — the tooltip bg is semi-transparent
        // so the cleared area will show through.
        offscreen->setFillColor (CColor (0, 0, 0, 0));
        offscreen->drawRect (CRect (0, 0, tipWidth_, tipHeight_), kDrawFilled);

        CRect tipRect (0, 0, tipWidth_, tipHeight_);
        CCoord radius = 6.0;

        // --- Filled rounded rect background (semi-transparent, alpha ≈ 0.96) ---
        auto bgPath = owned (offscreen->createGraphicsPath ());
        if (bgPath)
        {
            bgPath->addRoundRect (tipRect, radius);
            offscreen->setFillColor (kTooltipBg);
            offscreen->drawGraphicsPath (bgPath, CDrawContext::kPathFilled);
        }

        // --- Cyan border ---
        auto borderPath = owned (offscreen->createGraphicsPath ());
        if (borderPath)
        {
            borderPath->addRoundRect (tipRect, radius);
            offscreen->setFrameColor (kTooltipBorder);
            offscreen->setLineWidth (1.0);
            offscreen->drawGraphicsPath (borderPath, CDrawContext::kPathStroked);
        }

        // --- White text ---
        auto font = makeOwned<CFontDesc> ("Arial", 12, kBoldFace);
        offscreen->setFont (font);
        offscreen->setFontColor (kTooltipText);
        offscreen->drawString (text_, tipRect, kCenterText);

        offscreen->endDraw ();

        // Cache the bitmap. This CBitmap wraps a WIC IWICBitmap internally.
        // On Wine, this same WIC bitmap may be used as both the source for
        // DrawBitmap AND as the backing store that receives GPU readback
        // after EndDraw — which is the root cause of the accumulation bug.
        cachedTipBitmap_ = offscreen->getBitmap ();
    }

    CCoord tipX_ = 0;
    CCoord tipY_ = 0;
    CCoord tipWidth_ = 130;
    CCoord tipHeight_ = 30;
    char text_[128];
    SharedPointer<CBitmap> cachedTipBitmap_;
};

//------------------------------------------------------------------------
// SynthKnobView — simplified drag-only knob with tooltip integration
//------------------------------------------------------------------------
class SynthKnobView : public CControl
{
public:
    SynthKnobView (const CRect& size, IControlListener* listener,
                   int32_t tag, float defaultValue)
        : CControl (size, listener, tag)
    {
        setValue (defaultValue);
        setMin (0.f);
        setMax (1.f);
        setWantsFocus (true);
    }

    void setTooltipOverlay (TooltipOverlay* overlay) { tooltipOverlay_ = overlay; }

    void draw (CDrawContext* context) override
    {
        auto r = getViewSize ();

        // Background
        context->setFillColor (kBgColor);
        context->drawRect (r, kDrawFilled);

        // Knob circle
        auto cx = r.left + r.getWidth () * 0.5;
        auto cy = r.top + r.getHeight () * 0.5;
        auto radius = (std::min) (r.getWidth (), r.getHeight ()) * 0.42;

        // Fill
        CRect knobRect (cx - radius, cy - radius, cx + radius, cy + radius);
        context->setFillColor (kKnobFill);
        context->drawEllipse (knobRect, kDrawFilled);

        // Stroke
        context->setFrameColor (kKnobStroke);
        context->setLineWidth (1.5);
        context->drawEllipse (knobRect, kDrawStroked);

        // Arc track (background)
        auto arcRadius = radius + 5;
        auto path = owned (context->createGraphicsPath ());
        if (path)
        {
            CRect arcRect (cx - arcRadius, cy - arcRadius, cx + arcRadius, cy + arcRadius);
            path->addArc (arcRect, -225, 45, true);
            context->setFrameColor (CColor (50, 50, 60, 255));
            context->setLineWidth (3.0);
            context->drawGraphicsPath (path, CDrawContext::kPathStroked);
        }

        // Arc value
        float val = getValue ();
        {
            auto vpath = owned (context->createGraphicsPath ());
            if (vpath)
            {
                double endAngle = -225.0 + val * 270.0;
                if (endAngle < -225.0) endAngle = -225.0;
                CRect arcRect (cx - arcRadius, cy - arcRadius, cx + arcRadius, cy + arcRadius);
                vpath->addArc (arcRect, -225, endAngle, true);
                context->setFrameColor (kKnobIndicator);
                context->setLineWidth (3.0);
                context->drawGraphicsPath (vpath, CDrawContext::kPathStroked);
            }
        }

        // Indicator line
        {
            double angle = (-225.0 + val * 270.0) * M_PI / 180.0;
            auto ix = cx + (radius - 8) * cos (angle);
            auto iy = cy + (radius - 8) * sin (angle);
            auto ox = cx + (radius - 1) * cos (angle);
            auto oy = cy + (radius - 1) * sin (angle);
            context->setFrameColor (kKnobIndicator);
            context->setLineWidth (2.5);
            context->drawLine (CPoint (ix, iy), CPoint (ox, oy));
        }
    }

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override
    {
        if (!(buttons & kLButton))
            return kMouseEventNotHandled;
        lastY_ = where.y;
        beginEdit ();
        showTooltip ();
        return kMouseEventHandled;
    }

    CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override
    {
        if (!(buttons & kLButton))
            return kMouseEventNotHandled;

        float delta = (float)(lastY_ - where.y) * 0.005f;
        float newVal = getValue () + delta;
        if (newVal < 0.f) newVal = 0.f;
        if (newVal > 1.f) newVal = 1.f;
        setValue (newVal);
        valueChanged ();
        lastY_ = where.y;

        showTooltip ();
        return kMouseEventHandled;
    }

    CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override
    {
        endEdit ();
        if (tooltipOverlay_)
            tooltipOverlay_->hide ();
        return kMouseEventHandled;
    }

    CLASS_METHODS (SynthKnobView, CControl)

private:
    void showTooltip ()
    {
        if (!tooltipOverlay_)
            return;

        char buf[64];
        snprintf (buf, sizeof (buf), "Value: %.2f", getValue ());

        auto r = getViewSize ();
        CPoint center (r.left + r.getWidth () * 0.5, r.top);
        tooltipOverlay_->show (center, buf);
    }

    CCoord lastY_ = 0;
    TooltipOverlay* tooltipOverlay_ = nullptr;
};

} // namespace TooltipTest