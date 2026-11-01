#pragma once

#include "Channels/MiiChannel/PlazaGround.h"

#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/TitleContents.h"

#include "Formats/Font.h"
#include "Formats/NumberedMessageFile.h"
#include "Layout/LayoutArchive.h"
#include "Mii/MiiBodyFile.h"
#include "Mii/MiiResources.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Channels
{

// The Mii Channel 6 (HACA): content 2 holds the channel's files, content 3 is the face library's parts RFL_Res.dat, content 4 the
// shared HOME Button Menu (docs/research/Channels.md 2.1).
constexpr std::uint16_t MiiChannelFilesContent = 2;
constexpr std::uint16_t MiiChannelFacePartsContent = 3;
constexpr std::uint16_t MiiChannelHomeMenuContent = 4;

// Everything the Mii Channel reads from its title when it starts (the loads of 8000F704, 80010E80 and 80017EB0): the layouts and fonts of
// Channel/ScreenAll/Cmn/Layout00.szs, the English messages of Channel/ScreenAll/Usa/Layout00.szs, the bodies and the ground of
// Channel/Character.szs with its effects, the keyboard of Channel/sofkeybdkr.szs and the parts of the faces.
struct MiiChannelFiles
{
    std::unique_ptr<Layout::LayoutArchive> layouts;
    Formats::NumberedMessageFile messages;
    Formats::Font mainFont;
    Formats::Font nameFont;
    Mii::MiiBodyFiles bodies;
    PlazaGroundModel ground;
    PlazaGroundModel road;
    std::unique_ptr<Mii::MiiResources> faceParts;
    Formats::Bytes keyboardArchive;

    // The hands of the HOME Button Menu, which show while it is open.
    Formats::Bytes handArchive;

    // The particle effects of Character.szs: the channel's own hands and the effects of the Miis (CHN-94).
    Formats::Bytes handEffects;
    Formats::Bytes handPictures;
    Formats::Bytes miiEffects;
    Formats::Bytes miiEffectPictures;
};

// Throws a ChannelError or a FormatError when a file is missing or broken.
MiiChannelFiles ReadMiiChannelFiles(SystemMenu::TitleContents& contents);

// The text of a message, or an empty text for a number the file does not have.
std::u16string MessageText(const Formats::NumberedMessageFile& messages, std::uint32_t messageNumber);

// The text boxes of the Mii Channel's layouts name their message in their user data ("0100100" is "New Mii"), and the program fills them
// all when it makes the layout (80008F6C). Text boxes without a number keep the text of the layout.
void FillTextsFromUserData(SystemMenu::MenuLayout& layout, const Formats::NumberedMessageFile& messages);

}
