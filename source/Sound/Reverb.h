#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Sound
{

// The numbers that describe a reverb. Reverb limits each of them to its range, which is the range of the original.
struct ReverbParameters
{
    // The seconds before the sound reaches the reverb, 0 to 0.1.
    float preDelaySeconds;

    // The seconds the tail of the reverb takes to fall by 60 decibels, 0 to 10.
    float timeSeconds;

    // The gain of the all-pass filters, 0 to 1. A higher number smears the sound more.
    float coloration;

    // 0 to 1. A higher number lets the filter that takes the high tones out of the tail work less.
    float damping;

    // 0 to 1. How much of each channel is added to the other.
    float crosstalk;

    // 0 to 1. How much of the reverb comes out. The reverb replaces its input, so this is the wet part.
    float mix;
};

// The numbers the menu gives the reverb of its effect bus A (they sit at 0x81632f78 in the program of the menu).
constexpr ReverbParameters MenuReverbParameters = {0.02f, 3.0f, 0.6f, 0.4f, 0.1f, 1.0f};

// The reverb of the menu: the late reverb of the high quality reverb of the AX effect library, for two channels.
// The sound passes the pre-delay and three comb filters side by side, then two all-pass filters, the damping filter and
// a last all-pass filter of its own length for each channel. The channels are then mixed a little into each other.
// It follows the original sample by sample at its rate of 32 000 Hz. At another rate the lengths of the delay lines and
// the damping filter are scaled, so that the tail has the same time and the same colour. There is no early reflection:
// the menu gives the early reflections of the original the gain 0.
class Reverb
{
public:
    Reverb(std::uint32_t sampleRate, const ReverbParameters& wantedParameters);

    // The parameters as the reverb uses them.
    const ReverbParameters& Parameters() const;

    // Replaces the samples of both channels with the sound of the reverb. The spans have the same length.
    // The reverb goes on with the tail of what it heard before, so that the next call continues the sound.
    void Process(std::span<float> left, std::span<float> right);

private:
    static constexpr std::size_t CombCount = 3;
    static constexpr std::size_t AllPassCount = 2;
    static constexpr std::size_t ChannelCount = 2;

    // Holds the last samples. The oldest is the sample from length samples ago; Replace puts the newest in its place.
    class DelayLine
    {
    public:
        explicit DelayLine(std::size_t length = 0);

        float Oldest() const;
        void Replace(float newest);

        // Replaces the oldest sample and gives it. A line without any length gives the newest at once.
        float Exchange(float newest);

        void Clear();

    private:
        std::vector<float> samples;
        std::size_t position = 0;
    };

    // Everything one channel has: its own delay lines and the state of its damping filter.
    struct Channel
    {
        DelayLine preDelay;
        std::array<DelayLine, CombCount> combs;
        std::array<DelayLine, AllPassCount> allPasses;
        DelayLine lastAllPass;
        float damped = 0.0f;
    };

    static float Comb(DelayLine& line, float input, float feedback);
    static float AllPass(DelayLine& line, float input, float gain);

    float ProcessChannel(Channel& channel, float input);
    void Clear();

    ReverbParameters parameters;
    std::array<Channel, ChannelCount> channels;
    std::array<float, CombCount> combFeedbacks;
    float dampingWeight;
    float inputWeight;
    std::size_t samplesUntilSilent;
    std::size_t quietSamples = 0;
    bool isCleared = true;
};

}
