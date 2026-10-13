// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "../analysis/SpectralAnalyzer.h"
#include "../analysis/PitchDetector.h"

namespace Soundstave {

struct AppSettings
{
    AnalysisParams  analysis;
    DetectionParams detection;
    float           displayRangeDb = 80.0f;

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& vt);
};

// Advanced settings. Changes are reported immediately through onChanged.
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel(const AppSettings& initial);

    std::function<void(const AppSettings&)> onChanged;

    void resized() override;
    void paint(juce::Graphics& g) override;

    static void show(juce::Component* parent, const AppSettings& current,
                     std::function<void(const AppSettings&)> onChanged);

private:
    void readFromControls();
    void writeToControls();
    void addRow(juce::Label& label, juce::Component& control, const char* text, const char* tip);

    AppSettings settings;

    juce::Label    hdrDisplay, hdrAnalysis, note;
    juce::Label    lRange, lFft, lSens, lPoly, lMinLen, lLow, lHigh;
    juce::Slider   sRange, sSens, sPoly, sMinLen, sLow, sHigh;
    juce::ComboBox cFft;
    juce::TextButton btnDefaults, btnClose;

    struct Row { juce::Label* label; juce::Component* control; };
    std::vector<Row> displayRows, analysisRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};

} // namespace Soundstave
