#pragma once

#include "SystemMenu/TitleContents.h"

#include "Formats/Bytes.h"
#include "Formats/MessageFile.h"
#include "Formats/U8Archive.h"
#include "Layout/LayoutArchive.h"

#include <memory>
#include <string_view>

namespace Channels
{

// The files of the Wii Speak Channel 2.0 (HCFE v512) in its content 2 (docs/research/Channels.md 8.1): the system archive
// "system.szs" with the common layouts, the fonts and the messages, the archives of the scenes "scene/<name>.szs", the sound archive
// and the music of the performance check. The .szs archives are packed with Yaz0; each holds the layout bundles of a scene as
// "<number>_<name>.arc", whose folders blyt, anim and timg lie at the top.
class WiiSpeakFiles
{
public:
    // Throws a ChannelError when the content or a file in it is missing.
    explicit WiiSpeakFiles(SystemMenu::TitleContents& contents);

    // A layout bundle of the system archive, for example "01030_dialog.arc".
    std::unique_ptr<Layout::LayoutArchive> SystemLayouts(std::string_view bundleName) const;

    // A layout bundle of a scene, for example scene "talk" and bundle "02000_button.arc".
    std::unique_ptr<Layout::LayoutArchive> SceneLayouts(std::string_view sceneName, std::string_view bundleName) const;

    // The English messages of the USA, message/WiiMic_us_en.bmg (0x800B8E88 of the program picks the file by area and language).
    Formats::MessageFile EnglishMessages() const;

    // The font of the layouts, pac_nRodDb_32_I4.brfnt: Rodin DB in 32 points, the face of the console's own font.
    Formats::ByteView LayoutFont() const;

    // The large Rodin DB of the countdown before a recording, pac_nRodDb_200_IA4.brfnt.
    Formats::ByteView LargeLayoutFont() const;

    // The music of the performance check, eccheckbgm.bns.
    Formats::ByteView CheckMusic() const;

private:
    Formats::U8Archive resources;
    Formats::U8Archive systemArchive;
};

}
