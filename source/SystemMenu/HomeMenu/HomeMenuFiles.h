#pragma once

#include "SystemMenu/MenuFiles.h"

#include "Formats/RgbaImage.h"
#include "Formats/U8Archive.h"
#include "Input/RemoteOutputs.h"
#include "Input/RemoteState.h"
#include "Layout/LayoutArchive.h"

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The texts of homebutton/home.csv, in English.
struct HomeMenuTexts
{
    // "Simultaneously press 1 and 2 on each Wii Remote in the desired player order."
    std::u16string reconnectInstruction;

    // "Disconnecting..."
    std::u16string disconnecting;

    // The question of each big button: "Return to the Wii Menu?", then "Reset the software?".
    std::vector<std::u16string> buttonQuestions;
};

// The files of the HOME Button Menu in the resource content of the System Menu (docs/research/SystemMenuStructure.md 4.13).
struct HomeMenuFiles
{
    // The layout, its animations and its pictures: the English archive layout/eng/homeBtn1.ash and the common one
    // layout/common/homeBtn1.ash, read as one archive.
    std::unique_ptr<Layout::LayoutArchive> archive;

    // From homebutton/config.txt: the name of the layout, the start of the names of its animations,
    // and for every big button whether it asks before it acts.
    std::string layoutName;
    std::string animationPrefix;
    std::vector<bool> buttonAsks;

    HomeMenuTexts texts;

    // The sounds for the speaker of the remote (homebutton/SpeakerSe.arc): one for a change of the volume,
    // and one for each player whose remote connects.
    Input::SpeakerSound volumeSound;
    std::array<Input::SpeakerSound, Input::RemoteChannelCount> connectSounds;

    // homebutton/homeBtnIcon.tpl: the picture that says that the HOME menu cannot open now.
    Formats::RgbaImage prohibitedIcon;
};

// Throws a MenuError or a FormatError when a file is missing or damaged.
HomeMenuFiles ReadHomeMenuFiles(const MenuFiles& files);

// The same files of the copy that a channel carries in its shared content of the HOME menu, in the folder of the version that its
// program opens, for example "./HomeButton3". The channel offers the first offeredButtonCount big buttons: a channel whose manual
// cannot be shown leaves out the third one.
HomeMenuFiles ReadChannelHomeMenuFiles(const Formats::U8Archive& homeMenuContent, std::string_view folder, std::size_t offeredButtonCount);

}
