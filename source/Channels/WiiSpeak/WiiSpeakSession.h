#pragma once

#include "Channels/ChannelServices.h"
#include "Channels/WiiSpeak/MiiFaces.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakDialog.h"
#include "Channels/WiiSpeak/WiiSpeakFiles.h"
#include "Channels/WiiSpeak/WiiSpeakFriends.h"
#include "Channels/WiiSpeak/WiiSpeakMessages.h"
#include "Channels/WiiSpeak/WiiSpeakRipples.h"
#include "Channels/WiiSpeak/WiiSpeakSaveData.h"

#include "SystemMenu/OwnServer.h"
#include "SystemMenu/SoftKeyboard.h"

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
    ChooseMiis,
    AddressBook,
    ShowPhotos,
};

// What every screen of the channel shares: the services of the channel system, the files and messages of the title, the message
// window, the soft keyboard, the save file, the friend server and what the microphone heard in this step.
struct WiiSpeakSession
{
    WiiSpeakSession(ChannelServices& channelServices, const WiiSpeakFiles& titleFiles, const WiiSpeakMessages& titleMessages,
        WiiSpeakDialog& messageWindow, WiiSpeakSaveFile& file, WiiSpeakFriendServer& friendServer);
    WiiSpeakSession(const WiiSpeakSession&) = delete;
    WiiSpeakSession& operator=(const WiiSpeakSession&) = delete;
    ~WiiSpeakSession();

    ChannelServices& services;
    const WiiSpeakFiles& files;
    const WiiSpeakMessages& messages;
    WiiSpeakDialog& dialog;
    WiiSpeakSaveFile& saveFile;
    WiiSpeakSaveData saveData;
    WiiSpeakFriendServer& friends;

    // The keyboard of the scene "keyboard". While it is open the channel moves it on instead of the screen.
    SystemMenu::SoftKeyboard keyboard;

    // The Miis of the console, read from RFL_DB.dat at the start, in the order of their slots, and their faces.
    std::vector<Mii::MiiRecord> consoleMiis;
    MiiFaces miiFaces;

    // The ripples on the water, moved by the channel in every step and drawn by the screens over their water.
    WiiSpeakRipples ripples;

    // The headphone mode of the channel. It is not saved ("Headphone settings will not be saved.", 4915230).
    bool usesHeadphones = false;

    // The photo that Show Photos chose for the chat room, as a texture of the device, and whether the chat room has yet to print it.
    std::optional<Layout::LoadedTexture> sharedPhoto;
    bool isSharedPhotoNew = false;
    void SharePhoto(const Formats::RgbaImage& photo);
    void PutThePhotoAway();

    // The samples that the microphone heard in this step.
    std::vector<std::int16_t> heardThisStep;

    std::u16string Text(std::uint32_t messageId, std::u16string_view argument = {}) const;

    // Opens the message window with a text of the channel and the labels of its buttons, all by message id.
    void ShowMessage(std::uint32_t messageId, const std::vector<std::uint32_t>& buttonMessageIds);

    // "An Internet connection cannot be established, so WiiConnect24 cannot be used." with the code of the failed connection (CHN-04),
    // when the friend server does not answer.
    void ShowWiiConnect24Unreachable();
    static SystemMenu::ServerAnswer WiiConnect24ServerAnswerOf(const std::string& serverAddress);
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
