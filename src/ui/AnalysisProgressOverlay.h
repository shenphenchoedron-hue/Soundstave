// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"

namespace Soundstave {

// Progress card shown over the spectrogram while analysis runs.
class AnalysisProgressOverlay : public juce::Component
{
public:
    AnalysisProgressOverlay();

    void setProgress(float progress, const juce::String& message = {});

    std::function<void()> onCancel;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Rectangle<int> cardBounds() const;

    float            progress = 0.0f;
    juce::String     message;
    juce::TextButton btnCancel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalysisProgressOverlay)
};

} // namespace Soundstave
