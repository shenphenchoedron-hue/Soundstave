// SPDX-License-Identifier: GPL-3.0-or-later
#include "WaveformDisplay.h"
#include <cmath>

namespace Soundstave {

WaveformDisplay::WaveformDisplay()
{
    setOpaque(true);
    setMouseCursor(juce::MouseCursor::IBeamCursor);
}

void WaveformDisplay::setAudioBuffer(const juce::AudioBuffer<float>* buf, double sr)
{
    buffer = buf;
    sampleRate = sr;
    bucketMin.clear();
    bucketMax.clear();
    if (buffer != nullptr && buffer->getNumSamples() > 0)
    {
        const int total = buffer->getNumSamples();
        const int nBuckets = (total + kBucket - 1) / kBucket;
        bucketMin.assign((size_t)nBuckets, 0.0f);
        bucketMax.assign((size_t)nBuckets, 0.0f);
        for (int i = 0; i < nBuckets; ++i)
        {
            const int s0 = i * kBucket, n = std::min(kBucket, total - s0);
            float mn = 0.0f, mx = 0.0f;
            for (int ch = 0; ch < buffer->getNumChannels(); ++ch)
            {
                auto r = juce::FloatVectorOperations::findMinAndMax(buffer->getReadPointer(ch, s0), n);
                mn = std::min(mn, r.getStart());
                mx = std::max(mx, r.getEnd());
            }
            bucketMin[(size_t)i] = mn;
            bucketMax[(size_t)i] = mx;
        }
    }
    peaksDirty = true;
    repaint();
}

void WaveformDisplay::clearAudio()
{
    buffer = nullptr;
    bucketMin.clear();
    bucketMax.clear();
    peaksMin.clear();
    peaksMax.clear();
    loopStartSec = loopEndSec = -1.0;
    repaint();
}

void WaveformDisplay::setViewState(const ViewState& v)
{
    if (juce::exactlyEqual(v.startTimeSec, vs.startTimeSec) && juce::exactlyEqual(v.endTimeSec, vs.endTimeSec)) { vs = v; return; }
    vs = v;
    peaksDirty = true;
    repaint();
}

void WaveformDisplay::setPlayheadPosition(double t)
{
    if (juce::exactlyEqual(t, playheadSec)) return;
    const int oldX = (int)vs.timeToX(playheadSec, getWidth());
    const int newX = (int)vs.timeToX(t, getWidth());
    playheadSec = t;
    repaint(oldX - 7, 0, 15, getHeight());
    repaint(newX - 7, 0, 15, getHeight());
}

void WaveformDisplay::setLoopRegion(double s, double e, bool enabled)
{
    loopStartSec = s; loopEndSec = e; loopOn = enabled;
    repaint();
}

void WaveformDisplay::resized() { peaksDirty = true; }

void WaveformDisplay::rebuildPeaks()
{
    peaksDirty = false;
    const int w = getWidth();
    peaksMin.assign((size_t)std::max(0, w), 0.0f);
    peaksMax.assign((size_t)std::max(0, w), 0.0f);
    if (buffer == nullptr || buffer->getNumSamples() == 0 || w <= 0) return;

    const int total = buffer->getNumSamples();
    const int numCh = buffer->getNumChannels();

    for (int px = 0; px < w; ++px)
    {
        int s0 = (int)(vs.xToTime((double)px, w) * sampleRate);
        int s1 = (int)(vs.xToTime((double)(px + 1), w) * sampleRate);
        if (s1 <= 0 || s0 >= total) continue;
        s0 = juce::jlimit(0, total - 1, s0);
        s1 = juce::jlimit(s0 + 1, total, s1);

        float mn = 0.0f, mx = 0.0f;
        if (s1 - s0 >= kBucket * 2 && !bucketMax.empty())
        {
            const int b0 = s0 / kBucket;
            const int b1 = std::min((int)bucketMax.size() - 1, (s1 - 1) / kBucket);
            for (int b = b0; b <= b1; ++b)
            {
                mn = std::min(mn, bucketMin[(size_t)b]);
                mx = std::max(mx, bucketMax[(size_t)b]);
            }
        }
        else
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto range = juce::FloatVectorOperations::findMinAndMax(buffer->getReadPointer(ch, s0), s1 - s0);
                mn = std::min(mn, range.getStart());
                mx = std::max(mx, range.getEnd());
            }
        }
        peaksMin[(size_t)px] = mn;
        peaksMax[(size_t)px] = mx;
    }
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    if (peaksDirty) rebuildPeaks();

    const int w = getWidth(), h = getHeight();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFDFEFF), 0.0f, 0.0f,
                                           juce::Colour(0xFFEFF3F9), 0.0f, (float)h, false));
    g.fillAll();

    if (loopStartSec >= 0.0 && loopEndSec > loopStartSec)
    {
        const float x1 = (float)vs.timeToX(loopStartSec, w);
        const float x2 = (float)vs.timeToX(loopEndSec, w);
        g.setColour(juce::Colour(Colours::AccentDim).withAlpha(loopOn ? 0.55f : 0.22f));
        g.fillRect(x1, 0.0f, x2 - x1, (float)h);
        g.setColour(juce::Colour(Colours::Accent).withAlpha(loopOn ? 1.0f : 0.4f));
        g.fillRect(x1 - 1.0f, 0.0f, 2.0f, (float)h);
        g.fillRect(x2 - 1.0f, 0.0f, 2.0f, (float)h);
    }

    const float mid = (float)h * 0.5f;
    g.setColour(juce::Colour(Colours::GridLine));
    g.drawHorizontalLine((int)mid, 0.0f, (float)w);

    if (buffer == nullptr)
    {
        g.setColour(juce::Colour(Colours::TextSecondary));
        g.setFont(SoundstaveLookAndFeel::getDefaultFont(13.0f));
        g.drawText(U("Ingen lydfil indlæst"), getLocalBounds(), juce::Justification::centred);
    }
    else if (!peaksMax.empty())
    {
        const float amp = mid * 0.92f;
        juce::Path p;
        p.startNewSubPath(0.0f, mid - peaksMax[0] * amp);
        for (int x = 1; x < (int)peaksMax.size(); ++x) p.lineTo((float)x, mid - peaksMax[(size_t)x] * amp);
        for (int x = (int)peaksMin.size() - 1; x >= 0; --x) p.lineTo((float)x, mid - peaksMin[(size_t)x] * amp - 0.5f);
        p.closeSubPath();

        g.setGradientFill(juce::ColourGradient(juce::Colour(Colours::Waveform).withAlpha(0.95f), 0.0f, 0.0f,
                                               juce::Colour(Colours::Waveform).withAlpha(0.55f), 0.0f, mid, true));
        g.fillPath(p);
    }

    const int px = (int)vs.timeToX(playheadSec, w);
    if (px >= -6 && px < w + 6)
    {
        g.setColour(juce::Colour(Colours::Playhead));
        g.fillRect(px, 0, 2, h);
        juce::Path tri;
        tri.addTriangle((float)px - 5.0f, 0.0f, (float)px + 7.0f, 0.0f, (float)px + 1.0f, 7.0f);
        g.fillPath(tri);
    }

    g.setColour(juce::Colour(Colours::Border));
    g.drawHorizontalLine(h - 1, 0.0f, (float)w);
}

double WaveformDisplay::timeAt(const juce::MouseEvent& e, bool snapIt) const
{
    const double len = buffer != nullptr ? (double)buffer->getNumSamples() / sampleRate : 0.0;
    double t = juce::jlimit(0.0, len, vs.xToTime(e.position.x, getWidth()));
    if (snapIt && snapFn && !e.mods.isAltDown()) t = juce::jlimit(0.0, len, snapFn(t));
    return t;
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (buffer == nullptr) return;
    dragger.mouseDown(timeAt(e, true), e.position.x,
                      (float)vs.timeToX(loopStartSec, getWidth()), (float)vs.timeToX(loopEndSec, getWidth()),
                      hasLoop(), loopStartSec, loopEndSec);
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (buffer == nullptr) return;
    if (dragger.mouseDrag(timeAt(e, true), e.getDistanceFromDragStart()))
    {
        loopStartSec = dragger.start;
        loopEndSec   = dragger.end;
        repaint();
        if (onLoopDragged) onLoopDragged(loopStartSec, loopEndSec, false);
    }
}

void WaveformDisplay::mouseUp(const juce::MouseEvent& e)
{
    if (buffer == nullptr) return;
    const auto mode = dragger.mouseUp();
    if (mode == LoopDragger::Mode::Click)
    {
        if (onSeek) onSeek(timeAt(e, false));
    }
    else if (mode != LoopDragger::Mode::None && onLoopDragged)
        onLoopDragged(dragger.start, dragger.end, true);
}

void WaveformDisplay::mouseDoubleClick(const juce::MouseEvent&)
{
    if (onLoopDragged) onLoopDragged(-1.0, -1.0, true);
}

void WaveformDisplay::mouseMove(const juce::MouseEvent& e)
{
    const bool edge = LoopDragger::isOverEdge(e.position.x, (float)vs.timeToX(loopStartSec, getWidth()),
                                              (float)vs.timeToX(loopEndSec, getWidth()), hasLoop());
    setMouseCursor(edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::IBeamCursor);
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (onWheel) onWheel(e, w);
}

} // namespace Soundstave
