#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/OfflineConnection.h"
#include "Channels/OperaLanguageFile.h"
#include "Channels/PagePainter.h"

#include "SystemMenu/CursorManager.h"

#include "Formats/U8Archive.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Channels
{

// The Internet Channel without a network (docs/research/Channels.md 7). The channel shows its start page at once and joins the network
// in the background (0x800320CC). The join fails, and while the start page is on screen the channel shows its dialog
// "%06d: Unable to connect to the internet. ..." with OK (0x800338B0). The page "Wii Menu" asks "Return to the Wii Menu?" with OK and
// Cancel. The start page is the browser's local page startpage.html, built by startpage.js with the styles and pictures of its
// widescreen folder; WeeOS draws it natively from those files and the texts of english.lng.
class InternetChannel : public Channel
{
public:
    enum class Screen
    {
        StartPage,

        // The network dialog: the code of the connection that failed, and OK.
        NetworkError,

        // "Return to the Wii Menu?" with OK and Cancel.
        ReturnQuestion,
    };

    explicit InternetChannel(ChannelServices& channelServices);

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;

    Screen CurrentScreen() const;

    // The text of the dialog that shows, with the code filled in.
    std::u16string DialogText() const;

    // A text of english.lng by its name, for example "S_START".
    const std::u16string& Text(std::string_view name) const;

private:
    // The places of the start page that the pointer can point at, in the order of startpage.js.
    enum class Place
    {
        Search,
        Favorites,
        WebAddress,
        OperationsGuide,
        Settings,
        WiiMenu,
    };

    void UpdateStartPage();
    void UpdateDialog();
    std::optional<Place> PointedPlace() const;
    std::optional<Formats::Vector2> PointerOnPage() const;
    bool IsPressed() const;
    PageRectangle DialogBox() const;
    PageRectangle OkButton() const;
    PageRectangle CancelButton() const;

    void DrawStartPage();
    void DrawOrbs();
    void DrawButtons();
    void DrawDialog();

    std::unique_ptr<PagePicture> ReadPicture(std::string_view path) const;

    ChannelServices& services;
    Formats::U8Archive pages;
    OperaLanguageFile texts;
    PageFrame frame;
    float containerLeft;
    std::unique_ptr<PagePainter> painter;
    std::unique_ptr<SystemMenu::CursorManager> cursors;

    std::unique_ptr<PagePicture> background;
    std::unique_ptr<PagePicture> logo;
    std::unique_ptr<PagePicture> homeIcon;
    std::unique_ptr<PagePicture> orb;
    std::unique_ptr<PagePicture> gloss;
    std::array<std::unique_ptr<PagePicture>, 3> orbSymbols;
    std::unique_ptr<PagePicture> button;
    std::array<std::unique_ptr<PagePicture>, 3> buttonIcons;
    std::unique_ptr<PagePicture> controller;
    std::unique_ptr<PagePicture> dialogBack;
    std::unique_ptr<PagePicture> infoSign;
    std::unique_ptr<PagePicture> questionSign;
    std::unique_ptr<PagePicture> okButton;
    std::unique_ptr<PagePicture> cancelButton;

    Screen screen = Screen::StartPage;
    ConnectionFailure failure{};
    bool hasShownNetworkError = false;
    std::optional<Place> focusedPlace;
};

}
