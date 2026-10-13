// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <atomic>
#include <vector>
#include <array>

namespace Soundstave {

// A note for MIDI playback (independent of the model, so audio/ does not
// depend on model/).
struct PlaybackNote
{
    double startSec;
    double endSec;
    int    midi;
    int    velocity;
};

// -------------------------------------------------------------------------
// Built-in instrument: a small additive "electric piano" voice
// -------------------------------------------------------------------------
struct SynthVoice
{
    enum class Source { Preview, Sequence };
    enum class Stage  { Off, Attack, Decay, Release };

    int    midi      = -1;
    float  gain      = 0.0f;
    double phase     = 0.0;
    double phaseInc  = 0.0;     // cycles per sample
    float  env       = 0.0f;
    float  decayCoef = 0.0f;
    float  releaseCoef = 0.0f;
    float  attackInc = 0.0f;
    float  brightness = 1.0f;   // decays: upper partials fade faster
    float  brightCoef = 1.0f;
    Stage  stage     = Stage::Off;
    Source source    = Source::Preview;
    int    samplesLeft = -1;    // auto-release for previews (-1 = hold)
    juce::uint64 age = 0;

    void start(int note, int vel, double sampleRate, Source src, int holdSamples, juce::uint64 order);
    void release();
    bool isActive() const noexcept { return stage != Stage::Off; }
    float render() noexcept;
};

// -------------------------------------------------------------------------
// AudioEngine – file loading, transport, loop, MIDI playback, preview
// -------------------------------------------------------------------------
class AudioEngine : public juce::AudioSource,
                    private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // ---- Device ----------------------------------------------------------------
    bool initAudioDevice();          // false if no output device could be opened
    void shutdownAudio();
    juce::String getDeviceError() const { return deviceError; }

    // ---- File ------------------------------------------------------------------
    bool   loadFile(const juce::File& file, juce::String& errorOut);
    void   closeFile();
    bool   isFileLoaded() const noexcept { return memorySource != nullptr; }
    double getTotalLengthSeconds() const noexcept;
    double getSampleRate() const noexcept { return fileSampleRate; }
    const juce::AudioBuffer<float>* getAudioBuffer() const noexcept;
    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    // ---- Transport -------------------------------------------------------------
    void   play();
    void   pause();
    void   stop();
    void   setPosition(double seconds);
    double getPosition() const noexcept;
    bool   isPlaying() const noexcept;

    // Loop section (clamped to the file). Looping is active only when enabled
    // and the section is at least 50 ms long.
    void setLoop(bool enabled, double startSec, double endSec);
    void clearLoopRegion();
    bool isLooping() const noexcept { return loopEnabled.load(); }

    // ---- Mixer -----------------------------------------------------------------
    void setAudioVolume(float v) noexcept { audioVolume = v; }
    void setMidiVolume (float v) noexcept { midiVolume  = v; }
    void setAudioEnabled(bool b) noexcept { audioEnabled = b; }
    void setMidiEnabled (bool b) noexcept { midiEnabled  = b; }

    // ---- MIDI ------------------------------------------------------------------
    void setPlaybackNotes(std::vector<PlaybackNote> notes);   // message thread
    void previewNote(int midi, int velocity, double durationSec = 0.5);

    // Called on the message thread ~30x per second
    std::function<void(double positionSec)> onPositionChanged;

    // ---- AudioSource -----------------------------------------------------------
    void prepareToPlay(int blockSize, double sr) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;

private:
    void timerCallback() override;

    struct SynthEvent { int offset; bool on; int midi; int vel; };
    void processSegment(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                        bool playing, double pos0);
    void handlePreviewQueue();
    void renderSynth(float* left, float* right, int numSamples,
                     const SynthEvent* events, int numEvents, float gain);
    void releaseSequenceVoices();
    void noteOnVoice(int midi, int vel, SynthVoice::Source src, int holdSamples);
    void noteOffVoice(int midi);

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioSourcePlayer  sourcePlayer;
    juce::String             deviceError;

    juce::AudioBuffer<float>                  decodedBuffer;
    std::unique_ptr<juce::MemoryAudioSource>  memorySource;
    juce::AudioTransportSource                transport;
    double fileSampleRate  = 44100.0;
    int    fileNumChannels = 0;

    std::atomic<float> audioVolume  { 0.8f };
    std::atomic<float> midiVolume   { 0.6f };
    std::atomic<bool>  audioEnabled { true };
    std::atomic<bool>  midiEnabled  { true };

    std::atomic<bool>   loopEnabled { false };
    std::atomic<double> loopStart   { 0.0 };
    std::atomic<double> loopEnd     { 0.0 };

    // Sequence (shared with audio thread)
    juce::SpinLock            sequenceLock;
    std::vector<PlaybackNote> sequence;

    // Preview requests: message thread → audio thread, lock-free
    struct PreviewRequest { int midi; int velocity; double duration; };
    juce::AbstractFifo                 previewFifo { 64 };
    std::array<PreviewRequest, 64>     previewQueue {};

    // Voices – owned exclusively by the audio thread
    static constexpr int kVoices = 32;
    std::array<SynthVoice, kVoices> voices;
    juce::uint64 voiceCounter = 0;
    std::array<SynthEvent, 1024> eventScratch {};

    double playbackSampleRate = 44100.0;
    bool   wasPlaying   = false;
    double lastBlockEnd = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

} // namespace Soundstave
