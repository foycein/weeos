#include "Sound/WaveTableSynth.h"

#include "Sound/Resampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Sound
{

namespace
{

constexpr int ChannelCount = 16;
constexpr std::uint8_t VolumeController = 7;
constexpr std::uint8_t PanController = 10;
constexpr std::uint8_t StartingVolume = 100;
constexpr std::uint8_t MiddlePan = 64;
constexpr double HighestMidiValue = 127;

// Loudness in tenths of a decibel.
constexpr double SilentTenths = -960;
constexpr double QuietestHeardTenths = -904;
constexpr double LoudestTenths = 59;
constexpr double EndingTenths = -720;
constexpr double TenthsPerDecade = 200;

// The attack climbs a curve of 100 steps; the decay falls 96 decibels.
constexpr double AttackCurveTop = 99;
constexpr double AttackSteps = 100;
constexpr double MillisecondsPerFrame = 3;
constexpr double MillisecondsPerSecond = 1000;

constexpr double CentsPerKey = 100;
constexpr double CentsPerOctave = 1200;

// A sound that never ends is cut here.
constexpr double LongestSeconds = 10;

constexpr int StereoChannels = 2;

enum class EnvelopeStage
{
    Attack,
    Decay,
    Sustain,
    Release,
    Finished,
};

struct ChannelState
{
    std::uint8_t program = 0;
    std::uint8_t volume = StartingVolume;
    std::uint8_t pan = MiddlePan;
};

struct SoundingNote
{
    const WaveTableRegion* region = nullptr;
    int channel = 0;
    std::uint8_t key = 0;
    double velocityTenths = 0;
    double position = 0;
    double step = 1;
    EnvelopeStage stage = EnvelopeStage::Sustain;
    double levelTenths = 0;
    double attackPosition = 0;
    double attackStep = AttackSteps;
    double decayStep = SilentTenths;
    std::array<float, StereoChannels> gains = {0, 0};
    bool hasPlayed = false;
};

// 40 log10(value / 127) decibels: the curve of velocity and volume.
double TenthsOfMidiValue(std::uint8_t value)
{
    if (value == 0)
        return SilentTenths;
    return 2 * TenthsPerDecade * std::log10(value / HighestMidiValue);
}

// The share of the power that a side gets.
double TenthsOfPanShare(double share)
{
    if (share <= 0)
        return QuietestHeardTenths;
    return TenthsPerDecade / 2 * std::log10(share);
}

float GainOfTenths(double tenths)
{
    if (tenths <= QuietestHeardTenths)
        return 0;
    return static_cast<float>(std::pow(10.0, std::min(tenths, LoudestTenths) / TenthsPerDecade));
}

// A time of the envelope in whole frames, as the program counts it; nothing (at once) is no frame.
int FramesOf(std::optional<double> seconds)
{
    if (!seconds)
        return 0;
    int milliseconds = static_cast<int>(*seconds * MillisecondsPerSecond);
    return static_cast<int>(milliseconds / MillisecondsPerFrame);
}

double AttackCurve(double attackPosition)
{
    if (attackPosition >= AttackCurveTop)
        return 0;
    double step = std::floor(attackPosition);
    if (step <= 0)
        return SilentTenths;
    return TenthsPerDecade * std::log10(step / AttackCurveTop);
}

void StartEnvelope(SoundingNote& note)
{
    const WaveTableEnvelope& envelope = note.region->envelope;
    int decayFrames = FramesOf(envelope.decaySeconds);
    note.decayStep = decayFrames == 0 ? SilentTenths : SilentTenths / decayFrames;

    if (envelope.attackSeconds)
    {
        int attackFrames = FramesOf(envelope.attackSeconds);
        note.stage = EnvelopeStage::Attack;
        note.levelTenths = SilentTenths;
        note.attackStep = attackFrames == 0 ? AttackSteps : AttackSteps / attackFrames;
        return;
    }

    if (envelope.decaySeconds)
    {
        note.stage = EnvelopeStage::Decay;
        note.levelTenths = 0;
        return;
    }

    note.stage = EnvelopeStage::Sustain;
    note.levelTenths = envelope.sustainTenthsOfDecibel;
}

void MoveEnvelope(SoundingNote& note)
{
    const WaveTableEnvelope& envelope = note.region->envelope;
    switch (note.stage)
    {
    case EnvelopeStage::Attack:
        note.attackPosition += note.attackStep;
        note.levelTenths = AttackCurve(note.attackPosition);
        if (note.levelTenths == 0)
            note.stage = EnvelopeStage::Decay;
        return;
    case EnvelopeStage::Decay:
        note.levelTenths += note.decayStep;
        if (note.levelTenths <= envelope.sustainTenthsOfDecibel)
        {
            note.levelTenths = envelope.sustainTenthsOfDecibel;
            note.stage = EnvelopeStage::Sustain;
        }
        if (note.levelTenths <= EndingTenths)
            note.stage = EnvelopeStage::Finished;
        return;
    case EnvelopeStage::Release:
        if (note.levelTenths < EndingTenths)
        {
            note.stage = EnvelopeStage::Finished;
            return;
        }
        note.levelTenths += envelope.releaseTenthsOfDecibelPerFrame;
        return;
    case EnvelopeStage::Sustain:
    case EnvelopeStage::Finished:
        return;
    }
}

std::array<float, StereoChannels> GainsOf(const SoundingNote& note, const ChannelState& channel)
{
    double noteTenths = note.region->attenuationTenthsOfDecibel + note.velocityTenths + note.levelTenths;
    double volumeTenths = TenthsOfMidiValue(channel.volume);
    double leftShare = (HighestMidiValue - channel.pan) / HighestMidiValue;
    double rightShare = channel.pan / HighestMidiValue;
    float noteGain = GainOfTenths(noteTenths);
    return {noteGain * GainOfTenths(volumeTenths + TenthsOfPanShare(leftShare)),
        noteGain * GainOfTenths(volumeTenths + TenthsOfPanShare(rightShare))};
}

class Synth
{
public:
    explicit Synth(const WaveTable& waveTable)
        : table(waveTable)
    {
    }

    void Play(const MidiEvent& event)
    {
        ChannelState& channel = channels[event.channel];
        switch (event.kind)
        {
        case MidiEventKind::NoteOn:
            LetGo(event.channel, event.number);
            StartNote(event);
            return;
        case MidiEventKind::NoteOff:
            LetGo(event.channel, event.number);
            return;
        case MidiEventKind::ProgramChange:
            channel.program = event.number;
            return;
        case MidiEventKind::Controller:
            if (event.number == VolumeController)
                channel.volume = event.value;
            if (event.number == PanController)
                channel.pan = event.value;
            return;
        }
    }

    // One frame of samples, added to the end of the two sides; then the envelopes move on.
    void RenderFrame(std::vector<float>& left, std::vector<float>& right)
    {
        std::size_t frameStart = left.size();
        left.resize(frameStart + SynthFrameSamples, 0.0f);
        right.resize(frameStart + SynthFrameSamples, 0.0f);
        for (SoundingNote& note : notes)
            RenderNote(note, std::span<float>(left).subspan(frameStart), std::span<float>(right).subspan(frameStart));

        for (SoundingNote& note : notes)
            MoveEnvelope(note);
        std::erase_if(notes, [](const SoundingNote& note) { return note.stage == EnvelopeStage::Finished; });
    }

    bool IsSilent() const
    {
        return notes.empty();
    }

private:
    void StartNote(const MidiEvent& event)
    {
        const WaveTableRegion* region = table.FindRegion(channels[event.channel].program, event.number);
        if (region == nullptr || region->wave->samples.empty())
            return;

        SoundingNote note;
        note.region = region;
        note.channel = event.channel;
        note.key = event.number;
        note.velocityTenths = TenthsOfMidiValue(event.value);
        double cents = (event.number - region->unityKey) * CentsPerKey + region->fineTuneCents;
        note.step = static_cast<double>(region->wave->sampleRate) / SynthSampleRate * std::pow(2.0, cents / CentsPerOctave);
        StartEnvelope(note);
        notes.push_back(note);
    }

    void LetGo(int channel, std::uint8_t key)
    {
        for (SoundingNote& note : notes)
        {
            bool isTheKey = note.channel == channel && note.key == key;
            if (isTheKey && note.stage != EnvelopeStage::Finished)
                note.stage = EnvelopeStage::Release;
        }
    }

    // The loudness glides from the last frame's to this frame's across the frame, as the DSP ramps the volume of a voice.
    void RenderNote(SoundingNote& note, std::span<float> left, std::span<float> right)
    {
        std::array<float, StereoChannels> newGains = GainsOf(note, channels[note.channel]);
        std::array<float, StereoChannels> oldGains = note.hasPlayed ? note.gains : newGains;
        note.gains = newGains;
        note.hasPlayed = true;

        const WaveTableWave& wave = *note.region->wave;
        WaveSamples samples{wave.samples, wave.loops, wave.loopStartSample};
        for (std::uint32_t sample = 0; sample < SynthFrameSamples; ++sample)
        {
            bool hasEnded = !wave.loops && note.position >= static_cast<double>(wave.samples.size());
            if (hasEnded)
            {
                note.stage = EnvelopeStage::Finished;
                return;
            }

            float value = resampler.Read(samples, note.position, note.step);
            float share = static_cast<float>(sample) / SynthFrameSamples;
            left[sample] += value * (oldGains[0] + (newGains[0] - oldGains[0]) * share);
            right[sample] += value * (oldGains[1] + (newGains[1] - oldGains[1]) * share);
            note.position += note.step;
        }
    }

    const WaveTable& table;
    Resampler resampler;
    std::array<ChannelState, ChannelCount> channels;
    std::vector<SoundingNote> notes;
};

}

DecodedWave RenderMidi(const MidiFile& file, const WaveTable& table)
{
    Synth synth(table);
    std::vector<float> left;
    std::vector<float> right;
    std::size_t nextEvent = 0;
    double frameSeconds = static_cast<double>(SynthFrameSamples) / SynthSampleRate;
    for (double seconds = 0; seconds < LongestSeconds; seconds += frameSeconds)
    {
        while (nextEvent < file.events.size() && file.events[nextEvent].seconds <= seconds)
        {
            synth.Play(file.events[nextEvent]);
            ++nextEvent;
        }

        bool hasEnded = nextEvent == file.events.size() && seconds >= file.lengthSeconds && synth.IsSilent();
        if (hasEnded)
            break;

        synth.RenderFrame(left, right);
    }

    DecodedWave wave;
    wave.sampleRate = SynthSampleRate;
    wave.loops = false;
    wave.loopStartSample = 0;
    wave.channels = {std::move(left), std::move(right)};
    return wave;
}

}
