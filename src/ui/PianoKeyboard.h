// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include <bitset>

namespace Soundstave {

// Vertical keyboard labelling the pitch axis. Each key occupies exactly the
// same lane as the corresponding note in the spectrogram/note layer.
class PianoKeyboard : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PianoKeyboard();

    void setRange(int minMidi, int maxMidi);
    void setActiveNotes(const std::bitset<128>& active);

    std::function<void(int midiNote)> onKeyPressed;
    std::function<void(const juce::MouseEvent&, const juce::MouseWheelDetails&)> onWheel;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    static constexpr int kWidth = 56;

private:
    int midiAt(float y) const noexcept;

    int minMidi = 21, maxMidi = 108;
    int pressed = -1, hover = -1;
    std::bitset<128> active;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoKeyboard)
};

} // namespace Soundstave
