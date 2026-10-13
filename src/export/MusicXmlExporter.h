// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../model/NoteModel.h"
#include "QuantizationEngine.h"
#include <juce_core/juce_core.h>

namespace Soundstave {

// Writes a MusicXML 4.0 partwise score. Notes are quantized, split at bar
// lines (with ties), grouped into chords and distributed into up to four
// voices per staff. A grand staff (treble + bass) is used when the note
// range needs it.
class MusicXmlExporter
{
public:
    static bool exportToFile(const std::vector<Note>& notes,
                             const TempoInfo& tempo,
                             QuantizeResolution resolution,
                             const juce::File& destination,
                             const juce::String& title = "Soundstave");

    static juce::String buildXml(const std::vector<Note>& notes,
                                 const TempoInfo& tempo,
                                 QuantizeResolution resolution,
                                 const juce::String& title);
};

} // namespace Soundstave
