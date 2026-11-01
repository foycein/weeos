#include "Channels/NativeChannels.h"

#include "Channels/ChannelFiles.h"
#include "Channels/InternetChannel.h"
#include "Channels/MiiChannel/MiiChannel.h"
#include "Channels/Photo/PhotoChannel.h"
#include "Channels/ShopChannel.h"
#include "Channels/WiiConnect24Channel.h"
#include "Channels/WiiSpeak/WiiSpeakChannel.h"

#include "Formats/ByteReader.h"
#include "Formats/Huffman.h"

#include <array>
#include <span>

namespace Channels
{

namespace
{

// The Forecast Channel 7 (HAFE): content 9 holds the layouts, content 6 is the shared HOME Button Menu.
constexpr WiiConnect24ChannelFiles ForecastFiles = {
    9, "./weather_layout.arc.LZ", "arc/font/font_weather.brfnt", "font_weather.brfnt", 8, 6};

// The News Channel 7 (HAGE): the same places, its own layouts and font, and one more message, the update the News Channel needs.
constexpr WiiConnect24ChannelFiles NewsFiles = {
    9, "./news_layout.arc.LZ", "arc/font/font_news.brfnt", "font_news.brfnt", 9, 6};

// The fonts and the HOME menu of each channel: the Forecast and the News open HomeButton3 (0x8019B978 and 0x801B2488 of their programs),
// the Shop HomeButton2 (0x802F75B8).
constexpr std::size_t WiiMenuAndReset = 2;
constexpr ChannelSharedFiles ForecastSharedFiles = {std::uint16_t{7}, 6, "./HomeButton3", true, WiiMenuAndReset};
constexpr ChannelSharedFiles NewsSharedFiles = {std::uint16_t{7}, 6, "./HomeButton3", true, WiiMenuAndReset};
constexpr ChannelSharedFiles ShopSharedFiles = {std::uint16_t{9}, 7, "./HomeButton2", true, WiiMenuAndReset};

// The Mii Channel 6 has no content of the system fonts; its content 4 is the shared HomeButton2 (the strings of 8001C330 of its program).
constexpr ChannelSharedFiles MiiSharedFiles = {std::nullopt, 4, "./HomeButton2", true, WiiMenuAndReset};

// The Photo Channel 1.1 has no content of the system fonts; its content 3 is the shared HomeButton2 (0x80011824 of its program).
constexpr ChannelSharedFiles PhotoSharedFiles = {std::nullopt, 3, "./HomeButton2", true, WiiMenuAndReset};

// The Internet Channel keeps its own HOME menu, not packed, in its content 5 (/cnt/00000005/homebutton/ of its program).
constexpr ChannelSharedFiles InternetSharedFiles = {std::uint16_t{8}, 5, "homebutton", false, WiiMenuAndReset};

// Wii Speak opens HomeButton3 of its content 3 and has no content of fonts: its program uses the console's fonts (CHN-33).
constexpr ChannelSharedFiles WiiSpeakSharedFiles = {std::nullopt, 3, "./HomeButton3", true, WiiMenuAndReset};

// A sound archive of a channel: the content that holds it, its path there, and whether it is packed with Huffman.
struct ChannelSoundArchive
{
    std::uint16_t contentIndex;
    std::string_view path;
    bool isHuffmanPacked;
};

// Forecast and News play their own archive and the sounds of HomeButton3; the Shop's archive holds the sounds of its HOME menu too.
// The Internet Channel's sounds of its HOME menu are wave tables of the browser (HomeButtonSe.arc), which WeeOS does not read.
constexpr ChannelSoundArchive HomeButton3Sounds = {6, "./HomeButton3/Huf8_HomeButtonSe.brsar", true};
constexpr std::array<ChannelSoundArchive, 2> ForecastSounds = {{{9, "./rev_wtr.brsar", false}, HomeButton3Sounds}};
constexpr std::array<ChannelSoundArchive, 2> NewsSounds = {{{9, "./rev_news.brsar", false}, HomeButton3Sounds}};
constexpr std::array<ChannelSoundArchive, 1> ShopSounds = {{{2, "arc/sound/IplSound.brsar", false}}};
constexpr std::array<ChannelSoundArchive, 2> WiiSpeakSounds = {{{2, "./MicChatSound.brsar", false}, {3, "./HomeButton3/Huf8_HomeButtonSe.brsar", true}}};

// The Mii Channel plays its own archive (80017EB0) and the sounds of HomeButton2, which content 4 keeps beside its layouts (CHN-70).
constexpr std::array<ChannelSoundArchive, 2> MiiSounds = {{{2, "Channel/Sound/NigaoeSound.brsar", false},
    {4, "./HomeButton2/Huf8_HomeButtonSe.brsar", true}}};

// The Photo Channel's music and effects are in content 2. Its HOME menu sounds (HomeButtonSe/Huf8_HomeButtonSe.arc) are not a sound archive.
constexpr std::array<ChannelSoundArchive, 1> PhotoSounds = {{{2, "./rev_photo.brsar", false}}};

// The Internet Channel's sounds and those of its soft keyboard are in remote/operaSound.brsar of content 2. Its HOME menu sounds
// (homebutton/HomeButtonSe.arc of content 5) are not a sound archive.
constexpr std::array<ChannelSoundArchive, 1> InternetSounds = {{{2, "./remote/operaSound.brsar", false}}};

std::span<const ChannelSoundArchive> SoundArchivesOf(Ios::TitleId titleId)
{
    if (titleId == ForecastChannelTitleId)
        return ForecastSounds;

    if (titleId == NewsChannelTitleId)
        return NewsSounds;

    if (titleId == WiiShopChannelTitleId)
        return ShopSounds;

    if (titleId == MiiChannelTitleId)
        return MiiSounds;

    if (titleId == PhotoChannelTitleId)
        return PhotoSounds;

    if (titleId == InternetChannelTitleId)
        return InternetSounds;

    if (titleId == WiiSpeakChannelTitleId)
        return WiiSpeakSounds;

    return {};
}

}

namespace
{

// A file of a U8 archive may have padding behind it (the Internet Channel's operaSound.brsar has 2 bytes); the sound archive is the
// part that its header counts: the size of the file is the big-endian word after the tag, the byte order mark and the version.
Formats::ByteView WithoutPadding(Formats::ByteView file)
{
    constexpr std::size_t FileSizeOffset = 8;
    Formats::ByteReader reader(file, "sound archive");
    reader.Skip(FileSizeOffset);
    std::uint32_t archiveSize = reader.ReadUint32();
    if (archiveSize > file.size())
        return file;

    return file.first(archiveSize);
}

}

std::vector<Formats::Bytes> ReadSoundArchives(Ios::TitleId titleId, SystemMenu::TitleContents& contents)
{
    std::vector<Formats::Bytes> archives;
    for (const ChannelSoundArchive& soundArchive : SoundArchivesOf(titleId))
    {
        Formats::U8Archive content = ReadArchiveContent(contents, soundArchive.contentIndex);
        Formats::ByteView file = WithoutPadding(FindFileOrFail(content, soundArchive.path));
        if (soundArchive.isHuffmanPacked)
            archives.push_back(Formats::DecompressHuffman(file));
        else
            archives.emplace_back(file.begin(), file.end());
    }
    return archives;
}

std::optional<ChannelSharedFiles> SharedFilesOf(Ios::TitleId titleId)
{
    if (titleId == WiiShopChannelTitleId)
        return ShopSharedFiles;

    if (titleId == ForecastChannelTitleId)
        return ForecastSharedFiles;

    if (titleId == NewsChannelTitleId)
        return NewsSharedFiles;

    if (titleId == InternetChannelTitleId)
        return InternetSharedFiles;

    if (titleId == MiiChannelTitleId)
        return MiiSharedFiles;

    if (titleId == PhotoChannelTitleId)
        return PhotoSharedFiles;

    if (titleId == WiiSpeakChannelTitleId)
        return WiiSpeakSharedFiles;

    return std::nullopt;
}

bool HasNativeProgram(Ios::TitleId titleId)
{
    return SharedFilesOf(titleId).has_value();
}

std::unique_ptr<Channel> MakeChannel(Ios::TitleId titleId, ChannelServices& services)
{
    if (titleId == WiiShopChannelTitleId)
        return std::make_unique<ShopChannel>(services);

    if (titleId == ForecastChannelTitleId)
        return std::make_unique<WiiConnect24Channel>(services, ForecastFiles);

    if (titleId == NewsChannelTitleId)
        return std::make_unique<WiiConnect24Channel>(services, NewsFiles);

    if (titleId == InternetChannelTitleId)
        return std::make_unique<InternetChannel>(services);

    if (titleId == MiiChannelTitleId)
        return std::make_unique<MiiChannel>(services);

    if (titleId == PhotoChannelTitleId)
        return std::make_unique<PhotoChannel>(services);

    if (titleId == WiiSpeakChannelTitleId)
        return std::make_unique<WiiSpeakChannel>(services);

    return nullptr;
}

}
