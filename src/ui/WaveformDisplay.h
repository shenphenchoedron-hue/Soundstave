// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "TimelineRuler.h"
#include "LookAndFeel.h"

namespace Soundstave {

// Overview waveform. Click = move playhead, drag = select loop region,
// double-click = remove loop.
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();

    void setAudioBuffer(const juce::AudioBuffer<float>* buffer, double sampleRate);
    void clearAudio();

    void setViewState(const ViewState& vs);
    void setPlayheadPosition(double timeSec);
    void setLoopRegion(double startSec, double endSec, bool enabled);   // < 0 = none
    void setSnap(std::function<double(double)> fn) { snapFn = std::move(fn); }

    std::function<void(double)> onSeek;
    // Loop section being dragged (finished = mouse released). (-1,-1) = removed.
    std::function<void(double, double, bool finished)> onLoopDragged;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    void rebuildPeaks();

    const juce::AudioBuffer<float>* buffer = nullptr;
    double sampleRate = 44100.0;
    std::vector<float> peaksMin, peaksMax;
    bool   peaksDirty = true;

    // Min/max per kBucket samples (all channels), built once per file so that
    // zoomed-out redraws don't rescan the raw audio
    static constexpr int kBucket = 256;
    std::vector<float> bucketMin, bucketMax;

    ViewState vs;
    double playheadSec = 0.0;
    double loopStartSec = -1.0, loopEndSec = -1.0;
    bool   loopOn = false;

    double timeAt(const juce::MouseEvent& e, bool snapIt) const;
    bool   hasLoop() const noexcept { return loopStartSec >= 0.0 && loopEndSec > loopStartSec; }
    LoopDragger dragger;
    std::function<double(double)> snapFn;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};

} // namespace Soundstave
