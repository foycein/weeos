#pragma once

#include "Channels/ChannelServices.h"

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"

#include <array>
#include <optional>

namespace Channels
{

enum class SlideOrder
{
    Date,
    Random,
};

enum class SlideEffect
{
    Dramatic,
    Simple,
    Nostalgic,
};

constexpr int PuzzleSizeCount = 5;

// What the channel keeps in noerase/savedata.dat: the settings of the slide show, the best time of each puzzle size and whether the
// number of pieces may be chosen ("You can now choose the number of pieces!").
struct PhotoSettings
{
    SlideOrder order = SlideOrder::Date;
    SlideEffect effect = SlideEffect::Simple;

    // 0 to 5 are the built-in songs (BGM_SLIDE1 to 6), 6 is "All AAC files", 7 "Shuffle AAC files", from 8 on one AAC file of the card.
    int song = 0;
    std::array<int, PuzzleSizeCount> puzzleBestSteps{};
    bool mayChoosePieceCount = false;
};

// The sizes of the photo posted to the Wii Menu (0x8001DDD8): the icon is 160 x 120, the channel preview 480 x 272.
constexpr int IconPictureWidth = 160;
constexpr int IconPictureHeight = 120;
constexpr int PreviewPictureWidth = 480;
constexpr int PreviewPictureHeight = 272;

// The files of the channel's data folder (docs/audit/Photo.md "The files"). Every file is made at the first start with its full size,
// as the original makes them (0x80009E20, 0x8001AA2C).
class PhotoStorage
{
public:
    enum class StartResult
    {
        Ready,

        // "Unable to create a save file." (error1)
        CannotCreate,

        // "The save file is corrupted. Now formatting Photo Channel save data." (error2): its check number does not fit.
        Corrupted,
    };

    explicit PhotoStorage(const ChannelStorage& channelStorage);

    StartResult Start();

    PhotoSettings& Settings();
    bool SaveSettings();

    bool HasWiiMenuPhoto();
    bool PostToWiiMenu(const Formats::RgbaImage& icon, const Formats::RgbaImage& preview);
    bool RemoveWiiMenuPhoto();

private:
    std::optional<Formats::Bytes> ReadFile(std::string_view name);
    bool WriteFile(std::string_view name, Formats::ByteView bytes);
    bool MakeFileWhenMissing(std::string_view name, std::size_t size);

    ChannelStorage storage;
    PhotoSettings settings;
};

// A picture file of the Wii Menu photo (photo_i.dat, photo_b.dat): the header (the code HAYA, the size of the file, the CRC-32 of
// everything after it, width and height) and the picture as a GX texture of RGB565 in tiles of 4 x 4.
Formats::Bytes MakePostedPictureFile(const Formats::RgbaImage& picture);

// The picture of such a file; nothing for an empty file (all zeros) or one whose header or check number does not fit.
std::optional<Formats::RgbaImage> ReadPostedPictureFile(Formats::ByteView bytes, int width, int height);

// The bytes of noerase/savedata.dat for the settings, with the CRC-32 in its last four bytes, and back.
Formats::Bytes MakeSaveFile(const PhotoSettings& settings);
std::optional<PhotoSettings> ReadSaveFile(Formats::ByteView bytes);

}
