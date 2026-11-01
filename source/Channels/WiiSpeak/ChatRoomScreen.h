#pragma once

#include "Channels/OfflineConnection.h"
#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"

#include <memory>
#include <optional>

namespace Channels
{

// The chat room (scene "talk", ChatRippleScene 0x800A89B0 of the program): the water background, the clock, the circle of the player,
// which lights up while the microphone hears a voice, the mute sign and the buttons on both sides.
//
// The buttons WeeOS has: Wii Menu (with the question "Returning to the Wii Menu. Is this OK?"), Help, Settings, Create Message and
// Lobby. The Lobby connects to Nintendo Wi-Fi Connection, which fails as with an unplugged cable: "Connecting to Nintendo Wi-Fi
// Connection." until the connection gives up, then "Unable to connect to the Internet." with the support code (CHN-04).
// Choose Miis, Show Photos, Wii Number and the Address Book are not built and are not shown (docs/audit/WiiSpeak.md).
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
    void ShowMute();
    std::optional<WiiSpeakScreenName> Press(const std::string& button);
    void StartConnecting();
    void FollowTheMessageWindow();
    void ToggleMute();

    WiiSpeakSession& session;
    SceneLayout background;
    SceneLayout buttons;
    SceneLayout clock;
    SceneLayout muteSign;
    SceneLayout circle;
    Waiting waiting = Waiting::Nothing;
    std::optional<ConnectionFailure> connection;
    double connectionStartSeconds = 0.0;
    bool isCircleLit = false;
};

}
