// SPDX-License-Identifier: GPL-3.0-or-later
#include "TempoEstimator.h"
#include <cmath>
#include <algorithm>

namespace Soundstave {

namespace {

struct Onset { double t; double w; };

// Circular resultant of onsets for a pulse period: 1 = every onset on the grid
double resultant(const std::vector<Onset>& onsets, double totalW, double period, double* phaseOut = nullptr)
{
    const double twoPi = juce::MathConstants<double>::twoPi;
    double re = 0.0, im = 0.0;
    for (auto& o : onsets)
    {
        const double ph = twoPi * o.t / period;
        re += o.w * std::cos(ph);
        im += o.w * std::sin(ph);
    }
    if (phaseOut) *phaseOut = std::atan2(im, re);
    return std::sqrt(re * re + im * im) / totalW;
}

} // namespace

TempoEstimator::Result TempoEstimator::estimate(const std::vector<Note>& notes,
                                                double minBpm, double maxBpm)
{
    Result r;

    // Distinct onsets (chords count once), weighted by velocity
    std::vector<Onset> onsets;
    {
        std::vector<Note> sorted = notes;
        std::sort(sorted.begin(), sorted.end(),
                  [](const Note& a, const Note& b){ return a.startSec < b.startSec; });
        for (auto& n : sorted)
        {
            const double w = 0.3 + (double)n.velocity / 127.0;
            if (!onsets.empty() && n.startSec - onsets.back().t < 0.035)
                onsets.back().w = std::max(onsets.back().w, w);
            else
                onsets.push_back({ n.startSec, w });
        }
    }
    if (onsets.size() < 4) return r;

    double totalW = 0.0;
    for (auto& o : onsets) totalW += o.w;

    // 1) Tatum: the slowest pulse that (almost) all onsets fit on.
    //    Faster multiples fit equally well, slower ones split onsets between
    //    on- and off-beats and score low, so "slowest good fit" is the tatum.
    constexpr double kMinTatum = 40.0, kMaxTatum = 480.0;
    std::vector<std::pair<double, double>> scores;   // (bpm, R)
    double best = 0.0;
    for (double bpm = kMinTatum; bpm <= kMaxTatum; bpm *= 1.0015)
    {
        const double R = resultant(onsets, totalW, 60.0 / bpm);
        scores.push_back({ bpm, R });
        best = std::max(best, R);
    }
    if (best < 0.3) return r;

    double tatum = 0.0;
    for (size_t i = 0; i < scores.size(); ++i)
    {
        if (scores[i].second < 0.85 * best) continue;
        // walk to the local maximum of this peak
        size_t j = i;
        while (j + 1 < scores.size() && scores[j + 1].second >= scores[j].second) ++j;
        tatum = scores[j].first;
        break;
    }
    if (tatum <= 0.0) return r;

    // 2) Beat level: tatum / k, preferring tempi near 110 BPM
    double bpm = tatum;
    double bestDist = 1e9;
    for (int k = 1; k <= 4; ++k)
    {
        const double cand = tatum / k;
        if (cand < minBpm || cand > maxBpm) continue;
        const double dist = std::abs(std::log2(cand / 110.0));
        if (dist < bestDist) { bestDist = dist; bpm = cand; }
    }
    if (bestDist > 1e8) bpm = juce::jlimit(minBpm, maxBpm, tatum);
    const int k = std::max(1, juce::roundToInt(tatum / bpm));

    // 3) Phase: tatum grid phase, then choose which tatum position is the beat
    //    (the one carrying the strongest onsets)
    const double twoPi = juce::MathConstants<double>::twoPi;
    const double tatumPeriod = 60.0 / tatum;
    double phase = 0.0;
    const double confidence = resultant(onsets, totalW, tatumPeriod, &phase);
    const double tatumOffset = phase / twoPi * tatumPeriod;

    const double beat = 60.0 / bpm;
    double bestOffset = tatumOffset, bestScore = -1e9;
    for (int j = 0; j < k; ++j)
    {
        const double off = tatumOffset + j * tatumPeriod;
        double s = 0.0;
        for (auto& o : onsets) s += o.w * std::cos(twoPi * (o.t - off) / beat);
        if (s > bestScore) { bestScore = s; bestOffset = off; }
    }

    double offset = std::fmod(bestOffset, beat);
    while (offset > 0.0)    offset -= beat;
    while (offset <= -beat) offset += beat;

    r.valid      = true;
    // Recordings are usually played to whole metronome values
    r.bpm        = std::abs(bpm - std::round(bpm)) < 0.75 ? std::round(bpm) : std::round(bpm * 2.0) / 2.0;
    r.offsetSec  = offset;
    r.confidence = confidence;
    return r;
}

// =========================================================================
// Audio-based estimate
// =========================================================================
TempoEstimator::Result TempoEstimator::estimate(const SpectrogramData& d, double minBpm, double maxBpm)
{
    Result r;
    const int K = d.shortNumFrames, nb = d.shortNumBins;
    if (K < 64 || nb < 8 || d.shortHopSize <= 0) return r;
    const double fps = d.sampleRate / (double)d.shortHopSize;
    if ((double)K / fps < 3.0) return r;   // need a few seconds

    // 1) Onset strength: half-wave rectified log-spectral flux, with a floor
    //    so background noise does not contribute
    const float floorDb = d.maxDb - 60.0f;
    std::vector<double> onset((size_t)K, 0.0);
    for (int k = 1; k < K; ++k)
    {
        const float* a = d.shortFrame(k - 1);
        const float* b = d.shortFrame(k);
        double flux = 0.0;
        for (int i = 1; i < nb; ++i)
        {
            const float diff = std::max(b[i], floorDb) - std::max(a[i], floorDb);
            if (diff > 0.0f) flux += diff;
        }
        onset[(size_t)k] = flux;
    }

    // Remove the slowly varying part (≈0.4 s moving average) and rectify
    {
        const int half = std::max(1, (int)(0.2 * fps));
        std::vector<double> prefix((size_t)K + 1, 0.0);
        for (int k = 0; k < K; ++k) prefix[(size_t)k + 1] = prefix[(size_t)k] + onset[(size_t)k];
        std::vector<double> out((size_t)K);
        for (int k = 0; k < K; ++k)
        {
            const int a = std::max(0, k - half), b = std::min(K, k + half + 1);
            const double mean = (prefix[(size_t)b] - prefix[(size_t)a]) / (double)(b - a);
            out[(size_t)k] = std::max(0.0, onset[(size_t)k] - mean);
        }
        onset.swap(out);
    }

    double energy = 0.0;
    for (double v : onset) energy += v * v;
    if (energy <= 1e-9) return r;

    // 2) Autocorrelation over the lags of interest (40–240 BPM)
    auto acf = [&](double lag)
    {
        const int L = (int)std::floor(lag);
        const double f = lag - L;
        double sum = 0.0;
        for (int k = 0; k + L + 1 < K; ++k)
            sum += onset[(size_t)k] * (onset[(size_t)(k + L)] * (1.0 - f) + onset[(size_t)(k + L + 1)] * f);
        return sum / energy;
    };

    const int lagMin = std::max(2, (int)std::floor(fps * 60.0 / 240.0));
    const int lagMax = std::min(K / 2, (int)std::ceil(fps * 60.0 / 40.0));
    if (lagMax <= lagMin + 2) return r;

    // 3) Tempo score: a true beat period also lines up at 2, 3 and 4 beats
    //    (a dotted-rhythm period does not), so sum the periodicity over these
    //    multiples. A broad log-normal prior around 105 BPM breaks the
    //    remaining half/double ambiguity.
    std::vector<double> score((size_t)lagMax + 2, 0.0);
    double bestScore = -1.0;
    int bestLag = 0;
    for (int L = lagMin; L <= lagMax + 1; ++L)
    {
        double s = 0.0;
        for (int m = 1; m <= 4; ++m)
            if (m * L < K / 2) s += acf((double)(m * L)) / std::sqrt((double)m);
        score[(size_t)L] = s;
    }
    for (int L = lagMin + 1; L <= lagMax; ++L)
    {
        const double bpm = 60.0 * fps / L;
        if (bpm < minBpm || bpm > maxBpm) continue;
        const double lg = std::log2(bpm / 105.0);
        const double s = score[(size_t)L] * std::exp(-0.5 * lg * lg);
        if (s > bestScore) { bestScore = s; bestLag = L; }
    }
    if (bestLag <= 0 || acf((double)bestLag) <= 0.02) return r;

    // Parabolic refinement of the lag
    double lag = (double)bestLag;
    {
        const double y0 = score[(size_t)bestLag - 1], y1 = score[(size_t)bestLag], y2 = score[(size_t)bestLag + 1];
        const double den = y0 - 2.0 * y1 + y2;
        if (std::abs(den) > 1e-12) lag += juce::jlimit(-0.5, 0.5, 0.5 * (y0 - y2) / den);
    }
    // 4) Joint refinement of period and phase: a comb over the onset envelope
    //    (±1 frame tolerance for human timing). Refining the period here keeps
    //    a tiny period error from drifting the grid over a long piece.
    auto combSum = [&](double period, double ph)
    {
        double s = 0.0;
        for (double t = ph; t < (double)K - 1; t += period)
        {
            const int k = (int)std::round(t);
            s += std::max({ onset[(size_t)std::max(0, k - 1)], onset[(size_t)k],
                            onset[(size_t)std::min(K - 1, k + 1)] });
        }
        return s;
    };

    double bestPhaseScore = -1.0, bestPhase = 0.0, bestPeriod = lag;
    for (double period = lag * 0.985; period <= lag * 1.015; period += lag * 0.001)
    {
        const int phases = std::max(8, (int)std::round(period * 2.0));
        for (int p = 0; p < phases; ++p)
        {
            const double ph = period * (double)p / (double)phases;
            const double sc = combSum(period, ph);
            if (sc > bestPhaseScore) { bestPhaseScore = sc; bestPhase = ph; bestPeriod = period; }
        }
    }
    lag = bestPeriod;
    const double bpm = 60.0 * fps / lag;
    const double beat = 60.0 / bpm;

    double offset = std::fmod(bestPhase / fps, beat);
    while (offset > 0.0)    offset -= beat;
    while (offset <= -beat) offset += beat;

    r.valid      = true;
    r.bpm        = std::abs(bpm - std::round(bpm)) < 0.75 ? std::round(bpm) : std::round(bpm * 2.0) / 2.0;
    r.offsetSec  = offset;
    r.confidence = juce::jlimit(0.0, 1.0, acf(lag));
    return r;
}

} // namespace Soundstave
