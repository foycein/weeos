#include "Channels/WiiSpeak/WiiSpeakSaveData.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Ios/FileSystem/OpenFile.h"
#include "Ios/Microphone/MicrophoneService.h"
#include "Mii/MiiChecksum.h"

#include <algorithm>

namespace Channels
{

namespace
{

constexpr std::string_view SaveFileName = "mic_chat.dat";
constexpr std::size_t VersionOffset = 0;
constexpr std::uint8_t NoFlags = 0;
constexpr int BitsPerByte = 8;
constexpr std::uint16_t LowByteMask = 0xFF;

// The channel's files are its own: the owner reads and writes, nobody else (as the save files of the console's titles are).
constexpr Ios::EntryPermissions SaveFilePermissions = {Ios::AccessRights::ReadWrite, Ios::AccessRights::None, Ios::AccessRights::None};

std::uint32_t ReadVersion(Formats::ByteView bytes)
{
    Formats::ByteReader reader(bytes, "Wii Speak save file");
    reader.MoveTo(VersionOffset);
    return reader.ReadUint32();
}

// The version first, the rest zero.
Formats::Bytes MakeNewFileBytes()
{
    Formats::ByteWriter writer;
    writer.WriteUint32(WiiSpeakSaveData::Version);
    writer.WriteZeros(WiiSpeakSaveData::FileSize - sizeof(std::uint32_t));
    return writer.GetBytes();
}

}

WiiSpeakSaveData::WiiSpeakSaveData()
    : bytes(MakeNewFileBytes())
{
    SetSensitivity(Ios::DefaultMicrophoneSensitivity);
}

WiiSpeakSaveData::WiiSpeakSaveData(Formats::Bytes fileBytes)
    : bytes(std::move(fileBytes))
{
}

std::optional<WiiSpeakSaveData> WiiSpeakSaveData::FromBytes(Formats::ByteView fileBytes)
{
    if (fileBytes.size() != FileSize)
        return std::nullopt;

    if (ReadVersion(fileBytes) != Version)
        return std::nullopt;

    return WiiSpeakSaveData(Formats::Bytes(fileBytes.begin(), fileBytes.end()));
}

const Formats::Bytes& WiiSpeakSaveData::Bytes() const
{
    return bytes;
}

// A sensitivity outside 1 to 5 is clamped, as the microphone controller of the program does (0x801B24A0).
int WiiSpeakSaveData::Sensitivity() const
{
    return std::clamp(int(bytes[SensitivityOffset]), Ios::LowestMicrophoneSensitivity, Ios::HighestMicrophoneSensitivity);
}

void WiiSpeakSaveData::SetSensitivity(int sensitivity)
{
    int clamped = std::clamp(sensitivity, Ios::LowestMicrophoneSensitivity, Ios::HighestMicrophoneSensitivity);
    bytes[SensitivityOffset] = static_cast<Formats::Byte>(clamped);
}

std::vector<Mii::MiiRecord> WiiSpeakSaveData::ChosenMiis() const
{
    std::vector<Mii::MiiRecord> miis;
    for (std::size_t entry = 0; entry < MostChosenMiis; ++entry)
    {
        std::size_t start = ChosenMiisOffset + entry * ChosenMiiEntrySize;
        std::span<const Formats::Byte, Mii::MiiRecordSize> record(bytes.data() + start, Mii::MiiRecordSize);
        Formats::ByteReader reader(bytes, "Wii Speak save file");
        reader.MoveTo(start + Mii::MiiRecordSize);
        std::uint16_t storedChecksum = reader.ReadUint16();
        bool isFilled = std::any_of(record.begin(), record.end(), [](Formats::Byte value) { return value != 0; });
        if (isFilled && storedChecksum == Mii::CalculateMiiChecksum(record))
            miis.push_back(Mii::ReadMiiRecord(record));
    }
    return miis;
}

void WiiSpeakSaveData::SetChosenMiis(const std::vector<Mii::MiiRecord>& miis)
{
    std::fill_n(bytes.begin() + ChosenMiisOffset, MostChosenMiis * ChosenMiiEntrySize, Formats::Byte{0});
    for (std::size_t entry = 0; entry < miis.size() && entry < MostChosenMiis; ++entry)
    {
        std::size_t start = ChosenMiisOffset + entry * ChosenMiiEntrySize;
        std::span<Formats::Byte, Mii::MiiRecordSize> record(bytes.data() + start, Mii::MiiRecordSize);
        Mii::WriteMiiRecord(miis[entry], record);
        // The checksum follows the Mii, big endian.
        std::uint16_t checksum = Mii::CalculateMiiChecksum(record);
        bytes[start + Mii::MiiRecordSize] = static_cast<Formats::Byte>(checksum >> BitsPerByte);
        bytes[start + Mii::MiiRecordSize + 1] = static_cast<Formats::Byte>(checksum & LowByteMask);
    }
}

WiiSpeakSaveFile::WiiSpeakSaveFile(Ios::FileSystemService& fileSystemService, Ios::Caller titleCaller, Ios::NandPath dataFolder)
    : fileSystem(fileSystemService)
    , caller(titleCaller)
    , filePath(dataFolder.Child(SaveFileName).Value())
{
}

LoadedSaveData WiiSpeakSaveFile::Load() const
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(filePath, Ios::OpenMode::Read, caller);
    if (opened.Failed())
        return {SaveFileState::Missing, WiiSpeakSaveData()};

    // One byte more than the file should have, so that a longer file is noticed.
    std::size_t bytesToRead = WiiSpeakSaveData::FileSize + 1;
    Ios::Result<Formats::Bytes> read = opened.Value().Read(bytesToRead);
    if (read.Failed())
        return {SaveFileState::Damaged, WiiSpeakSaveData()};

    std::optional<WiiSpeakSaveData> data = WiiSpeakSaveData::FromBytes(read.Value());
    if (!data)
        return {SaveFileState::Damaged, WiiSpeakSaveData()};

    return {SaveFileState::Read, *data};
}

bool WiiSpeakSaveFile::Save(const WiiSpeakSaveData& data)
{
    Ios::Result<Ios::OpenFile> opened = fileSystem.Open(filePath, Ios::OpenMode::Write, caller);
    if (opened.Failed())
    {
        Ios::Result<void> created = fileSystem.CreateFile(filePath, SaveFilePermissions, NoFlags, caller);
        if (created.Failed())
            return false;

        opened = fileSystem.Open(filePath, Ios::OpenMode::Write, caller);
        if (opened.Failed())
            return false;
    }

    Ios::Result<std::size_t> written = opened.Value().Write(data.Bytes());
    return written.Succeeded() && written.Value() == data.Bytes().size();
}

}
