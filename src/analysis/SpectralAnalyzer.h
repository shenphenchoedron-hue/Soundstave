// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>

namespace Soundstave {

// -------------------------------------------------------------------------
// STFT parameters
// -------------------------------------------------------------------------
// Sample rates outside this range are rejected (malformed headers)
constexpr double kMinSampleRate = 8000.0;
constexpr double kMaxSampleRate = 768000.0;

struct AnalysisParams
{
    int   fftOrder   = 13;      // FFT size = 2^fftOrder (8192 → 5.4 Hz/bin @ 44.1 kHz)
    int   hopDivisor = 8;       // hop = fftSize / hopDivisor (8192/8 = 1024 → 23 ms)
    float maxFreqHz  = 6000.0f; // bins above this are not stored (C8 = 4186 Hz)
    float floorDb    = -140.0f; // magnitudes are clamped to this floor
};

// -------------------------------------------------------------------------
// Immutable STFT result, shared between display and pitch detection.
// Magnitudes are stored contiguously: frame-major, in dBFS (0 dB = full
// scale sine). Frame i is centred at time i * hopSize / sampleRate.
// -------------------------------------------------------------------------
struct SpectrogramData
{
    std::vector<float> magsDb;
    int    numFrames  = 0;
    int    numBins    = 0;
    int    fftSize    = 0;
    int    hopSize    = 0;
    double sampleRate = 44100.0;
    float  maxDb      = -140.0f;   // loudest bin in the whole file
    float  floorDb    = -140.0f;

    // Companion short-window STFT (≈46 ms) used for onset detection: the long
    // window resolves pitch, the short one resolves re-attacks in time.
    std::vector<float> shortMagsDb;
    int    shortNumFrames = 0;
    int    shortNumBins   = 0;
    int    shortFftSize   = 0;
    int    shortHopSize   = 0;

    const float* frame(int i) const noexcept { return magsDb.data() + (size_t)i * (size_t)numBins; }
    const float* shortFrame(int k) const noexcept { return shortMagsDb.data() + (size_t)k * (size_t)shortNumBins; }
    double shortFrameTime(int k) const noexcept { return (double)k * (double)shortHopSize / sampleRate; }
    double shortFreqToBin(double f) const noexcept { return f * (double)shortFftSize / sampleRate; }
    double frameTime(int i) const noexcept   { return (double)i * (double)hopSize / sampleRate; }
    double frameDuration() const noexcept    { return (double)hopSize / sampleRate; }
    double binToFreq(double bin) const noexcept { return bin * sampleRate / (double)fftSize; }
    double freqToBin(double f) const noexcept   { return f * (double)fftSize / sampleRate; }
};

using SpectrogramPtr = std::shared_ptr<const SpectrogramData>;

// -------------------------------------------------------------------------
// SpectralAnalyzer – runs the STFT on a background thread.
// Progress is published through getProgress() (poll from the UI);
// completion is delivered on the message thread.
// -------------------------------------------------------------------------
class SpectralAnalyzer
{
public:
    SpectralAnalyzer();
    ~SpectralAnalyzer();

    void analyse(const juce::AudioBuffer<float>& buffer,
                 double sampleRate,
                 const AnalysisParams& params,
                 std::function<void(SpectrogramPtr)> onComplete);

    void  cancel();
    bool  isRunning() const noexcept   { return running.load(); }
    float getProgress() const noexcept { return progress.load(); }

    // Synchronous STFT core (used by the worker thread and by tests)
    static std::shared_ptr<SpectrogramData> compute(const std::vector<float>& mono,
                                                    double sampleRate,
                                                    const AnalysisParams& params,
                                                    const std::atomic<bool>* cancelFlag = nullptr,
                                                    std::atomic<float>* progressOut = nullptr);

private:
    void stopWorker();

    std::atomic<bool>  running   { false };
    std::atomic<bool>  cancelled { false };
    std::atomic<float> progress  { 0.0f };
    std::atomic<int>   generation { 0 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    std::thread worker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralAnalyzer)
};

} // namespace Soundstave
