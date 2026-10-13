# Soundstave – Agent Guidelines

This file provides guidance for AI coding agents working on the Soundstave codebase.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel 2>&1 | head -100
```

Always build before committing. Fix all errors and warnings before declaring work done.

## Code conventions

- C++20 throughout. Use `std::` types, not JUCE wrappers, where both are equally convenient.
- All Soundstave types live in the `Soundstave` namespace.
- Public headers must compile cleanly with `-Wall -Wextra`.
- JUCE components must be created and destroyed on the message thread.
- Heavy work (FFT, pitch detection) runs in `std::thread`; results are delivered back via `juce::MessageManager::callAsync`.
- `NoteModel` is the single source of truth for note data. All edits go through it; never store notes elsewhere long-term.
- Undo-able actions use `juce::UndoManager` via the command classes in `src/model/NoteEditCommands.h`.

## Module boundaries

| Module | Responsibility | Must NOT |
|---|---|---|
| `audio/` | Audio I/O, playback, MIDI synth | Know about UI |
| `analysis/` | STFT, pitch detection | Know about UI or NoteModel directly |
| `model/` | Note data, undo history | Know about UI or audio |
| `export/` | MIDI / MusicXML generation | Know about UI |
| `project/` | Serialise/deserialise app state | Know about audio engine internals |
| `ui/` | All visual components | Contain business logic |

## Adding a new pitch detection algorithm

1. Create a class in `src/analysis/` following the same interface as `PitchDetector`.
2. Wire it into `MainComponent::runAnalysis()`.
3. The `SpectralAnalyzer` frames (STFT output) are already available as input.
4. Do not change `NoteModel` or any export module.

## Licensing

All new source files must begin with the GPL-3 SPDX header:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
```

Third-party libraries must have GPL-compatible licenses. Check before adding dependencies.
