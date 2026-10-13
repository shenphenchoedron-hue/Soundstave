// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "SpectralAnalyzer.h"
#include "../model/NoteModel.h"
#include <juce_events/juce_events.h>
#include <functional>
#include <atomic>
#include <thread>

namespace Soundstave {

// -------------------------------------------------------------------------
// Detection parameters (defaults work for most recordings)
// -------------------------------------------------------------------------
struct DetectionParams
{
    float  sensitivityDb   = 50.0f;  // ignore peaks more than this below the loudest bin in the file
    float  frameRangeDb    = 30.0f;  // ignore peaks more than this below the loudest peak in the frame
    float  harmonicKeepDb  = 3.0f;   // a harmonic louder than its fundamental by this much may be its own note
    int    minMidiNote     = 21;     // A0
    int    maxMidiNote     = 108;    // C8
    double minNoteDuration = 0.06;   // seconds
    float  onsetDb         = 6.0f;   // level jump that starts a new note on the same pitch
    int    gapFrames       = 2;      // dropouts tolerated inside a note
    bool   polyphonic      = true;
    int    maxPolyphony    = 6;
};

// -------------------------------------------------------------------------
// PitchDetector: spectral peaks → harmonic salience (greedy harmonic
// subtraction) → per-pitch note tracking. Runs on a background thread;
// the result is delivered on the message thread.
//
// To add a different algorithm, implement the same detect()/cancel()/
// getProgress() interface on SpectrogramPtr input (see AGENTS.md).
// -------------------------------------------------------------------------
class PitchDetector
{
public:
    PitchDetector();
    ~PitchDetector();

    void detect(SpectrogramPtr spectrogram,
                const DetectionParams& params,
                std::function<void(std::vector<Note>)> onComplete);

    void  cancel();
    bool  isRunning() const noexcept   { return running.load(); }
    float getProgress() const noexcept { return progress.load(); }

    // Synchronous core (also used by tests)
    static std::vector<Note> run(const SpectrogramData& data,
                                 const DetectionParams& params,
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchDetector)
};

} // namespace Soundstave
