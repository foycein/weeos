#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/PagePainter.h"
#include "Channels/ShopAccessMark.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/NetworkStart.h"
#include "SystemMenu/OwnServer.h"

#include "Text/MessageCatalog.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

// The Wii Shop Channel. The Shop is a browser around the pages of a server. Its server is WeeOS's
// own server, which does not exist yet, so all it shows are the start-up picture and its local error pages,
// which WeeOS draws natively from the pages' files:
//  1. the start-up picture, the Wii logo of the area (StartupTM for the USA, 0x8001B420), with the turning access mark under the logo and
//     its loading sound, while the network starts and the first page of the server is asked for (CHN-93);
//  2. the page of the error: internet.html, with the button Wii Menu, for the codes of a missing Internet connection, and error.html,
//     with Wii Menu and Try Again, for the others (0x8002BA40). A server that cannot be reached gives error.html. Try Again asks again:
//     the page stays, with the access mark small in its upper left corner, until the next page comes.
// The texts are the Shop's English message file of the USA, which is inside its program.
class ShopChannel : public Channel
{
public:
    enum class Stage
    {
        // The start-up picture while the first page is asked for.
        StartUp,

        // Try Again: the page of the error while the page is asked for again.
        Connecting,

        ShowingError,
    };

    enum class ErrorPage
    {
        // internet.html: "Unable to connect to the Internet", the button Wii Menu.
        Internet,

        // error.html: "Unable to connect to the server", the buttons Wii Menu and Try Again.
        Server,
    };

    explicit ShopChannel(ChannelServices& channelServices);

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;

    Stage CurrentStage() const;
    ErrorPage ShownErrorPage() const;

    // "Error Code: 209516" and the message, as the page shows them.
    std::u16string ErrorCodeText() const;
    std::u16string ErrorMessageText() const;

    // A text of the Shop's message file.
    const std::u16string& Message(std::uint16_t messageNumber) const;

private:
    struct PageButton
    {
        PageRectangle shadow;
        PageRectangle banner;
        PageRectangle word;
        std::uint16_t message;
    };

    void UpdateStage();
    void StartConnecting();
    std::optional<std::uint32_t> FindErrorCode();
    void UpdateButtons();
    std::vector<PageButton> ShownButtons() const;
    bool IsPointedAt(const PageButton& button) const;

    void UpdateAccessMark();
    void DrawWallpaper();
    void DrawError();
    void DrawButton(const PageButton& button);

    std::optional<Formats::Vector2> PointerOnPage(int playerIndex) const;

    ChannelServices& services;
    Text::MessageCatalog messages;
    std::unique_ptr<PagePainter> painter;
    std::unique_ptr<PagePicture> startupPicture;
    std::unique_ptr<PagePicture> wallpaper;
    std::unique_ptr<PagePicture> buttonPicture;
    std::unique_ptr<PagePicture> focusedButtonPicture;
    std::unique_ptr<PagePicture> buttonShadow;
    std::unique_ptr<SystemMenu::CursorManager> cursors;
    std::unique_ptr<ShopAccessMark> accessMark;
    bool playsTheLoadingSound = false;

    Stage stage = Stage::StartUp;
    SystemMenu::NetworkStart networkStart;
    std::unique_ptr<SystemMenu::ServerContact> server;
    std::uint32_t errorCode = 0;
    double connectingStartSeconds = 0;
    std::optional<std::uint16_t> focusedButtonMessage;
};

}
