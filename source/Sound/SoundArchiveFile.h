#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Sound
{

enum class SoundKind : std::uint8_t
{
    // Commands in a sequence file, played with the instruments of a bank.
    Sequence = 1,

    // One wave of a wave sound file.
    WaveSound = 3,
};

struct SequenceSettings
{
    // Where the sequence starts, counted from the first command of its sequence file.
    std::uint32_t startOffset;

    // The number of the bank in the list of banks of the archive.
    std::uint32_t bankNumber;

    // Which of the 16 tracks the sequence may use, one bit for each track.
    std::uint32_t trackMask;

    std::uint8_t channelPriority;
    std::uint8_t releasePriorityFix;
};

struct WaveSoundSettings
{
    // The number of the sound in its wave sound file.
    std::uint32_t soundNumber;

    std::uint32_t trackMask;
    std::uint8_t channelPriority;
    std::uint8_t releasePriorityFix;
};

struct SoundInfo
{
    std::string name;
    std::uint32_t fileId;

    // The number of the player in the list of players of the archive. The player decides how many of its sounds may play at once.
    std::uint32_t playerNumber;

    // 0 to 127.
    std::uint8_t volume;

    // 0 to 127. When a player is full, the sound with the lower priority has to give way.
    std::uint8_t playerPriority;

    SoundKind kind;

    // Only one of the two is filled in, the one that fits the kind.
    SequenceSettings sequence;
    WaveSoundSettings waveSound;
};

struct BankInfo
{
    std::string name;
    std::uint32_t fileId;
};

struct PlayerInfo
{
    std::string name;
    std::uint8_t maximumSoundCount;
};

// A group is a block of files and a block of wave data in the archive. The files of the group lie in the file block
// and their waves in the wave block.
struct GroupEntry
{
    std::uint32_t fileId;
    std::uint32_t fileOffset;
    std::uint32_t fileSize;
    std::uint32_t waveDataOffset;
    std::uint32_t waveDataSize;
};

struct GroupInfo
{
    std::string name;
    std::uint32_t fileBlockOffset;
    std::uint32_t fileBlockSize;
    std::uint32_t waveBlockOffset;
    std::uint32_t waveBlockSize;
    std::vector<GroupEntry> entries;
};

// A file of the archive (a sequence, a bank or a wave sound file) together with the samples of its waves.
// Both point into the bytes of the archive.
struct EmbeddedFile
{
    Formats::ByteView fileBytes;
    Formats::ByteView waveData;
};

// A sound archive (RSAR) holds many sounds in one file: the list of the sounds, the files that tell how to play them,
// and the samples. It is read completely into memory.
struct SoundArchiveFile
{
    Formats::Bytes bytes;
    std::vector<SoundInfo> sounds;
    std::vector<BankInfo> banks;
    std::vector<PlayerInfo> players;
    std::vector<GroupInfo> groups;

    // The number of the sound with this name, which is the number the menu asks to play.
    std::optional<std::size_t> FindSoundNumber(std::string_view name) const;

    // Finds a file in the first group that has it. Throws a FormatError when there is none.
    // The result points into the archive, so it is only valid while the archive exists.
    EmbeddedFile FindFile(std::uint32_t fileId) const&;
    EmbeddedFile FindFile(std::uint32_t fileId) const&& = delete;
};

SoundArchiveFile ReadSoundArchiveFile(Formats::ByteView bytes);

}
