#pragma once

#include "SystemMenu/NetworkStart.h"
#include "SystemMenu/OwnServer.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"
#include "Channels/WiiSpeak/WiiSpeakWater.h"

#include <ctime>
#include <memory>
#include <optional>

namespace Channels
{

// The chat room (scene "talk", ChatRippleScene 0x800A89B0 of the program): the water background, the clock, the circle of the player,
// which lights up while the microphone hears a voice, the mute sign and the buttons on both sides.
//
// The buttons: Wii Menu (with the question "Returning to the Wii Menu. Is this OK?"), Choose Miis, Show Photos, Help, Settings,
// Wii Number, Create Message, Address Book and Lobby. The Lobby connects to Nintendo Wi-Fi Connection, which fails as with an
// unplugged cable: "Connecting to Nintendo Wi-Fi Connection." until the connection gives up, then "Unable to connect to the
// Internet." with the support code (CHN-04). The chosen Miis stand in the circle; a photo of Show Photos comes out of the slot.
// The - button mutes the accessory and unmutes it again.
class ChatRoomScreen : public WiiSpeakScreen
{
public:
    explicit ChatRoomScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    bool IsCircleLit() const;

private:
    enum class Waiting
    {
        Nothing,
        AnswerToReturning,
        Connecting,
        ConnectionError,
    };

    void ShowTexts();
    void ShowClock();
    void StartClockRipples(const std::tm& calendar);
    void ShowMute();
    std::optional<WiiSpeakScreenName> Press(const std::string& button);
    void StartConnecting();
    std::optional<std::uint32_t> FindConnectionFailure();
    void FollowTheMessageWindow();
    void ToggleMute();
    void ShowChosenMiis();
    void ShowSharedPhoto();
    void ShowWiiNumber();

    WiiSpeakSession& session;
    std::unique_ptr<WiiSpeakWater> water;
    SceneLayout buttons;
    SceneLayout clock;
    SceneLayout muteSign;
    SceneLayout circle;
    std::unique_ptr<SceneLayout> miisInCircle;
    int lastShownSecond = -1;
    std::unique_ptr<SceneLayout> photo;
    Waiting waiting = Waiting::Nothing;
    SystemMenu::NetworkStart networkStart;
    std::unique_ptr<SystemMenu::ServerContact> server;
    double connectionStartSeconds = 0.0;
    bool isCircleLit = false;
};

}
