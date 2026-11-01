#include "Channels/NativeChannels.h"

#include "Channels/ChannelFiles.h"
#include "Channels/InternetChannel.h"
#include "Channels/MiiChannel/MiiChannel.h"
#include "Channels/Photo/PhotoChannel.h"
#include "Channels/ShopChannel.h"
#include "Channels/WiiConnect24Channel.h"

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
constexpr std::size_t WiiMenuResetAndManual = 3;

// The manuals of the Forecast and the News Channel: html-us.arc of content 10, which their programs open as arc:/html/startup.html
// (0x8003B2C8 of the Forecast). The browser plays the sounds of the channel by their numbers in the archive, which both archives
// share: 0x17 for the focus, 0x18 for a choice, 0x16 for scrolling, 0x1B for closing (0x8007E2A8; CHN-13).
constexpr ManualSounds ManualSoundNames = {"WTR_SE_COM_RECEIVE", "WTR_SE_COM_SUCCESS", "WTR_SE_B_SCROLL", "WTR_SE_COM_BUTTON_B"};
constexpr ManualLocation WiiConnect24ChannelManual = {10, "html-us.arc", "html/startup.html", ManualSoundNames};

constexpr ChannelSharedFiles ForecastSharedFiles = {std::uint16_t{7}, 6, "./HomeButton3", true, WiiMenuResetAndManual, WiiConnect24ChannelManual};
constexpr ChannelSharedFiles NewsSharedFiles = {std::uint16_t{7}, 6, "./HomeButton3", true, WiiMenuResetAndManual, WiiConnect24ChannelManual};
constexpr ChannelSharedFiles ShopSharedFiles = {std::uint16_t{9}, 7, "./HomeButton2", true, WiiMenuAndReset};

// The Mii Channel 6 has no content of the system fonts; its content 4 is the shared HomeButton2 (the strings of 8001C330 of its program).
constexpr ChannelSharedFiles MiiSharedFiles = {std::nullopt, 4, "./HomeButton2", true, WiiMenuAndReset};

// The Photo Channel 1.1 has no content of the system fonts; its content 3 is the shared HomeButton2 (0x80011824 of its program).
constexpr ChannelSharedFiles PhotoSharedFiles = {std::nullopt, 3, "./HomeButton2", true, WiiMenuAndReset};

// The Internet Channel keeps its own HOME menu, not packed, in its content 5 (/cnt/00000005/homebutton/ of its program).
constexpr ChannelSharedFiles InternetSharedFiles = {std::uint16_t{8}, 5, "homebutton", false, WiiMenuAndReset};

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

// The Mii Channel plays its own archive (80017EB0) and the sounds of HomeButton2, which content 4 keeps beside its layouts (CHN-70).
constexpr std::array<ChannelSoundArchive, 2> MiiSounds = {{{2, "Channel/Sound/NigaoeSound.brsar", false},
    {4, "./HomeButton2/Huf8_HomeButtonSe.brsar", true}}};

// The Photo Channel's music and effects are in content 2. Its HOME menu sounds (HomeButtonSe/Huf8_HomeButtonSe.arc) are not a sound archive.
constexpr std::array<ChannelSoundArchive, 1> PhotoSounds = {{{2, "./rev_photo.brsar", false}}};

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

    return {};
}

}

std::vector<Formats::Bytes> ReadSoundArchives(Ios::TitleId titleId, SystemMenu::TitleContents& contents)
{
    std::vector<Formats::Bytes> archives;
    for (const ChannelSoundArchive& soundArchive : SoundArchivesOf(titleId))
    {
        Formats::U8Archive content = ReadArchiveContent(contents, soundArchive.contentIndex);
        Formats::ByteView file = FindFileOrFail(content, soundArchive.path);
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

    return nullptr;
}

}
