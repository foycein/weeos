#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/OfflineConnection.h"
#include "Channels/WaitingMark.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/MenuLayout.h"

#include "Layout/LayoutArchive.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace Channels
{

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

// The Forecast and the News Channel without a network (docs/research/Channels.md 5.5 and 6.4). Both need their data from WiiConnect24
// before they can show anything, and WeeOS has none: the channel fades in "One moment, please..." (layout error1) while its
// download tries to connect, with its waiting mark under it, fades out when the download fails, and fades in the error layout error0 with the message of an
// Internet connection that cannot be made, the error code under it and the button "Back to the Wii Menu".
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

    // The error layout shows the message with this number when the download cannot connect to the Internet (0x8000B46C).
    static constexpr int CannotConnectMessage = 2;

    WiiConnect24Channel(ChannelServices& channelServices, const WiiConnect24ChannelFiles& files);

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;

    Stage CurrentStage() const;
    const WaitingMark& Mark() const;

    // The text of the error code under the message, for example "Error Code: 107245".
    std::u16string ErrorCodeText() const;

private:
    void UpdateStage();
    void UpdateOneMoment();
    void UpdateLeavingOneMoment();
    void UpdateError();
    void ShowErrorLayout();
    bool IsButtonPressed() const;
    SystemMenu::MenuLayout& ShownLayout();

    ChannelServices& services;
    ConnectionFailure failure;
    std::unique_ptr<Layout::LayoutArchive> layoutArchive;
    SystemMenu::MenuFonts fonts;
    std::unique_ptr<SystemMenu::MenuLayout> oneMomentLayout;
    std::unique_ptr<SystemMenu::MenuLayout> errorLayout;
    std::unique_ptr<SystemMenu::CursorManager> cursors;
    std::unique_ptr<WaitingMark> waitingMark;
    int errorMessageCount;
    Stage stage = Stage::OneMoment;
};

}
