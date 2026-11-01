#pragma once

#include "Channels/Channel.h"
#include "Channels/ChannelServices.h"
#include "Channels/Internet/FavoritesScreen.h"
#include "Channels/Internet/OperaData.h"
#include "Channels/Internet/PageToolbar.h"
#include "Channels/Internet/PageView.h"
#include "Channels/Internet/SettingsScreen.h"
#include "Channels/OperaLanguageFile.h"
#include "Channels/PagePainter.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/NetworkStart.h"
#include "SystemMenu/SoftKeyboard.h"

#include "Formats/U8Archive.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Channels
{

// The Internet Channel (docs/research/Channels.md 7, docs/audit/Browser.md). The channel shows its start page at once and joins the
// network in the background (0x800320CC). The console is online through the PC (docs/DECISIONS.md, Online): while the PC is connected
// the join succeeds; while it is not, the join fails after the time-out, and while the start page is on screen it shows its dialog
// "%06d: Unable to connect to the internet. ..." with OK (0x800338B0). The start page is the browser's local page startpage.html,
// drawn natively from its widescreen styles, pictures and the texts of english.lng.
//
// Its buttons work as the original's: Search and Web Address open the channel's soft keyboard, Favorites shows the favorites that the
// channel keeps in opera.vff in the NAND, Settings shows the settings that mean something on a PC, the Operations Guide shows the
// manual pages of the title, and Wii Menu asks "Return to the Wii Menu?". Web pages are Chromium's (the part Browser), drawn into the
// picture with the toolbar of the original under them. Pages of Nintendo's services go to the owner's server, and fail while there is none.
class InternetChannel : public Channel
{
public:
    // What takes the pointer now.
    enum class Screen
    {
        StartPage,

        // The network dialog: the code of the connection that failed, and OK.
        NetworkError,

        // "Return to the Wii Menu?" with OK and Cancel.
        ReturnQuestion,

        // A web page with the toolbar.
        Page,

        Favorites,
        Settings,
        Keyboard,

        // Any other dialog: a message of a page, a question, a failed load.
        Message,
    };

    // The places of the start page that the pointer can point at, in the order of startpage.js.
    enum class Place
    {
        Search,
        Favorites,
        WebAddress,
        OperationsGuide,
        Settings,
        WiiMenu,

        // The picture of the remote with "Controls".
        Controls,
    };

    explicit InternetChannel(ChannelServices& channelServices);
    ~InternetChannel() override;

    void Update() override;
    void Draw() override;
    SystemMenu::CursorManager& Hands() override;

    Screen CurrentScreen() const;

    // The text of the dialog that shows, with the code filled in.
    std::u16string DialogText() const;

    // A text of english.lng by its name, for example "S_START".
    const std::u16string& Text(std::string_view name) const;

    const OperaData& Data() const;

    // The page of the engine. Nothing before the first page was opened.
    Browser::WebPage* Page();

    // Where a part of the screen lies in the picture, in layout units (the middle is (0, 0), y grows upward). Nothing when it is not shown.
    std::optional<Formats::Vector2> PlaceOfStartPagePlace(Place place) const;
    std::optional<Formats::Vector2> PlaceOfToolbarButton(ToolbarButton button) const;
    std::optional<Formats::Vector2> PlaceOfFavorite(std::size_t index) const;
    std::optional<Formats::Vector2> PlaceOfSettingsItem(const SettingsScreen::Item& item) const;
    std::optional<Formats::Vector2> PlaceOfEditOption(FavoritesScreen::EditOption option) const;
    std::optional<Formats::Vector2> PlaceOfDialogOk() const;

private:
    // What lies under the dialogs and the keyboard.
    enum class View
    {
        StartPage,
        Page,
        Favorites,
        Settings,
    };

    // Why a dialog is open; OK does what the question asks.
    enum class Question
    {
        NetworkError,
        ReturnToWiiMenu,
        PageMessage,
        PageQuestion,
        LoadFailure,
        PageInformation,
        DeleteFavorite,
        AlreadyFavorite,
        FavoritesFull,
        ChooseSearchEngine,
        ConfirmSearchEngine,
        SearchEngineSet,
        DeleteCookies,
        CookiesDeleted,
        Controls,
        SingleColumn,
    };

    struct Dialog
    {
        Question question;
        std::u16string text;
        bool hasCancel = false;
    };

    // What the soft keyboard types for.
    enum class Typing
    {
        WebAddress,
        Search,
        PageField,
        PagePrompt,
        FavoriteTitle,
    };

    // The answer of a dialog: OK, Cancel, or for the choice of the search engine the engine itself.
    enum class DialogAnswer
    {
        Ok,
        Cancel,
        Google,
        Yahoo,
    };

    void UpdateStartPage();
    void UpdatePage();
    void UpdatePageButtons(const Input::RemoteState& state);
    void UpdatePageScrolling(const Input::RemoteState& state);
    void UpdateFavorites();
    void UpdateEditOptions();
    void UpdateMovingFavorite(const Input::RemoteState& state);
    void PressEditOption(FavoritesScreen::EditOption option);
    void ShowThumbnails();
    void ToggleSingleColumn();
    void PlaySound(std::string_view soundName);
    void PlayRolloverWhenTheFocusMoves();
    void PlayLoadFinishWhenThePageIsLoaded();
    void UpdateSettings();
    void UpdateDialog();
    void UpdateToolbarVisibility();
    void TakeWhatThePageAsks();
    bool ShowNetworkErrorWhenDue();

    void PressStartPagePlace(Place place);
    void PressToolbarButton(ToolbarButton button);
    void PressFavorite(std::size_t index);
    void PressSettingsItem(const SettingsScreen::Item& item);
    void Answer(DialogAnswer answer);
    void UseKeyboardAnswer(const SystemMenu::KeyboardAnswer& answer);

    void OpenAddress(const std::string& address);
    void Search(const std::u16string& words);
    void OpenOperationsGuide();
    void OpenFavorites();
    void OpenSettings();
    void ShowView(View newView);
    void CloseList();
    void AddThePageToFavorites();
    void OpenKeyboard(Typing purpose, const std::u16string& title, const std::u16string& text);
    void Ask(Question question, std::u16string text, bool hasCancel = false);

    PageView& EnsurePageView();
    void ShowToolbarButtons();
    ToolbarState CurrentToolbarState() const;
    bool IsToolbarShown() const;
    float PageAreaHeight() const;
    std::u16string LoadFailureText(Browser::LoadError error) const;
    std::u16string ControlsText() const;

    std::optional<Place> PointedPlace() const;
    std::optional<Formats::Vector2> PointerOnPage() const;
    std::optional<Formats::Vector2> PointerOnFlatPage() const;
    std::optional<ToolbarButton> PointedToolbarButton() const;
    std::optional<DialogAnswer> PointedDialogAnswer() const;
    bool IsPressed() const;
    PageRectangle DialogPlace() const;
    PageRectangle OkButton() const;
    PageRectangle CancelButton() const;
    Formats::Vector2 PageToPicture(Formats::Vector2 pagePoint, const PageFrame& pageFrame) const;

    void DrawStartPage();
    void DrawOrbs();
    void DrawButtons();
    void DrawDialog();

    std::unique_ptr<PagePicture> ReadPicture(std::string_view path) const;

    ChannelServices& services;
    Formats::U8Archive pages;
    OperaLanguageFile texts;
    PageFrame frame;
    PageFrame flatFrame;
    float containerLeft;
    std::unique_ptr<PagePainter> painter;
    std::unique_ptr<PagePainter> flatPainter;
    std::unique_ptr<SystemMenu::CursorManager> cursors;
    std::unique_ptr<SystemMenu::SoftKeyboard> keyboard;
    std::unique_ptr<OperaData> data;
    std::unique_ptr<PageToolbar> toolbar;
    std::unique_ptr<FavoritesScreen> favoritesScreen;
    std::unique_ptr<SettingsScreen> settingsScreen;
    std::unique_ptr<PageView> pageView;

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
    std::unique_ptr<PagePicture> googleButton;
    std::unique_ptr<PagePicture> yahooButton;

    View view = View::StartPage;
    View viewBeforeList = View::StartPage;
    std::optional<Dialog> dialog;
    Typing typing = Typing::WebAddress;
    SystemMenu::NetworkStart networkStart;
    bool hasShownNetworkError = false;
    std::optional<Place> focusedPlace;
    std::optional<ToolbarButton> focusedToolbarButton;
    std::optional<std::size_t> focusedFavorite;
    std::optional<SettingsScreen::Item> focusedSettingsItem;
    std::optional<FavoritesScreen::EditOption> focusedEditOption;
    std::optional<std::size_t> renamedFavorite;
    bool isSingleColumn = false;
    bool wasPageLoading = false;
    std::string focusBeforeTheStep;
    std::optional<std::size_t> favoriteToDelete;
    std::u16string wordsToSearch;
    SearchEngine engineToConfirm = SearchEngine::None;
    bool isPagePressed = false;
    bool isToolbarToggledOn = true;
    double secondsToolbarWasPointedAt = 0;
    std::optional<Formats::Vector2> dragPoint;
};

}
