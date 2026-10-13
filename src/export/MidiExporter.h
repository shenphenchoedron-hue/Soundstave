#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../model/NoteModel.h"

namespace Soundstave {

class MidiExporter
{
public:
    // Export notes to a Standard MIDI File.
    // Returns true on success.
    static bool exportToFile(const std::vector<Note>& notes,
                              const TempoInfo& tempo,
                              const juce::File& destination);

    // Build a MidiMessageSequence from notes (useful for internal preview)
    static juce::MidiMessageSequence buildSequence(const std::vector<Note>& notes,
                                                    const TempoInfo& tempo);
};

} // namespace Soundstave
