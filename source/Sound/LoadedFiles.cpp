#include "Sound/LoadedFiles.h"

namespace Sound
{

namespace
{

std::vector<std::shared_ptr<const DecodedWave>> DecodeWaves(const std::vector<WaveInfo>& waveInfos, Formats::ByteView waveData)
{
    std::vector<std::shared_ptr<const DecodedWave>> waves;
    for (const WaveInfo& info : waveInfos)
        waves.push_back(std::make_shared<const DecodedWave>(DecodeWave(info, waveData)));

    return waves;
}

}

LoadedWaveSoundFile LoadWaveSoundFile(const SoundArchiveFile& archive, std::uint32_t fileId)
{
    EmbeddedFile embedded = archive.FindFile(fileId);

    LoadedWaveSoundFile loaded;
    loaded.file = ReadWaveSoundFile(embedded.fileBytes);
    loaded.waves = DecodeWaves(loaded.file.waves, embedded.waveData);
    return loaded;
}

LoadedBank LoadBank(const SoundArchiveFile& archive, std::uint32_t fileId)
{
    EmbeddedFile embedded = archive.FindFile(fileId);

    LoadedBank loaded;
    loaded.file = ReadBankFile(embedded.fileBytes);
    loaded.waves = DecodeWaves(loaded.file.waves, embedded.waveData);
    return loaded;
}

SequenceFile LoadSequenceFile(const SoundArchiveFile& archive, std::uint32_t fileId)
{
    return ReadSequenceFile(archive.FindFile(fileId).fileBytes);
}

}
