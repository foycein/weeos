#include "Sound/WaveArchiveFile.h"

#include "Sound/SoundFileHeader.h"

#include "Formats/ByteReader.h"

#include <format>

namespace Sound
{

namespace
{

constexpr std::string_view DataDescription = "Wave archive";
constexpr std::uint16_t ArchiveVersion = 0x0100;
constexpr std::uint16_t WaveVersion = 0x0102;
constexpr std::size_t HeaderSize = 0x20;
constexpr std::size_t SectionStartSize = 8;
constexpr std::size_t FirstSectionOffsetPosition = 0x10;

// In the table of the archive a wave is given by what its offset counts from, the offset and the size. This offset counts from the
// start of the data section of the archive.
constexpr std::uint8_t OffsetFromDataSection = 1;
constexpr std::size_t UnusedAfterOffsetTypeSize = 3;

// The samples of a wave lie behind its description: the data location counts from eight bytes behind the start of the description,
// which itself is eight bytes into the information section (0x801346f4 in the program of the Photo Channel).
ArchivedWave ReadWave(Formats::ByteView waveBytes)
{
    const KnownVersion knownVersions[] = {{WaveVersion, {"INFO", "DATA"}}};
    ReadSoundFileHeader(waveBytes, "RWAV", knownVersions, HeaderSize, DataDescription);

    Formats::ByteReader reader(waveBytes, DataDescription);
    reader.MoveTo(FirstSectionOffsetPosition);
    std::uint32_t informationOffset = reader.ReadUint32();

    ArchivedWave wave;
    wave.fileBytes = waveBytes;
    wave.info = ReadWaveInfo(waveBytes, informationOffset + SectionStartSize, DataDescription);
    wave.info.dataLocation += informationOffset + 2 * SectionStartSize;
    return wave;
}

}

WaveArchiveFile ReadWaveArchiveFile(Formats::ByteView bytes)
{
    const KnownVersion knownVersions[] = {{ArchiveVersion, {"TABL", "DATA"}}};
    SoundFile file = ReadSoundFileHeader(bytes, "RWAR", knownVersions, HeaderSize, DataDescription);
    Formats::ByteView tableSection = file.sections[0];
    Formats::ByteView dataSection = file.sections[1];

    Formats::ByteReader reader(tableSection, DataDescription);
    reader.Skip(SectionStartSize);
    std::uint32_t waveCount = reader.ReadUint32();

    WaveArchiveFile archive;
    for (std::uint32_t waveNumber = 0; waveNumber < waveCount; ++waveNumber)
    {
        std::uint8_t offsetType = reader.ReadByte();
        reader.Skip(UnusedAfterOffsetTypeSize);
        std::uint32_t offset = reader.ReadUint32();
        std::uint32_t size = reader.ReadUint32();

        if (offsetType != OffsetFromDataSection)
            reader.Fail(std::format("the wave {} has an offset of the type {}, only type 1 is supported", waveNumber, offsetType));

        if (!Formats::IsRangeInside(offset, size, dataSection.size()))
            reader.Fail(std::format("the wave {} lies outside the data of the archive", waveNumber));

        archive.waves.push_back(ReadWave(dataSection.subspan(offset, size)));
    }
    return archive;
}

}
