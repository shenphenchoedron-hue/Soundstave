// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../model/NoteModel.h"
#include "SpectralAnalyzer.h"
#include <vector>

namespace Soundstave {

// Estimates tempo (quarter-note BPM) and beat phase from note onsets.
// Each candidate tempo is scored by how strongly the onsets cluster on a
// common beat phase (circular mean resultant), with a mild preference for
// tempi around 110 BPM to avoid half/double-tempo errors.
class TempoEstimator
{
public:
    struct Result
    {
        bool   valid      = false;
        double bpm        = 120.0;
        double offsetSec  = 0.0;   // grid origin in (-beat, 0]
        double confidence = 0.0;   // 0..1
    };

    // From note onsets (needs detected notes)
    static Result estimate(const std::vector<Note>& notes,
                           double minBpm = 60.0, double maxBpm = 180.0);

    // From the audio itself: onset-strength envelope (spectral flux of the
    // short-window STFT) → autocorrelation with a tempo prior → beat phase.
    // Works before note detection and tolerates loose, human timing.
    static Result estimate(const SpectrogramData& spectrogram,
                           double minBpm = 60.0, double maxBpm = 180.0);
};

} // namespace Soundstave
