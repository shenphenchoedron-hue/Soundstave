// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpectrogramDisplay.h"
#include "../model/NoteModel.h"
#include <cmath>
#include <algorithm>

namespace Soundstave {

static constexpr juce::uint32 kSpectroBackground = 0xFF0A0C20;

SpectrogramDisplay::SpectrogramDisplay()
{
    setOpaque(true);
    buildPalette();
}

SpectrogramDisplay::~SpectrogramDisplay() = default;

void SpectrogramDisplay::buildPalette()
{
    // Dark navy → blue → purple → cyan → green → yellow → orange
    struct Stop { float pos; float r, g, b; };
    static const Stop stops[] = {
        { 0.00f,  10,  12,  32 },
        { 0.16f,  22,  34, 112 },
        { 0.34f,  96,  42, 168 },
        { 0.52f,  24, 156, 214 },
        { 0.68f,  44, 196,  96 },
        { 0.84f, 244, 222,  48 },
        { 1.00f, 255, 140,  24 },
    };
    constexpr int nStops = (int)(sizeof(stops) / sizeof(stops[0]));

    for (int i = 0; i < 256; ++i)
    {
        const float v = (float)i / 255.0f;
        int k = 0;
        while (k < nStops - 2 && v > stops[k + 1].pos) ++k;
        const float t = juce::jlimit(0.0f, 1.0f, (v - stops[k].pos) / (stops[k + 1].pos - stops[k].pos));
        auto lerp = [t](float a, float b) { return (juce::uint8)juce::jlimit(0.0f, 255.0f, a + t * (b - a)); };
        palette[(size_t)i].setARGB(255, lerp(stops[k].r, stops[k + 1].r),
                                        lerp(stops[k].g, stops[k + 1].g),
                                        lerp(stops[k].b, stops[k + 1].b));
    }
}

void SpectrogramDisplay::setData(SpectrogramPtr d)
{
    data = std::move(d);
    buildCoarseLevel();
    colWidth = 0;
    imageDirty = true;
    repaint();
}

void SpectrogramDisplay::clearData()
{
    data.reset();
    coarse.clear();
    coarseFrames = 0;
    colWidth = 0;
    imageDirty = true;
    repaint();
}

void SpectrogramDisplay::buildCoarseLevel()
{
    coarse.clear();
    coarseFrames = 0;
    if (data == nullptr || data->numFrames < kCoarse * 4) return;

    const int nb = data->numBins;
    coarseFrames = (data->numFrames + kCoarse - 1) / kCoarse;
    coarse.assign((size_t)coarseFrames * (size_t)nb, data->floorDb);
    for (int f = 0; f < data->numFrames; ++f)
    {
        float* dst = coarse.data() + (size_t)(f / kCoarse) * (size_t)nb;
        const float* src = data->frame(f);
        for (int k = 0; k < nb; ++k) dst[k] = std::max(dst[k], src[k]);
    }
}

void SpectrogramDisplay::setViewState(const ViewState& v)
{
    if (juce::exactlyEqual(v.startTimeSec, vs.startTimeSec) && juce::exactlyEqual(v.endTimeSec, vs.endTimeSec)
        && v.midiLow == vs.midiLow && v.midiHigh == vs.midiHigh)
        return;
    vs = v;
    imageDirty = true;
    repaint();
}

void SpectrogramDisplay::setPlayheadPosition(double t)
{
    if (juce::exactlyEqual(t, playheadSec)) return;
    const int oldX = (int)vs.timeToX(playheadSec, getWidth());
    const int newX = (int)vs.timeToX(t, getWidth());
    playheadSec = t;
    repaint(oldX - 2, 0, 5, getHeight());
    repaint(newX - 2, 0, 5, getHeight());
}

void SpectrogramDisplay::setLoopRegion(double s, double e, bool enabled)
{
    loopStart = s; loopEnd = e; loopOn = enabled;
    repaint();
}

void SpectrogramDisplay::setDynamicRange(float rangeDb)
{
    dynamicRangeDb = juce::jlimit(20.0f, 140.0f, rangeDb);
    imageDirty = true;
    repaint();
}

void SpectrogramDisplay::setHintText(const juce::String& text)
{
    hint = text;
    repaint();
}

void SpectrogramDisplay::resized()
{
    imageDirty = true;
}

// -------------------------------------------------------------------------
// Rendering
//  - pitch axis: identical lane mapping as keyboard and notes (pitchToY)
//  - zoomed out: max over all frames/bins that fall into a pixel (no aliasing)
//  - zoomed in : linear interpolation between frames/bins (no blocks)
// -------------------------------------------------------------------------
void SpectrogramDisplay::computeColumns(int w)
{
    const auto& d = *data;
    const int nb = d.numBins;
    const double fd = d.frameDuration();
    const double lastTime = d.frameTime(d.numFrames - 1) + fd;

    columns.assign((size_t)w * (size_t)nb, d.floorDb);
    columnOutside.assign((size_t)w, false);

    for (int px = 0; px < w; ++px)
    {
        float* column = columns.data() + (size_t)px * (size_t)nb;
        const double t0 = vs.xToTime((double)px, w);
        const double t1 = vs.xToTime((double)(px + 1), w);
        if (t1 < 0.0 || t0 > lastTime) { columnOutside[(size_t)px] = true; continue; }

        const int f0 = (int)std::ceil(t0 / fd);
        const int f1 = (int)std::floor(t1 / fd - 1e-9);

        if (f1 >= f0)
        {
            // Several frames in this pixel → max-pool (via the coarse level when possible)
            int a = juce::jlimit(0, d.numFrames - 1, f0);
            const int b = juce::jlimit(0, d.numFrames - 1, f1);
            std::copy(d.frame(a), d.frame(a) + nb, column);
            for (int f = a + 1; f <= b; )
            {
                const float* fr;
                if (coarseFrames > 0 && f % kCoarse == 0 && f + kCoarse - 1 <= b)
                {
                    fr = coarse.data() + (size_t)(f / kCoarse) * (size_t)nb;
                    f += kCoarse;
                }
                else
                {
                    fr = d.frame(f);
                    ++f;
                }
                for (int k = 0; k < nb; ++k) column[k] = std::max(column[k], fr[k]);
            }
        }
        else
        {
            // Between two frames → interpolate
            const double pos = (t0 + t1) * 0.5 / fd;
            const int i0 = juce::jlimit(0, d.numFrames - 1, (int)std::floor(pos));
            const int i1 = std::min(d.numFrames - 1, i0 + 1);
            const float a = (float)juce::jlimit(0.0, 1.0, pos - std::floor(pos));
            const float* A = d.frame(i0);
            const float* B = d.frame(i1);
            for (int k = 0; k < nb; ++k) column[k] = A[k] + (B[k] - A[k]) * a;
        }
    }

    colStart = vs.startTimeSec;
    colEnd   = vs.endTimeSec;
    colWidth = w;
}

// -------------------------------------------------------------------------
// Rendering
//  - pitch axis: identical lane mapping as keyboard and notes (pitchToY)
//  - zoomed out: max over all frames/bins that fall into a pixel (no aliasing)
//  - zoomed in : linear interpolation between frames/bins (no blocks)
// -------------------------------------------------------------------------
void SpectrogramDisplay::rebuildImage()
{
    imageDirty = false;
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;

    if (!image.isValid() || image.getWidth() != w || image.getHeight() != h)
        image = juce::Image(juce::Image::RGB, w, h, false);

    juce::Image::BitmapData bmp(image, juce::Image::BitmapData::writeOnly);
    juce::PixelRGB background;
    background.setARGB(255, 10, 12, 32);

    if (data == nullptr || data->numFrames == 0)
    {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                *reinterpret_cast<juce::PixelRGB*>(bmp.getPixelPointer(x, y)) = background;
        return;
    }

    if (colWidth != w || !juce::exactlyEqual(colStart, vs.startTimeSec) || !juce::exactlyEqual(colEnd, vs.endTimeSec))
        computeColumns(w);

    const auto& d = *data;
    const int nb = d.numBins;
    const int lo = vs.midiLow, hi = vs.midiHigh;
    const float bottom = d.maxDb - dynamicRangeDb;
    const float scale = 255.0f / dynamicRangeDb;

    for (int py = 0; py < h; ++py)
    {
        const float mTop = yToPitch((float)py,       lo, hi, h);
        const float mBot = yToPitch((float)(py + 1), lo, hi, h);
        const float bl = (float)d.freqToBin(Note::midiToFreq((double)mBot));
        const float bh = (float)d.freqToBin(Note::midiToFreq((double)mTop));
        const bool  aboveRange = bl >= (float)(nb - 1);   // not stored (above maxFreqHz)
        const bool  narrow = bh - bl < 1.0f;
        const float centre = juce::jlimit(0.0f, (float)(nb - 1), (bl + bh) * 0.5f);
        const int   c0 = (int)centre, c1 = std::min(nb - 1, c0 + 1);
        const float cf = centre - (float)c0;
        const int   k0 = juce::jlimit(0, nb - 1, (int)std::floor(bl));
        const int   k1 = juce::jlimit(0, nb - 1, (int)std::ceil(bh));

        auto* row = reinterpret_cast<juce::PixelRGB*>(bmp.getLinePointer(py));
        for (int px = 0; px < w; ++px)
        {
            auto& pix = *reinterpret_cast<juce::PixelRGB*>(reinterpret_cast<juce::uint8*>(row) + px * bmp.pixelStride);
            if (aboveRange || columnOutside[(size_t)px]) { pix = background; continue; }

            const float* column = columns.data() + (size_t)px * (size_t)nb;
            float db;
            if (narrow)
                db = column[c0] + (column[c1] - column[c0]) * cf;
            else
            {
                db = column[k0];
                for (int k = k0 + 1; k <= k1; ++k) db = std::max(db, column[k]);
            }
            pix = palette[(size_t)juce::jlimit(0, 255, (int)((db - bottom) * scale))];
        }
    }
}

void SpectrogramDisplay::paint(juce::Graphics& g)
{
    if (imageDirty) rebuildImage();

    if (image.isValid())
        g.drawImageAt(image, 0, 0);
    else
        g.fillAll(juce::Colour(kSpectroBackground));

    const int w = getWidth(), h = getHeight();

    if (loopStart >= 0.0 && loopEnd > loopStart)
    {
        const float x1 = (float)vs.timeToX(loopStart, w);
        const float x2 = (float)vs.timeToX(loopEnd, w);
        g.setColour(juce::Colours::white.withAlpha(loopOn ? 0.07f : 0.03f));
        g.fillRect(x1, 0.0f, x2 - x1, (float)h);
        g.setColour(juce::Colour(Colours::Accent).withAlpha(loopOn ? 0.8f : 0.3f));
        g.drawVerticalLine((int)x1, 0.0f, (float)h);
        g.drawVerticalLine((int)x2, 0.0f, (float)h);
    }

    if (hint.isNotEmpty())
    {
        auto box = getLocalBounds().withSizeKeepingCentre(std::min(w - 40, 520), 80).toFloat();
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.fillRoundedRectangle(box, 12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(SoundstaveLookAndFeel::getDefaultFont(17.0f));
        g.drawFittedText(hint, box.toNearestInt().reduced(16, 8), juce::Justification::centred, 2);
    }

    const int px = (int)vs.timeToX(playheadSec, w);
    if (px >= 0 && px < w)
    {
        g.setColour(juce::Colour(Colours::Playhead));
        g.fillRect(px, 0, 2, h);
    }
}

void SpectrogramDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (onWheel) onWheel(e, w);
}

void SpectrogramDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (onSeek) onSeek(vs.xToTime(e.position.x, getWidth()));
}

} // namespace Soundstave
