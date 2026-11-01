#pragma once

#include "Sound/BankFile.h"
#include "Sound/SequenceFile.h"
#include "Sound/SoundArchiveFile.h"
#include "Sound/WaveArchiveFile.h"
#include "Sound/WaveDecoder.h"
#include "Sound/WaveSoundFile.h"

#include <memory>
#include <vector>

namespace Sound
{

// A wave sound file with the samples of all its waves decoded and ready to play.
struct LoadedWaveSoundFile
{
    WaveSoundFile file;
    std::vector<std::shared_ptr<const DecodedWave>> waves;
};

// A bank with the samples of all its waves decoded and ready to play.
struct LoadedBank
{
    BankFile file;
    std::vector<std::shared_ptr<const DecodedWave>> waves;
};

LoadedWaveSoundFile LoadWaveSoundFile(const SoundArchiveFile& archive, std::uint32_t fileId);
LoadedBank LoadBank(const SoundArchiveFile& archive, std::uint32_t fileId);
SequenceFile LoadSequenceFile(const SoundArchiveFile& archive, std::uint32_t fileId);

}
