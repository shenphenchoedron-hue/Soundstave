// SPDX-License-Identifier: GPL-3.0-or-later
#include "MusicXmlExporter.h"
#include <algorithm>
#include <map>

namespace Soundstave {

namespace {

struct Chord
{
    int start;
    int dur;
    std::vector<int> pitches;
    int velocity;
    int end() const noexcept { return start + dur; }
};

struct Voice
{
    std::vector<Chord> chords;
    int lastEnd = 0;
};

struct DurInfo { const char* type; int dots; bool triplet; };

bool describeDuration(int ticks, DurInfo& out)
{
    switch (ticks)
    {
        case 96: out = { "whole",   0, false }; return true;
        case 72: out = { "half",    1, false }; return true;
        case 48: out = { "half",    0, false }; return true;
        case 36: out = { "quarter", 1, false }; return true;
        case 24: out = { "quarter", 0, false }; return true;
        case 18: out = { "eighth",  1, false }; return true;
        case 12: out = { "eighth",  0, false }; return true;
        case 9:  out = { "16th",    1, false }; return true;
        case 6:  out = { "16th",    0, false }; return true;
        case 3:  out = { "32nd",    0, false }; return true;
        case 16: out = { "quarter", 0, true  }; return true;
        case 8:  out = { "eighth",  0, true  }; return true;
        case 4:  out = { "16th",    0, true  }; return true;
        case 2:  out = { "32nd",    0, true  }; return true;
        default: return false;
    }
}

// Splits a duration into notatable values (largest first)
std::vector<int> decompose(int r)
{
    static const int plain[]   = { 96, 72, 48, 36, 24, 18, 12, 9, 6, 3 };
    static const int triplet[] = { 16, 8, 4, 2 };
    std::vector<int> parts;
    while (r > 0)
    {
        int pick = 0;
        if (r % 3 == 0)
        {
            for (int v : plain) if (v <= r) { pick = v; break; }
        }
        else
        {
            for (int t : triplet) if (t <= r && (r - t) % 3 == 0) { pick = t; break; }
            if (pick == 0) for (int t : triplet) if (t <= r) { pick = t; break; }
        }
        if (pick == 0) pick = r;   // unrepresentable remainder (should not happen)
        parts.push_back(pick);
        r -= pick;
    }
    return parts;
}

void spell(int midi, const char*& step, int& alter, int& octave)
{
    static const char* steps[]  = { "C", "C", "D", "D", "E", "F", "F", "G", "G", "A", "A", "B" };
    static const int   alters[] = {  0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0  };
    const int pc = ((midi % 12) + 12) % 12;
    step   = steps[pc];
    alter  = alters[pc];
    octave = midi / 12 - 1;
}

juce::String escape(const juce::String& s)
{
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;");
}

class Writer
{
public:
    juce::String x;
    int numStaves = 1;

    void piece(const std::vector<int>& pitches, int ticks, int voice, int staff,
               bool tieStop, bool tieStart, int velocity, bool hiddenRest)
    {
        DurInfo d {};
        const bool known = describeDuration(ticks, d);
        const bool rest = pitches.empty();
        const size_t count = rest ? 1 : pitches.size();

        for (size_t i = 0; i < count; ++i)
        {
            x << "      <note";
            if (rest && hiddenRest) x << " print-object=\"no\"";
            if (!rest) x << " dynamics=\"" << juce::String(velocity * 100.0 / 90.0, 1) << "\"";
            x << ">\n";
            if (i > 0) x << "        <chord/>\n";
            if (rest)
                x << "        <rest/>\n";
            else
            {
                const char* step; int alter, octave;
                spell(pitches[i], step, alter, octave);
                x << "        <pitch><step>" << step << "</step>";
                if (alter != 0) x << "<alter>" << alter << "</alter>";
                x << "<octave>" << octave << "</octave></pitch>\n";
            }
            x << "        <duration>" << ticks << "</duration>\n";
            if (!rest && tieStop)  x << "        <tie type=\"stop\"/>\n";
            if (!rest && tieStart) x << "        <tie type=\"start\"/>\n";
            x << "        <voice>" << voice << "</voice>\n";
            if (known)
            {
                x << "        <type>" << d.type << "</type>\n";
                for (int k = 0; k < d.dots; ++k) x << "        <dot/>\n";
                if (d.triplet)
                    x << "        <time-modification><actual-notes>3</actual-notes>"
                         "<normal-notes>2</normal-notes></time-modification>\n";
            }
            if (numStaves > 1) x << "        <staff>" << staff << "</staff>\n";
            if (!rest && (tieStop || tieStart))
            {
                x << "        <notations>";
                if (tieStop)  x << "<tied type=\"stop\"/>";
                if (tieStart) x << "<tied type=\"start\"/>";
                x << "</notations>\n";
            }
            x << "      </note>\n";
        }
    }

    // A (possibly long) chord or rest segment, decomposed with ties
    void segment(const std::vector<int>& pitches, int ticks, int voice, int staff,
                 bool tieIn, bool tieOut, int velocity, bool hiddenRest)
    {
        const auto parts = decompose(ticks);
        for (size_t i = 0; i < parts.size(); ++i)
        {
            const bool stop  = (i == 0) ? tieIn : true;
            const bool start = (i + 1 == parts.size()) ? tieOut : true;
            piece(pitches, parts[i], voice, staff, stop, start, velocity, hiddenRest);
        }
    }

    void measureRest(int ticks, int voice, int staff)
    {
        x << "      <note>\n        <rest measure=\"yes\"/>\n"
          << "        <duration>" << ticks << "</duration>\n"
          << "        <voice>" << voice << "</voice>\n";
        if (numStaves > 1) x << "        <staff>" << staff << "</staff>\n";
        x << "      </note>\n";
    }
};

// Groups notes into chords and distributes them into non-overlapping voices
std::vector<Voice> buildVoices(const std::vector<QuantizedNote>& notes)
{
    std::map<std::pair<int, int>, Chord> chordMap;
    for (auto& n : notes)
    {
        auto key = std::make_pair(n.startTick, n.durTicks);
        auto it = chordMap.find(key);
        if (it == chordMap.end())
            chordMap.emplace(key, Chord { n.startTick, n.durTicks, { n.midi }, n.velocity });
        else
        {
            it->second.pitches.push_back(n.midi);
            it->second.velocity = std::max(it->second.velocity, n.velocity);
        }
    }

    std::vector<Chord> chords;
    for (auto& [k, c] : chordMap)
    {
        std::sort(c.pitches.begin(), c.pitches.end());
        chords.push_back(c);
    }
    std::sort(chords.begin(), chords.end(), [](const Chord& a, const Chord& b)
    {
        return a.start != b.start ? a.start < b.start : a.dur > b.dur;
    });

    constexpr int kMaxVoices = 4;
    std::vector<Voice> voices;
    for (auto& c : chords)
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (v.lastEnd <= c.start) { target = &v; break; }

        if (target == nullptr && (int)voices.size() < kMaxVoices)
        {
            voices.emplace_back();
            target = &voices.back();
        }
        if (target == nullptr)
        {
            // All voices busy: shorten the voice that frees up first
            target = &voices[0];
            for (auto& v : voices) if (v.lastEnd < target->lastEnd) target = &v;
            Chord& last = target->chords.back();
            if (last.start == c.start)
            {
                for (int p : c.pitches)
                    if (std::find(last.pitches.begin(), last.pitches.end(), p) == last.pitches.end())
                        last.pitches.push_back(p);
                std::sort(last.pitches.begin(), last.pitches.end());
                target->lastEnd = last.end();
                continue;
            }
            last.dur = c.start - last.start;
        }
        target->chords.push_back(c);
        target->lastEnd = c.end();
    }
    return voices;
}

} // namespace

juce::String MusicXmlExporter::buildXml(const std::vector<Note>& notes,
                                        const TempoInfo& tempo,
                                        QuantizeResolution resolution,
                                        const juce::String& title)
{
    const auto score = QuantizationEngine::quantize(notes, tempo, resolution);
    const int tpm = score.ticksPerMeasure;

    // ---- Staff layout ----
    int lo = 127, hi = 0;
    for (auto& n : score.notes) { lo = std::min(lo, n.midi); hi = std::max(hi, n.midi); }
    const bool grand  = !score.notes.empty() && lo < 57 && hi > 64;
    const bool bassOnly = !grand && !score.notes.empty() && hi <= 64 && lo < 57;

    std::vector<QuantizedNote> staffNotes[2];
    for (auto& n : score.notes)
        staffNotes[(grand && n.midi < 60) ? 1 : 0].push_back(n);

    const int numStaves = grand ? 2 : 1;
    std::vector<Voice> staffVoices[2];
    for (int s = 0; s < numStaves; ++s)
    {
        staffVoices[s] = buildVoices(staffNotes[s]);
        if (staffVoices[s].empty()) staffVoices[s].emplace_back();
    }

    Writer w;
    w.numStaves = numStaves;
    auto& x = w.x;

    x << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
      << "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" "
         "\"http://www.musicxml.org/dtds/partwise.dtd\">\n"
      << "<score-partwise version=\"4.0\">\n"
      << "  <work><work-title>" << escape(title) << "</work-title></work>\n"
      << "  <identification>\n    <encoding>\n"
      << "      <software>Soundstave</software>\n"
      << "      <encoding-date>" << juce::Time::getCurrentTime().formatted("%Y-%m-%d") << "</encoding-date>\n"
      << "    </encoding>\n  </identification>\n"
      << "  <part-list>\n    <score-part id=\"P1\">\n"
      << "      <part-name>Transskription</part-name>\n"
      << "      <score-instrument id=\"P1-I1\"><instrument-name>Piano</instrument-name></score-instrument>\n"
      << "      <midi-instrument id=\"P1-I1\"><midi-channel>1</midi-channel><midi-program>1</midi-program></midi-instrument>\n"
      << "    </score-part>\n  </part-list>\n"
      << "  <part id=\"P1\">\n";

    for (int m = 0; m < score.numMeasures; ++m)
    {
        const int m0 = m * tpm, m1 = m0 + tpm;
        x << "    <measure number=\"" << (m + 1) << "\">\n";

        if (m == 0)
        {
            x << "      <attributes>\n"
              << "        <divisions>" << kTicksPerQuarter << "</divisions>\n"
              << "        <key><fifths>0</fifths><mode>major</mode></key>\n"
              << "        <time><beats>" << tempo.numerator << "</beats><beat-type>"
              << tempo.denominator << "</beat-type></time>\n";
            if (grand)
                x << "        <staves>2</staves>\n"
                  << "        <clef number=\"1\"><sign>G</sign><line>2</line></clef>\n"
                  << "        <clef number=\"2\"><sign>F</sign><line>4</line></clef>\n";
            else if (bassOnly)
                x << "        <clef><sign>F</sign><line>4</line></clef>\n";
            else
                x << "        <clef><sign>G</sign><line>2</line></clef>\n";
            x << "      </attributes>\n";

            const int bpm = juce::roundToInt(tempo.bpm);
            x << "      <direction placement=\"above\">\n"
              << "        <direction-type><metronome parentheses=\"no\"><beat-unit>quarter</beat-unit>"
              << "<per-minute>" << bpm << "</per-minute></metronome></direction-type>\n";
            if (numStaves > 1) x << "        <staff>1</staff>\n";
            x << "        <sound tempo=\"" << bpm << "\"/>\n"
              << "      </direction>\n";
        }

        bool firstVoiceWritten = false;
        for (int s = 0; s < numStaves; ++s)
        {
            for (size_t vi = 0; vi < staffVoices[s].size(); ++vi)
            {
                const Voice& voice = staffVoices[s][vi];
                const int voiceNo = s * 4 + (int)vi + 1;
                const int staffNo = s + 1;
                const bool primary = (vi == 0);

                bool hasContent = false;
                for (auto& c : voice.chords)
                    if (c.start < m1 && c.end() > m0) { hasContent = true; break; }
                if (!hasContent && !primary) continue;

                if (firstVoiceWritten)
                    x << "      <backup><duration>" << tpm << "</duration></backup>\n";
                firstVoiceWritten = true;

                if (!hasContent)
                {
                    w.measureRest(tpm, voiceNo, staffNo);
                    continue;
                }

                int cursor = m0;
                for (auto& c : voice.chords)
                {
                    if (c.end() <= m0 || c.start >= m1) continue;
                    const int segStart = std::max(c.start, m0);
                    const int segEnd   = std::min(c.end(), m1);
                    if (segStart > cursor)
                        w.segment({}, segStart - cursor, voiceNo, staffNo, false, false, 0, !primary);
                    w.segment(c.pitches, segEnd - segStart, voiceNo, staffNo,
                              c.start < m0, c.end() > m1, c.velocity, false);
                    cursor = segEnd;
                }
                if (cursor < m1)
                    w.segment({}, m1 - cursor, voiceNo, staffNo, false, false, 0, !primary);
            }
        }

        if (m == score.numMeasures - 1)
            x << "      <barline location=\"right\"><bar-style>light-heavy</bar-style></barline>\n";
        x << "    </measure>\n";
    }

    x << "  </part>\n</score-partwise>\n";
    return x;
}

bool MusicXmlExporter::exportToFile(const std::vector<Note>& notes,
                                    const TempoInfo& tempo,
                                    QuantizeResolution resolution,
                                    const juce::File& destination,
                                    const juce::String& title)
{
    return destination.replaceWithText(buildXml(notes, tempo, resolution, title), false, false, "\n");
}

} // namespace Soundstave
