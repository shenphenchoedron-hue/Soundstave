// SPDX-License-Identifier: GPL-3.0-or-later
#include "PianoRoll.h"
#include <cmath>

namespace Soundstave {

PianoRoll::PianoRoll()
{
    setOpaque(false);
    setInterceptsMouseClicks(false, false);
}

void PianoRoll::setViewState(const ViewState& v) { vs = v; repaint(); }
void PianoRoll::setTempo(const TempoInfo& t)     { tempo = t; repaint(); }
void PianoRoll::setGridSeconds(double g)         { gridSec = g; repaint(); }

void PianoRoll::paint(juce::Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    const int lo = vs.midiLow, hi = vs.midiHigh;
    const float lane = laneHeight(lo, hi, h);

    // ---- Horizontal: key lanes ----
    for (int m = lo; m <= hi; ++m)
    {
        const float yTop = pitchToY((float)m + 0.5f, lo, hi, h);
        if (isBlackKey(m))
        {
            g.setColour(juce::Colours::black.withAlpha(0.16f));
            g.fillRect(0.0f, yTop, (float)w, lane);
        }
        const float yBottom = yTop + lane;
        if (m % 12 == 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.22f));
            g.drawHorizontalLine((int)yBottom, 0.0f, (float)w);
        }
        else if (lane >= 7.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawHorizontalLine((int)yBottom, 0.0f, (float)w);
        }
    }

    // ---- Vertical: bars, beats, grid ----
    const double pxPerSec = (double)w / vs.visibleDuration();
    const double beat = tempo.beatSec();
    const double bar  = tempo.barSec();
    if (beat <= 0.0 || bar <= 0.0) return;

    auto drawLines = [&](double step, juce::Colour c, double minSpacingPx)
    {
        if (step <= 0.0 || step * pxPerSec < minSpacingPx) return false;
        const long long first = (long long)std::floor((vs.startTimeSec - tempo.offsetSec) / step);
        const long long last  = (long long)std::ceil ((vs.endTimeSec   - tempo.offsetSec) / step);
        g.setColour(c);
        for (long long i = first; i <= last; ++i)
        {
            const double t = tempo.offsetSec + (double)i * step;
            if (t < 0.0) continue;
            g.drawVerticalLine((int)vs.timeToX(t, w), 0.0f, (float)h);
        }
        return true;
    };

    if (gridSec > 0.0 && std::abs(gridSec - beat) > 1e-6)
        drawLines(gridSec, juce::Colours::white.withAlpha(0.05f), 7.0);
    drawLines(beat, juce::Colours::white.withAlpha(0.11f), 10.0);
    drawLines(bar,  juce::Colours::white.withAlpha(0.30f), 4.0);

    // Bar numbers
    if (bar * pxPerSec >= 24.0)
    {
        g.setFont(SoundstaveLookAndFeel::getBoldFont(10.0f));
        const long long first = std::max(0LL, (long long)std::floor((vs.startTimeSec - tempo.offsetSec) / bar));
        const long long last  = (long long)std::ceil((vs.endTimeSec - tempo.offsetSec) / bar);
        for (long long i = first; i <= last; ++i)
        {
            const double t = tempo.offsetSec + (double)i * bar;
            const int x = (int)vs.timeToX(t, w);
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.drawText(juce::String(i + 1), x + 3, 2, 40, 12, juce::Justification::centredLeft, false);
        }
    }
}

} // namespace Soundstave
