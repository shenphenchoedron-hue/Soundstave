// SPDX-License-Identifier: GPL-3.0-or-later
#include "QuantizationEngine.h"
#include <cmath>
#include <algorithm>

namespace Soundstave {

int QuantizationEngine::gridTicks(QuantizeResolution r) noexcept
{
    switch (r)
    {
        case QuantizeResolution::Quarter:       return 24;
        case QuantizeResolution::Eighth:        return 12;
        case QuantizeResolution::Sixteenth:     return 6;
        case QuantizeResolution::ThirtySecond:  return 3;
        case QuantizeResolution::EighthTriplet: return 8;
    }
    return 12;
}

juce::String QuantizationEngine::name(QuantizeResolution r)
{
    switch (r)
    {
        case QuantizeResolution::Quarter:       return juce::String::fromUTF8("1/4-dele");
        case QuantizeResolution::Eighth:        return juce::String::fromUTF8("1/8-dele");
        case QuantizeResolution::Sixteenth:     return juce::String::fromUTF8("1/16-dele");
        case QuantizeResolution::ThirtySecond:  return juce::String::fromUTF8("1/32-dele");
        case QuantizeResolution::EighthTriplet: return juce::String::fromUTF8("1/8-trioler");
    }
    return {};
}

double QuantizationEngine::secondsToTicks(double sec, const TempoInfo& tempo) noexcept
{
    return (sec - tempo.offsetSec) / tempo.quarterSec() * (double)kTicksPerQuarter;
}

QuantizedScore QuantizationEngine::quantize(const std::vector<Note>& notes,
                                            const TempoInfo& tempo,
                                            QuantizeResolution resolution)
{
    QuantizedScore score;
    score.ticksPerMeasure = std::max(1, tempo.numerator * kTicksPerQuarter * 4 / tempo.denominator);

    const int grid = gridTicks(resolution);
    // Clamp before converting to int (times are already validated by the model,
    // this guards against overflow for extreme inputs)
    const double maxTicks = secondsToTicks(kMaxTimeSec, tempo);
    auto snap = [grid, maxTicks](double ticks)
    {
        if (!std::isfinite(ticks)) return 0;
        const double t = juce::jlimit(0.0, std::max(0.0, maxTicks), ticks);
        return (int)std::round(t / grid) * grid;
    };

    for (auto& n : notes)
    {
        const int s = snap(secondsToTicks(n.startSec, tempo));
        int e = snap(secondsToTicks(n.endSec, tempo));
        if (e <= s) e = s + grid;
        score.notes.push_back({ n.midiNote, s, e - s, n.velocity });
    }

    std::sort(score.notes.begin(), score.notes.end(), [](const QuantizedNote& a, const QuantizedNote& b)
    {
        if (a.startTick != b.startTick) return a.startTick < b.startTick;
        if (a.midi != b.midi)           return a.midi < b.midi;
        return a.durTicks > b.durTicks;
    });

    // Readability clean-up per pitch: drop duplicates and resolve overlaps
    std::vector<QuantizedNote> cleaned;
    cleaned.reserve(score.notes.size());
    std::vector<int> lastIndexForPitch(128, -1);
    for (auto& qn : score.notes)
    {
        const int li = lastIndexForPitch[(size_t)qn.midi];
        if (li >= 0)
        {
            auto& prev = cleaned[(size_t)li];
            if (prev.startTick == qn.startTick)
            {
                prev.durTicks = std::max(prev.durTicks, qn.durTicks);
                prev.velocity = std::max(prev.velocity, qn.velocity);
                continue;
            }
            if (prev.startTick + prev.durTicks > qn.startTick)
                prev.durTicks = qn.startTick - prev.startTick;
        }
        lastIndexForPitch[(size_t)qn.midi] = (int)cleaned.size();
        cleaned.push_back(qn);
    }
    score.notes = std::move(cleaned);

    int maxEnd = 0;
    for (auto& qn : score.notes) maxEnd = std::max(maxEnd, qn.startTick + qn.durTicks);
    score.numMeasures = std::max(1, (maxEnd + score.ticksPerMeasure - 1) / score.ticksPerMeasure);
    return score;
}

} // namespace Soundstave
