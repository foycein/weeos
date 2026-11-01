#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakDialog.h"
#include "Channels/WiiSpeak/WiiSpeakFiles.h"
#include "Channels/WiiSpeak/WiiSpeakMessages.h"
#include "Channels/WiiSpeak/WiiSpeakSaveData.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

// The screens of the Wii Speak Channel that WeeOS has (docs/audit/WiiSpeak.md).
enum class WiiSpeakScreenName
{
    ChatRoom,
    SettingsMenu,
    Sensitivity,
    Headphones,
    PerformanceCheck,
    Precautions,
    CreateMessage,
};

// What every screen of the channel shares: the services of the channel system, the files and messages of the title, the message
// window, the save file and what the microphone heard in this step.
struct WiiSpeakSession
{
    WiiSpeakSession(ChannelServices& channelServices, const WiiSpeakFiles& titleFiles, const WiiSpeakMessages& titleMessages,
        WiiSpeakDialog& messageWindow, WiiSpeakSaveFile& file);

    ChannelServices& services;
    const WiiSpeakFiles& files;
    const WiiSpeakMessages& messages;
    WiiSpeakDialog& dialog;
    WiiSpeakSaveFile& saveFile;
    WiiSpeakSaveData saveData;

    // The headphone mode of the channel. It is not saved ("Headphone settings will not be saved.", 4915230).
    bool usesHeadphones = false;

    // The samples that the microphone heard in this step.
    std::vector<std::int16_t> heardThisStep;

    std::u16string Text(std::uint32_t messageId, std::u16string_view argument = {}) const;

    // Opens the message window with a text of the channel and the labels of its buttons, all by message id.
    void ShowMessage(std::uint32_t messageId, const std::vector<std::uint32_t>& buttonMessageIds);
};

// A screen of the channel. Each step it moves on and says which screen comes next, if another one does.
class WiiSpeakScreen
{
public:
    WiiSpeakScreen() = default;
    WiiSpeakScreen(const WiiSpeakScreen&) = delete;
    WiiSpeakScreen& operator=(const WiiSpeakScreen&) = delete;
    virtual ~WiiSpeakScreen() = default;

    // While the message window is open the channel does not ask the screen; the screen gets the choice of the window.
    virtual std::optional<WiiSpeakScreenName> Update() = 0;
    virtual void Draw() = 0;

    // The layouts that the screen shows, the one in front last.
    virtual std::vector<SceneLayout*> ShownLayouts() = 0;
};

}
