#include "Channels/Photo/PhotoStorage.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/Crc32.h"
#include "Formats/TextureDecoder.h"

#include "Ios/FileSystem/NandPath.h"

#include <algorithm>
#include <string>

namespace Channels
{

namespace
{

constexpr std::string_view SaveFolder = "/title/00010002/48415941/data/noerase";
constexpr std::string_view SaveFileName = "savedata.dat";
constexpr std::string_view IconFileName = "photo_i.dat";
constexpr std::string_view PreviewFileName = "photo_b.dat";

// The sizes of the files in the dump of the reference console.
constexpr std::size_t SaveFileSize = 64160;
constexpr std::size_t PictureHeaderSize = 0x20;
constexpr std::size_t BytesPerRgb565Pixel = 2;

// The header of a picture file: the code of the title, the size, the CRC-32 of everything behind it, width and height (0x80009F5C).
constexpr std::uint32_t PictureFileCode = 0x48415941;
constexpr std::size_t ChecksumPlace = 8;
constexpr std::size_t CheckedPartStart = 12;
constexpr std::size_t HeaderFieldsSize = 20;

// The save file has no header; its last four bytes are the CRC-32 of the rest (0x8001AAC8). The places of the settings in it were
// not read from the program; they are WeeOS's own (Assumed, docs/audit/OpenPoints/Channels.md CHN-33).
constexpr std::size_t OrderPlace = 0;
constexpr std::size_t EffectPlace = 1;
constexpr std::size_t SongPlace = 2;
constexpr std::size_t FlagsPlace = 3;
constexpr std::size_t PuzzleRecordsPlace = 4;
constexpr Formats::Byte PieceCountMayBeChosenFlag = 0x01;
constexpr std::size_t ChecksumSize = 4;

// RGB565: five bits of red, six of green, five of blue. A GX texture keeps the pixels in tiles of 4 x 4, row by row.
constexpr int TileSide = 4;
constexpr int RedShift = 11;
constexpr int GreenShift = 5;
constexpr int FiveBitDrop = 3;
constexpr int SixBitDrop = 2;

const Ios::EntryPermissions OwnerReadsAndWrites{Ios::AccessRights::ReadWrite, Ios::AccessRights::None, Ios::AccessRights::None};
constexpr std::uint8_t NoAttributeFlags = 0;

std::uint16_t ToRgb565(const Formats::Rgba8Pixel& pixel)
{
    int red = pixel.red >> FiveBitDrop;
    int green = pixel.green >> SixBitDrop;
    int blue = pixel.blue >> FiveBitDrop;
    return static_cast<std::uint16_t>((red << RedShift) | (green << GreenShift) | blue);
}

std::size_t PictureFileSize(int width, int height)
{
    return PictureHeaderSize + static_cast<std::size_t>(width) * height * BytesPerRgb565Pixel;
}

Ios::NandPath PathOf(std::string_view name)
{
    std::string text = std::string(SaveFolder);
    if (!name.empty())
        text += "/" + std::string(name);

    return Ios::NandPath::Parse(text).Value();
}

}

Formats::Bytes MakePostedPictureFile(const Formats::RgbaImage& picture)
{
    Formats::ByteWriter pixels;
    for (std::uint32_t tileTop = 0; tileTop < picture.height; tileTop += TileSide)
    {
        for (std::uint32_t tileLeft = 0; tileLeft < picture.width; tileLeft += TileSide)
        {
            for (std::uint32_t y = tileTop; y < tileTop + TileSide; ++y)
            {
                for (std::uint32_t x = tileLeft; x < tileLeft + TileSide; ++x)
                    pixels.WriteUint16(ToRgb565(picture.pixels[static_cast<std::size_t>(y) * picture.width + x]));
            }
        }
    }

    Formats::ByteWriter checkedPart;
    checkedPart.WriteUint32(picture.width);
    checkedPart.WriteUint32(picture.height);
    checkedPart.WriteZeros(PictureHeaderSize - HeaderFieldsSize);
    checkedPart.WriteBytes(pixels.GetBytes());

    Formats::ByteWriter file;
    file.WriteUint32(PictureFileCode);
    file.WriteUint32(static_cast<std::uint32_t>(PictureHeaderSize + pixels.GetBytes().size()));
    file.WriteUint32(Formats::Crc32(checkedPart.GetBytes()));
    file.WriteBytes(checkedPart.GetBytes());
    return file.GetBytes();
}

std::optional<Formats::RgbaImage> ReadPostedPictureFile(Formats::ByteView bytes, int width, int height)
{
    if (bytes.size() != PictureFileSize(width, height))
        return std::nullopt;

    Formats::ByteReader reader(bytes, "posted picture");
    bool isRightFile = reader.ReadUint32() == PictureFileCode && reader.ReadUint32() == bytes.size();
    std::uint32_t checksum = reader.ReadUint32();
    bool hasRightSize = reader.ReadUint32() == static_cast<std::uint32_t>(width) && reader.ReadUint32() == static_cast<std::uint32_t>(height);
    if (!isRightFile || !hasRightSize || checksum != Formats::Crc32(bytes.subspan(CheckedPartStart)))
        return std::nullopt;

    return Formats::DecodeTexture(Formats::TextureFormat::Rgb565, width, height, bytes.subspan(PictureHeaderSize), std::nullopt);
}

Formats::Bytes MakeSaveFile(const PhotoSettings& settings)
{
    Formats::Bytes bytes(SaveFileSize, 0);
    bytes[OrderPlace] = static_cast<Formats::Byte>(settings.order);
    bytes[EffectPlace] = static_cast<Formats::Byte>(settings.effect);
    bytes[SongPlace] = static_cast<Formats::Byte>(settings.song);
    bytes[FlagsPlace] = settings.mayChoosePieceCount ? PieceCountMayBeChosenFlag : 0;

    Formats::ByteWriter records;
    for (int steps : settings.puzzleBestSteps)
        records.WriteUint32(static_cast<std::uint32_t>(steps));
    std::copy(records.GetBytes().begin(), records.GetBytes().end(), bytes.begin() + PuzzleRecordsPlace);

    Formats::ByteWriter checksum;
    checksum.WriteUint32(Formats::Crc32(Formats::ByteView(bytes).first(SaveFileSize - ChecksumSize)));
    std::copy(checksum.GetBytes().begin(), checksum.GetBytes().end(), bytes.end() - ChecksumSize);
    return bytes;
}

std::optional<PhotoSettings> ReadSaveFile(Formats::ByteView bytes)
{
    if (bytes.size() != SaveFileSize)
        return std::nullopt;

    Formats::ByteReader reader(bytes, "Photo Channel save file");
    reader.MoveTo(SaveFileSize - ChecksumSize);
    if (reader.ReadUint32() != Formats::Crc32(bytes.first(SaveFileSize - ChecksumSize)))
        return std::nullopt;

    PhotoSettings settings;
    settings.order = static_cast<SlideOrder>(bytes[OrderPlace]);
    settings.effect = static_cast<SlideEffect>(bytes[EffectPlace]);
    settings.song = bytes[SongPlace];
    settings.mayChoosePieceCount = (bytes[FlagsPlace] & PieceCountMayBeChosenFlag) != 0;
    reader.MoveTo(PuzzleRecordsPlace);
    for (int& steps : settings.puzzleBestSteps)
        steps = static_cast<int>(reader.ReadUint32());

    return settings;
}

PhotoStorage::PhotoStorage(const ChannelStorage& channelStorage)
    : storage(channelStorage)
{
}

PhotoStorage::StartResult PhotoStorage::Start()
{
    Ios::NandPath folder = PathOf("");
    if (storage.fileSystem.GetAttributes(folder, storage.caller).Failed())
    {
        if (storage.fileSystem.CreateDirectory(folder, OwnerReadsAndWrites, NoAttributeFlags, storage.caller).Failed())
            return StartResult::CannotCreate;
    }

    bool hasEveryFile = MakeFileWhenMissing(SaveFileName, SaveFileSize)
        && MakeFileWhenMissing(IconFileName, PictureFileSize(IconPictureWidth, IconPictureHeight))
        && MakeFileWhenMissing(PreviewFileName, PictureFileSize(PreviewPictureWidth, PreviewPictureHeight));
    if (!hasEveryFile)
        return StartResult::CannotCreate;

    std::optional<Formats::Bytes> saveFile = ReadFile(SaveFileName);
    bool isEmpty = saveFile && std::all_of(saveFile->begin(), saveFile->end(), [](Formats::Byte value) { return value == 0; });
    std::optional<PhotoSettings> read = saveFile ? ReadSaveFile(*saveFile) : std::nullopt;
    if (read)
    {
        settings = *read;
        return StartResult::Ready;
    }

    settings = PhotoSettings();
    if (!SaveSettings())
        return StartResult::CannotCreate;

    return isEmpty ? StartResult::Ready : StartResult::Corrupted;
}

PhotoSettings& PhotoStorage::Settings()
{
    return settings;
}

bool PhotoStorage::SaveSettings()
{
    return WriteFile(SaveFileName, MakeSaveFile(settings));
}

bool PhotoStorage::HasWiiMenuPhoto()
{
    std::optional<Formats::Bytes> icon = ReadFile(IconFileName);
    return icon && ReadPostedPictureFile(*icon, IconPictureWidth, IconPictureHeight).has_value();
}

bool PhotoStorage::PostToWiiMenu(const Formats::RgbaImage& icon, const Formats::RgbaImage& preview)
{
    return WriteFile(IconFileName, MakePostedPictureFile(icon)) && WriteFile(PreviewFileName, MakePostedPictureFile(preview));
}

// Removing the photo leaves the files with zeros, as the files of a channel that never posted.
bool PhotoStorage::RemoveWiiMenuPhoto()
{
    Formats::Bytes emptyIcon(PictureFileSize(IconPictureWidth, IconPictureHeight), 0);
    Formats::Bytes emptyPreview(PictureFileSize(PreviewPictureWidth, PreviewPictureHeight), 0);
    return WriteFile(IconFileName, emptyIcon) && WriteFile(PreviewFileName, emptyPreview);
}

std::optional<Formats::Bytes> PhotoStorage::ReadFile(std::string_view name)
{
    Ios::Result<Ios::OpenFile> file = storage.fileSystem.Open(PathOf(name), Ios::OpenMode::Read, storage.caller);
    if (file.Failed())
        return std::nullopt;

    Ios::Result<Ios::FileStatistics> statistics = file.Value().GetStatistics();
    if (statistics.Failed())
        return std::nullopt;

    Ios::Result<Formats::Bytes> bytes = file.Value().Read(statistics.Value().length);
    file.Value().Close();
    if (bytes.Failed())
        return std::nullopt;

    return bytes.Value();
}

bool PhotoStorage::WriteFile(std::string_view name, Formats::ByteView bytes)
{
    Ios::Result<Ios::OpenFile> file = storage.fileSystem.Open(PathOf(name), Ios::OpenMode::Write, storage.caller);
    if (file.Failed())
        return false;

    Ios::Result<std::size_t> written = file.Value().Write(bytes);
    Ios::Result<void> closed = file.Value().Close();
    return written.Succeeded() && written.Value() == bytes.size() && closed.Succeeded();
}

bool PhotoStorage::MakeFileWhenMissing(std::string_view name, std::size_t size)
{
    Ios::NandPath path = PathOf(name);
    if (storage.fileSystem.GetAttributes(path, storage.caller).Succeeded())
        return true;

    if (storage.fileSystem.CreateFile(path, OwnerReadsAndWrites, NoAttributeFlags, storage.caller).Failed())
        return false;

    return WriteFile(name, Formats::Bytes(size, 0));
}

}
