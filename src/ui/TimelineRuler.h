// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace Soundstave {

// -------------------------------------------------------------------------
// Pitch axis mapping shared by keyboard, spectrogram, grid and notes.
// Each MIDI note owns a lane of equal height; the lane centre is the exact
// note frequency. Because MIDI is logarithmic in frequency, a linear MIDI
// axis IS a logarithmic frequency axis.
// -------------------------------------------------------------------------
inline float pitchToY(float midi, int lo, int hi, int height) noexcept
{
    return (float)height * (((float)hi + 0.5f) - midi) / (float)(hi - lo + 1);
}

inline float yToPitch(float y, int lo, int hi, int height) noexcept
{
    return ((float)hi + 0.5f) - y / (float)std::max(1, height) * (float)(hi - lo + 1);
}

inline float laneHeight(int lo, int hi, int height) noexcept
{
    return (float)height / (float)(hi - lo + 1);
}

inline bool isBlackKey(int midi) noexcept
{
    const int pc = ((midi % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

// -------------------------------------------------------------------------
// Shared view state (time window + visible pitch range)
// -------------------------------------------------------------------------
struct ViewState
{
    double startTimeSec = 0.0;
    double endTimeSec   = 10.0;

    int midiLow  = 21;   // lowest visible MIDI note
    int midiHigh = 108;  // highest visible MIDI note

    double visibleDuration() const noexcept { return std::max(1e-6, endTimeSec - startTimeSec); }

    double timeToX(double t, int width) const noexcept
    {
        return (t - startTimeSec) / visibleDuration() * (double)width;
    }
    double xToTime(double x, int width) const noexcept
    {
        return startTimeSec + x / (double)std::max(1, width) * visibleDuration();
    }

    void zoom(double factor, double focusSec, double totalDuration)
    {
        if (totalDuration <= 0.0) return;
        const double newDur = juce::jlimit(0.05, std::max(0.05, totalDuration), visibleDuration() * factor);
        const double ratio  = (focusSec - startTimeSec) / visibleDuration();
        startTimeSec = focusSec - ratio * newDur;
        endTimeSec   = startTimeSec + newDur;
        clamp(totalDuration);
    }

    void scroll(double deltaSec, double totalDuration)
    {
        startTimeSec += deltaSec;
        endTimeSec   += deltaSec;
        clamp(totalDuration);
    }

    void clamp(double totalDuration)
    {
        const double dur = visibleDuration();
        startTimeSec = juce::jlimit(0.0, std::max(0.0, totalDuration - dur), startTimeSec);
        endTimeSec   = startTimeSec + dur;
    }

    int midiRange() const noexcept { return midiHigh - midiLow; }

    void scrollMidi(int delta)
    {
        const int range = midiRange();
        midiLow  = juce::jlimit(0, 127 - range, midiLow + delta);
        midiHigh = midiLow + range;
    }

    // Zoom the pitch axis around a centre note (factor < 1 = zoom in)
    void zoomMidi(double factor, float centre)
    {
        const int newRange = juce::jlimit(12, 127, (int)std::round((double)midiRange() * factor));
        int lo = (int)std::round(centre - (float)newRange * 0.5f);
        lo = juce::jlimit(0, 127 - newRange, lo);
        midiLow  = lo;
        midiHigh = lo + newRange;
    }
};

// -------------------------------------------------------------------------
// Mouse logic for creating and adjusting the loop section on a time strip
// (shared by the ruler and the waveform so both behave identically).
//   click            → seek (reported by the owner)
//   drag             → new loop section
//   drag a loop edge → move that edge
// -------------------------------------------------------------------------
struct LoopDragger
{
    enum class Mode { None, Click, Create, MoveEdge };

    static constexpr float kEdgePx = 6.0f;

    Mode   mode   = Mode::None;
    double anchor = 0.0;      // fixed end of the section while dragging
    double start  = -1.0, end = -1.0;

    void mouseDown(double t, float x, float xLoopStart, float xLoopEnd, bool hasLoop,
                   double loopStart, double loopEnd)
    {
        start = loopStart; end = loopEnd;
        if (hasLoop && std::abs(x - xLoopStart) <= kEdgePx)    { mode = Mode::MoveEdge; anchor = loopEnd; }
        else if (hasLoop && std::abs(x - xLoopEnd) <= kEdgePx) { mode = Mode::MoveEdge; anchor = loopStart; }
        else                                                    { mode = Mode::Click;    anchor = t; }
    }

    // Returns true while the section is being created or adjusted
    bool mouseDrag(double t, int distanceFromStartPx)
    {
        if (mode == Mode::Click)
        {
            if (distanceFromStartPx < 4) return false;
            mode = Mode::Create;
        }
        if (mode == Mode::None) return false;
        start = std::min(anchor, t);
        end   = std::max(anchor, t);
        return true;
    }

    Mode mouseUp() { const auto m = mode; mode = Mode::None; return m; }

    static bool isOverEdge(float x, float xLoopStart, float xLoopEnd, bool hasLoop)
    {
        return hasLoop && (std::abs(x - xLoopStart) <= kEdgePx || std::abs(x - xLoopEnd) <= kEdgePx);
    }
};

// -------------------------------------------------------------------------
// Horizontal time ruler shared by all panels
// -------------------------------------------------------------------------
class TimelineRuler : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    TimelineRuler();
    void setViewState(const ViewState& vs, double totalDuration);

    void setLoopRegion(double startSec, double endSec, bool enabled);
    void setSnap(std::function<double(double)> fn) { snapFn = std::move(fn); }

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    std::function<void(double)> onSeek;
    std::function<void(double, double, bool finished)> onLoopDragged;

    static constexpr int kHeight = 22;

private:
    double timeAt(const juce::MouseEvent& e, bool snapIt) const;

    ViewState vs;
    double    totalDuration = 0.0;
    double    loopStart = -1.0, loopEnd = -1.0;
    bool      loopOn = false;
    LoopDragger dragger;
    std::function<double(double)> snapFn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimelineRuler)
};

} // namespace Soundstave
