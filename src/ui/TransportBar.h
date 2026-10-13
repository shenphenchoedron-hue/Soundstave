// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "LookAndFeel.h"
#include "../model/NoteModel.h"

namespace Soundstave {

// Grid divisions offered for snapping (combo box IDs)
enum GridId { GridFree = 1, GridQuarter, GridEighth, GridSixteenth, GridThirtySecond, GridEighthTriplet };

class TransportBar : public juce::Component
{
public:
    TransportBar();

    std::function<void()>          onPlay;
    std::function<void()>          onPause;
    std::function<void()>          onStop;
    std::function<void()>          onToggleLoop;
    std::function<void(double)>    onTempoChanged;
    std::function<void(int, int)>  onTimeSignatureChanged;
    std::function<void(int)>       onGridChanged;
    std::function<void()>          onEstimateTempo;
    std::function<void(float)>     onAudioVolumeChanged;
    std::function<void(float)>     onMidiVolumeChanged;
    std::function<void(bool)>      onAudioEnabledChanged;
    std::function<void(bool)>      onMidiEnabledChanged;

    void setPlaying(bool playing);
    void setPosition(double seconds, double total);
    void setTempo(const TempoInfo& tempo);
    void setGrid(int gridId);
    void setMixer(float audioVol, float midiVol, bool audioOn, bool midiOn);
    void setHasAudio(bool hasAudio);
    void setLoopState(bool looping);

    void resized() override;
    void paint(juce::Graphics& g) override;

    static constexpr int kHeight = 50;

private:
    juce::TextButton btnPlay, btnStop, btnLoop, btnEstimate, btnAudio, btnMidi;
    juce::Label      lblTime, lblTempo, lblMeter, lblGrid;
    juce::Slider     sldTempo, sldAudio, sldMidi;
    juce::ComboBox   cmbMeter, cmbGrid;

    bool isPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace Soundstave
