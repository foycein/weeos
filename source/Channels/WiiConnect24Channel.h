#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/WaitingMark.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/NetworkStart.h"
#include "SystemMenu/OwnServer.h"

#include "Layout/LayoutArchive.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Channels
{

// The code of the WiiConnect24 download (KD 0x13DB3760): while the network does not start, from the error of SOStartup; while it does,
// from what WeeOS's own server answered (CHN-04).
std::uint32_t DownloadErrorCodeOfStart(int startupError);
std::uint32_t DownloadErrorCodeOfServer(SystemMenu::ServerAnswer answer);

// Where a channel that lives on WiiConnect24 data keeps the files of its start, and what differs between the Forecast and the News Channel.
struct WiiConnect24ChannelFiles
{
    // The content with the layouts, the fonts and the sounds, and the packed layout archive in it.
    std::uint16_t resourceContentIndex;
    std::string_view layoutArchivePath;

    // The font of the layouts: its path in the layout archive and the name the layouts give it.
    std::string_view fontPath;
    std::string_view fontName;

    // How many messages the error layout holds (textTUS0, textTUS1, ...).
    int errorMessageCount;

    // The shared content of the HOME Button Menu, which holds the hands of the four players.
    std::uint16_t homeMenuContentIndex;

    // The mark under "One moment, please..." that shows that the channel is busy.
    WaitingMarkLook waitingMark;
};

// The Forecast and the News Channel. Both need their data from a server before they can show
// anything. Their server is WeeOS's own server, which does not exist yet: the channel fades in
// "One moment, please..." (layout error1) with its waiting mark under it while its download tries to reach the server, fades out when
// the download fails, and fades in the error layout error0 with the message of the failure, the error code under it and the button
// "Back to the Wii Menu".
// The message and the code are the ones of the WiiConnect24 download of the original (CHN-04): the Internet cannot be reached while the
// PC is not connected or the name of the server is not known, the service is unavailable while the server does not answer.
// The order of the fades is the original's (Forecast 0x8000AD28), the choice of the message is the one of 0x8000B46C.
class WiiConnect24Channel : public Channel
{
public:
    enum class Stage
    {
        // "One moment, please..." fades in and stays until the download has failed.
        OneMoment,

        // "One moment, please..." fades out.
        LeavingOneMoment,

        // The error fades in and waits for the button.
        ShowingError,
    };

    // The error layout shows the message with this number when the download cannot reach the Internet, and the next one when the
    // service is unavailable (0x8000B46C).
    static constexpr int CannotConnectMessage = 2;
    static constexpr int ServiceUnavailableMessage = 3;

    WiiConnect24Channel(ChannelServices& channelServices, const WiiConnect24ChannelFiles& files);

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;
    EffectHands* OwnHands() override;

    Stage CurrentStage() const;
    const WaitingMark& Mark() const;

    // The message of the error layout, and the text of the error code under it, for example "Error Code: 107304".
    // Nothing and an empty text while the download still tries.
    std::optional<int> ShownMessage() const;
    std::u16string ErrorCodeText() const;

private:
    // How the download failed: the message of the error layout and the code of the WiiConnect24 module.
    struct DownloadFailure
    {
        int message;
        std::uint32_t code;
    };

    std::optional<DownloadFailure> FindDownloadFailure();
    void UpdateStage();
    void UpdateOneMoment();
    void UpdateLeavingOneMoment();
    void UpdateError();
    void ShowErrorLayout();
    bool IsButtonPressed() const;
    SystemMenu::MenuLayout& ShownLayout();

    ChannelServices& services;
    SystemMenu::NetworkStart networkStart;
    std::unique_ptr<SystemMenu::ServerContact> server;
    std::optional<DownloadFailure> failure;
    std::unique_ptr<Layout::LayoutArchive> layoutArchive;
    SystemMenu::MenuFonts fonts;
    std::unique_ptr<SystemMenu::MenuLayout> oneMomentLayout;
    std::unique_ptr<SystemMenu::MenuLayout> errorLayout;
    std::unique_ptr<SystemMenu::CursorManager> cursors;
    std::unique_ptr<EffectHands> ownHands;
    std::unique_ptr<WaitingMark> waitingMark;
    int errorMessageCount;
    Stage stage = Stage::OneMoment;
};

}
