// SPDX-License-Identifier: GPL-3.0-or-later
#include "MidiExporter.h"
#include <cmath>

namespace Soundstave {

static constexpr int kPpq = 960;

juce::MidiMessageSequence MidiExporter::buildSequence(const std::vector<Note>& notes,
                                                      const TempoInfo& tempo)
{
    // MIDI timing is exact: seconds are converted with a single constant
    // tempo, so the file plays back exactly like the transcription.
    const double ticksPerSec = tempo.bpm / 60.0 * (double)kPpq;
    juce::MidiMessageSequence seq;

    // Note-offs are added first so that, after the stable sort, an off and an
    // on at the same tick for the same pitch are ordered off → on.
    for (auto& n : notes)
        seq.addEvent(juce::MidiMessage::noteOff(n.channel, n.midiNote, (juce::uint8)0),
                     std::round(n.endSec * ticksPerSec));
    for (auto& n : notes)
        seq.addEvent(juce::MidiMessage::noteOn(n.channel, n.midiNote,
                                               (juce::uint8)juce::jlimit(1, 127, n.velocity)),
                     std::round(n.startSec * ticksPerSec));

    seq.sort();
    seq.updateMatchedPairs();
    return seq;
}

bool MidiExporter::exportToFile(const std::vector<Note>& notes,
                                const TempoInfo& tempo,
                                const juce::File& destination)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(kPpq);

    juce::MidiMessageSequence tempoTrack;
    tempoTrack.addEvent(juce::MidiMessage::textMetaEvent(3, "Soundstave"), 0.0);
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent((int)std::round(60000000.0 / tempo.bpm)), 0.0);
    tempoTrack.addEvent(juce::MidiMessage::timeSignatureMetaEvent(tempo.numerator, tempo.denominator), 0.0);
    midiFile.addTrack(tempoTrack);

    juce::MidiMessageSequence noteTrack;
    noteTrack.addEvent(juce::MidiMessage::textMetaEvent(3, "Transskription"), 0.0);
    noteTrack.addSequence(buildSequence(notes, tempo), 0.0);
    noteTrack.updateMatchedPairs();
    midiFile.addTrack(noteTrack);

    destination.deleteFile();
    juce::FileOutputStream stream(destination);
    if (!stream.openedOk())
        return false;
    return midiFile.writeTo(stream);
}

} // namespace Soundstave
