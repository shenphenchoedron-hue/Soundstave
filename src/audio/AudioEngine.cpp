// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioEngine.h"
#include <cmath>
#include <algorithm>

namespace Soundstave {

// =========================================================================
// SynthVoice
// =========================================================================
void SynthVoice::start(int note, int vel, double sampleRate, Source src, int holdSamples, juce::uint64 order)
{
    midi     = note;
    source   = src;
    age      = order;
    phase    = 0.0;
    const double freq = 440.0 * std::pow(2.0, (note - 69) / 12.0);
    phaseInc = freq / sampleRate;

    const float v = juce::jlimit(1, 127, vel) / 127.0f;
    gain = 0.22f * (0.25f + 0.75f * v * v);

    // Lower notes ring longer, like a real piano/EP
    const double tau = juce::jlimit(0.25, 3.0, 1.2 * std::pow(440.0 / freq, 0.35));
    decayCoef   = (float)std::exp(-1.0 / (tau * sampleRate));
    releaseCoef = (float)std::exp(-1.0 / (0.09 * sampleRate));
    attackInc   = (float)(1.0 / (0.004 * sampleRate));
    brightness  = 1.0f;
    brightCoef  = (float)std::exp(-1.0 / (0.35 * sampleRate));

    env = 0.0f;
    stage = Stage::Attack;
    samplesLeft = holdSamples;
}

void SynthVoice::release()
{
    if (stage != Stage::Off) stage = Stage::Release;
}

float SynthVoice::render() noexcept
{
    switch (stage)
    {
        case Stage::Off: return 0.0f;
        case Stage::Attack:
            env += attackInc;
            if (env >= 1.0f) { env = 1.0f; stage = Stage::Decay; }
            break;
        case Stage::Decay:
        {
            constexpr float sustain = 0.32f;
            env = sustain + (env - sustain) * decayCoef;
            break;
        }
        case Stage::Release:
            env *= releaseCoef;
            if (env < 1.0e-4f) { stage = Stage::Off; env = 0.0f; return 0.0f; }
            break;
    }

    if (samplesLeft > 0 && --samplesLeft == 0) release();

    brightness = 0.25f + (brightness - 0.25f) * brightCoef;

    const float p = (float)(phase * juce::MathConstants<double>::twoPi);
    const float b = brightness;
    const float s = std::sin(p)
                  + 0.50f * b * std::sin(2.0f * p)
                  + 0.22f * b * std::sin(3.0f * p)
                  + 0.10f * b * b * std::sin(4.0f * p);

    phase += phaseInc;
    if (phase >= 1.0) phase -= 1.0;

    return s * env * gain;
}

// =========================================================================
// AudioEngine
// =========================================================================
AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    shutdownAudio();
}

bool AudioEngine::initAudioDevice()
{
    startTimerHz(30);
    deviceError = deviceManager.initialiseWithDefaultDevices(0, 2);
    sourcePlayer.setSource(this);
    deviceManager.addAudioCallback(&sourcePlayer);
    return deviceError.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr;
}

void AudioEngine::shutdownAudio()
{
    transport.stop();
    deviceManager.removeAudioCallback(&sourcePlayer);
    sourcePlayer.setSource(nullptr);
    transport.setSource(nullptr);
    memorySource.reset();
}

bool AudioEngine::loadFile(const juce::File& file, juce::String& errorOut)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
    {
        errorOut = juce::String::fromUTF8("Filformatet understøttes ikke, eller filen er beskadiget.");
        return false;
    }
    if (!(reader->sampleRate >= 8000.0 && reader->sampleRate <= 768000.0))
    {
        errorOut = juce::String::fromUTF8("Filen har en ugyldig samplerate (")
                 + juce::String(reader->sampleRate, 0) + " Hz).";
        return false;
    }
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64)std::numeric_limits<int>::max() / 2)
    {
        errorOut = juce::String::fromUTF8("Filen er tom eller for lang.");
        return false;
    }

    juce::AudioBuffer<float> newBuffer;
    try
    {
        newBuffer.setSize((int)juce::jlimit(1u, 2u, reader->numChannels), (int)reader->lengthInSamples);
    }
    catch (const std::bad_alloc&)
    {
        errorOut = juce::String::fromUTF8("Ikke nok hukommelse til at indlæse filen.");
        return false;
    }
    reader->read(&newBuffer, 0, (int)reader->lengthInSamples, 0, true, newBuffer.getNumChannels() > 1);

    // Swap in the new file while the transport has no source
    transport.stop();
    transport.setSource(nullptr);
    memorySource.reset();

    decodedBuffer   = std::move(newBuffer);
    fileSampleRate  = reader->sampleRate;
    fileNumChannels = decodedBuffer.getNumChannels();

    memorySource = std::make_unique<juce::MemoryAudioSource>(decodedBuffer, false, false);
    transport.setSource(memorySource.get(), 0, nullptr, fileSampleRate, 2);
    transport.setPosition(0.0);
    clearLoopRegion();
    return true;
}

void AudioEngine::closeFile()
{
    transport.stop();
    transport.setSource(nullptr);
    memorySource.reset();
    decodedBuffer.setSize(0, 0);
    fileNumChannels = 0;
}

double AudioEngine::getTotalLengthSeconds() const noexcept
{
    return fileSampleRate > 0.0 ? (double)decodedBuffer.getNumSamples() / fileSampleRate : 0.0;
}

const juce::AudioBuffer<float>* AudioEngine::getAudioBuffer() const noexcept
{
    return decodedBuffer.getNumSamples() > 0 ? &decodedBuffer : nullptr;
}

// ---- Transport ----------------------------------------------------------
void AudioEngine::play()
{
    if (!isFileLoaded()) return;
    const double p   = transport.getCurrentPosition();
    const double len = getTotalLengthSeconds();
    if (loopEnabled && (p < loopStart.load() || p >= loopEnd.load() || p >= len - 0.01))
        transport.setPosition(loopStart.load());
    else if (p >= len - 0.01)
        transport.setPosition(0.0);
    transport.start();
}

void AudioEngine::pause() { transport.stop(); }

void AudioEngine::stop()
{
    transport.stop();
    transport.setPosition(loopEnabled ? loopStart.load() : 0.0);
}

void AudioEngine::setPosition(double seconds)
{
    transport.setPosition(juce::jlimit(0.0, getTotalLengthSeconds(), seconds));
}

double AudioEngine::getPosition() const noexcept { return transport.getCurrentPosition(); }
bool   AudioEngine::isPlaying()   const noexcept { return transport.isPlaying(); }

void AudioEngine::setLoop(bool enabled, double startSec, double endSec)
{
    // The transport reads slightly ahead and stops itself at end-of-file, so
    // a loop ending at the very end must wrap a few milliseconds earlier.
    const double len = std::max(0.0, getTotalLengthSeconds() - 0.01);
    startSec = juce::jlimit(0.0, len, startSec);
    endSec   = juce::jlimit(0.0, len, endSec);
    if (endSec - startSec < 0.05) enabled = false;
    loopStart   = startSec;
    loopEnd     = endSec;
    loopEnabled = enabled;
}

void AudioEngine::clearLoopRegion()
{
    loopEnabled = false;
}

// ---- MIDI -----------------------------------------------------------------
void AudioEngine::setPlaybackNotes(std::vector<PlaybackNote> notes)
{
    std::sort(notes.begin(), notes.end(),
              [](const PlaybackNote& a, const PlaybackNote& b){ return a.startSec < b.startSec; });
    const juce::SpinLock::ScopedLockType sl(sequenceLock);
    sequence.swap(notes);
}

void AudioEngine::previewNote(int midi, int velocity, double durationSec)
{
    int s1, n1, s2, n2;
    previewFifo.prepareToWrite(1, s1, n1, s2, n2);
    if (n1 > 0) previewQueue[(size_t)s1] = { midi, velocity, durationSec };
    previewFifo.finishedWrite(n1 + n2);
}

void AudioEngine::handlePreviewQueue()
{
    int s1, n1, s2, n2;
    previewFifo.prepareToRead(previewFifo.getNumReady(), s1, n1, s2, n2);
    auto handle = [this](const PreviewRequest& r)
    {
        noteOnVoice(r.midi, r.velocity, SynthVoice::Source::Preview,
                    std::max(1, (int)(r.duration * playbackSampleRate)));
    };
    for (int i = 0; i < n1; ++i) handle(previewQueue[(size_t)(s1 + i)]);
    for (int i = 0; i < n2; ++i) handle(previewQueue[(size_t)(s2 + i)]);
    previewFifo.finishedRead(n1 + n2);
}

void AudioEngine::noteOnVoice(int midi, int vel, SynthVoice::Source src, int holdSamples)
{
    SynthVoice* target = nullptr;
    for (auto& v : voices) if (!v.isActive()) { target = &v; break; }
    if (target == nullptr)   // steal the oldest voice
    {
        target = &voices[0];
        for (auto& v : voices) if (v.age < target->age) target = &v;
    }
    target->start(midi, vel, playbackSampleRate, src, holdSamples, ++voiceCounter);
}

void AudioEngine::noteOffVoice(int midi)
{
    SynthVoice* oldest = nullptr;
    for (auto& v : voices)
        if (v.source == SynthVoice::Source::Sequence && v.midi == midi
            && v.isActive() && v.stage != SynthVoice::Stage::Release)
            if (oldest == nullptr || v.age < oldest->age) oldest = &v;
    if (oldest) oldest->release();
}

void AudioEngine::releaseSequenceVoices()
{
    for (auto& v : voices)
        if (v.source == SynthVoice::Source::Sequence) v.release();
}

void AudioEngine::renderSynth(float* left, float* right, int numSamples,
                              const SynthEvent* events, int numEvents, float gain)
{
    int e = 0;
    for (int s = 0; s < numSamples; ++s)
    {
        while (e < numEvents && events[e].offset <= s)
        {
            if (events[e].on) noteOnVoice(events[e].midi, events[e].vel, SynthVoice::Source::Sequence, -1);
            else              noteOffVoice(events[e].midi);
            ++e;
        }

        float sum = 0.0f;
        for (auto& v : voices)
            if (v.isActive()) sum += v.render();

        sum = std::tanh(sum * gain * 1.4f) * 0.75f;   // gentle soft clip
        left[s] += sum;
        if (right != left) right[s] += sum;
    }
}

// ---- AudioSource -------------------------------------------------------------
void AudioEngine::prepareToPlay(int blockSize, double sr)
{
    playbackSampleRate = sr > 0 ? sr : 44100.0;
    transport.prepareToPlay(blockSize, playbackSampleRate);
}

void AudioEngine::releaseResources()
{
    transport.releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    handlePreviewQueue();

    // Split the block at the loop end so the loop wraps sample-accurately
    int done = 0;
    while (done < info.numSamples)
    {
        const bool playing = transport.isPlaying();
        double pos0 = transport.getCurrentPosition();
        int len = info.numSamples - done;

        if (playing && loopEnabled)
        {
            const double ls = loopStart.load(), le = loopEnd.load();
            if (le > ls)
            {
                if (pos0 >= le - 1.0e-9)
                {
                    transport.setPosition(ls);
                    pos0 = ls;
                }
                const double remaining = (le - pos0) * playbackSampleRate;
                if (remaining < (double)len)
                    len = std::max(1, (int)std::ceil(remaining));
            }
        }

        processSegment(*info.buffer, info.startSample + done, len, playing, pos0);
        done += len;
    }
}

void AudioEngine::processSegment(juce::AudioBuffer<float>& buffer, int st, int n, bool playing, double pos0)
{
    const juce::AudioSourceChannelInfo seg(&buffer, st, n);
    if (isFileLoaded())
        transport.getNextAudioBlock(seg);
    else
        seg.clearActiveBufferRegion();

    if (fileNumChannels == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom(1, st, buffer, 0, st, n);

    const float aGain = audioEnabled ? audioVolume.load() : 0.0f;
    buffer.applyGain(st, n, aGain);

    // ---- Sequence events for this segment ----
    int numEvents = 0;
    const double pos1 = pos0 + (double)n / playbackSampleRate;
    const bool seqOn = playing && midiEnabled;

    if (seqOn)
    {
        // A jump (seek, loop wrap, start) releases sounding notes and restarts
        // the notes that are held at the new position
        const bool jumped = !wasPlaying || std::abs(pos0 - lastBlockEnd) > 0.03;
        if (jumped) releaseSequenceVoices();

        const juce::SpinLock::ScopedTryLockType lock(sequenceLock);
        if (lock.isLocked())
        {
            const int maxEv = (int)eventScratch.size();
            auto toOffset = [&](double t)
            {
                return juce::jlimit(0, n - 1, (int)((t - pos0) * playbackSampleRate));
            };
            for (auto& sn : sequence)
            {
                if (sn.startSec >= pos1) break;   // sorted by start
                if (numEvents >= maxEv - 2) break;

                if (sn.startSec >= pos0)
                    eventScratch[(size_t)numEvents++] = { toOffset(sn.startSec), true, sn.midi, sn.velocity };
                else if (jumped && sn.endSec > pos0 + 0.02)
                    eventScratch[(size_t)numEvents++] = { 0, true, sn.midi, sn.velocity };

                if (sn.endSec >= pos0 && sn.endSec < pos1)
                    eventScratch[(size_t)numEvents++] = { toOffset(sn.endSec), false, sn.midi, 0 };
            }
        }
        lastBlockEnd = pos1;
    }
    else if (wasPlaying)
    {
        releaseSequenceVoices();
    }
    wasPlaying = playing;

    std::sort(eventScratch.begin(), eventScratch.begin() + numEvents,
              [](const SynthEvent& a, const SynthEvent& b)
              {
                  if (a.offset != b.offset) return a.offset < b.offset;
                  return !a.on && b.on;   // note-off before note-on at the same time
              });

    float* left  = buffer.getWritePointer(0, st);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, st) : left;
    renderSynth(left, right, n, eventScratch.data(), numEvents, midiVolume.load());
}

void AudioEngine::timerCallback()
{
    if (onPositionChanged && isFileLoaded())
        onPositionChanged(getPosition());
}

} // namespace Soundstave
