# Soundstave

**Soundstave** is an open-source (GPL-3.0) desktop application for spectral analysis of
recordings, automatic note detection and note editing, with export to Standard MIDI
(.mid) and MusicXML (.musicxml). It is inspired by Melodyne and AnthemScore.

Built with C++20, JUCE 8 and CMake for Linux, Windows and macOS.

## Features

- **Import** WAV, FLAC, AIFF and MP3 – via *Åbn* or drag-and-drop into the window.
- **Waveform** with timeline and playhead; click to seek.
- **Loop a section**: drag in the time ruler or waveform (snaps to the grid, hold *Alt*
  for free), drag the loop edges to adjust, *Loop* button / `L` to switch it on and off,
  `Ctrl+L` loops the selected notes. Looping is sample-accurate for both the audio and
  the MIDI notes, and the section is saved with the project.
- **High-resolution spectrogram** (8192-point STFT, Hann window, 23 ms hop) on a
  logarithmic, note-aligned pitch axis with a keyboard on the left. Every key, every
  spectrogram row and every note use the same lane mapping, so a note sits exactly on its
  fundamental. Adjustable dynamic range (default 80 dB below the loudest bin).
- **Piano-roll layer** over the spectrogram (toggle with *Pianoroll* / `P`): key lanes,
  bar/beat/grid lines and bar numbers.
- **Polyphonic note detection** in a background thread with progress and cancel:
  spectral peaks with sub-bin interpolation → harmonic salience with greedy harmonic
  subtraction (overtones are not reported as notes) → per-pitch note tracking with
  onset refinement and a short-window (46 ms) companion STFT that separates repeated
  notes on the same pitch. Estimates pitch, start, end, duration and velocity. Manually
  drawn/edited notes are kept when re-analysing.
- **Tempo detection** (*Find* button, automatic after the first analysis) finds BPM and
  beat phase so the grid lines up with a freely played recording.
- **Editing** directly on the spectrogram: select, rubber-band select, move (time and
  pitch), resize either end, draw, split, merge, delete, copy/cut/paste, transpose,
  nudge. Snapping to 1/4, 1/8, 1/16, 1/32, 1/8-triplet or free (hold *Alt* for free
  placement while dragging). Every edit is undoable.
- **Playback** of the original audio and the MIDI transcription, separately or together,
  with separate volume and on/off controls, loop playback, and a built-in electric-piano
  style instrument. Clicking a note or a key plays it; the keyboard lights up the notes
  that sound during playback.
- **MIDI export**: pitch, timing, duration, velocity and polyphony, tempo and time
  signature meta events, 960 PPQ.
- **MusicXML export** with a preview dialog: choose the quantization and see the
  quantized notes over the original timing before saving. Output has bars, time
  signature, tempo, rests, ties across bar lines, dotted values and triplets, chords,
  up to four voices per staff and a grand staff (treble + bass) when the range needs it.
  Opens in MuseScore, Dorico and Sibelius.
- **Projects** (`.soundstave`): audio reference (absolute and relative path), notes
  (analysis results and manual edits), tempo, meter, grid offset, view and analysis
  settings. Unsaved changes are indicated with `*` in the title and you are asked to save
  when closing or opening another file. Export does not require saving first.
- **Remembers layout**: window size/position, waveform height (drag the divider),
  piano-roll visibility, grid, volumes and settings.
- **Advanced settings** (*Indstillinger*): dynamic range, FFT size, sensitivity,
  max simultaneous notes (1 = monophonic), shortest note, lowest/highest note.

## Building

Requirements: CMake ≥ 3.22, a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022),
JUCE 8 in `libs/JUCE/`.

Linux packages:

```bash
sudo apt install libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev \
                 libxcursor-dev libfreetype-dev libfontconfig1-dev
```

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/Soundstave_artefacts/Release/Soundstave            # start
./build/Soundstave_artefacts/Release/Soundstave --analyse song.wav   # open + analyse
```

### Tests

`SoundstaveTests` runs synthetic signals through the STFT, note detection, tempo
estimation and both exporters (A4/C4/E2/C6 tones, a C-E-G chord with harmonics,
a sixteenth-note scale, repeated notes, a long note, MusicXML bar integrity, MIDI
round trip):

```bash
cmake --build build --target SoundstaveTests
./build/SoundstaveTests_artefacts/Release/SoundstaveTests
```

## Using Soundstave

1. Open a recording (*Åbn* or drag it into the window). The spectrogram is computed.
2. Click *Analysér* (`Ctrl+R`). Detected notes appear on top of the spectrogram.
3. Correct notes: drag to move, drag the ends to resize, *Tegn* to draw missing notes,
   right-click or `Delete` to remove, `Ctrl+Z` to undo.
4. Check the result: *Afspil* plays audio and MIDI together; use the *Lyd*/*MIDI*
   buttons and sliders to listen to one of them.
5. Check tempo and meter (*Find*, *Tempo*, *Takt*), then export *MIDI* or *MusicXML*.

### Mouse

| Action | Mouse |
|---|---|
| Scroll in time | Wheel / horizontal touchpad swipe / scrollbar |
| Scroll pitch | Shift + wheel / vertical scrollbar |
| Zoom time | Ctrl + wheel |
| Zoom pitch | Alt + wheel |
| Move playhead | Click ruler or waveform, double-click empty spectrogram |
| Loop | Drag in the ruler or waveform; drag the edges to adjust; double-click the waveform to remove |
| Delete note | Right-click (deletes the whole selection if the note is selected) |

### Keyboard

| Action | Key |
|---|---|
| Play / pause · Stop · Go to start | Space · Shift+Space · Home |
| Loop on/off · Loop selected notes | L · Ctrl+L |
| Open · Save · Save as | Ctrl+O · Ctrl+S · Ctrl+Shift+S |
| Analyse | Ctrl+R |
| Undo · Redo | Ctrl+Z · Ctrl+Shift+Z / Ctrl+Y |
| Select all · Deselect | Ctrl+A · Esc |
| Delete | Delete / Backspace |
| Copy · Cut · Paste (at playhead) | Ctrl+C · Ctrl+X · Ctrl+V |
| Merge selected notes | Ctrl+J |
| Transpose semitone · octave | ↑/↓ · Shift+↑/↓ |
| Nudge by one grid step | ←/→ |
| Zoom in · out · fit | Ctrl+= · Ctrl+- · Ctrl+0 |
| Tools: select · draw · split | S · D · C |
| Piano-roll layer | P |
| Export MIDI · MusicXML | Ctrl+E · Ctrl+Shift+E |
| Settings | Ctrl+, |

## Known limitations

- Detection is signal-processing based (no machine learning). It works well for
  piano, guitar, voice and other harmonic instruments; dense mixes with drums, strong
  reverb or many voices in the same octave produce more errors that need manual editing.
- A note exactly one octave above a louder note can be absorbed as its harmonic.
- Repeated notes are separated by their re-attack; a perfectly sustained repeat with no
  level change (e.g. organ) is detected as one long note.
- One tempo per project (no tempo changes). The key signature is always C major
  (accidentals are written as sharps).
- MusicXML triplets are written with `<time-modification>` but without explicit tuplet
  brackets.

## Architecture

```
src/
├── Main.cpp, MainWindow.*   application, window, close handling
├── audio/      AudioEngine – file decoding, transport, loop, MIDI playback, synth
├── analysis/   SpectralAnalyzer (STFT), PitchDetector, TempoEstimator
├── model/      NoteModel (single source of truth), snapshot undo
├── export/     QuantizationEngine, MidiExporter, MusicXmlExporter
├── project/    ProjectManager (.soundstave)
└── ui/         MainComponent and all views/dialogs
tests/          AnalysisTests (headless)
```

`PitchDetector` consumes an immutable `SpectrogramData` block, so another algorithm
(e.g. a neural model) can be added by implementing the same `detect()` interface – see
`AGENTS.md`.

## License

Soundstave is free software under the **GNU General Public License v3.0 or later**
(see [LICENSE](LICENSE)). JUCE is used under its GPLv3 option (`libs/JUCE/LICENSE.md`).
No other third-party libraries or models are used.
