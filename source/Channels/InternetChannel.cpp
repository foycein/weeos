#include "Channels/InternetChannel.h"

#include "Channels/ChannelFiles.h"
#include "Channels/ChannelText.h"

#include "Formats/PngImage.h"

#include <format>
#include <string>

namespace Channels
{

namespace
{

// The contents of the Internet Channel (HADE v1024): the built-in pages with english.lng, and the cursors and the HOME menu.
constexpr std::uint16_t PagesContentIndex = 2;
constexpr std::uint16_t InterfaceContentIndex = 5;
constexpr std::string_view CursorArchivePath = "cursor.arc";
constexpr std::string_view LanguageFilePath = "./english.lng";
constexpr std::string_view StartPageFolder = "./startpage/stylesWideScreen/";
constexpr std::string_view DialogPictureFolder = "./images/wideScreen/";

// The widescreen styles lay the page out 450 pixels high (the background of startpage.css is 450 high), so one pixel of the page is
// 456 / 450 layout units and the page is as wide as the picture.
constexpr float WidePageHeight = 450;

// The places of the widescreen startpage.css, in pixels of the page, from the left edge of #frontcontainer, which is 547 pixels wide
// and stands in the middle. The tops follow from the heights and margins of the blocks above them.
constexpr float ContainerWidth = 547;
constexpr PageRectangle LogoPlace = {436, 13, 117, 18};
constexpr PageRectangle LogoPart = {0, 0, 117, 18};
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

constexpr std::array<std::string_view, 3> OrbTitles = {"S_HOKKAIDO_LABEL_SEARCH", "S_FAV_PAGE_FAV", "S_ENTER_WEB_ADDRESS"};
constexpr std::array<std::string_view, 3> OrbSymbolFiles = {"symSearch.png", "symFavorites.png", "symURL.png"};
constexpr std::array<std::string_view, 3> ButtonTitles = {
    "S_HOKKAIDO_LABEL_OPERATIONS_GUIDE", "S_HOKKAIDO_LABEL_SETTINGS", "S_HOKKAIDO_LABEL_WII_MENU"};
constexpr std::array<std::string_view, 3> ButtonIconFiles = {"link_help.png", "link_settings.png", "link_wiimenu.png"};

// The program gives the network dialog its text by the id itself (0x800338B0).
constexpr std::int32_t NetworkErrorTextId = static_cast<std::int32_t>(0xF5525EA9);
constexpr std::string_view ReturnQuestionText = "S_HOKKAIDO_LABEL_RETURN_WII_MENU";

std::u16string ReplaceCodeMark(std::u16string text, std::uint32_t code)
{
    std::u16string mark = AsciiToText(NetworkErrorCodeMark);
    std::size_t place = text.find(mark);
    if (place != std::u16string::npos)
        text.replace(place, mark.size(), AsciiToText(std::format("{:06}", code)));

    return text;
}

PageRectangle Shifted(const PageRectangle& place, float left, float top)
{
    return {place.left + left, place.top + top, place.width, place.height};
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
    containerLeft = (frame.width - ContainerWidth) / 2;
    painter = std::make_unique<PagePainter>(services.display.Device(), services.systemFonts.Find("wbf1.brfna"), frame);

    Formats::U8Archive interfaceFiles = ReadArchiveContent(services.contents, InterfaceContentIndex);
    Formats::U8Archive handFiles = Formats::ReadU8Archive(FindFileOrFail(interfaceFiles, CursorArchivePath));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, std::make_unique<Layout::LayoutArchive>(std::move(handFiles)));

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

    failure = TryToConnect(services.network);
}

void InternetChannel::Update()
{
    if (services.wiiMenu.IsRequested())
        return;

    if (services.fader.GetStatus() == SystemMenu::FaderStatus::Covered)
        services.fader.StartFadeIn();

    if (screen == Screen::StartPage)
        UpdateStartPage();
    else
        UpdateDialog();
}

void InternetChannel::Draw()
{
    DrawStartPage();
    if (screen != Screen::StartPage)
        DrawDialog();
}

SystemMenu::CursorManager& InternetChannel::Hands()
{
    return *cursors;
}

InternetChannel::Screen InternetChannel::CurrentScreen() const
{
    return screen;
}

std::u16string InternetChannel::DialogText() const
{
    if (screen == Screen::ReturnQuestion)
        return Text(ReturnQuestionText);

    return ReplaceCodeMark(texts.FindById(NetworkErrorTextId), failure.supportCode);
}

const std::u16string& InternetChannel::Text(std::string_view name) const
{
    return texts.Find(name);
}

// The join of the network ends while the start page shows, and the channel tells it once (0x800320CC, 0x800338B0).
void InternetChannel::UpdateStartPage()
{
    bool hasJoinFailed = services.clock.Seconds() >= failure.secondsUntilFailure;
    if (hasJoinFailed && !hasShownNetworkError)
    {
        hasShownNetworkError = true;
        focusedPlace.reset();
        screen = Screen::NetworkError;
        return;
    }

    focusedPlace = PointedPlace();
    if (focusedPlace == Place::WiiMenu && IsPressed())
        screen = Screen::ReturnQuestion;
}

void InternetChannel::UpdateDialog()
{
    if (!IsPressed())
        return;

    std::optional<Formats::Vector2> pointer = PointerOnPage();
    if (!pointer)
        return;

    bool isOkPressed = OkButton().Contains(*pointer);
    bool isCancelPressed = screen == Screen::ReturnQuestion && CancelButton().Contains(*pointer);
    if (isOkPressed && screen == Screen::ReturnQuestion)
    {
        services.wiiMenu.Request();
        return;
    }

    if (isOkPressed || isCancelPressed)
        screen = Screen::StartPage;
}

std::optional<InternetChannel::Place> InternetChannel::PointedPlace() const
{
    std::optional<Formats::Vector2> pointer = PointerOnPage();
    if (!pointer)
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
    return std::nullopt;
}

std::optional<Formats::Vector2> InternetChannel::PointerOnPage() const
{
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(0);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;

    return painter->PictureToPage(services.display.PointerToCanvas(player->state.pointer));
}

bool InternetChannel::IsPressed() const
{
    return services.input.HasPlayerPressed(0, SystemMenu::SelectButtons);
}

PageRectangle InternetChannel::DialogBox() const
{
    return {frame.width / 2 - DialogHalfWidth, DialogTop, DialogWidth, DialogHeight};
}

// An alert has its OK in the middle (.alert #ok-button), a question has OK on the left and Cancel on the right.
PageRectangle InternetChannel::OkButton() const
{
    PageRectangle box = DialogBox();
    float margin = screen == Screen::ReturnQuestion ? ButtonMargin : AlertOkMargin;
    return {box.left + DialogButtonsLeft + margin, box.top + DialogButtonsTop, DialogButtonWidth, DialogButtonHeight};
}

PageRectangle InternetChannel::CancelButton() const
{
    PageRectangle box = DialogBox();
    float left = box.left + DialogButtonsLeft + ButtonMargin + DialogButtonWidth + ButtonMargin;
    return {left, box.top + DialogButtonsTop, DialogButtonWidth, DialogButtonHeight};
}

void InternetChannel::DrawStartPage()
{
    painter->FillRectangle({0, 0, frame.width, frame.height}, White);
    painter->DrawPicture(*background, {0, BackgroundTop, frame.width, WidePageHeight});

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

void InternetChannel::DrawDialog()
{
    painter->FillRectangle({0, 0, frame.width, frame.height}, DialogShade);

    PageRectangle box = DialogBox();
    painter->DrawPicture(*dialogBack, box);

    bool isQuestion = screen == Screen::ReturnQuestion;
    painter->DrawPicture(isQuestion ? *questionSign : *infoSign, Shifted(SignPlace, box.left, box.top));
    painter->DrawText(DialogText(), MessageStyle, Shifted(MessageBox, box.left, box.top));

    PageRectangle firstState = {0, 0, DialogButtonWidth, DialogButtonHeight};
    painter->DrawPicturePart(*okButton, OkButton(), firstState);
    if (isQuestion)
        painter->DrawPicturePart(*cancelButton, CancelButton(), firstState);
}

std::unique_ptr<PagePicture> InternetChannel::ReadPicture(std::string_view path) const
{
    return std::make_unique<PagePicture>(services.display.Device(), Formats::ReadPngImage(FindFileOrFail(pages, path)));
}

}
