#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/OfflineConnection.h"
#include "Channels/PagePainter.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/MenuFonts.h"

#include "Text/MessageCatalog.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Channels
{

// The Wii Shop Channel without a network (docs/research/Channels.md 4.4). The Shop is a browser around the pages of a server, so
// all it can show offline are the start-up picture and its local error pages, which WeeOS draws natively from the pages' files:
//  1. the start-up picture, the Wii logo of the area (StartupTM for the USA, 0x8001B420);
//  2. "Connecting. Please wait..." on the wallpaper of the pages while the connection is tried;
//  3. the page of the error: internet.html, with the button Wii Menu, for the codes of a missing Internet connection, and error.html,
//     with Wii Menu and Try Again, for the others (0x8002BA40). Try Again tries the connection again.
// The texts are the Shop's English message file of the USA, which is inside its program.
class ShopChannel : public Channel
{
public:
    enum class Stage
    {
        StartUp,
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

    Stage CurrentStage() const;
    ErrorPage ShownErrorPage() const;

    // "Error Code: 50299" and the message, as the page shows them.
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
    void UpdateButtons();
    std::vector<PageButton> ShownButtons() const;
    bool IsPointedAt(const PageButton& button) const;

    void DrawWallpaper();
    void DrawConnecting();
    void DrawError();
    void DrawButton(const PageButton& button);

    std::optional<Formats::Vector2> PointerOnPage(int playerIndex) const;

    ChannelServices& services;
    Text::MessageCatalog messages;
    SystemMenu::MenuFonts fonts;
    std::unique_ptr<PagePainter> painter;
    std::unique_ptr<PagePicture> startupPicture;
    std::unique_ptr<PagePicture> wallpaper;
    std::unique_ptr<PagePicture> buttonPicture;
    std::unique_ptr<PagePicture> focusedButtonPicture;
    std::unique_ptr<PagePicture> buttonShadow;
    std::unique_ptr<SystemMenu::CursorManager> cursors;

    Stage stage = Stage::StartUp;
    ConnectionFailure failure;
    double connectingStartSeconds = 0;
    std::optional<std::uint16_t> focusedButtonMessage;
};

}
