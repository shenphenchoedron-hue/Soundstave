// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../analysis/SpectralAnalyzer.h"
#include "TimelineRuler.h"
#include "LookAndFeel.h"
#include <array>

namespace Soundstave {

class SpectrogramDisplay : public juce::Component
{
public:
    SpectrogramDisplay();
    ~SpectrogramDisplay() override;

    void setData(SpectrogramPtr data);
    void clearData();
    bool hasData() const noexcept { return data != nullptr; }

    void setViewState(const ViewState& vs);
    void setPlayheadPosition(double timeSec);
    void setLoopRegion(double startSec, double endSec, bool enabled);
    void setDynamicRange(float rangeDb);     // dB shown below the loudest bin
    void setHintText(const juce::String& text);

    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;
    std::function<void(double)> onSeek;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDown(const juce::MouseEvent& e) override;

private:
    void rebuildImage();
    void buildPalette();

    SpectrogramPtr data;
    ViewState  vs;
    double     playheadSec = 0.0;
    double     loopStart = -1.0, loopEnd = -1.0;
    bool       loopOn = false;
    float      dynamicRangeDb = 80.0f;
    juce::String hint;

    void buildCoarseLevel();
    void computeColumns(int w);

    juce::Image image;
    bool        imageDirty = true;

    // Max-pooled copy with kCoarse frames per entry, used when zoomed out
    static constexpr int kCoarse = 8;
    std::vector<float> coarse;
    int coarseFrames = 0;

    // Per-pixel-column spectra for the current time window (w × numBins).
    // Only recomputed when the time window or width changes, so pitch
    // scrolling/zooming just re-maps rows.
    std::vector<float> columns;
    std::vector<bool>  columnOutside;
    double colStart = -1.0, colEnd = -1.0;
    int    colWidth = 0;
    // ARGB, not RGB: on macOS JUCE silently stores Image::RGB as 4-byte ARGB,
    // so writing 3-byte PixelRGB values corrupts colours and alpha there.
    std::array<juce::PixelARGB, 256> palette {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramDisplay)
};

} // namespace Soundstave
