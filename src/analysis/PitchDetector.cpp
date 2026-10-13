// SPDX-License-Identifier: GPL-3.0-or-later
#include "PitchDetector.h"
#include <cmath>
#include <algorithm>
#include <map>

namespace Soundstave {

PitchDetector::PitchDetector() = default;

PitchDetector::~PitchDetector()
{
    alive->store(false);
    stopWorker();
}

void PitchDetector::stopWorker()
{
    cancelled.store(true);
    if (worker.joinable())
        worker.join();
}

void PitchDetector::cancel()
{
    cancelled.store(true);
    ++generation;
}

void PitchDetector::detect(SpectrogramPtr spectrogram,
                           const DetectionParams& params,
                           std::function<void(std::vector<Note>)> onComplete)
{
    stopWorker();
    if (spectrogram == nullptr) return;

    cancelled.store(false);
    running.store(true);
    progress.store(0.0f);
    const int gen = ++generation;
    auto aliveFlag = alive;

    worker = std::thread([this, spectrogram, params, gen, aliveFlag, onComplete]
    {
        auto notes = run(*spectrogram, params, &cancelled, &progress);
        const bool wasCancelled = cancelled.load();
        running.store(false);

        if (!wasCancelled)
        {
            juce::MessageManager::callAsync([this, aliveFlag, gen, onComplete, notes = std::move(notes)]() mutable
            {
                if (aliveFlag->load() && gen == generation.load() && onComplete)
                    onComplete(std::move(notes));
            });
        }
    });
}

// =========================================================================
// Core algorithm
// =========================================================================
namespace {

struct Peak
{
    double freq;     // Hz (parabolically interpolated)
    double log2f;
    float  db;
    float  amp;      // linear amplitude
};

struct Detected
{
    int   midi;
    float db;
};

struct Track
{
    bool   active = false;
    int    startFrame = 0;
    int    lastFrame  = 0;
    float  lastDb     = -200.0f;
    double forcedStart = -1.0;   // exact start from onset detection (re-attack)
    std::vector<float> levels;   // dB per frame from startFrame (gaps filled with floor)
};

// Level of a frequency in the short-window spectrogram (max of 3 bins)
float shortLevel(const SpectrogramData& d, int k, double freq)
{
    if (k < 0 || k >= d.shortNumFrames) return d.floorDb;
    const int b = (int)std::round(d.shortFreqToBin(freq));
    if (b < 1 || b >= d.shortNumBins - 1) return d.floorDb;
    const float* fr = d.shortFrame(k);
    return std::max({ fr[b - 1], fr[b], fr[b + 1] });
}

// Looks for a re-attack of an already sounding pitch between tA and tB using
// the short-window spectrogram. Both the fundamental and the 2nd harmonic
// must jump, so a new note an octave higher does not split a held note.
double findReattack(const SpectrogramData& d, int midi, double tA, double tB,
                    double noteStart, float onsetDb, float minLevel)
{
    if (d.shortNumFrames == 0) return -1.0;
    const double hop = (double)d.shortHopSize / d.sampleRate;
    const double f0 = Note::midiToFreq(midi);
    const int k0 = std::max(4, (int)std::ceil(tA / hop));
    const int k1 = std::min(d.shortNumFrames - 1, (int)std::floor(tB / hop));

    for (int k = k0; k <= k1; ++k)
    {
        if (d.shortFrameTime(k) - noteStart < 0.08) continue;
        float rise = 1.0e9f, level = -1.0e9f;
        for (int h = 1; h <= 2; ++h)
        {
            const double f = f0 * h;
            if (d.shortFreqToBin(f) >= d.shortNumBins - 2) continue;
            const float L = shortLevel(d, k, f);
            float base = L;
            for (int j = 1; j <= 4; ++j) base = std::min(base, shortLevel(d, k - j, f));
            rise  = std::min(rise, L - base);
            level = std::max(level, L);
        }
        if (rise < 1.0e8f && rise > onsetDb && level > minLevel)
            return std::max(noteStart + 0.05, d.shortFrameTime(k) - hop);
    }
    return -1.0;
}

inline float dbToAmp(float db) { return std::pow(10.0f, db / 20.0f); }

// Peaks in one frame, sorted by frequency
void findPeaks(const SpectrogramData& d, const float* mags, float threshold,
               int minBin, std::vector<Peak>& out)
{
    out.clear();
    const int last = d.numBins - 2;
    for (int b = std::max(1, minBin); b <= last; ++b)
    {
        const float c = mags[b];
        if (c < threshold || c < mags[b - 1] || c <= mags[b + 1]) continue;

        const float l = mags[b - 1], r = mags[b + 1];
        const float denom = l - 2.0f * c + r;
        float p = std::abs(denom) > 1.0e-9f ? 0.5f * (l - r) / denom : 0.0f;
        p = juce::jlimit(-0.5f, 0.5f, p);
        const float db = c - 0.25f * (l - r) * p;
        const double f = d.binToFreq((double)b + (double)p);
        if (f <= 0.0) continue;
        out.push_back({ f, std::log2(f), db, dbToAmp(db) });
    }

    // Keep the 48 strongest, then sort by frequency for harmonic look-ups
    if (out.size() > 48)
    {
        std::nth_element(out.begin(), out.begin() + 48, out.end(),
                         [](const Peak& a, const Peak& b){ return a.amp > b.amp; });
        out.resize(48);
    }
    std::sort(out.begin(), out.end(), [](const Peak& a, const Peak& b){ return a.freq < b.freq; });
}

// Index of the peak closest to target (log2 Hz) within tolerance, or -1
int nearestPeak(const std::vector<Peak>& peaks, double targetLog2, double tol)
{
    auto it = std::lower_bound(peaks.begin(), peaks.end(), targetLog2,
                               [](const Peak& p, double v){ return p.log2f < v; });
    int best = -1;
    double bestDist = tol;
    for (auto cand : { it, it == peaks.begin() ? it : it - 1 })
    {
        if (cand == peaks.end()) continue;
        const double dist = std::abs(cand->log2f - targetLog2);
        if (dist <= bestDist) { bestDist = dist; best = (int)(cand - peaks.begin()); }
    }
    return best;
}

constexpr int    kMaxHarmonics = 12;
constexpr double kHarmTol      = 0.035;   // ≈ 42 cents

// Harmonic salience of a candidate fundamental (peak index c)
float salience(const std::vector<Peak>& peaks, const std::vector<float>& avail, int c,
               int* matches = nullptr)
{
    const float a1 = avail[(size_t)c];
    if (a1 <= 0.0f) return 0.0f;
    float score = a1;
    int count = 1;
    for (int h = 2; h <= kMaxHarmonics; ++h)
    {
        const int k = nearestPeak(peaks, peaks[(size_t)c].log2f + std::log2((double)h), kHarmTol);
        if (k < 0 || k == c) continue;
        // Cap each harmonic's contribution so a weak spurious "fundamental"
        // cannot borrow the energy of a real note one octave up.
        const float contrib = std::min(avail[(size_t)k], 2.0f * a1);
        score += contrib / std::pow((float)h, 0.6f);
        ++count;
    }
    if (matches) *matches = count;
    return score;
}

void detectFrame(const std::vector<Peak>& peaks, const DetectionParams& p,
                 double minF, double maxF, std::vector<Detected>& out)
{
    out.clear();
    if (peaks.empty()) return;

    std::vector<float> avail(peaks.size());
    for (size_t i = 0; i < peaks.size(); ++i) avail[i] = peaks[i].amp;

    const int maxVoices = p.polyphonic ? std::max(1, p.maxPolyphony) : 1;
    const float keepRatio = dbToAmp(p.harmonicKeepDb);
    float firstScore = 0.0f;

    for (int voice = 0; voice < maxVoices; ++voice)
    {
        int best = -1;
        float bestScore = 0.0f;
        for (int c = 0; c < (int)peaks.size(); ++c)
        {
            if (peaks[(size_t)c].freq < minF || peaks[(size_t)c].freq > maxF) continue;
            const float s = salience(peaks, avail, c);
            if (s > bestScore) { bestScore = s; best = c; }
        }
        if (best < 0) break;
        if (voice > 0 && bestScore < firstScore * 0.10f) break;
        if (voice == 0) firstScore = bestScore;

        const double f0 = peaks[(size_t)best].freq;
        const int midi = juce::roundToInt(69.0 + 12.0 * std::log2(f0 / 440.0));
        const float a1 = avail[(size_t)best];

        bool duplicate = false;
        for (auto& d : out) if (d.midi == midi) duplicate = true;
        if (!duplicate && midi >= p.minMidiNote && midi <= p.maxMidiNote)
            out.push_back({ midi, peaks[(size_t)best].db });

        // Subtract this note's partials
        avail[(size_t)best] = 0.0f;
        for (int h = 2; h <= kMaxHarmonics; ++h)
        {
            const int k = nearestPeak(peaks, peaks[(size_t)best].log2f + std::log2((double)h), kHarmTol);
            if (k < 0) continue;
            if (peaks[(size_t)k].amp <= a1 * keepRatio)
                avail[(size_t)k] = 0.0f;                                  // fully explained
            else
                avail[(size_t)k] = std::max(0.0f, avail[(size_t)k] - a1); // possibly its own note
        }
    }
}

} // namespace

std::vector<Note> PitchDetector::run(const SpectrogramData& d,
                                     const DetectionParams& p,
                                     const std::atomic<bool>* cancelFlag,
                                     std::atomic<float>* progressOut)
{
    std::vector<Note> result;
    if (d.numFrames <= 0 || d.numBins <= 4) return result;

    const double minF = Note::midiToFreq((double)p.minMidiNote - 0.5);
    const double maxF = Note::midiToFreq((double)p.maxMidiNote + 0.5);
    const int minBin  = std::max(1, (int)std::floor(d.freqToBin(minF)) - 1);
    const float globalThr = d.maxDb - p.sensitivityDb;
    const double frameDur = d.frameDuration();

    std::vector<Track> tracks(128);
    std::vector<Peak> peaks;
    std::vector<Detected> detected;

    auto closeTrack = [&](int midi, double forcedEnd)
    {
        Track& t = tracks[(size_t)midi];
        if (!t.active) return;
        t.active = false;

        const int len = t.lastFrame - t.startFrame + 1;
        if (len <= 0 || t.levels.empty()) return;
        const float peakDb = *std::max_element(t.levels.begin(), t.levels.end());

        // Onset refinement: the long analysis window "sees" a note before it
        // starts. The window centre reaches the onset roughly when the level is
        // within a few dB of its maximum during the attack.
        int s = 0;
        while (s < len - 1 && t.levels[(size_t)s] < peakDb - 6.0f) ++s;
        // Release: last frame that is still within 30 dB of the peak
        int e = len - 1;
        while (e > s && t.levels[(size_t)e] < peakDb - 30.0f) --e;

        const double start = t.forcedStart >= 0.0 ? t.forcedStart
                                                  : d.frameTime(t.startFrame + s) - frameDur * 0.5;
        const double end   = forcedEnd >= 0.0 ? forcedEnd
                                              : d.frameTime(t.startFrame + e) + frameDur * 0.5;
        if (end - start < p.minNoteDuration) return;

        Note n;
        n.midiNote = midi;
        n.startSec = std::max(0.0, start);
        n.endSec   = end;
        const float rel = peakDb - d.maxDb;   // ≤ 0
        n.velocity = juce::jlimit(1, 127, juce::roundToInt(127.0f + rel * (107.0f / 40.0f)));
        n.fromAnalysis = true;
        result.push_back(n);
    };

    for (int f = 0; f < d.numFrames; ++f)
    {
        if (cancelFlag != nullptr && cancelFlag->load()) return {};
        if (progressOut != nullptr && (f & 63) == 0)
            progressOut->store((float)f / (float)d.numFrames);

        const float* mags = d.frame(f);
        float frameMax = d.floorDb;
        for (int b = minBin; b < d.numBins; ++b) frameMax = std::max(frameMax, mags[b]);
        const float thr = std::max(globalThr, frameMax - p.frameRangeDb);

        findPeaks(d, mags, thr, minBin, peaks);
        detectFrame(peaks, p, minF, maxF, detected);

        bool seen[128] = {};
        for (auto& det : detected)
        {
            seen[det.midi] = true;
            Track& t = tracks[(size_t)det.midi];
            double newStart = -1.0;
            if (t.active)
            {
                // Re-attack on the same pitch → new note
                const double noteStart = t.forcedStart >= 0.0 ? t.forcedStart : d.frameTime(t.startFrame);
                const double reattack = findReattack(d, det.midi, d.frameTime(t.lastFrame), d.frameTime(f),
                                                     noteStart, p.onsetDb, globalThr);
                const bool levelJump = det.db > t.lastDb + p.onsetDb && f - t.startFrame >= 3;

                if (reattack >= 0.0 || levelJump)
                {
                    closeTrack(det.midi, reattack);
                    newStart = reattack;
                }
                else
                {
                    for (int g = t.lastFrame + 1; g < f; ++g) t.levels.push_back(d.floorDb);
                    t.levels.push_back(det.db);
                    t.lastFrame = f;
                    t.lastDb = det.db;
                    continue;
                }
            }
            t.active = true;
            t.startFrame = t.lastFrame = f;
            t.lastDb = det.db;
            t.forcedStart = newStart;
            t.levels.assign(1, det.db);
        }

        for (int m = 0; m < 128; ++m)
            if (tracks[(size_t)m].active && !seen[m] && f - tracks[(size_t)m].lastFrame > p.gapFrames)
                closeTrack(m, -1.0);
    }
    for (int m = 0; m < 128; ++m) closeTrack(m, -1.0);

    std::sort(result.begin(), result.end(), [](const Note& a, const Note& b)
    {
        return !juce::exactlyEqual(a.startSec, b.startSec) ? a.startSec < b.startSec : a.midiNote < b.midiNote;
    });
    if (progressOut != nullptr) progressOut->store(1.0f);
    return result;
}

} // namespace Soundstave
