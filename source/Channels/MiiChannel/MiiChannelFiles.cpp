#include "Channels/MiiChannel/MiiChannelFiles.h"

#include "Channels/ChannelFiles.h"

#include "Formats/Lz77.h"
#include "Formats/Yaz0.h"

#include <charconv>

namespace Channels
{

namespace
{

constexpr std::string_view CommonLayoutsPath = "Channel/ScreenAll/Cmn/Layout00.szs";
constexpr std::string_view EnglishLayoutsPath = "Channel/ScreenAll/Usa/Layout00.szs";
constexpr std::string_view EnglishMessagesPath = "raw/message/nigaoe_message_U.bmg";
constexpr std::string_view MainFontPath = "font/newR_font_32_I4.brfnt";
constexpr std::string_view NameFontPath = "font/nigaoe_font_32_I4.brfnt";
constexpr std::string_view CharacterArchivePath = "Channel/Character.szs";
constexpr std::string_view GroundModelPath = "graund_model.brres";
constexpr std::string_view KeyboardArchivePath = "Channel/sofkeybdkr.szs";
constexpr std::string_view FacePartsPath = "RFL_Res.dat";

// The folders blyt, anim and timg of Layout00.szs lie at its root.
const std::string LayoutsAtTheRoot = "";

// The channel's own hand is a NintendoWare effect (nw4r_defcursor_all01 in Character.szs) that WeeOS cannot draw; the hands are the ones of
// its HOME Button Menu, as for the other channels (CHN-26).
constexpr std::string_view HandArchivePath = "./HomeButton2/LZ77_homeBtn.arc";

Formats::U8Archive ReadPackedArchive(const Formats::U8Archive& content, std::string_view path)
{
    return Formats::ReadU8Archive(Formats::DecompressYaz0(FindFileOrFail(content, path)));
}

Formats::Bytes CopyOf(Formats::ByteView bytes)
{
    return {bytes.begin(), bytes.end()};
}

}

MiiChannelFiles ReadMiiChannelFiles(SystemMenu::TitleContents& contents)
{
    Formats::U8Archive channelFiles = ReadArchiveContent(contents, MiiChannelFilesContent);

    MiiChannelFiles files;
    Formats::U8Archive commonLayouts = ReadPackedArchive(channelFiles, CommonLayoutsPath);
    files.mainFont = Formats::ReadFont(FindFileOrFail(commonLayouts, MainFontPath));
    files.nameFont = Formats::ReadFont(FindFileOrFail(commonLayouts, NameFontPath));
    files.layouts = std::make_unique<Layout::LayoutArchive>(std::move(commonLayouts), LayoutsAtTheRoot);

    Formats::U8Archive englishLayouts = ReadPackedArchive(channelFiles, EnglishLayoutsPath);
    files.messages = Formats::ReadNumberedMessageFile(FindFileOrFail(englishLayouts, EnglishMessagesPath));

    Formats::ByteView characters = FindFileOrFail(channelFiles, CharacterArchivePath);
    files.bodies = Mii::ReadMiiBodyFiles(characters);
    Formats::U8Archive characterFiles = ReadPackedArchive(channelFiles, CharacterArchivePath);
    files.ground = ReadPlazaGroundModel(FindFileOrFail(characterFiles, GroundModelPath));

    files.keyboardArchive = Formats::DecompressYaz0(FindFileOrFail(channelFiles, KeyboardArchivePath));

    Formats::U8Archive facePartsContent = ReadArchiveContent(contents, MiiChannelFacePartsContent);
    files.faceParts = std::make_unique<Mii::MiiResources>(CopyOf(FindFileOrFail(facePartsContent, FacePartsPath)));

    Formats::U8Archive homeMenuFiles = ReadArchiveContent(contents, MiiChannelHomeMenuContent);
    files.handArchive = Formats::DecompressLz77(FindFileOrFail(homeMenuFiles, HandArchivePath));
    return files;
}

std::u16string MessageText(const Formats::NumberedMessageFile& messages, std::uint32_t messageNumber)
{
    auto found = messages.messages.find(messageNumber);
    if (found == messages.messages.end())
        return {};

    return found->second;
}

void FillTextsFromUserData(SystemMenu::MenuLayout& layout, const Formats::NumberedMessageFile& messages)
{
    for (Formats::Pane* pane : layout.Live().AllPanes())
    {
        if (!pane->textBox || pane->userInformation.empty())
            continue;

        std::uint32_t messageNumber = 0;
        const std::string& number = pane->userInformation;
        auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), messageNumber);
        if (error != std::errc() || end != number.data() + number.size())
            continue;

        auto found = messages.messages.find(messageNumber);
        if (found != messages.messages.end())
            pane->textBox->text = found->second;
    }
}

}
