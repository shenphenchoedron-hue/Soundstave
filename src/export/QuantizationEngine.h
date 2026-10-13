// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../model/NoteModel.h"
#include <vector>

namespace Soundstave {

enum class QuantizeResolution
{
    Quarter,
    Eighth,
    Sixteenth,
    ThirtySecond,
    EighthTriplet,
};

// Ticks per quarter note used for quantization and MusicXML <divisions>.
// 24 represents every supported grid (1/4, 1/8, 1/16, 1/32 and 1/8 triplets).
constexpr int kTicksPerQuarter = 24;

struct QuantizedNote
{
    int midi;
    int startTick;   // from the start of bar 1
    int durTicks;
    int velocity;
};

struct QuantizedScore
{
    std::vector<QuantizedNote> notes;   // sorted by start, then pitch
    int ticksPerMeasure = 96;
    int numMeasures     = 1;
};

class QuantizationEngine
{
public:
    static QuantizedScore quantize(const std::vector<Note>& notes,
                                   const TempoInfo& tempo,
                                   QuantizeResolution resolution);

    static int          gridTicks(QuantizeResolution r) noexcept;
    static juce::String name(QuantizeResolution r);
    static double       secondsToTicks(double sec, const TempoInfo& tempo) noexcept;
};

} // namespace Soundstave
