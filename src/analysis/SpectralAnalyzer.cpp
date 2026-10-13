// SPDX-License-Identifier: GPL-3.0-or-later
#include "SpectralAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace Soundstave {

SpectralAnalyzer::SpectralAnalyzer() = default;

SpectralAnalyzer::~SpectralAnalyzer()
{
    alive->store(false);
    stopWorker();
}

void SpectralAnalyzer::stopWorker()
{
    cancelled.store(true);
    if (worker.joinable())
        worker.join();
}

void SpectralAnalyzer::cancel()
{
    cancelled.store(true);
    ++generation;   // drop any completion message that is already queued
}

void SpectralAnalyzer::analyse(const juce::AudioBuffer<float>& buffer,
                               double sampleRate,
                               const AnalysisParams& params,
                               std::function<void(SpectrogramPtr)> onComplete)
{
    stopWorker();

    cancelled.store(false);
    running.store(true);
    progress.store(0.0f);
    const int gen = ++generation;

    // Mix down to mono on the calling thread (cheap) so the worker owns its data
    const int total = buffer.getNumSamples();
    const int numCh = std::max(1, buffer.getNumChannels());
    auto mono = std::make_shared<std::vector<float>>((size_t)total, 0.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        for (int s = 0; s < total; ++s)
            (*mono)[(size_t)s] += src[s] / (float)numCh;
    }

    auto aliveFlag = alive;
    worker = std::thread([this, mono, sampleRate, params, gen, aliveFlag, onComplete]
    {
        auto data = compute(*mono, sampleRate, params, &cancelled, &progress);
        const bool wasCancelled = cancelled.load();
        running.store(false);

        if (!wasCancelled)   // data == nullptr means the input could not be analysed
        {
            SpectrogramPtr result = data;
            juce::MessageManager::callAsync([this, aliveFlag, gen, result, onComplete]
            {
                if (aliveFlag->load() && gen == generation.load() && onComplete)
                    onComplete(result);
            });
        }
    });
}

namespace {

// One STFT pass: periodic Hann window, frames centred on k*hop (zero padded
// at the edges), output in dBFS where a full-scale sine reads 0 dB.
// Returns false if cancelled. Progress is reported in [p0, p1].
bool runStft(const std::vector<float>& mono, int N, int hop, int numBins, float floorDb,
             std::vector<float>& out, int numFrames, float* maxDbOut,
             const std::atomic<bool>* cancelFlag, std::atomic<float>* progressOut, float p0, float p1)
{
    const int totalSamples = (int)mono.size();
    out.assign((size_t)numFrames * (size_t)numBins, floorDb);

    std::vector<float> window((size_t)N);
    for (int i = 0; i < N; ++i)
        window[(size_t)i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)N));
    const float normRef = (float)N * 0.25f;   // coherent gain 0.5 → sine peak = N/4

    juce::dsp::FFT fft(juce::roundToInt(std::log2((double)N)));
    std::vector<float> buf((size_t)N * 2, 0.0f);
    float maxDb = floorDb;

    for (int f = 0; f < numFrames; ++f)
    {
        if (cancelFlag != nullptr && cancelFlag->load()) return false;

        const int start = f * hop - N / 2;
        std::fill(buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i)
        {
            const int s = start + i;
            if (s >= 0 && s < totalSamples)
                buf[(size_t)i] = mono[(size_t)s] * window[(size_t)i];
        }

        fft.performFrequencyOnlyForwardTransform(buf.data(), true);

        float* row = out.data() + (size_t)f * (size_t)numBins;
        for (int b = 0; b < numBins; ++b)
        {
            const float db = 20.0f * std::log10(std::max(buf[(size_t)b] / normRef, 1e-9f));
            row[b] = std::max(floorDb, db);
            if (b > 2) maxDb = std::max(maxDb, row[b]);
        }

        if (progressOut != nullptr && (f & 63) == 0)
            progressOut->store(p0 + (p1 - p0) * (float)f / (float)numFrames);
    }

    if (maxDbOut != nullptr) *maxDbOut = maxDb;
    return true;
}

int binsUpTo(float maxFreqHz, int N, double sampleRate)
{
    return juce::jlimit(2, N / 2 + 1, (int)std::ceil((double)maxFreqHz * (double)N / sampleRate) + 2);
}

} // namespace

std::shared_ptr<SpectrogramData> SpectralAnalyzer::compute(const std::vector<float>& mono,
                                                           double sampleRate,
                                                           const AnalysisParams& params,
                                                           const std::atomic<bool>* cancelFlag,
                                                           std::atomic<float>* progressOut)
{
    if (!(sampleRate >= kMinSampleRate && sampleRate <= kMaxSampleRate) || mono.empty())
        return nullptr;

    auto data = std::make_shared<SpectrogramData>();
    const int totalSamples = (int)mono.size();
    const float maxFreq = std::min(params.maxFreqHz, (float)(sampleRate * 0.5));

    // ---- Long window: pitch resolution ----
    const int N   = 1 << juce::jlimit(9, 15, params.fftOrder);
    const int hop = std::max(64, N / std::max(1, params.hopDivisor));
    data->fftSize    = N;
    data->hopSize    = hop;
    data->sampleRate = sampleRate;
    data->floorDb    = params.floorDb;
    data->numBins    = binsUpTo(maxFreq, N, sampleRate);
    data->numFrames  = totalSamples / hop + 1;
    if (!runStft(mono, N, hop, data->numBins, params.floorDb, data->magsDb, data->numFrames,
                 &data->maxDb, cancelFlag, progressOut, 0.0f, 0.8f))
        return nullptr;

    // ---- Short window (30–60 ms): onset/re-attack timing ----
    int Ns = 2048;
    while (Ns > 256   && (double)Ns / sampleRate > 0.06) Ns /= 2;
    while (Ns < 32768 && (double)Ns / sampleRate < 0.03) Ns *= 2;
    const int hopS = std::max(1, Ns / 4);
    data->shortFftSize   = Ns;
    data->shortHopSize   = hopS;
    data->shortNumBins   = binsUpTo(maxFreq, Ns, sampleRate);
    data->shortNumFrames = totalSamples / hopS + 1;
    if (!runStft(mono, Ns, hopS, data->shortNumBins, params.floorDb, data->shortMagsDb, data->shortNumFrames,
                 nullptr, cancelFlag, progressOut, 0.8f, 1.0f))
        return nullptr;

    if (progressOut != nullptr) progressOut->store(1.0f);
    return data;
}

} // namespace Soundstave
