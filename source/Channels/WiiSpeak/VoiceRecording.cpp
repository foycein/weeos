#include "Channels/WiiSpeak/VoiceRecording.h"

#include "Ios/Microphone/MicrophoneService.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Channels
{

namespace
{

constexpr std::size_t LongestRecordingSamples = std::size_t(LongestRecordingSeconds * Ios::MicrophoneSampleRate);
constexpr float SampleUnitsPerFullScale = 32768.0f;

// The pieces of the pitch change: 40 milliseconds, a new one every 10 milliseconds, so four overlap at every moment.
constexpr std::size_t PieceSamples = 640;
constexpr std::size_t PieceSpacing = 160;
constexpr double WindowSumOfFourPieces = 2.0;

// A raised cosine (Hann) window over the piece: 0 at both ends, 1 in the middle.
double WindowAt(std::size_t positionInPiece)
{
    double phase = double(positionInPiece) / double(PieceSamples);
    return 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phase);
}

// The voice between two samples, by a straight line.
double VoiceAt(std::span<const float> voice, double position)
{
    std::size_t before = std::size_t(position);
    if (before + 1 >= voice.size())
        return 0.0;

    double fraction = position - double(before);
    return voice[before] * (1.0 - fraction) + voice[before + 1] * fraction;
}

}

std::size_t VoiceRecording::Add(std::span<const std::int16_t> heard)
{
    std::size_t room = LongestRecordingSamples - samples.size();
    std::size_t count = std::min(room, heard.size());
    samples.insert(samples.end(), heard.begin(), heard.begin() + static_cast<std::ptrdiff_t>(count));
    return count;
}

bool VoiceRecording::IsFull() const
{
    return samples.size() >= LongestRecordingSamples;
}

bool VoiceRecording::IsEmpty() const
{
    return samples.empty();
}

double VoiceRecording::Seconds() const
{
    return double(samples.size()) / double(Ios::MicrophoneSampleRate);
}

void VoiceRecording::Clear()
{
    samples.clear();
}

const std::vector<std::int16_t>& VoiceRecording::Samples() const
{
    return samples;
}

std::shared_ptr<const Sound::DecodedWave> VoiceRecording::ToWave(bool withChangedVoice) const
{
    std::vector<float> voice;
    for (std::int16_t sample : samples)
        voice.push_back(float(sample) / SampleUnitsPerFullScale);

    auto wave = std::make_shared<Sound::DecodedWave>();
    wave->sampleRate = Ios::MicrophoneSampleRate;
    wave->loops = false;
    wave->loopStartSample = 0;
    wave->channels.push_back(withChangedVoice ? RaisePitch(voice, ChangedVoicePitch) : voice);
    return wave;
}

// Each piece starts at its own place in the voice and is read faster by the factor; its window lets it fade in and out.
std::vector<float> RaisePitch(std::span<const float> voice, double factor)
{
    std::vector<double> output(voice.size(), 0.0);
    for (std::size_t pieceStart = 0; pieceStart < voice.size(); pieceStart += PieceSpacing)
    {
        for (std::size_t positionInPiece = 0; positionInPiece < PieceSamples; ++positionInPiece)
        {
            std::size_t outputPosition = pieceStart + positionInPiece;
            if (outputPosition >= output.size())
                break;

            double readPosition = double(pieceStart) + double(positionInPiece) * factor;
            output[outputPosition] += WindowAt(positionInPiece) * VoiceAt(voice, readPosition);
        }
    }

    std::vector<float> raised;
    for (double sample : output)
        raised.push_back(float(sample / WindowSumOfFourPieces));
    return raised;
}

}
