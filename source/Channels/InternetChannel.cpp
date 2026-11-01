#include "Channels/InternetChannel.h"

#include "Channels/ChannelFiles.h"
#include "Channels/ChannelText.h"
#include "Channels/Internet/ThumbnailPicture.h"
#include "Channels/Internet/TypedAddresses.h"

#include "Browser/TextConversion.h"
#include "Formats/PngImage.h"

#include <format>
#include <map>
#include <string>

namespace Channels
{

namespace
{

// The contents of the Internet Channel (HADE v1024): the defaults (operarc.nand), the built-in pages with english.lng, the cursors, the
// soft keyboard and the HOME menu, and the pictures of the toolbar.
constexpr std::uint16_t DefaultsContentIndex = 3;
constexpr std::uint16_t PagesContentIndex = 2;
constexpr std::uint16_t InterfaceContentIndex = 5;
constexpr std::uint16_t ToolbarContentIndex = 11;
constexpr std::string_view CursorArchivePath = "cursor.arc";
constexpr std::string_view KeyboardArchivePath = "sofkeybd.arc";
constexpr std::string_view DefaultPreferencesPath = "operarc.nand";
constexpr std::string_view DefaultFavoritesPath = "./default_bookmarks/us_en/append1.adr";
constexpr std::string_view LanguageFilePath = "./english.lng";
constexpr std::string_view StartPageFolder = "./startpage/stylesWideScreen/";
constexpr std::string_view DialogPictureFolder = "./images/wideScreen/";
constexpr std::string_view ManualFolder = "./help_A/";

// The manual is served by the program at this address; the host can never be a real one (.invalid).
constexpr std::string_view ManualHost = "operations-guide.invalid";
constexpr std::string_view ManualStartPage = "start_ukv.html";

// The Opera logo of the start page opens opera:about (startpage.js): about.html of the pages with the style sheets of styles/, served
// the same way.
constexpr std::string_view ContentRoot = "./";
constexpr std::string_view AboutPagePath = "./about.html";
constexpr std::string_view StylesFolder = "./styles/";
constexpr std::string_view AboutHost = "opera-about.invalid";
constexpr std::string_view AboutPage = "about.html";

// about.html asks the channel's Opera for its version, its build and its build date (the host function 0x800361D4 gives "9.30",
// "3642" and "109-6-5", the year counted from 1900).
constexpr std::u16string_view OperaVersionScript =
    u"window.opera = window.opera || {};"
    u" window.opera.getoperaversion = function(part) { return ['9.30', '3642', '109-6-5'][part]; };";

// The widescreen styles lay the page out 450 pixels high (the background of startpage.css is 450 high), so one pixel of the page is
// 456 / 450 layout units and the page is as wide as the picture.
constexpr float WidePageHeight = 450;

// The places of the widescreen startpage.css, in pixels of the page, from the left edge of #frontcontainer, which is 547 pixels wide
// and stands in the middle. The tops follow from the heights and margins of the blocks above them.
constexpr float ContainerWidth = 547;
constexpr PageRectangle LogoPlace = {436, 13, 117, 18};
constexpr PageRectangle LogoPart = {0, 0, 117, 18};

// The logo's box of startpage.css (#opera-logo, 117 by 27 from 4 pixels down); with the focus its picture moves up to show the larger
// logo below (.focusLogo).
constexpr PageRectangle LogoBox = {436, 4, 117, 27};
constexpr PageRectangle FocusedLogoPlace = {436, 4, 117, 25};
constexpr PageRectangle FocusedLogoPart = {0, 20, 117, 25};
constexpr float HomeIconLeft = 4;
constexpr float HomeIconTop = 1;
constexpr PageRectangle TitleBox = {37, 4, 300, 26};
constexpr float OrbsTop = 41;
constexpr float OrbWidth = 142;
constexpr float OrbPictureLeft = 19;
constexpr float OrbPictureSize = 105;
constexpr float OrbPressableHeight = 160;
constexpr float OrbLabelTop = 167;
constexpr float OrbLabelHeight = 30;
constexpr float ButtonsTop = 210;
constexpr float ButtonHeight = 45;
constexpr float ButtonPictureWidth = 428;
constexpr float ButtonPressableWidth = 427;
constexpr PageRectangle ButtonIconPart = {0, 0, 60, 45};
constexpr float ButtonIconLeft = 11;
constexpr float ButtonTextLeft = 71;
constexpr float ButtonTextTop = 12;
constexpr PageRectangle ControllerPlace = {446, 97, 67, 223};
constexpr PageRectangle ControllerPart = {0, 0, 67, 223};
constexpr PageRectangle ControlsTextBox = {412, 320, 120, 25};

constexpr float BackgroundTop = 4;
constexpr Formats::Rgba8Color White = {255, 255, 255, 255};

constexpr PageTextStyle TitleStyle = {22, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle LabelStyle = {18, {0, 0, 0, 255}, Text::HorizontalAlignment::Center};
constexpr PageTextStyle ButtonStyle = {18, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};

// dialog16x9.css: the box is 532 by 315, 259 pixels left of the middle and 26 from the top; the sign, the message beside it and the buttons.
constexpr float DialogHalfWidth = 259;
constexpr float DialogTop = 26;
constexpr float DialogWidth = 532;
constexpr float DialogHeight = 315;
constexpr PageRectangle SignPlace = {45, 22, 64, 64};
constexpr PageRectangle MessageBox = {131, 36, 342, 170};
constexpr float DialogButtonsLeft = 52;
constexpr float DialogButtonsTop = 214;
constexpr float DialogButtonWidth = 150;
constexpr float DialogButtonHeight = 76;
constexpr float AlertOkMargin = 131;
constexpr float ButtonMargin = 37;
constexpr PageTextStyle MessageStyle = {22, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};

// The page under a dialog is darkened (CHN-31, Assumed: #dialogBackground is black, how much of the page shows through is not known).
constexpr Formats::Rgba8Color DialogShade = {0, 0, 0, 128};

// The pictures hold their states one under the other: the state of a place that the pointer is on is the second one.
constexpr int BlurredState = 0;
constexpr int FocusedState = 1;

constexpr std::string_view NetworkErrorCodeMark = "%06d";
constexpr std::string_view SearchEngineMark = "%s";

constexpr std::array<std::string_view, 3> OrbTitles = {"S_HOKKAIDO_LABEL_SEARCH", "S_FAV_PAGE_FAV", "S_ENTER_WEB_ADDRESS"};
constexpr std::array<std::string_view, 3> OrbSymbolFiles = {"symSearch.png", "symFavorites.png", "symURL.png"};
constexpr std::array<std::string_view, 3> ButtonTitles = {
    "S_HOKKAIDO_LABEL_OPERATIONS_GUIDE", "S_HOKKAIDO_LABEL_SETTINGS", "S_HOKKAIDO_LABEL_WII_MENU"};
constexpr std::array<std::string_view, 3> ButtonIconFiles = {"link_help.png", "link_settings.png", "link_wiimenu.png"};

// The texts that the program asks for by their ids (0x800338B0 for the network error; the others have no name in the pages).
constexpr std::int32_t NetworkErrorTextId = static_cast<std::int32_t>(0xF5525EA9);
constexpr std::int32_t EnterWebAddressTextId = 1284175149;
constexpr std::int32_t EnterSearchTextId = -1450916355;
constexpr std::int32_t AlreadyFavoriteTextId = -893767412;
constexpr std::int32_t FavoritesFullTextId = 2007929367;
constexpr std::int32_t DeleteCookiesQuestionTextId = 2018127623;
constexpr std::int32_t CookiesDeletedTextId = 499028666;
constexpr std::int32_t ChooseSearchEngineTextId = -310784171;
constexpr std::int32_t UseSearchEngineTextId = -460825821;
constexpr std::int32_t SearchEngineSetTextId = 671856908;

// The texts of a page that could not be loaded (Assumed: the Opera texts of english.lng that tell the same failure).
constexpr std::int32_t ServerNotFoundTextId = -303587263;
constexpr std::int32_t TimedOutTextId = -751126418;
constexpr std::int32_t ConnectionClosedTextId = 406813980;
constexpr std::int32_t SecureConnectionFailedTextId = -609046020;
constexpr std::int32_t LoadFailedTextId = -767243423;

constexpr std::string_view ReturnQuestionText = "S_HOKKAIDO_LABEL_RETURN_WII_MENU";
constexpr std::string_view EditTitleText = "S_EDIT_TITLE";
constexpr std::int32_t SingleColumnTextId = 2004134353;

// The sounds of remote/operaSound.brsar in content 2 that the pages ask for with "tv-remote-sound" (startpage.js, favorites.js,
// modalBox.js); the channel plays the ones of the television, not of the remote's speaker. The moments of the toolbar, the zoom and
// the load are the names' own (Assumed: the program's calls were not read).
namespace OperaSound
{

constexpr std::string_view Decide = "WOP_SE_DECIDE_TV";
constexpr std::string_view Rollover = "WOP_SE_ROLLOVER_TV";
constexpr std::string_view Invalid = "WOP_SE_INVALID";
constexpr std::string_view DialogAppears = "WOP_SE_DIALOG_APP";
constexpr std::string_view DialogOk = "WOP_SE_DIALOG_OK";
constexpr std::string_view DialogNo = "WOP_SE_DIALOG_NO";
constexpr std::string_view FavoriteAdded = "WOP_SE_FAVORITE_ADD";
constexpr std::string_view FavoriteDeleted = "WOP_SE_FAVORITE_DEL";
constexpr std::string_view ToolbarIn = "WOP_SE_TOOLBAR_IN";
constexpr std::string_view ToolbarOut = "WOP_SE_TOOLBAR_OUT";
constexpr std::string_view PageLoaded = "WOP_SE_LOAD_FINISH";
constexpr std::string_view ZoomIn = "WOP_SE_IZOOM_IN";
constexpr std::string_view ZoomOut = "WOP_SE_IZOOM_OUT";
constexpr std::string_view ZoomBackToNormal = "WOP_SE_ZOOM_100";

}

// The longest title of a favorite (the original's "Edit Title" prompt, favorites.js).
constexpr std::size_t LongestFavoriteTitle = 64;

// The help of the remote that "Controls" shows (helpRemoteDialogTemplate of startpage.js): the sign of the button in the console's font
// and its text. The line of Single Column Mode is left out, because WeeOS has no such mode (BRW-16).
struct ControlsLine
{
    std::u16string_view buttonSigns;
    std::string_view text;
};

constexpr std::array<ControlsLine, 3> ControlsLines = {{
    {u"\uE042", "S_HOKKAIDO_HELP_SELECT"},
    {u"\uE043", "S_HOKKAIDO_HELP_SCROLL"},
    {u"\uE045\uE046", "S_HOKKAIDO_HELP_ZOOM"},
}};
constexpr std::string_view DeleteFavoriteText = "D_DELETE_FAV";
constexpr std::string_view GoogleText = "S_HOKKAIDO_GOOGLE";
constexpr std::string_view YahooText = "S_HOKKAIDO_YAHOO";

// The longest address and search text of the keyboard (Assumed: the original's limits were not read).
constexpr std::size_t LongestTypedText = 1024;

// Scrolling with the +Control Pad, and with B held while the remote moves (the manual, page 3; Assumed speeds).
constexpr double ScrollPixelsPerStep = 12;
constexpr double DragScrollFactor = 1.5;

// A toolbar that hides itself goes away this long after the pointer left it (Assumed: "If you wait a few moments", the manual page 7).
constexpr double SecondsBeforeToolbarHides = 3;

std::u16string ReplaceMark(std::u16string text, std::string_view mark, std::u16string_view value)
{
    std::u16string markText = AsciiToText(mark);
    std::size_t place = text.find(markText);
    if (place != std::u16string::npos)
        text.replace(place, markText.size(), value);

    return text;
}

std::u16string ReplaceCodeMark(std::u16string text, std::uint32_t code)
{
    return ReplaceMark(std::move(text), NetworkErrorCodeMark, AsciiToText(std::format("{:06}", code)));
}

PageRectangle Shifted(const PageRectangle& place, float left, float top)
{
    return {place.left + left, place.top + top, place.width, place.height};
}

bool HasButton(Input::ButtonMask buttons, Input::ButtonMask button)
{
    return (buttons & button) != 0;
}

using ServedFiles = std::map<std::string, std::vector<std::uint8_t>>;

// The files of the pages whose path starts with the folder, by their path below the served root.
void AddFilesOfFolder(ServedFiles& files, const Formats::U8Archive& pages, std::string_view folder, std::string_view servedRoot)
{
    for (const Formats::U8Entry& entry : pages.entries)
    {
        if (entry.isDirectory || !entry.path.starts_with(folder))
            continue;

        Formats::ByteView bytes = pages.FileData(entry);
        files[entry.path.substr(servedRoot.size())] = std::vector<std::uint8_t>(bytes.begin(), bytes.end());
    }
}

// The files of the manual, help_A of the pages, by their path below it.
ServedFiles ManualFiles(const Formats::U8Archive& pages)
{
    ServedFiles files;
    AddFilesOfFolder(files, pages, ManualFolder, ManualFolder);
    return files;
}

ServedFiles AboutFiles(const Formats::U8Archive& pages)
{
    ServedFiles files;
    AddFilesOfFolder(files, pages, AboutPagePath, ContentRoot);
    AddFilesOfFolder(files, pages, StylesFolder, ContentRoot);
    return files;
}

}

InternetChannel::InternetChannel(ChannelServices& channelServices)
    : services(channelServices)
    , pages(ReadArchiveContent(channelServices.contents, PagesContentIndex))
    , texts(FindFileOrFail(pages, LanguageFilePath))
{
    Graphics::Canvas canvas = services.display.GetCanvas();
    frame.scale = canvas.height / WidePageHeight;
    frame.width = canvas.width / frame.scale;
    frame.height = WidePageHeight;
    flatFrame = {canvas.width, canvas.height, 1};
    containerLeft = (frame.width - ContainerWidth) / 2;

    Graphics::GraphicsDevice& device = services.display.Device();
    const Layout::DrawableFont& pageFont = services.systemFonts.Find("wbf1.brfna");
    painter = std::make_unique<PagePainter>(device, pageFont, frame);
    flatPainter = std::make_unique<PagePainter>(device, pageFont, flatFrame);
    services.display.SetTextFonts(services.systemFonts);

    Formats::U8Archive interfaceFiles = ReadArchiveContent(services.contents, InterfaceContentIndex);
    Formats::U8Archive handFiles = Formats::ReadU8Archive(FindFileOrFail(interfaceFiles, CursorArchivePath));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, std::make_unique<Layout::LayoutArchive>(std::move(handFiles)));

    Formats::U8Archive keyboardFiles = Formats::ReadU8Archive(FindFileOrFail(interfaceFiles, KeyboardArchivePath));
    keyboard = std::make_unique<SystemMenu::SoftKeyboard>(services.display, std::make_unique<Layout::LayoutArchive>(std::move(keyboardFiles)),
        SystemMenu::KeyboardButtonTexts{}, services.clock, services.input, services.sounds);
    if (services.pc.keyboard != nullptr)
        keyboard->UsePhysicalKeyboard(*services.pc.keyboard);

    Formats::U8Archive defaults = ReadArchiveContent(services.contents, DefaultsContentIndex);
    data = std::make_unique<OperaData>(services.storage.fileSystem, services.storage.fileSystemCaller, FindFileOrFail(pages, DefaultFavoritesPath),
        FindFileOrFail(defaults, DefaultPreferencesPath), services.consoleTime);

    toolbar = std::make_unique<PageToolbar>(device, ReadArchiveContent(services.contents, ToolbarContentIndex));
    favoritesScreen = std::make_unique<FavoritesScreen>(device, pages, texts);
    settingsScreen = std::make_unique<SettingsScreen>(device, pages, texts);

    std::string startPage(StartPageFolder);
    background = ReadPicture(startPage + "startBackground.png");
    logo = ReadPicture(startPage + "logoOpera.png");
    homeIcon = ReadPicture(startPage + "operaHome.png");
    orb = ReadPicture(startPage + "orbBtn.png");
    gloss = ReadPicture(startPage + "gloss.png");
    button = ReadPicture(startPage + "button.png");
    controller = ReadPicture(startPage + "controller.png");
    for (std::size_t index = 0; index < orbSymbols.size(); ++index)
    {
        orbSymbols[index] = ReadPicture(startPage + std::string(OrbSymbolFiles[index]));
        buttonIcons[index] = ReadPicture(startPage + std::string(ButtonIconFiles[index]));
    }

    std::string dialogPictures(DialogPictureFolder);
    dialogBack = ReadPicture(dialogPictures + "backDialog.png");
    infoSign = ReadPicture(dialogPictures + "symInfoSign.png");
    questionSign = ReadPicture(dialogPictures + "symHelpInverted.png");
    okButton = ReadPicture(dialogPictures + "btnCheck.png");
    cancelButton = ReadPicture(dialogPictures + "btnCancel.png");
    googleButton = ReadPicture(dialogPictures + "google.png");
    yahooButton = ReadPicture(dialogPictures + "yahoo.png");

    ShowToolbarButtons();
    networkStart = SystemMenu::StartNetwork(services.network.configuration, services.network.sockets);
}

InternetChannel::~InternetChannel() = default;

void InternetChannel::Update()
{
    if (services.wiiMenu.IsRequested())
        return;

    if (services.fader.GetStatus() == SystemMenu::FaderStatus::Covered)
        services.fader.StartFadeIn();

    if (pageView)
        pageView->Update();
    PlayLoadFinishWhenThePageIsLoaded();

    if (keyboard->IsOpen())
    {
        keyboard->Update();
        return;
    }

    std::optional<SystemMenu::KeyboardAnswer> keyboardAnswer = keyboard->TakeAnswer();
    if (keyboardAnswer)
    {
        UseKeyboardAnswer(*keyboardAnswer);
        return;
    }

    if (dialog)
    {
        UpdateDialog();
        return;
    }

    TakeWhatThePageAsks();
    if (dialog || keyboard->IsOpen() || ShowNetworkErrorWhenDue())
        return;

    UpdateToolbarVisibility();
    focusedToolbarButton = PointedToolbarButton();
    switch (view)
    {
    case View::StartPage:
        UpdateStartPage();
        break;
    case View::Page:
        UpdatePage();
        break;
    case View::Favorites:
        UpdateFavorites();
        break;
    case View::Settings:
        UpdateSettings();
        break;
    }
    PlayRolloverWhenTheFocusMoves();
}

void InternetChannel::Draw()
{
    switch (view)
    {
    case View::StartPage:
        DrawStartPage();
        break;
    case View::Page:
        pageView->Draw(*flatPainter);
        break;
    case View::Favorites:
        favoritesScreen->Draw(*flatPainter, flatFrame, data->Favorites(), focusedFavorite, focusedEditOption);
        break;
    case View::Settings:
        settingsScreen->Draw(*flatPainter, flatFrame, *data, focusedSettingsItem);
        break;
    }

    if (IsToolbarShown())
        toolbar->Draw(*flatPainter, flatFrame, CurrentToolbarState());

    if (dialog)
        DrawDialog();

    if (keyboard->IsOpen())
        keyboard->Draw();
}

SystemMenu::CursorManager& InternetChannel::Hands()
{
    return *cursors;
}

InternetChannel::Screen InternetChannel::CurrentScreen() const
{
    if (keyboard->IsOpen())
        return Screen::Keyboard;

    if (dialog)
    {
        if (dialog->question == Question::NetworkError)
            return Screen::NetworkError;
        if (dialog->question == Question::ReturnToWiiMenu)
            return Screen::ReturnQuestion;
        return Screen::Message;
    }

    switch (view)
    {
    case View::Page:
        return Screen::Page;
    case View::Favorites:
        return Screen::Favorites;
    case View::Settings:
        return Screen::Settings;
    default:
        return Screen::StartPage;
    }
}

std::u16string InternetChannel::DialogText() const
{
    if (!dialog)
        return {};

    return dialog->text;
}

const std::u16string& InternetChannel::Text(std::string_view name) const
{
    return texts.Find(name);
}

const OperaData& InternetChannel::Data() const
{
    return *data;
}

Browser::WebPage* InternetChannel::Page()
{
    if (!pageView)
        return nullptr;

    return &pageView->Page();
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfStartPagePlace(Place place) const
{
    if (view != View::StartPage)
        return std::nullopt;

    if (place == Place::OperaLogo)
        return PageToPicture({containerLeft + LogoBox.left + LogoBox.width / 2, LogoBox.top + LogoBox.height / 2}, frame);

    if (place == Place::Controls)
        return PageToPicture({containerLeft + ControllerPlace.left + ControllerPlace.width / 2, ControllerPlace.top + ControllerPlace.height / 2}, frame);

    std::size_t index = static_cast<std::size_t>(place);
    if (index < OrbTitles.size())
        return PageToPicture({containerLeft + OrbWidth * index + OrbWidth / 2, OrbsTop + OrbPictureSize / 2}, frame);

    std::size_t buttonIndex = index - OrbTitles.size();
    return PageToPicture({containerLeft + ButtonPressableWidth / 2, ButtonsTop + ButtonHeight * buttonIndex + ButtonHeight / 2}, frame);
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfToolbarButton(ToolbarButton wantedButton) const
{
    if (!IsToolbarShown())
        return std::nullopt;

    for (float x = 0; x < flatFrame.width; x += 1)
    {
        Formats::Vector2 point = {x, flatFrame.height - PageToolbar::Height / 2};
        if (toolbar->ButtonAt(point, flatFrame) == wantedButton)
            return PageToPicture({x + PageToolbar::Height / 2, point.y}, flatFrame);
    }
    return std::nullopt;
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfFavorite(std::size_t index) const
{
    if (view != View::Favorites)
        return std::nullopt;

    for (float y = 0; y < flatFrame.height; y += 4)
    {
        for (float x = 0; x < flatFrame.width; x += 4)
        {
            if (favoritesScreen->FavoriteAt({x, y}, flatFrame, data->Favorites().size()) == index)
                return PageToPicture({x + 40, y + 40}, flatFrame);
        }
    }
    return std::nullopt;
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfSettingsItem(const SettingsScreen::Item& item) const
{
    if (view != View::Settings)
        return std::nullopt;

    for (float y = 0; y < flatFrame.height; y += 4)
    {
        Formats::Vector2 point = {flatFrame.width / 2, y};
        if (settingsScreen->ItemAt(point, flatFrame) == item)
            return PageToPicture({point.x, y + 10}, flatFrame);
    }
    return std::nullopt;
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfEditOption(FavoritesScreen::EditOption option) const
{
    if (view != View::Favorites)
        return std::nullopt;

    for (float y = 0; y < flatFrame.height; y += 2)
    {
        for (float x = 0; x < flatFrame.width; x += 8)
        {
            if (favoritesScreen->EditOptionAt({x, y}, flatFrame, data->Favorites().size()) == option)
                return PageToPicture({x + 30, y + 20}, flatFrame);
        }
    }
    return std::nullopt;
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfDialogOk() const
{
    if (!dialog)
        return std::nullopt;

    PageRectangle place = OkButton();
    return PageToPicture({place.left + place.width / 2, place.top + place.height / 2}, frame);
}

std::optional<Formats::Vector2> InternetChannel::PlaceOfDialogCancel() const
{
    if (!dialog || !dialog->hasCancel)
        return std::nullopt;

    PageRectangle place = CancelButton();
    return PageToPicture({place.left + place.width / 2, place.top + place.height / 2}, frame);
}

// The join of the network ends while the start page shows, and the channel tells it once (0x800320CC, 0x800338B0).
bool InternetChannel::ShowNetworkErrorWhenDue()
{
    bool hasJoinFailed = !networkStart.isOnline && services.clock.Seconds() >= networkStart.secondsUntilFailure;
    if (!hasJoinFailed || hasShownNetworkError || view != View::StartPage)
        return false;

    hasShownNetworkError = true;
    focusedPlace.reset();
    Ask(Question::NetworkError, ReplaceCodeMark(texts.FindById(NetworkErrorTextId), networkStart.supportCode));
    return true;
}

void InternetChannel::UpdateStartPage()
{
    focusedPlace = PointedPlace();
    if (!IsPressed())
        return;

    if (focusedToolbarButton)
    {
        PressToolbarButton(*focusedToolbarButton);
        return;
    }

    if (focusedPlace)
        PressStartPagePlace(*focusedPlace);
}

void InternetChannel::UpdatePage()
{
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(0);
    if (!player)
        return;

    const Input::RemoteState& state = player->state;
    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    bool isOverPage = pointer && pageView->Contains(*pointer) && !focusedToolbarButton;
    if (isOverPage)
        pageView->MovePointer(*pointer);

    UpdatePageScrolling(state);
    UpdatePageButtons(state);

    if (HasButton(state.buttonsPressed, Input::RemoteButtons::A))
    {
        if (focusedToolbarButton)
        {
            PressToolbarButton(*focusedToolbarButton);
            return;
        }
        if (isOverPage)
        {
            pageView->Page().PressPointer();
            isPagePressed = true;
        }
    }

    if (HasButton(state.buttonsReleased, Input::RemoteButtons::A) && isPagePressed)
    {
        pageView->Page().ReleasePointer();
        isPagePressed = false;
    }
}

// + and - zoom; with B held they go forward and back, and the +Control Pad chooses Reload, Favorites, Search and Web Address
// (the manual, page 3). 1 shows and hides a toolbar that is toggled.
void InternetChannel::UpdatePageButtons(const Input::RemoteState& state)
{
    Input::ButtonMask pressed = state.buttonsPressed;
    bool isBHeld = HasButton(state.buttonsHeld, Input::RemoteButtons::B);
    if (isBHeld)
    {
        if (HasButton(pressed, Input::RemoteButtons::Plus))
            PressToolbarButton(ToolbarButton::Forward);
        else if (HasButton(pressed, Input::RemoteButtons::Minus))
            PressToolbarButton(ToolbarButton::Back);
        else if (HasButton(pressed, Input::RemoteButtons::Up))
            PressToolbarButton(ToolbarButton::Reload);
        else if (HasButton(pressed, Input::RemoteButtons::Down))
            PressToolbarButton(ToolbarButton::Favorites);
        else if (HasButton(pressed, Input::RemoteButtons::Left))
            PressToolbarButton(ToolbarButton::Search);
        else if (HasButton(pressed, Input::RemoteButtons::Right))
            PressToolbarButton(ToolbarButton::WebAddress);
        return;
    }

    bool wantsZoom = HasButton(pressed, Input::RemoteButtons::Plus) || HasButton(pressed, Input::RemoteButtons::Minus);
    if (wantsZoom && isSingleColumn)
        PlaySound(OperaSound::Invalid);
    else if (HasButton(pressed, Input::RemoteButtons::Plus))
    {
        pageView->ZoomIn();
        PlaySound(OperaSound::ZoomIn);
    }
    else if (HasButton(pressed, Input::RemoteButtons::Minus))
    {
        pageView->ZoomOut();
        PlaySound(pageView->IsAtNormalZoom() ? OperaSound::ZoomBackToNormal : OperaSound::ZoomOut);
    }

    if (HasButton(pressed, Input::RemoteButtons::Two))
        ToggleSingleColumn();

    if (HasButton(pressed, Input::RemoteButtons::One) && data->GetToolbarMode() == ToolbarMode::ToggledWithOne)
    {
        isToolbarToggledOn = !isToolbarToggledOn;
        PlaySound(isToolbarToggledOn ? OperaSound::ToolbarIn : OperaSound::ToolbarOut);
        pageView->SetArea(flatFrame.width, PageAreaHeight());
    }
}

// Button 2 switches Single Column Mode; switched on, the channel tells what it does, and the zoom goes back to the page's size.
void InternetChannel::ToggleSingleColumn()
{
    isSingleColumn = !isSingleColumn;
    pageView->Page().SetSingleColumn(isSingleColumn);
    if (!isSingleColumn)
        return;

    pageView->ResetZoom();
    Ask(Question::SingleColumn, texts.FindById(SingleColumnTextId));
}

// The +Control Pad scrolls the way it points; with B held, the page follows the hand.
void InternetChannel::UpdatePageScrolling(const Input::RemoteState& state)
{
    Input::ButtonMask held = state.buttonsHeld;
    bool isBHeld = HasButton(held, Input::RemoteButtons::B);
    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    if (isBHeld && pointer && dragPoint)
    {
        double right = (pointer->x - dragPoint->x) * DragScrollFactor;
        double down = (pointer->y - dragPoint->y) * DragScrollFactor;
        if (right != 0 || down != 0)
            pageView->Page().Scroll(right, down);
    }
    dragPoint = isBHeld ? pointer : std::nullopt;
    if (isBHeld)
        return;

    double right = 0;
    double down = 0;
    if (HasButton(held, Input::RemoteButtons::Left))
        right -= ScrollPixelsPerStep;
    if (HasButton(held, Input::RemoteButtons::Right))
        right += ScrollPixelsPerStep;
    if (HasButton(held, Input::RemoteButtons::Up))
        down -= ScrollPixelsPerStep;
    if (HasButton(held, Input::RemoteButtons::Down))
        down += ScrollPixelsPerStep;
    if (right != 0 || down != 0)
        pageView->Page().Scroll(right, down);
}

void InternetChannel::UpdateFavorites()
{
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(0);
    if (player && favoritesScreen->MovingFavorite())
    {
        UpdateMovingFavorite(player->state);
        return;
    }

    if (favoritesScreen->EditedFavorite())
    {
        UpdateEditOptions();
        return;
    }

    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    std::size_t favoriteCount = data->Favorites().size();
    focusedFavorite = pointer ? favoritesScreen->FavoriteAt(*pointer, flatFrame, favoriteCount) : std::nullopt;

    if (player && HasButton(player->state.buttonsPressed, Input::RemoteButtons::Up))
        favoritesScreen->ScrollRows(-1, favoriteCount);
    if (player && HasButton(player->state.buttonsPressed, Input::RemoteButtons::Down))
        favoritesScreen->ScrollRows(1, favoriteCount);

    if (!IsPressed())
        return;

    if (focusedToolbarButton)
        PressToolbarButton(*focusedToolbarButton);
    else if (focusedFavorite)
        PressFavorite(*focusedFavorite);
}

// The menu of edit options of a favorite: a press on a row does what it says, a press beside the menu or B closes it.
void InternetChannel::UpdateEditOptions()
{
    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    std::size_t favoriteCount = data->Favorites().size();
    focusedEditOption = pointer ? favoritesScreen->EditOptionAt(*pointer, flatFrame, favoriteCount) : std::nullopt;

    bool isBPressed = services.input.HasPlayerPressed(0, {Input::RemoteButtons::B, Input::ClassicButtons::B});
    if (isBPressed)
    {
        favoritesScreen->CloseEditOptions();
        focusedEditOption.reset();
        return;
    }

    if (!IsPressed())
        return;

    if (focusedEditOption)
        PressEditOption(*focusedEditOption);
    else
        favoritesScreen->CloseEditOptions();
    focusedEditOption.reset();
}

// The +Control Pad moves the favorite one place to the side or one row up or down; A or B puts it down where it is.
void InternetChannel::UpdateMovingFavorite(const Input::RemoteState& state)
{
    std::size_t index = *favoritesScreen->MovingFavorite();
    std::size_t lastIndex = data->Favorites().size() - 1;
    std::size_t newIndex = index;
    Input::ButtonMask pressed = state.buttonsPressed;
    if (HasButton(pressed, Input::RemoteButtons::Left) && index > 0)
        newIndex = index - 1;
    if (HasButton(pressed, Input::RemoteButtons::Right) && index < lastIndex)
        newIndex = index + 1;
    if (HasButton(pressed, Input::RemoteButtons::Up) && index >= FavoritesScreen::TilesPerRow)
        newIndex = index - FavoritesScreen::TilesPerRow;
    if (HasButton(pressed, Input::RemoteButtons::Down) && index + FavoritesScreen::TilesPerRow <= lastIndex)
        newIndex = index + FavoritesScreen::TilesPerRow;

    if (newIndex != index)
    {
        data->MoveFavorite(data->Favorites()[index].number, newIndex);
        favoritesScreen->SetMovingFavorite(newIndex, flatFrame);
        PlaySound(OperaSound::Rollover);
    }

    bool isPutDown = HasButton(pressed, Input::RemoteButtons::A) || HasButton(pressed, Input::RemoteButtons::B);
    if (isPutDown)
    {
        favoritesScreen->StopMoving();
        PlaySound(OperaSound::Decide);
    }
}

void InternetChannel::PressEditOption(FavoritesScreen::EditOption option)
{
    std::size_t index = *favoritesScreen->EditedFavorite();
    PlaySound(OperaSound::Decide);
    switch (option)
    {
    case FavoritesScreen::EditOption::Delete:
        favoritesScreen->CloseEditOptions();
        favoriteToDelete = index;
        Ask(Question::DeleteFavorite, Text(DeleteFavoriteText), true);
        return;
    case FavoritesScreen::EditOption::Rename:
        favoritesScreen->CloseEditOptions();
        renamedFavorite = index;
        OpenKeyboard(Typing::FavoriteTitle, Text(EditTitleText), data->Favorites()[index].title);
        return;
    case FavoritesScreen::EditOption::Move:
        favoritesScreen->StartMoving(index);
        return;
    case FavoritesScreen::EditOption::Close:
        favoritesScreen->CloseEditOptions();
        return;
    }
}

void InternetChannel::UpdateSettings()
{
    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    focusedSettingsItem = pointer ? settingsScreen->ItemAt(*pointer, flatFrame) : std::nullopt;
    if (!IsPressed())
        return;

    if (focusedToolbarButton)
        PressToolbarButton(*focusedToolbarButton);
    else if (focusedSettingsItem)
        PressSettingsItem(*focusedSettingsItem);
}

void InternetChannel::UpdateDialog()
{
    if (!IsPressed())
        return;

    std::optional<DialogAnswer> answer = PointedDialogAnswer();
    if (answer)
        Answer(*answer);
}

// A toolbar that hides itself shows while the hand is below the picture or on the toolbar.
void InternetChannel::UpdateToolbarVisibility()
{
    if (data->GetToolbarMode() != ToolbarMode::HiddenUntilPointedAt)
        return;

    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    bool isPointedAt = pointer && pointer->y >= flatFrame.height - PageToolbar::Height;
    if (isPointedAt)
        secondsToolbarWasPointedAt = services.clock.Seconds();
}

// What a page asks while nothing else is open: its script dialogs, a text field that was clicked, a load that failed.
void InternetChannel::TakeWhatThePageAsks()
{
    if (!pageView || view != View::Page)
        return;

    Browser::WebPage& page = pageView->Page();
    std::optional<Browser::ScriptDialog> scriptDialog = page.TakeScriptDialog();
    if (scriptDialog)
    {
        if (scriptDialog->kind == Browser::ScriptDialog::Kind::Prompt)
            OpenKeyboard(Typing::PagePrompt, scriptDialog->message, scriptDialog->defaultText);
        else if (scriptDialog->kind == Browser::ScriptDialog::Kind::Alert)
            Ask(Question::PageMessage, scriptDialog->message);
        else
            Ask(Question::PageQuestion, scriptDialog->message, true);
        return;
    }

    std::optional<Browser::TextFieldRequest> textField = page.TakeTextFieldRequest();
    if (textField)
    {
        OpenKeyboard(Typing::PageField, {}, textField->text);
        return;
    }

    std::optional<Browser::LoadFailure> loadFailure = page.TakeLoadFailure();
    if (loadFailure)
        Ask(Question::LoadFailure, LoadFailureText(loadFailure->error));
}

void InternetChannel::PressStartPagePlace(Place place)
{
    PlaySound(OperaSound::Decide);
    switch (place)
    {
    case Place::Search:
        OpenKeyboard(Typing::Search, texts.FindById(EnterSearchTextId), {});
        return;
    case Place::Favorites:
        OpenFavorites();
        return;
    case Place::WebAddress:
        OpenKeyboard(Typing::WebAddress, texts.FindById(EnterWebAddressTextId), {});
        return;
    case Place::OperationsGuide:
        OpenOperationsGuide();
        return;
    case Place::Settings:
        OpenSettings();
        return;
    case Place::WiiMenu:
        Ask(Question::ReturnToWiiMenu, Text(ReturnQuestionText), true);
        return;
    case Place::Controls:
        Ask(Question::Controls, ControlsText());
        return;
    case Place::OperaLogo:
        OpenOperaAbout();
        return;
    }
}

void InternetChannel::PressToolbarButton(ToolbarButton pressedButton)
{
    if (CurrentToolbarState().disabledButtons.contains(pressedButton))
    {
        PlaySound(OperaSound::Invalid);
        return;
    }
    PlaySound(OperaSound::Decide);

    Browser::PageState pageState = pageView ? pageView->Page().State() : Browser::PageState{};
    switch (pressedButton)
    {
    case ToolbarButton::Back:
        if (view == View::StartPage)
            ShowView(View::Page);
        else if (pageState.canGoBack)
            pageView->Page().GoBack();
        else
            ShowView(View::StartPage);
        return;
    case ToolbarButton::Forward:
        pageView->Page().GoForward();
        return;
    case ToolbarButton::Reload:
        if (pageState.isLoading)
            pageView->Page().Stop();
        else
            pageView->Page().Reload();
        return;
    case ToolbarButton::Search:
        OpenKeyboard(Typing::Search, texts.FindById(EnterSearchTextId), {});
        return;
    case ToolbarButton::Favorites:
        OpenFavorites();
        return;
    case ToolbarButton::WebAddress:
        OpenKeyboard(Typing::WebAddress, texts.FindById(EnterWebAddressTextId), view == View::Page ? Browser::TextOfUtf8(pageState.address) : u"");
        return;
    case ToolbarButton::StartPage:
        ShowView(View::StartPage);
        return;
    case ToolbarButton::PageInfo:
        Ask(Question::PageInformation, pageState.title + u"\n" + Browser::TextOfUtf8(pageState.address));
        return;
    case ToolbarButton::AddFavorite:
        AddThePageToFavorites();
        return;
    case ToolbarButton::EditFavorites:
        favoritesScreen->SetEditing(!favoritesScreen->IsEditing());
        return;
    case ToolbarButton::Close:
        CloseList();
        return;
    }
}

void InternetChannel::PressFavorite(std::size_t index)
{
    PlaySound(OperaSound::Decide);
    if (favoritesScreen->IsEditing())
    {
        favoritesScreen->OpenEditOptions(index);
        return;
    }

    OpenAddress(data->Favorites()[index].address);
}

void InternetChannel::PressSettingsItem(const SettingsScreen::Item& item)
{
    PlaySound(OperaSound::Decide);
    if (item.entry == SettingsScreen::Entry::DeleteCookies)
    {
        Ask(Question::DeleteCookies, texts.FindById(DeleteCookiesQuestionTextId), true);
        return;
    }

    if (!item.choice)
    {
        settingsScreen->ToggleEntry(item.entry);
        return;
    }

    if (item.entry == SettingsScreen::Entry::SearchEngine)
        data->SetSearchEngine(static_cast<SearchEngine>(*item.choice));
    else
        data->SetToolbarMode(static_cast<ToolbarMode>(*item.choice));
}

void InternetChannel::Answer(DialogAnswer answer)
{
    Dialog answered = *dialog;
    dialog.reset();
    bool isOk = answer == DialogAnswer::Ok;
    PlaySound(answer == DialogAnswer::Cancel ? OperaSound::DialogNo : OperaSound::DialogOk);

    switch (answered.question)
    {
    case Question::ReturnToWiiMenu:
        if (isOk)
            services.wiiMenu.Request();
        return;
    case Question::PageMessage:
    case Question::PageQuestion:
        pageView->Page().AnswerScriptDialog(isOk);
        return;
    case Question::DeleteFavorite:
        if (isOk && favoriteToDelete && *favoriteToDelete < data->Favorites().size())
        {
            data->RemoveFavorite(data->Favorites()[*favoriteToDelete].number);
            PlaySound(OperaSound::FavoriteDeleted);
        }
        favoriteToDelete.reset();
        return;
    case Question::ChooseSearchEngine:
    {
        if (answer == DialogAnswer::Cancel)
            return;
        engineToConfirm = answer == DialogAnswer::Yahoo ? SearchEngine::Yahoo : SearchEngine::Google;
        std::u16string engineName = Text(engineToConfirm == SearchEngine::Yahoo ? YahooText : GoogleText);
        Ask(Question::ConfirmSearchEngine, ReplaceMark(texts.FindById(UseSearchEngineTextId), SearchEngineMark, engineName), true);
        return;
    }
    case Question::ConfirmSearchEngine:
    {
        if (!isOk)
            return;
        data->SetSearchEngine(engineToConfirm);
        std::u16string engineName = Text(engineToConfirm == SearchEngine::Yahoo ? YahooText : GoogleText);
        Ask(Question::SearchEngineSet, ReplaceMark(texts.FindById(SearchEngineSetTextId), SearchEngineMark, engineName));
        return;
    }
    case Question::SearchEngineSet:
        Search(wordsToSearch);
        return;
    case Question::DeleteCookies:
        if (!isOk)
            return;
        EnsurePageView().Page().DeleteCookies();
        Ask(Question::CookiesDeleted, texts.FindById(CookiesDeletedTextId));
        return;
    default:
        return;
    }
}

void InternetChannel::UseKeyboardAnswer(const SystemMenu::KeyboardAnswer& answer)
{
    if (typing == Typing::PagePrompt)
    {
        pageView->Page().AnswerScriptDialog(answer.wasConfirmed, answer.text);
        return;
    }

    if (!answer.wasConfirmed)
        return;

    switch (typing)
    {
    case Typing::WebAddress:
    {
        std::string address = AddressOfTypedText(answer.text);
        if (!address.empty())
            OpenAddress(address);
        return;
    }
    case Typing::Search:
        if (!answer.text.empty())
            Search(answer.text);
        return;
    case Typing::PageField:
        pageView->Page().FillTextField(answer.text);
        return;
    case Typing::FavoriteTitle:
        if (renamedFavorite && *renamedFavorite < data->Favorites().size() && !answer.text.empty())
            data->RenameFavorite(data->Favorites()[*renamedFavorite].number, answer.text);
        renamedFavorite.reset();
        return;
    default:
        return;
    }
}

void InternetChannel::OpenAddress(const std::string& address)
{
    EnsurePageView().Page().Open(address);
    ShowView(View::Page);
}

// The first search asks for the search engine (searchEngineSettingFlow.js): the choice, the question and the message, then the search.
void InternetChannel::Search(const std::u16string& words)
{
    SearchEngine engine = data->GetSearchEngine();
    if (engine == SearchEngine::None)
    {
        wordsToSearch = words;
        Ask(Question::ChooseSearchEngine, texts.FindById(ChooseSearchEngineTextId), true);
        return;
    }

    OpenAddress(SearchAddressOf(engine, words));
}

void InternetChannel::OpenOperationsGuide()
{
    PageView& manual = EnsurePageView();
    manual.Page().ServeFiles(std::string(ManualHost), ManualFiles(pages));
    OpenAddress(std::format("https://{}/{}", ManualHost, ManualStartPage));
}

void InternetChannel::OpenOperaAbout()
{
    PageView& aboutView = EnsurePageView();
    if (!isAboutServed)
    {
        aboutView.Page().ServeFiles(std::string(AboutHost), AboutFiles(pages));
        aboutView.Page().AddPageScript(std::u16string(OperaVersionScript));
        isAboutServed = true;
    }
    OpenAddress(std::format("https://{}/{}", AboutHost, AboutPage));
}

void InternetChannel::OpenFavorites()
{
    if (view != View::Favorites && view != View::Settings)
        viewBeforeList = view;
    favoritesScreen->Open();
    ShowThumbnails();
    ShowView(View::Favorites);
}

void InternetChannel::ShowThumbnails()
{
    std::map<int, std::unique_ptr<PagePicture>> thumbnails;
    for (const Favorite& favorite : data->Favorites())
    {
        std::optional<Formats::RgbaImage> thumbnail = data->Thumbnail(favorite.number);
        if (thumbnail)
            thumbnails[favorite.number] = std::make_unique<PagePicture>(services.display.Device(), *thumbnail);
    }
    favoritesScreen->SetThumbnails(std::move(thumbnails));
}

void InternetChannel::OpenSettings()
{
    if (view != View::Favorites && view != View::Settings)
        viewBeforeList = view;
    settingsScreen->Open();
    ShowView(View::Settings);
}

void InternetChannel::ShowView(View newView)
{
    view = newView;
    focusedPlace.reset();
    focusedFavorite.reset();
    focusedSettingsItem.reset();
    focusedEditOption.reset();
    ShowToolbarButtons();
    if (pageView)
        pageView->SetArea(flatFrame.width, PageAreaHeight());
}

void InternetChannel::CloseList()
{
    ShowView(viewBeforeList);
}

// The page that the favorites were opened from becomes the last favorite, unless it is one already or the list is full.
void InternetChannel::AddThePageToFavorites()
{
    Browser::PageState pageState = pageView->Page().State();
    if (data->FindFavorite(pageState.address))
    {
        Ask(Question::AlreadyFavorite, texts.FindById(AlreadyFavoriteTextId));
        return;
    }

    if (!data->AddFavorite(pageState.title, pageState.address))
    {
        Ask(Question::FavoritesFull, texts.FindById(FavoritesFullTextId));
        return;
    }

    const std::optional<Formats::RgbaImage>& pagePicture = pageView->LatestPicture();
    if (pagePicture)
        data->SetThumbnail(data->Favorites().back().number, MakeThumbnail(*pagePicture));
    ShowThumbnails();
    PlaySound(OperaSound::FavoriteAdded);
}

void InternetChannel::OpenKeyboard(Typing purpose, const std::u16string& title, const std::u16string& text)
{
    typing = purpose;
    SystemMenu::KeyboardRequest request;
    request.title = title;
    request.initialText = text;
    request.rules.maximumLength = purpose == Typing::FavoriteTitle ? LongestFavoriteTitle : LongestTypedText;
    request.rules.allowsLineFeeds = false;
    keyboard->Open(request);
}

void InternetChannel::Ask(Question question, std::u16string text, bool hasCancel)
{
    dialog = Dialog{question, std::move(text), hasCancel};
    focusedToolbarButton.reset();
    PlaySound(OperaSound::DialogAppears);
}

void InternetChannel::PlaySound(std::string_view soundName)
{
    services.sounds.Request(soundName);
}

// Every place that the hand comes onto rolls over: the places of the start page, the toolbar, the favorites, the edit options and the settings.
void InternetChannel::PlayRolloverWhenTheFocusMoves()
{
    std::string focus;
    if (focusedPlace)
        focus = std::format("place {}", static_cast<int>(*focusedPlace));
    else if (focusedToolbarButton)
        focus = std::format("toolbar {}", static_cast<int>(*focusedToolbarButton));
    else if (focusedEditOption)
        focus = std::format("edit option {}", static_cast<int>(*focusedEditOption));
    else if (focusedFavorite)
        focus = std::format("favorite {}", *focusedFavorite);
    else if (focusedSettingsItem)
        focus = std::format("setting {} {}", static_cast<int>(focusedSettingsItem->entry), focusedSettingsItem->choice.value_or(-1));

    bool hasMoved = focus != focusBeforeTheStep;
    focusBeforeTheStep = focus;
    if (hasMoved && !focus.empty())
        PlaySound(OperaSound::Rollover);
}

void InternetChannel::PlayLoadFinishWhenThePageIsLoaded()
{
    bool isLoading = pageView && pageView->Page().State().isLoading;
    if (wasPageLoading && !isLoading)
        PlaySound(OperaSound::PageLoaded);
    wasPageLoading = isLoading;
}

PageView& InternetChannel::EnsurePageView()
{
    if (!pageView)
    {
        Browser::AddressRedirect redirect{WiiServiceHosts(), services.network.serverAddress};
        pageView = std::make_unique<PageView>(services.display.Device(), services.pc.browser, redirect);
        pageView->SetArea(flatFrame.width, PageAreaHeight());
    }
    return *pageView;
}

void InternetChannel::ShowToolbarButtons()
{
    switch (view)
    {
    case View::Favorites:
        toolbar->ShowButtons({ToolbarButton::AddFavorite, ToolbarButton::EditFavorites, ToolbarButton::Close});
        return;
    case View::Settings:
        toolbar->ShowButtons({ToolbarButton::Close});
        return;
    default:
        toolbar->ShowButtons({ToolbarButton::Back, ToolbarButton::Forward, ToolbarButton::Reload, ToolbarButton::Search, ToolbarButton::Favorites,
            ToolbarButton::WebAddress, ToolbarButton::StartPage, ToolbarButton::PageInfo});
        return;
    }
}

// On the start page only the buttons that lead somewhere work: Back to the page that was open, Search, Favorites and Web Address
// (the picture of the start page in the manual, page 2).
ToolbarState InternetChannel::CurrentToolbarState() const
{
    ToolbarState state;
    state.focusedButton = focusedToolbarButton;
    if (view == View::Favorites)
    {
        bool hasPage = viewBeforeList == View::Page && pageView && !pageView->Page().State().address.empty();
        if (!hasPage)
            state.disabledButtons.insert(ToolbarButton::AddFavorite);
        return state;
    }

    if (view != View::StartPage && view != View::Page)
        return state;

    Browser::PageState pageState = pageView ? pageView->Page().State() : Browser::PageState{};
    state.isLoading = pageState.isLoading;
    state.loadingProgress = pageState.loadingProgress;
    state.isSecure = pageState.address.starts_with("https://");
    if (view == View::StartPage)
    {
        if (!pageView)
            state.disabledButtons.insert(ToolbarButton::Back);
        state.disabledButtons.insert({ToolbarButton::Forward, ToolbarButton::Reload, ToolbarButton::StartPage, ToolbarButton::PageInfo});
        state.isLoading = false;
        return state;
    }

    if (!pageState.canGoForward)
        state.disabledButtons.insert(ToolbarButton::Forward);
    return state;
}

bool InternetChannel::IsToolbarShown() const
{
    if (view != View::Page)
        return true;

    switch (data->GetToolbarMode())
    {
    case ToolbarMode::HiddenUntilPointedAt:
        return services.clock.Seconds() - secondsToolbarWasPointedAt < SecondsBeforeToolbarHides;
    case ToolbarMode::ToggledWithOne:
        return isToolbarToggledOn;
    default:
        return true;
    }
}

// A toolbar that is always there takes its place below the page; one that comes and goes lies over the page.
float InternetChannel::PageAreaHeight() const
{
    bool isToolbarBelowThePage = data->GetToolbarMode() == ToolbarMode::AlwaysShown ||
        (data->GetToolbarMode() == ToolbarMode::ToggledWithOne && isToolbarToggledOn);
    if (isToolbarBelowThePage)
        return flatFrame.height - PageToolbar::Height;

    return flatFrame.height;
}

std::u16string InternetChannel::LoadFailureText(Browser::LoadError error) const
{
    switch (error)
    {
    case Browser::LoadError::ServerNotFound:
    case Browser::LoadError::ServerNotSet:
        return texts.FindById(ServerNotFoundTextId);
    case Browser::LoadError::TimedOut:
        return texts.FindById(TimedOutTextId);
    case Browser::LoadError::ConnectionFailed:
        return texts.FindById(ConnectionClosedTextId);
    case Browser::LoadError::CertificateError:
        return texts.FindById(SecureConnectionFailedTextId);
    case Browser::LoadError::NoInternet:
        return ReplaceCodeMark(texts.FindById(NetworkErrorTextId), networkStart.supportCode);
    default:
        return texts.FindById(LoadFailedTextId);
    }
}

std::u16string InternetChannel::ControlsText() const
{
    std::u16string text;
    for (const ControlsLine& line : ControlsLines)
        text += std::u16string(line.buttonSigns) + u": " + Text(line.text) + u"\n";

    return text;
}

std::optional<InternetChannel::Place> InternetChannel::PointedPlace() const
{
    std::optional<Formats::Vector2> pointer = PointerOnPage();
    if (!pointer || focusedToolbarButton)
        return std::nullopt;

    for (std::size_t index = 0; index < OrbTitles.size(); ++index)
    {
        PageRectangle orbPlace = {containerLeft + OrbWidth * index, OrbsTop, OrbWidth, OrbPressableHeight};
        if (orbPlace.Contains(*pointer))
            return static_cast<Place>(index);
    }

    for (std::size_t index = 0; index < ButtonTitles.size(); ++index)
    {
        PageRectangle buttonPlace = {containerLeft, ButtonsTop + ButtonHeight * index, ButtonPressableWidth, ButtonHeight};
        if (buttonPlace.Contains(*pointer))
            return static_cast<Place>(OrbTitles.size() + index);
    }

    PageRectangle controlsPlace = {containerLeft + ControlsTextBox.left, ControllerPlace.top, ControlsTextBox.width,
        ControlsTextBox.top + ControlsTextBox.height - ControllerPlace.top};
    if (controlsPlace.Contains(*pointer))
        return Place::Controls;

    if (Shifted(LogoBox, containerLeft, 0).Contains(*pointer))
        return Place::OperaLogo;
    return std::nullopt;
}

std::optional<Formats::Vector2> InternetChannel::PointerOnPage() const
{
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(0);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;

    return painter->PictureToPage(services.display.PointerToCanvas(player->state.pointer));
}

std::optional<Formats::Vector2> InternetChannel::PointerOnFlatPage() const
{
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(0);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;

    return flatPainter->PictureToPage(services.display.PointerToCanvas(player->state.pointer));
}

std::optional<ToolbarButton> InternetChannel::PointedToolbarButton() const
{
    std::optional<Formats::Vector2> pointer = PointerOnFlatPage();
    if (!pointer || !IsToolbarShown())
        return std::nullopt;

    return toolbar->ButtonAt(*pointer, flatFrame);
}

std::optional<InternetChannel::DialogAnswer> InternetChannel::PointedDialogAnswer() const
{
    std::optional<Formats::Vector2> pointer = PointerOnPage();
    if (!pointer)
        return std::nullopt;

    bool isChoice = dialog->question == Question::ChooseSearchEngine;
    if (OkButton().Contains(*pointer))
        return isChoice ? DialogAnswer::Google : DialogAnswer::Ok;
    if (dialog->hasCancel && CancelButton().Contains(*pointer))
        return isChoice ? DialogAnswer::Yahoo : DialogAnswer::Cancel;
    return std::nullopt;
}

bool InternetChannel::IsPressed() const
{
    return services.input.HasPlayerPressed(0, SystemMenu::SelectButtons);
}

PageRectangle InternetChannel::DialogPlace() const
{
    return {frame.width / 2 - DialogHalfWidth, DialogTop, DialogWidth, DialogHeight};
}

// An alert has its OK in the middle (.alert #ok-button), a question has OK on the left and Cancel on the right.
PageRectangle InternetChannel::OkButton() const
{
    PageRectangle box = DialogPlace();
    bool hasCancel = dialog && dialog->hasCancel;
    float margin = hasCancel ? ButtonMargin : AlertOkMargin;
    return {box.left + DialogButtonsLeft + margin, box.top + DialogButtonsTop, DialogButtonWidth, DialogButtonHeight};
}

PageRectangle InternetChannel::CancelButton() const
{
    PageRectangle box = DialogPlace();
    float left = box.left + DialogButtonsLeft + ButtonMargin + DialogButtonWidth + ButtonMargin;
    return {left, box.top + DialogButtonsTop, DialogButtonWidth, DialogButtonHeight};
}

Formats::Vector2 InternetChannel::PageToPicture(Formats::Vector2 pagePoint, const PageFrame& pageFrame) const
{
    return {(pagePoint.x - pageFrame.width / 2) * pageFrame.scale, (pageFrame.height / 2 - pagePoint.y) * pageFrame.scale};
}

void InternetChannel::DrawStartPage()
{
    painter->FillRectangle({0, 0, frame.width, frame.height}, White);
    painter->DrawPicture(*background, {0, BackgroundTop, frame.width, WidePageHeight});

    if (focusedPlace == Place::OperaLogo)
        painter->DrawPicturePart(*logo, Shifted(FocusedLogoPlace, containerLeft, 0), FocusedLogoPart);
    else
        painter->DrawPicturePart(*logo, Shifted(LogoPlace, containerLeft, 0), LogoPart);
    painter->DrawPicture(*homeIcon, {containerLeft + HomeIconLeft, HomeIconTop, homeIcon->Width(), homeIcon->Height()});
    painter->DrawText(Text("S_START"), TitleStyle, Shifted(TitleBox, containerLeft, 0));

    DrawOrbs();
    DrawButtons();

    painter->DrawPicturePart(*controller, Shifted(ControllerPlace, containerLeft, 0), ControllerPart);
    painter->DrawText(Text("S_HOKKAIDO_LABEL_USE_WII_REMOTE"), LabelStyle, Shifted(ControlsTextBox, containerLeft, 0));
}

void InternetChannel::DrawOrbs()
{
    for (std::size_t index = 0; index < OrbTitles.size(); ++index)
    {
        float left = containerLeft + OrbWidth * index;
        bool isFocused = focusedPlace == static_cast<Place>(index);
        int state = isFocused ? FocusedState : BlurredState;

        PageRectangle picturePlace = {left + OrbPictureLeft, OrbsTop, OrbPictureSize, OrbPictureSize};
        PageRectangle firstState = {0, 0, OrbPictureSize, OrbPictureSize};
        painter->DrawPicturePart(*orb, picturePlace, {0, OrbPictureSize * state, OrbPictureSize, OrbPictureSize});
        painter->DrawPicturePart(*orbSymbols[index], picturePlace, firstState);
        painter->DrawPicturePart(*gloss, Shifted(picturePlace, 0, 1), firstState);
        painter->DrawText(Text(OrbTitles[index]), LabelStyle, {left, OrbLabelTop, OrbWidth, OrbLabelHeight});
    }
}

void InternetChannel::DrawButtons()
{
    for (std::size_t index = 0; index < ButtonTitles.size(); ++index)
    {
        float top = ButtonsTop + ButtonHeight * index;
        bool isFocused = focusedPlace == static_cast<Place>(OrbTitles.size() + index);
        int state = isFocused ? FocusedState : BlurredState;

        painter->DrawPicturePart(*button, {containerLeft, top, ButtonPictureWidth, ButtonHeight},
            {0, ButtonHeight * state, ButtonPictureWidth, ButtonHeight});
        painter->DrawPicturePart(*buttonIcons[index], {containerLeft + ButtonIconLeft, top, ButtonIconPart.width, ButtonIconPart.height},
            ButtonIconPart);
        painter->DrawText(Text(ButtonTitles[index]), ButtonStyle,
            {containerLeft + ButtonTextLeft, top + ButtonTextTop, ButtonPressableWidth - ButtonTextLeft, ButtonHeight});
    }
}

// The alert style of dialog16x9.css for a message, the confirm style for a question; the choice of the search engine shows the two
// logos of searchEngineSettingFlow.js as its buttons.
void InternetChannel::DrawDialog()
{
    painter->FillRectangle({0, 0, frame.width, frame.height}, DialogShade);

    PageRectangle box = DialogPlace();
    painter->DrawPicture(*dialogBack, box);

    painter->DrawPicture(dialog->hasCancel ? *questionSign : *infoSign, Shifted(SignPlace, box.left, box.top));
    painter->DrawText(dialog->text, MessageStyle, Shifted(MessageBox, box.left, box.top));

    if (dialog->question == Question::ChooseSearchEngine)
    {
        painter->DrawPicture(*googleButton, OkButton());
        painter->DrawPicture(*yahooButton, CancelButton());
        return;
    }

    PageRectangle firstState = {0, 0, DialogButtonWidth, DialogButtonHeight};
    painter->DrawPicturePart(*okButton, OkButton(), firstState);
    if (dialog->hasCancel)
        painter->DrawPicturePart(*cancelButton, CancelButton(), firstState);
}

std::unique_ptr<PagePicture> InternetChannel::ReadPicture(std::string_view path) const
{
    return std::make_unique<PagePicture>(services.display.Device(), Formats::ReadPngImage(FindFileOrFail(pages, path)));
}

}
