#pragma once

#include "Formats/Bytes.h"
#include "Ios/Caller.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/FileSystem/NandPath.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace Channels
{

// The save file of the Wii Speak Channel, mic_chat.dat in its data folder (0x800BFFF0 of its program): 0x1FA0 bytes that start with the
// version 0x12 and hold the sensitivity of the microphone at 0x1F88. The channel makes the file at the end of its first start, so a
// console without the file has never been through the first start. The other fields (the Wii Friends of the channel, the Miis that
// appear, the user data of Nintendo Wi-Fi Connection) belong to features that need the network or are not built; they stay zero and
// are kept as they were read.
class WiiSpeakSaveData
{
public:
    static constexpr std::size_t FileSize = 0x1FA0;
    static constexpr std::uint32_t Version = 0x12;
    static constexpr std::size_t SensitivityOffset = 0x1F88;

    // A new save file, with the sensitivity of a microphone that was never set.
    WiiSpeakSaveData();

    // Nothing when the bytes are not a save file of this version (the channel then asks whether to make new data).
    static std::optional<WiiSpeakSaveData> FromBytes(Formats::ByteView bytes);

    const Formats::Bytes& Bytes() const;

    int Sensitivity() const;
    void SetSensitivity(int sensitivity);

private:
    explicit WiiSpeakSaveData(Formats::Bytes fileBytes);

    Formats::Bytes bytes;
};

enum class SaveFileState
{
    Missing,
    Damaged,
    Read,
};

struct LoadedSaveData
{
    SaveFileState state;
    WiiSpeakSaveData data;
};

// The file of the channel's data folder, read and written as the channel's title through the file system of IOS.
class WiiSpeakSaveFile
{
public:
    WiiSpeakSaveFile(Ios::FileSystemService& fileSystem, Ios::Caller caller, Ios::NandPath dataFolder);

    LoadedSaveData Load() const;

    // Makes the file when it is not there, and writes the whole of it. False when the NAND refuses.
    bool Save(const WiiSpeakSaveData& data);

private:
    Ios::FileSystemService& fileSystem;
    Ios::Caller caller;
    Ios::NandPath filePath;
};

}
