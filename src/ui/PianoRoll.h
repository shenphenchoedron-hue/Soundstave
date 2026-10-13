// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/NoteModel.h"
#include "TimelineRuler.h"
#include "LookAndFeel.h"

namespace Soundstave {

// Piano-roll grid drawn as a transparent layer over the spectrogram:
// key lanes (black keys shaded), octave lines, bar/beat/grid lines and bar
// numbers. It does not take mouse input – note editing happens in
// NoteOverlay, which sits on top and uses exactly the same coordinates.
class PianoRoll : public juce::Component
{
public:
    PianoRoll();

    void setViewState(const ViewState& vs);
    void setTempo(const TempoInfo& tempo);
    void setGridSeconds(double gridSec);   // 0 = free (no subdivision lines)

    void paint(juce::Graphics& g) override;

private:
    ViewState vs;
    TempoInfo tempo;
    double    gridSec = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRoll)
};

} // namespace Soundstave
