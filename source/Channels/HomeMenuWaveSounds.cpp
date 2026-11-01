#include "Channels/HomeMenuWaveSounds.h"

#include "Channels/ChannelFiles.h"
#include "Channels/NativeChannels.h"

#include "Sound/MidiFile.h"
#include "Sound/WaveTable.h"
#include "Sound/WaveTableSynth.h"

#include <array>
#include <string_view>

namespace Channels
{

namespace
{

constexpr std::uint16_t InterfaceContentIndex = 5;
constexpr std::string_view HomeMenuSoundsPath = "homebutton/HomeButtonSe.arc";
constexpr std::string_view WaveTablePath = "wt\\HomeButtonSe.wt";
constexpr std::string_view WaveSamplesPath = "wt\\HomeButtonSe.pcm";

struct NamedMidiFile
{
    std::string_view path;
    std::string_view soundName;
};

// The files in the order of the program's table (0x809E02E8) and the names the HOME menu asks for. The program's table also names
// 22_manual_open and 23_manual_focus, which the archive does not have: the Internet Channel's HOME menu has no manual button.
constexpr std::array<NamedMidiFile, 22> HomeMenuMidiFiles = {{
    {"midi\\00_home_button.mid", "HOMESE_HOME_BUTTON"},
    {"midi\\01_return_app.mid", "HOMESE_RETURN_APP"},
    {"midi\\02_goto_menu.mid", "HOMESE_GOTO_MENU"},
    {"midi\\03_reset_app.mid", "HOMESE_RESET_APP"},
    {"midi\\04_focus.mid", "HOMESE_FOCUS"},
    {"midi\\05_select.mid", "HOMESE_SELECT"},
    {"midi\\06_cancel.mid", "HOMESE_CANCEL"},
    {"midi\\07_open_controller.mid", "HOMESE_OPEN_CONTROLLER"},
    {"midi\\08_close_controller.mid", "HOMESE_CLOSE_CONTROLLER"},
    {"midi\\09_volume_plus.mid", "HOMESE_VOLUME_PLUS"},
    {"midi\\10_volume_minus.mid", "HOMESE_VOLUME_MINUS"},
    {"midi\\11_volume_plus_limit.mid", "HOMESE_VOLUME_PLUS_LIMIT"},
    {"midi\\12_volume_minus_limit.mid", "HOMESE_VOLUME_MINUS_LIMIT"},
    {"midi\\13_nothing_done.mid", "HOMESE_NOTHING_DONE"},
    {"midi\\14_vibe_on.mid", "HOMESE_VIBE_ON"},
    {"midi\\15_vibe_off.mid", "HOMESE_VIBE_OFF"},
    {"midi\\16_start_connect_window.mid", "HOMESE_START_CONNECT_WINDOW"},
    {"midi\\17_connected1.mid", "HOMESE_CONNECTED"},
    {"midi\\18_connected2.mid", "HOMESE_CONNECTED2"},
    {"midi\\19_connected3.mid", "HOMESE_CONNECTED3"},
    {"midi\\20_connected4.mid", "HOMESE_CONNECTED4"},
    {"midi\\21_end_connect_window.mid", "HOMESE_END_CONNECT_WINDOW"},
}};

}

RenderedSounds RenderHomeMenuSounds(Ios::TitleId titleId, SystemMenu::TitleContents& contents)
{
    if (titleId != InternetChannelTitleId)
        return {};

    Formats::U8Archive interfaceFiles = ReadArchiveContent(contents, InterfaceContentIndex);
    Formats::U8Archive sounds = Formats::ReadU8Archive(FindFileOrFail(interfaceFiles, HomeMenuSoundsPath));
    Sound::WaveTable table(FindFileOrFail(sounds, WaveTablePath), FindFileOrFail(sounds, WaveSamplesPath));

    RenderedSounds rendered;
    for (const NamedMidiFile& file : HomeMenuMidiFiles)
    {
        Sound::MidiFile midi = Sound::ReadMidiFile(FindFileOrFail(sounds, file.path));
        rendered[std::string(file.soundName)] = std::make_shared<const Sound::DecodedWave>(Sound::RenderMidi(midi, table));
    }
    return rendered;
}

}
