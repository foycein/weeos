#include "Channels/ShopChannel.h"

#include "Channels/ChannelError.h"
#include "Channels/ChannelFiles.h"
#include "Channels/ChannelText.h"

#include "Formats/Ash0.h"
#include "Formats/ByteReader.h"
#include "Formats/DolExecutable.h"
#include "Formats/GifImage.h"
#include "Formats/MessageFile.h"
#include "Formats/TextureDecoder.h"
#include "Formats/TplTexture.h"

#include <array>
#include <format>
#include <string_view>
#include <utility>

namespace Channels
{

namespace
{

// The contents of the Shop (HABA v21): the program and the archive with the pages and the layouts.
constexpr std::uint16_t ProgramContentIndex = 1;
constexpr std::uint16_t ResourceContentIndex = 2;

// The Shop keeps its texts in message files inside its program, one for each area and language. 0x80019F30 gives the place of the
// English one of the USA: 0x803053C0 of the program in memory.
constexpr std::uint32_t UsaEnglishMessagesAddress = 0x803053C0;
constexpr std::size_t MessageFileSizeOffset = 8;

constexpr std::uint16_t ErrorCodeLabelMessage = 0;
constexpr std::uint16_t ServerErrorMessage = 1;
constexpr std::uint16_t InternetErrorMessage = 2;
constexpr std::uint16_t TitleMessage = 7;
constexpr std::uint16_t TryAgainMessage = 8;
constexpr std::uint16_t WiiMenuMessage = 9;
constexpr std::uint16_t ConnectingMessage = 16;

// The start-up picture of the USA is a raw 16:9 picture of 832 by 456 pixels in RGB565, packed with ASH0.
constexpr std::string_view StartupPicturePath = "arc/layout/StartupTM.ash";
constexpr std::uint32_t StartupPictureWidth = 832;
constexpr std::uint32_t StartupPictureHeight = 456;

// The wallpaper beside the page on a 16:9 screen: the first of the four pictures of BG_16x9.tpl is the one with the dots,
// the wallpaper the pages ask for unless they choose another (cWP_Dots of shop.js).
constexpr std::string_view WallpaperPath = "arc/layout/BG_16x9.tpl";
constexpr std::size_t DotsWallpaper = 0;

constexpr std::string_view ButtonPicturePath = "arc/error/image/banner/under_banner_a.gif";
constexpr std::string_view FocusedButtonPicturePath = "arc/error/image/banner/under_banner_b.gif";
constexpr std::string_view ButtonShadowPath = "arc/error/image/banner/under_banner_shadow.gif";
constexpr std::string_view CursorArchivePath = "arc/layout/common/cursor.ash";
constexpr std::string_view FontFile = "wbf1.brfna";

// How long the start-up picture shows before the pages begin (CHN-28, Assumed).
constexpr double StartupPictureSeconds = 2;

// The places and the styles of error.css and of the pages internet.html and error.html, in pixels of the page. On a 16:9 screen the
// wallpaper fills the room beside the page.
constexpr float WidePictureWidth = 832;
constexpr float WallpaperWidth = (WidePictureWidth - PageWidth) / 2;
constexpr PageRectangle WholePage = {0, 0, PageWidth, PageHeight};
constexpr PageRectangle LeftWallpaper = {-WallpaperWidth, 0, WallpaperWidth, PageHeight};
constexpr PageRectangle RightWallpaper = {PageWidth, 0, WallpaperWidth, PageHeight};
constexpr PageRectangle WholeWidePicture = {-WallpaperWidth, 0, WidePictureWidth, PageHeight};
constexpr PageRectangle TitleBox = {29, 28, 554, 28};
constexpr PageRectangle UpperDots = {4, 59, 600, 16};
constexpr PageRectangle LowerDots = {4, 363, 600, 16};
constexpr PageRectangle ErrorCodeBox = {66, 80, 476, 22};
constexpr PageRectangle ErrorTextBox = {66, 111, 476, 176};

constexpr PageTextStyle TitleStyle = {28, {0x34, 0xBE, 0xED, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle DotsStyle = {16, {0x8C, 0x8C, 0x8C, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle ErrorCodeStyle = {22, {0xFF, 0x00, 0x00, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle ErrorTextStyle = {16, {0xFF, 0x00, 0x00, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle ButtonTextStyle = {18, {0x32, 0x32, 0x32, 255}, Text::HorizontalAlignment::Center};

// "Connecting. Please wait..." stands in the middle of the page; the characters that run beside it are 3D models (CHN-28, Assumed).
constexpr PageRectangle ConnectingBox = {0, 214, PageWidth, 28};
constexpr PageTextStyle ConnectingStyle = {20, {0x32, 0x32, 0x32, 255}, Text::HorizontalAlignment::Center};

// The dotted lines are a row of halfwidth middle dots.
constexpr std::u16string_view DottedLine =
    u"･････････････････････････････････････････････････････････････････････････";

// internet.html has one button in the middle, error.html one on the left and one on the right.
struct ButtonPlaces
{
    PageRectangle shadow;
    PageRectangle banner;
    PageRectangle word;
};

constexpr ButtonPlaces MiddleButton = {{199, 287, 211, 75}, {211, 294, 187, 55}, {214, 308, 180, 22}};
constexpr ButtonPlaces LeftButton = {{13, 371, 211, 75}, {25, 379, 187, 55}, {27, 394, 185, 22}};
constexpr ButtonPlaces RightButton = {{387, 371, 211, 75}, {399, 379, 187, 55}, {400, 394, 185, 22}};

// The sound effects of the pages, cSE_Forcus and cSE_Decide of sound.js, by their names in IplSound.brsar (CHN-28, Assumed).
constexpr std::string_view FocusSound = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view DecideSound = "WIPL_SE_DECIDE";

// The support codes whose page is internet.html (0x8002F390): the families of a connection to the Internet that cannot be made.
constexpr std::array<std::uint32_t, 4> InternetPageFamilies = {50200, 50300, 52000, 52100};
constexpr std::uint32_t FamilySize = 100;

// The bytes of the program that are loaded at the address, up to the end of their section.
Formats::ByteView ProgramBytesAt(const Formats::DolExecutable& program, std::uint32_t address)
{
    for (const Formats::DolSection& section : program.sections)
    {
        bool isInSection = address >= section.loadAddress && address < section.loadAddress + section.size;
        if (isInSection)
            return program.SectionData(section).subspan(address - section.loadAddress);
    }
    throw ChannelError(std::format("The program of the Wii Shop Channel loads nothing at {:#010x}", address));
}

Formats::MessageFile ReadUsaEnglishMessages(SystemMenu::TitleContents& contents)
{
    Formats::DolExecutable program = Formats::ReadDolExecutable(contents.ReadContent(ProgramContentIndex));
    Formats::ByteView messageFile = ProgramBytesAt(program, UsaEnglishMessagesAddress);

    Formats::ByteReader reader(messageFile, "the message file of the Wii Shop Channel");
    reader.MoveTo(MessageFileSizeOffset);
    std::uint32_t fileSize = reader.ReadUint32();
    return Formats::ReadMessageFile(messageFile.subspan(0, fileSize));
}

// The messages are HTML: <BR> breaks the line, a line end of the file is only a space between words.
std::u16string PageTextOf(std::u16string_view htmlText)
{
    constexpr std::u16string_view LineBreakTag = u"<BR>";
    std::u16string text;
    for (std::size_t index = 0; index < htmlText.size(); ++index)
    {
        if (htmlText.substr(index, LineBreakTag.size()) == LineBreakTag)
        {
            text += u'\n';
            index += LineBreakTag.size() - 1;
            continue;
        }

        if (htmlText[index] != u'\n')
            text += htmlText[index];
    }
    return text;
}

bool ShowsInternetPage(std::uint32_t supportCode)
{
    std::uint32_t family = supportCode / FamilySize * FamilySize;
    for (std::uint32_t internetFamily : InternetPageFamilies)
    {
        if (family == internetFamily)
            return true;
    }
    return false;
}

std::unique_ptr<PagePicture> ReadGifPicture(Graphics::GraphicsDevice& device, const Formats::U8Archive& resources, std::string_view path)
{
    return std::make_unique<PagePicture>(device, Formats::ReadGifImage(FindFileOrFail(resources, path)));
}

}

ShopChannel::ShopChannel(ChannelServices& channelServices)
    : services(channelServices)
    , messages(ReadUsaEnglishMessages(channelServices.contents))
{
    Graphics::GraphicsDevice& device = services.display.Device();
    Formats::U8Archive resources = ReadArchiveContent(services.contents, ResourceContentIndex);
    painter = std::make_unique<PagePainter>(device, services.systemFonts.Find(std::string(FontFile)));

    Formats::Bytes startupPixels = Formats::DecompressAsh0(FindFileOrFail(resources, StartupPicturePath));
    startupPicture = std::make_unique<PagePicture>(device,
        Formats::DecodeTexture(Formats::TextureFormat::Rgb565, StartupPictureWidth, StartupPictureHeight, startupPixels, std::nullopt));

    std::vector<Formats::TplTexture> wallpapers = Formats::ReadTplTextures(FindFileOrFail(resources, WallpaperPath));
    wallpaper = std::make_unique<PagePicture>(device, Formats::DecodeTexture(wallpapers.at(DotsWallpaper)));

    buttonPicture = ReadGifPicture(device, resources, ButtonPicturePath);
    focusedButtonPicture = ReadGifPicture(device, resources, FocusedButtonPicturePath);
    buttonShadow = ReadGifPicture(device, resources, ButtonShadowPath);

    Formats::U8Archive handFiles = Formats::ReadU8Archive(Formats::DecompressAsh0(FindFileOrFail(resources, CursorArchivePath)));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, std::make_unique<Layout::LayoutArchive>(std::move(handFiles)));
}

void ShopChannel::Update()
{
    if (services.fader.GetStatus() == SystemMenu::FaderStatus::Covered && !services.wiiMenu.IsRequested())
        services.fader.StartFadeIn();

    if (!services.wiiMenu.IsRequested())
        UpdateStage();
}

void ShopChannel::Draw()
{
    if (stage == Stage::StartUp)
    {
        painter->DrawPicture(*startupPicture, WholeWidePicture);
        return;
    }

    DrawWallpaper();
    if (stage == Stage::Connecting)
        DrawConnecting();
    else
        DrawError();
}

SystemMenu::CursorManager& ShopChannel::Hands()
{
    return *cursors;
}

ShopChannel::Stage ShopChannel::CurrentStage() const
{
    return stage;
}

ShopChannel::ErrorPage ShopChannel::ShownErrorPage() const
{
    return ShowsInternetPage(failure.supportCode) ? ErrorPage::Internet : ErrorPage::Server;
}

std::u16string ShopChannel::ErrorCodeText() const
{
    return Message(ErrorCodeLabelMessage) + AsciiToText(std::to_string(failure.supportCode));
}

std::u16string ShopChannel::ErrorMessageText() const
{
    std::uint16_t message = ShownErrorPage() == ErrorPage::Internet ? InternetErrorMessage : ServerErrorMessage;
    return PageTextOf(Message(message));
}

const std::u16string& ShopChannel::Message(std::uint16_t messageNumber) const
{
    return messages.Find(messageNumber);
}

void ShopChannel::UpdateStage()
{
    double seconds = services.clock.Seconds();
    if (stage == Stage::StartUp && seconds >= StartupPictureSeconds)
        StartConnecting();

    if (stage == Stage::Connecting && seconds - connectingStartSeconds >= failure.secondsUntilFailure)
        stage = Stage::ShowingError;

    if (stage == Stage::ShowingError && services.fader.GetStatus() == SystemMenu::FaderStatus::Clear)
        UpdateButtons();
}

void ShopChannel::StartConnecting()
{
    failure = TryToConnect(services.network);
    connectingStartSeconds = services.clock.Seconds();
    focusedButtonMessage.reset();
    stage = Stage::Connecting;
}

void ShopChannel::UpdateButtons()
{
    std::vector<PageButton> buttons = ShownButtons();
    std::optional<std::uint16_t> pointedButton;
    for (const PageButton& button : buttons)
    {
        if (IsPointedAt(button))
            pointedButton = button.message;
    }

    if (pointedButton && pointedButton != focusedButtonMessage)
        services.sounds.Request(FocusSound);

    focusedButtonMessage = pointedButton;
    if (!pointedButton || !services.input.AnyPlayerPressed(SystemMenu::SelectButtons))
        return;

    services.sounds.Request(DecideSound);
    if (*pointedButton == WiiMenuMessage)
        services.wiiMenu.Request();
    else
        StartConnecting();
}

std::vector<ShopChannel::PageButton> ShopChannel::ShownButtons() const
{
    if (ShownErrorPage() == ErrorPage::Internet)
        return {{MiddleButton.shadow, MiddleButton.banner, MiddleButton.word, WiiMenuMessage}};

    return {{LeftButton.shadow, LeftButton.banner, LeftButton.word, WiiMenuMessage},
        {RightButton.shadow, RightButton.banner, RightButton.word, TryAgainMessage}};
}

bool ShopChannel::IsPointedAt(const PageButton& button) const
{
    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        std::optional<Formats::Vector2> pointer = PointerOnPage(playerIndex);
        if (pointer && button.banner.Contains(*pointer))
            return true;
    }
    return false;
}

std::optional<Formats::Vector2> ShopChannel::PointerOnPage(int playerIndex) const
{
    const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(playerIndex);
    if (!player || !player->state.pointerIsValid)
        return std::nullopt;

    return PagePainter::PictureToPage(services.display.PointerToCanvas(player->state.pointer));
}

void ShopChannel::DrawWallpaper()
{
    painter->DrawPicture(*wallpaper, LeftWallpaper);
    painter->DrawPicture(*wallpaper, RightWallpaper, true);
    painter->FillRectangle(WholePage, {255, 255, 255, 255});
}

void ShopChannel::DrawConnecting()
{
    painter->DrawText(Message(ConnectingMessage), ConnectingStyle, ConnectingBox);
}

// In the order of the z-index of the pages: the buttons lie under the dotted lines, the title and the texts.
void ShopChannel::DrawError()
{
    for (const PageButton& button : ShownButtons())
        DrawButton(button);

    painter->DrawText(DottedLine, DotsStyle, UpperDots);
    painter->DrawText(DottedLine, DotsStyle, LowerDots);
    painter->DrawText(Message(TitleMessage), TitleStyle, TitleBox);
    painter->DrawText(ErrorMessageText(), ErrorTextStyle, ErrorTextBox);
    painter->DrawText(ErrorCodeText(), ErrorCodeStyle, ErrorCodeBox);
}

void ShopChannel::DrawButton(const PageButton& button)
{
    bool isFocused = focusedButtonMessage == button.message;
    painter->DrawPicture(*buttonShadow, button.shadow);
    painter->DrawPicture(isFocused ? *focusedButtonPicture : *buttonPicture, button.banner);
    painter->DrawText(Message(button.message), ButtonTextStyle, button.word);
}

}
