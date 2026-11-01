#include "Sound/SpeakerSounds.h"

#include "Formats/FormatError.h"
#include "Formats/U8Archive.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::size_t BytesPerSample = 2;

DecodedWave MakeWave(Formats::ByteView fileBytes)
{
    std::uint32_t sampleCount = static_cast<std::uint32_t>(fileBytes.size() / BytesPerSample);

    DecodedWave wave;
    wave.sampleRate = SpeakerSoundSampleRate;
    wave.loops = false;
    wave.loopStartSample = 0;
    wave.channels.push_back(DecodePcm16(fileBytes, sampleCount));
    return wave;
}

}

std::vector<SpeakerSound> ReadSpeakerSounds(Formats::ByteView archiveBytes)
{
    Formats::U8Archive archive = Formats::ReadU8Archive(archiveBytes);

    std::vector<SpeakerSound> sounds;
    for (const Formats::U8Entry& entry : archive.entries)
    {
        if (entry.isDirectory)
            continue;

        Formats::ByteView fileBytes = archive.FileData(entry);
        if (fileBytes.size() % BytesPerSample != 0)
        {
            throw Formats::FormatError(std::format("Speaker sounds: the file {} has {} bytes, which is not a whole number of samples",
                entry.path, fileBytes.size()));
        }

        sounds.push_back(SpeakerSound{entry.path, MakeWave(fileBytes)});
    }
    return sounds;
}

}
