#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/MiiChannel/MiiChannelData.h"
#include "Channels/WiiSpeak/WiiSpeakDialog.h"
#include "Channels/WiiSpeak/WiiSpeakFiles.h"
#include "Channels/WiiSpeak/WiiSpeakMessages.h"
#include "Channels/WiiSpeak/WiiSpeakSaveData.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"

#include <cstddef>
#include <memory>
#include <optional>

namespace Channels
{

// The Wii Speak Channel 2.0 (HCFE v512) (docs/research/Channels.md 8, docs/audit/WiiSpeak.md).
//
// The start follows FirstSetUpScene (0x80094920 of the program): the Mii database must be readable ("Could not read save data from
// the Mii Channel." and back to the Wii Menu otherwise), the Wii Speak accessory must be there (its two messages, and back to the Wii
// Menu), and the save file mic_chat.dat is read. Without a save file this is the first start: three windows of welcome, the
// precautions, the save file is made ("Save data created!"). A damaged save file asks whether to make new data.
// Then the chat room runs, with Choose Miis, Show Photos, the Wii number, the address book, the settings, the performance check,
// the voice message and the lobby behind its buttons.
//
// The microphone stays open while the channel runs; every step the channel takes what it heard and gives it to the screen.
class WiiSpeakChannel : public Channel
{
public:
    enum class Stage
    {
        Starting,
        Refusing,
        AskingForNewData,
        Welcoming,
        ShowingPrecautions,
        TellingBeforeTheChatRoom,
        Running,
    };

    explicit WiiSpeakChannel(ChannelServices& services);
    ~WiiSpeakChannel() override;

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;

    Stage CurrentStage() const;
    std::optional<WiiSpeakScreenName> CurrentScreenName() const;
    WiiSpeakScreen* CurrentScreen();
    WiiSpeakSession& Session();
    std::size_t MiiCount() const;

    // Where a pane of the picture is, in layout units of the picture: the message window first, then the screen from the front.
    // Nothing when no shown layout has the pane.
    std::optional<Formats::Vector2> PlaceOfPane(std::string_view paneName);

private:
    void Start();
    bool CheckMiiDatabase();
    bool OpenMicrophone();
    void Refuse(std::uint32_t messageId);
    void ReadSaveFile();
    bool ForgetDeletedMiis();
    void FollowTheFirstStart();
    void ShowNextWelcome();
    void FinishFirstStart();
    void HearTheMicrophone();
    void ChangeScreen(WiiSpeakScreenName name);

    ChannelServices& services;
    WiiSpeakFiles files;
    WiiSpeakMessages messages;
    SystemMenu::MenuFonts fonts;
    std::unique_ptr<SystemMenu::CursorManager> hands;
    WiiSpeakDialog dialog;
    WiiSpeakSaveFile saveFile;
    OfflineFriendServer friendServer;
    WiiSpeakSession session;
    std::unique_ptr<WiiSpeakScreen> screen;
    std::optional<WiiSpeakScreenName> screenName;
    Stage stage = Stage::Starting;
    int welcomeNumber = 0;
};

}
