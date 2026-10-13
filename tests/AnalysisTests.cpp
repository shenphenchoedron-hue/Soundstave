// SPDX-License-Identifier: GPL-3.0-or-later
// Headless tests: synthetic signals → STFT → pitch detection → export.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "analysis/SpectralAnalyzer.h"
#include "analysis/PitchDetector.h"
#include "analysis/TempoEstimator.h"
#include "export/MusicXmlExporter.h"
#include "export/MidiExporter.h"
#include "audio/AudioEngine.h"
#include <cmath>
#include <set>
#include <iostream>
#include <limits>

using namespace Soundstave;

namespace {

int failures = 0, checks = 0;

void check(bool ok, const juce::String& what)
{
    ++checks;
    if (!ok) ++failures;
    std::cout << (ok ? "  [ok]   " : "  [FAIL] ") << what << std::endl;
}

constexpr double kSr = 44100.0;

struct Tone { int midi; double start; double end; float amp; int harmonics; double decay = 0.8; };

// Additive tone with 1/h harmonic rolloff and a short attack/release
std::vector<float> render(const std::vector<Tone>& tones, double totalSec)
{
    std::vector<float> out((size_t)(totalSec * kSr), 0.0f);
    for (auto& t : tones)
    {
        const double f0 = 440.0 * std::pow(2.0, (t.midi - 69) / 12.0);
        const int s0 = (int)(t.start * kSr), s1 = std::min((int)out.size(), (int)(t.end * kSr));
        for (int s = s0; s < s1; ++s)
        {
            const double tt = (s - s0) / kSr, rem = (s1 - s) / kSr;
            const double env = std::min({ 1.0, tt / 0.005, rem / 0.01 }) * std::exp(-tt * t.decay);
            double v = 0.0;
            for (int h = 1; h <= t.harmonics; ++h)
                if (f0 * h < kSr * 0.45) v += std::sin(2.0 * juce::MathConstants<double>::pi * f0 * h * tt) / h;
            out[(size_t)s] += (float)(v * env * t.amp);
        }
    }
    return out;
}

std::vector<Note> detect(const std::vector<float>& mono, DetectionParams p = {})
{
    auto data = SpectralAnalyzer::compute(mono, kSr, AnalysisParams{});
    return PitchDetector::run(*data, p);
}

std::set<int> pitchesOf(const std::vector<Note>& notes)
{
    std::set<int> s;
    for (auto& n : notes) s.insert(n.midiNote);
    return s;
}

juce::String describe(const std::vector<Note>& notes)
{
    juce::String s;
    for (auto& n : notes)
        s << Note::midiToName(n.midiNote) << "[" << juce::String(n.startSec, 3) << "-" << juce::String(n.endSec, 3) << "] ";
    return s;
}

// ---------------------------------------------------------------------------
void testSpectrogramCalibration()
{
    std::cout << "Spectrogram calibration (A4 sine, amplitude 0.5)" << std::endl;
    std::vector<float> mono((size_t)kSr);
    for (size_t i = 0; i < mono.size(); ++i)
        mono[i] = 0.5f * (float)std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * (double)i / kSr);
    auto d = SpectralAnalyzer::compute(mono, kSr, AnalysisParams{});
    const float* f = d->frame(d->numFrames / 2);
    int best = 0;
    for (int b = 1; b < d->numBins; ++b) if (f[b] > f[best]) best = b;
    const double peakHz = d->binToFreq(best);
    check(std::abs(peakHz - 440.0) < d->binToFreq(1) * 1.01, "peak bin at 440 Hz (got " + juce::String(peakHz, 1) + " Hz)");
    check(std::abs(f[best] - (-6.02f)) < 1.6f, "level approx -6 dBFS (got " + juce::String(f[best], 2) + " dB)");
    check(std::abs(d->frameTime(10) - 10.0 * d->hopSize / kSr) < 1e-9, "frame times are frame centres");
}

void testSingleNotes()
{
    std::cout << "Single notes" << std::endl;
    for (int midi : { 69, 60, 40, 84 })
    {
        auto notes = detect(render({ { midi, 0.2, 1.2, 0.5f, 8 } }, 1.6));
        const bool ok = notes.size() == 1 && notes[0].midiNote == midi
                     && std::abs(notes[0].startSec - 0.2) < 0.05 && std::abs(notes[0].endSec - 1.2) < 0.12;
        check(ok, Note::midiToName(midi) + " detected once with correct timing: " + describe(notes));
    }
    auto sine = detect(render({ { 69, 0.2, 1.2, 0.5f, 1 } }, 1.6));
    check(sine.size() == 1 && sine[0].midiNote == 69, "pure 440 Hz sine -> A4: " + describe(sine));
}

void testChord()
{
    std::cout << "Chord C4-E4-G4 with harmonics (no overtone ghosts)" << std::endl;
    auto notes = detect(render({ { 60, 0.1, 1.5, 0.3f, 8 }, { 64, 0.1, 1.5, 0.3f, 8 }, { 67, 0.1, 1.5, 0.3f, 8 } }, 1.8));
    check(pitchesOf(notes) == std::set<int>({ 60, 64, 67 }), "pitches are exactly {C4,E4,G4}: " + describe(notes));
}

void testMelodyAndRepeats()
{
    std::cout << "Fast scale, repeated notes and a long note" << std::endl;
    std::vector<Tone> tones;
    const int scale[] = { 60, 62, 64, 65, 67, 69, 71, 72 };
    for (int i = 0; i < 8; ++i)
        tones.push_back({ scale[i], 0.1 + i * 0.125, 0.1 + i * 0.125 + 0.115, 0.4f, 6 });
    for (int i = 0; i < 4; ++i)   // repeated A4 (piano-like decay), re-attacked without a gap
        tones.push_back({ 69, 1.3 + i * 0.25, 1.3 + (i + 1) * 0.25, 0.4f, 6, 4.0 });
    tones.push_back({ 55, 2.5, 4.0, 0.4f, 6 });   // long G3

    auto notes = detect(render(tones, 4.4));

    int scaleHits = 0;
    for (int i = 0; i < 8; ++i)
        for (auto& n : notes)
            if (n.midiNote == scale[i] && std::abs(n.startSec - (0.1 + i * 0.125)) < 0.045) { ++scaleHits; break; }
    check(scaleHits == 8, "all 8 sixteenth-note scale tones found with onset < 45 ms error (" + juce::String(scaleHits) + "/8)");

    int a4 = 0;
    for (auto& n : notes) if (n.midiNote == 69 && n.startSec > 1.25) ++a4;
    check(a4 == 4, "4 repeated A4 notes kept separate (got " + juce::String(a4) + ")");

    bool longOk = false;
    for (auto& n : notes) if (n.midiNote == 55 && n.duration() > 1.2) longOk = true;
    check(longOk, "long G3 detected as one sustained note");
    check(notes.size() <= 15, "no spurious extra notes (" + juce::String((int)notes.size()) + " total)");
}

void testTempo()
{
    std::cout << "Tempo estimation" << std::endl;
    std::vector<Note> notes;
    const double beat = 60.0 / 100.0, offset = 0.2;
    for (int i = 0; i < 32; ++i)
    {
        Note n; n.midiNote = 60 + (i % 5); n.startSec = offset + i * beat; n.endSec = n.startSec + beat * 0.8;
        n.velocity = (i % 4 == 0) ? 110 : 80;
        notes.push_back(n);
    }
    auto r = TempoEstimator::estimate(notes);
    check(r.valid && std::abs(r.bpm - 100.0) < 1.0, "100 BPM recognised (got " + juce::String(r.bpm, 2) + ")");
    const double phaseErr = std::fmod(std::abs(r.offsetSec - offset) + 1e-9, beat);
    check(std::min(phaseErr, beat - phaseErr) < 0.02, "beat phase aligned (offset " + juce::String(r.offsetSec, 3) + " s)");

    // Eighth notes at 120 BPM (onsets on and between beats) + a chord on beat 1
    std::vector<Note> eighths;
    for (int i = 0; i < 24; ++i)
    {
        Note n; n.midiNote = 60 + (i % 8); n.startSec = 0.5 + i * 0.25; n.endSec = n.startSec + 0.24;
        n.velocity = (i % 2 == 0) ? 100 : 80;
        eighths.push_back(n);
    }
    auto r2 = TempoEstimator::estimate(eighths);
    check(r2.valid && std::abs(r2.bpm - 120.0) < 1.0, "eighth-note melody -> 120 BPM (got " + juce::String(r2.bpm, 2) + ")");
    const double pe = std::fmod(std::abs(r2.offsetSec - 0.5) + 1e-9, 0.5);
    check(std::min(pe, 0.5 - pe) < 0.02, "beats fall on the strong onsets (offset " + juce::String(r2.offsetSec, 3) + " s)");
}

void testTempoFromAudio()
{
    std::cout << "Tempo from audio (no notes needed), human timing" << std::endl;
    juce::Random rnd(1234);
    // Values in beats: quarters, eighths, dotted quarter + eighth, half notes
    const double pattern[] = { 1, 1, 0.5, 0.5, 1, 1.5, 0.5, 1, 2, 0.5, 0.5, 0.5, 0.5, 1, 1, 2 };
    const int melody[] = { 60, 62, 64, 65, 67, 69, 67, 65, 64, 62, 64, 65, 67, 65, 64, 60 };

    for (double bpm : { 92.0, 128.0, 72.0 })
    {
        const double beat = 60.0 / bpm;
        std::vector<Tone> tones;
        double t = 0.4;
        for (int rep = 0; rep < 3; ++rep)
            for (int i = 0; i < 16; ++i)
            {
                const double jitter = (rnd.nextDouble() - 0.5) * 0.05;     // ±25 ms
                const double len = pattern[i] * beat;
                tones.push_back({ melody[i], t + jitter, t + jitter + len * 0.9, 0.3f, 6, 2.5 });
                t += len;
            }
        auto mono = render(tones, t + 1.0);
        auto data = SpectralAnalyzer::compute(mono, kSr, AnalysisParams{});
        auto r = TempoEstimator::estimate(*data);
        check(r.valid && std::abs(r.bpm - bpm) <= 2.0,
              juce::String(bpm, 0) + " BPM melody -> " + juce::String(r.bpm, 1) + " BPM");
        const double pe = std::fmod(std::abs(r.offsetSec - 0.4) + 1e-9, beat);
        check(std::min(pe, beat - pe) < 0.05, "  beat phase within 50 ms (offset " + juce::String(r.offsetSec, 3) + " s)");
    }

    auto silence = SpectralAnalyzer::compute(std::vector<float>((size_t)(kSr * 5), 0.0f), kSr, AnalysisParams{});
    check(!TempoEstimator::estimate(*silence).valid, "silence -> no tempo (no crash)");
}

void testMusicXml()
{
    std::cout << "MusicXML structure" << std::endl;
    std::vector<Note> notes;
    auto add = [&](int midi, double s, double e) { Note n; n.midiNote = midi; n.startSec = s; n.endSec = e; notes.push_back(n); };
    // 120 BPM: quarter = 0.5 s; a note crossing the bar line, a chord, overlapping lines, low notes
    add(72, 0.0, 0.5); add(74, 0.5, 1.0); add(76, 1.0, 3.0);          // E5 crosses bar 1→2
    add(60, 0.0, 1.0); add(64, 0.0, 1.0); add(67, 0.0, 1.0);          // C major chord
    add(48, 0.0, 4.0);                                                 // long bass note
    add(43, 2.0, 2.27);                                                // slightly off-grid
    add(79, 2.04, 2.29); add(77, 2.26, 2.52);                         // off-grid melody

    TempoInfo tempo;
    const auto xml = MusicXmlExporter::buildXml(notes, tempo, QuantizeResolution::Sixteenth, "Test");
    auto doc = juce::parseXML(xml);
    check(doc != nullptr && doc->hasTagName("score-partwise"), "XML parses as score-partwise");
    if (doc == nullptr) return;

    auto* part = doc->getChildByName("part");
    const int tpm = 96;
    bool allFull = true, hasTie = false, hasChord = false, hasTwoStaves = false;
    int measures = 0;
    for (auto* m : part->getChildWithTagNameIterator("measure"))
    {
        ++measures;
        std::map<int, int> voiceTicks;
        for (auto* el : m->getChildIterator())
        {
            if (el->hasTagName("attributes") && el->getChildByName("staves")) hasTwoStaves = true;
            if (!el->hasTagName("note")) continue;
            if (el->getChildByName("chord")) { hasChord = true; continue; }
            if (el->getChildByName("tie")) hasTie = true;
            const int v = el->getChildElementAllSubText("voice", "1").getIntValue();
            voiceTicks[v] += el->getChildElementAllSubText("duration", "0").getIntValue();
        }
        for (auto& [v, t] : voiceTicks)
            if (t != tpm) { allFull = false; std::cout << "     measure " << measures << " voice " << v << " = " << t << " ticks" << std::endl; }
    }
    check(measures == 2, "2 measures (got " + juce::String(measures) + ")");
    check(allFull, "every voice in every measure sums to exactly one bar");
    check(hasTie, "note across the bar line is tied");
    check(hasChord, "simultaneous notes written as a chord");
    check(hasTwoStaves, "wide range uses a grand staff");
}

void testMidi()
{
    std::cout << "MIDI export" << std::endl;
    std::vector<Note> notes;
    for (int i = 0; i < 5; ++i) { Note n; n.midiNote = 60 + i; n.startSec = i * 0.3; n.endSec = n.startSec + 0.25; n.velocity = 70 + i; notes.push_back(n); }
    Note chord1 = notes[0]; chord1.midiNote = 67; notes.push_back(chord1);

    auto f = juce::File::createTempFile(".mid");
    check(MidiExporter::exportToFile(notes, TempoInfo{}, f), "file written");
    juce::MidiFile mf;
    juce::FileInputStream in(f);
    check(in.openedOk() && mf.readFrom(in), "file reads back");
    mf.convertTimestampTicksToSeconds();
    int ons = 0; bool timingOk = true;
    for (int t = 0; t < mf.getNumTracks(); ++t)
        for (auto* ev : *mf.getTrack(t))
            if (ev->message.isNoteOn())
            {
                ++ons;
                bool match = false;
                for (auto& n : notes)
                    if (n.midiNote == ev->message.getNoteNumber() && std::abs(n.startSec - ev->message.getTimeStamp()) < 0.002) match = true;
                timingOk &= match;
            }
    check(ons == (int)notes.size(), "all notes present incl. polyphony (" + juce::String(ons) + ")");
    check(timingOk, "note-on times preserved within 2 ms");
    f.deleteFile();
}

void testRobustness()
{
    std::cout << "Robustness against malformed input" << std::endl;
    std::vector<float> mono(4000, 0.1f);
    for (double sr : { 20.0, 10.0, 1.0, 0.0, -44100.0, 1.0e9 })
        check(SpectralAnalyzer::compute(mono, sr, AnalysisParams{}) == nullptr,
              "sample rate " + juce::String(sr) + " Hz rejected without crash/hang");
    check(SpectralAnalyzer::compute(mono, 8000.0, AnalysisParams{}) != nullptr, "8 kHz accepted");

    juce::ValueTree vt("NoteModel");
    vt.setProperty("bpm", std::numeric_limits<double>::quiet_NaN(), nullptr);
    vt.setProperty("offset", std::numeric_limits<double>::infinity(), nullptr);
    juce::ValueTree n("Note");
    n.setProperty("midi", 60, nullptr);
    n.setProperty("start", std::numeric_limits<double>::quiet_NaN(), nullptr);
    n.setProperty("end", 1.0e300, nullptr);
    vt.addChild(n, -1, nullptr);

    NoteModel model;
    model.fromValueTree(vt);
    const auto& t = model.getTempo();
    check(std::isfinite(t.bpm) && t.bpm >= 20.0 && std::isfinite(t.offsetSec), "NaN/inf tempo replaced by safe values");
    const auto& notes = model.getNotes();
    check(notes.size() == 1 && std::isfinite(notes[0].startSec) && notes[0].endSec <= kMaxTimeSec,
          "NaN/huge note times clamped");
    const auto xml = MusicXmlExporter::buildXml(notes, t, QuantizeResolution::Sixteenth, "x");
    check(xml.length() < 50 * 1000 * 1000, "export of clamped data stays bounded");

    // Hash-indexed bulk update
    NoteModel m2;
    for (int i = 0; i < 3000; ++i) { Note x; x.midiNote = 40 + i % 40; x.startSec = i * 0.1; x.endSec = x.startSec + 0.05; m2.addNote(x); }
    m2.selectAll();
    auto sel = m2.getSelectedNotes();
    for (auto& x : sel) x.midiNote += 1;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    m2.updateNotes(sel);
    const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
    bool allMoved = true;
    for (size_t i = 0; i < m2.getNotes().size(); ++i) allMoved &= m2.getNotes()[i].midiNote == 41 + (int)i % 40;
    check(allMoved, "bulk update applied to all 3000 notes");
    check(ms < 20.0, "bulk update of 3000 notes is fast (" + juce::String(ms, 2) + " ms)");
}

// Renders the engine offline. The test file is a ramp whose value encodes
// the source position, so every output sample tells where it was read from.
void testLoopPlayback()
{
    std::cout << "Loop playback in the audio engine" << std::endl;
    const int total = (int)(2.0 * kSr);
    auto file = juce::File::createTempFile(".wav");
    {
        juce::AudioBuffer<float> ramp(1, total);
        for (int i = 0; i < total; ++i) ramp.setSample(0, i, (float)i / (float)total);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os(file.createOutputStream().release());
        auto opts = juce::AudioFormatWriterOptions{}.withSampleRate(kSr).withNumChannels(1).withBitsPerSample(32)
                        .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor(os, opts);
        check(writer != nullptr, "test file written");
        if (writer == nullptr) return;
        writer->writeFromAudioSampleBuffer(ramp, 0, total);
    }

    auto run = [&](double ls, double le, double startAt, double seconds, double& minT, double& maxT,
                   int& wraps, int& badSteps)
    {
        AudioEngine engine;
        juce::String err;
        if (!engine.loadFile(file, err)) { check(false, "load: " + err); return false; }
        engine.setAudioVolume(1.0f);
        engine.setMidiVolume(0.0f);
        engine.prepareToPlay(512, kSr);
        engine.setLoop(true, ls, le);
        engine.setPosition(startAt);
        engine.play();

        juce::AudioBuffer<float> out(2, 512);
        minT = 1e9; maxT = -1e9; wraps = 0; badSteps = 0;
        double prev = -1.0;
        const int blocks = (int)(seconds * kSr / 512.0);
        for (int b = 0; b < blocks; ++b)
        {
            out.clear();
            engine.getNextAudioBlock(juce::AudioSourceChannelInfo(&out, 0, 512));
            for (int i = 0; i < 512; ++i)
            {
                const double t = out.getSample(0, i) * 2.0;   // ramp value → seconds
                if (b > 0)   // skip the resampler's first block
                {
                    minT = std::min(minT, t); maxT = std::max(maxT, t);
                    if (prev >= 0.0)
                    {
                        const double step = t - prev;
                        if (step < -0.1) ++wraps;
                        else if (std::abs(step - 1.0 / kSr) > 4.0 / kSr) ++badSteps;
                    }
                }
                prev = t;
            }
        }
        engine.releaseResources();
        return true;
    };

    double mn, mx; int wraps, bad;
    if (run(0.5, 1.0, 0.5, 4.0, mn, mx, wraps, bad))
    {
        check(mn >= 0.5 - 0.002 && mx <= 1.0 + 0.002,
              "all output stays inside the loop 0.5-1.0 s (" + juce::String(mn, 4) + " - " + juce::String(mx, 4) + ")");
        check(wraps >= 7 && wraps <= 8, "loops 7-8 times in 4 s (" + juce::String(wraps) + ")");
        check(bad == 0, "no gaps or jumps inside the loop (" + juce::String(bad) + " bad steps)");
    }
    if (run(1.5, 5.0, 1.9, 3.0, mn, mx, wraps, bad))   // loop end beyond the file is clamped to 2.0 s
        check(wraps >= 5 && mn >= 1.5 - 0.002, "loop past the file end keeps looping (" + juce::String(wraps) + " wraps)");
    if (run(0.5, 1.0, 1.5, 1.0, mn, mx, wraps, bad))   // starting after the loop jumps into it
        check(mx <= 1.0 + 0.002 && wraps >= 1, "playback after the loop end jumps back into the loop");
    file.deleteFile();
}

} // namespace

int main()
{
    juce::MessageManager::getInstance();   // AudioTransportSource sends change messages
    testSpectrogramCalibration();
    testSingleNotes();
    testChord();
    testMelodyAndRepeats();
    testTempo();
    testTempoFromAudio();
    testMusicXml();
    testMidi();
    testRobustness();
    testLoopPlayback();
    std::cout << std::endl << (checks - failures) << "/" << checks << " checks passed" << std::endl;
    juce::MessageManager::deleteInstance();
    return failures == 0 ? 0 : 1;
}
