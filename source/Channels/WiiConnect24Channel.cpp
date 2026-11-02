#include "Channels/WiiConnect24Channel.h"

#include "Channels/ChannelFiles.h"
#include "Channels/ChannelText.h"

#include "SystemMenu/Display.h"

#include "Formats/Font.h"
#include "Formats/Lz77.h"

#include <array>
#include <format>
#include <string>
#include <utility>

namespace Channels
{

namespace
{

constexpr std::string_view OneMomentLayoutName = "error1";
constexpr std::string_view ErrorLayoutName = "error0";
constexpr std::string_view ErrorCodePane = "error_code";
constexpr std::string_view ButtonPane = "nextB";

// The English messages of a layout are the text boxes "textTUS0", "textTUS1" and so on. The News Channel's error1 has two of them,
// "One moment, please..." and "Downloading...", and shows the first while it starts.
constexpr std::string_view EnglishMessagePrefix = "textTUS";
constexpr std::string_view DownloadingMessagePane = "textTUS1";

// Every text of the layouts is there once for each language: the part of the layout, "T" and the language, as in textTJP or nextTGE.
// The program hides all but the language of the console.
constexpr std::array<std::string_view, 2> TextParts = {"textT", "nextT"};
constexpr std::array<std::string_view, 6> OtherLanguages = {"JP", "GE", "FR", "IT", "SP", "DU"};

// The hands of the channel's HOME Button Menu (HomeButton3), which show while it is open.
constexpr std::string_view HandArchivePath = "./HomeButton3/LZ77_homeBtn.arc";

// The channel's own hands are particle effects of its resource content. The Forecast draws them in a box of the size of the
// picture, the shadow 3 units right of and below the hand (0x8002C304).
constexpr std::string_view HandEffectsPath = "./nw4r_defcursor_all01.breff.LZ";
constexpr std::string_view HandPicturesPath = "./nw4r_defcursor_all01.breft.LZ";
constexpr EffectHandsLook HandsLook = {.showsEveryPlayer = true,
    .boxToCanvasScale = {1, 1},
    .shadowOffset = {3, -3},
    .holdingLift = 0,
    .closedHandTurns = true};

// The label in front of the code is a text of the program (0x8000B9C8 of the Forecast, "%ls %ls%06d").
constexpr std::u16string_view ErrorCodeLabel = u"Error Code: ";

// The code of the WiiConnect24 module (KD 0x13DB3760): 100000, plus 1000 for each step of its work (7 is the download), plus 100 for
// each kind of error, and then minus the error of the sockets (kind 2) or plus the error of the HTTP library (kind 3).
constexpr std::uint32_t DownloadSocketErrorCodeBase = 107200;
constexpr std::uint32_t DownloadHttpErrorCodeBase = 107300;

// The errors of the HTTP library (NHTTP) of the download. KD turns a name that is not found into a network error, which the channel
// shows as the Internet that cannot be reached, and a connection that fails and a reply it cannot read into a server error, which the
// channel shows as the service that is unavailable (0x13DB37E0; the Forecast's 0x800358A8 and 0x8000B46C).
constexpr std::uint32_t HttpNameNotFoundError = 4;
constexpr std::uint32_t HttpConnectError = 5;
constexpr std::uint32_t HttpReplyNotUnderstoodError = 7;

// The sounds of the error, by their names in rev_wtr.brsar and rev_news.brsar (Assumed).
constexpr std::string_view ErrorSound = "WTR_SE_COM_ERROR";
constexpr std::string_view ButtonSound = "WTR_SE_COM_BUTTON";

void HideOtherLanguages(SystemMenu::MenuLayout& layout)
{
    for (std::string_view part : TextParts)
    {
        for (std::string_view language : OtherLanguages)
        {
            Formats::Pane* pane = layout.FindPane(std::string(part) + std::string(language));
            if (pane != nullptr)
                pane->isVisible = false;
        }
    }
}

// The data formats of the downloads are not built; they come with WeeOS's own server. A server that answers gives the download a reply
// it cannot read.
std::uint32_t HttpErrorOf(SystemMenu::ServerAnswer answer)
{
    if (answer == SystemMenu::ServerAnswer::NameNotFound)
        return HttpNameNotFoundError;

    if (answer == SystemMenu::ServerAnswer::NoAnswer)
        return HttpConnectError;

    return HttpReplyNotUnderstoodError;
}

std::unique_ptr<Layout::LayoutArchive> ReadHandArchive(SystemMenu::TitleContents& contents, std::uint16_t homeMenuContentIndex)
{
    Formats::U8Archive homeMenuFiles = ReadArchiveContent(contents, homeMenuContentIndex);
    Formats::Bytes handArchive = ReadPackedFile(homeMenuFiles, HandArchivePath);
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(handArchive));
}

}

std::uint32_t DownloadErrorCodeOfStart(int startupError)
{
    return DownloadSocketErrorCodeBase + static_cast<std::uint32_t>(-startupError);
}

std::uint32_t DownloadErrorCodeOfServer(SystemMenu::ServerAnswer answer)
{
    return DownloadHttpErrorCodeBase + HttpErrorOf(answer);
}

WiiConnect24Channel::WiiConnect24Channel(ChannelServices& channelServices, const WiiConnect24ChannelFiles& files)
    : services(channelServices)
    , networkStart(SystemMenu::StartNetwork(channelServices.network.configuration, channelServices.network.sockets))
    , errorMessageCount(files.errorMessageCount)
{
    if (networkStart.isOnline)
        server = std::make_unique<SystemMenu::ServerContact>(services.network.sockets, services.network.serverAddress);

    Formats::U8Archive resources = ReadArchiveContent(services.contents, files.resourceContentIndex);
    Formats::U8Archive layoutFiles = Formats::ReadU8Archive(ReadPackedFile(resources, files.layoutArchivePath));
    waitingMark = std::make_unique<WaitingMark>(services.display.Device(), resources, files.waitingMark);

    Graphics::GraphicsDevice& device = services.display.Device();
    fonts.AddFont(device, Formats::ReadFont(FindFileOrFail(layoutFiles, files.fontPath)), {files.fontName});
    services.display.SetTextFonts(fonts.Fonts());

    layoutArchive = std::make_unique<Layout::LayoutArchive>(std::move(layoutFiles));
    oneMomentLayout = std::make_unique<SystemMenu::MenuLayout>(services.display, *layoutArchive, std::string(OneMomentLayoutName));
    errorLayout = std::make_unique<SystemMenu::MenuLayout>(services.display, *layoutArchive, std::string(ErrorLayoutName));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, ReadHandArchive(services.contents, files.homeMenuContentIndex));
    Formats::Bytes handEffects = ReadPackedFile(resources, HandEffectsPath);
    ownHands = std::make_unique<EffectHands>(services.display, handEffects, ReadPackedFile(resources, HandPicturesPath), HandsLook);

    HideOtherLanguages(*oneMomentLayout);
    Formats::Pane* downloadingMessage = oneMomentLayout->FindPane(DownloadingMessagePane);
    if (downloadingMessage != nullptr)
        downloadingMessage->isVisible = false;

    HideOtherLanguages(*errorLayout);
}

void WiiConnect24Channel::Update()
{
    if (!services.wiiMenu.IsRequested())
        UpdateStage();

    if (stage != Stage::ShowingError)
        waitingMark->Update();

    ShownLayout().Calculate();
}

void WiiConnect24Channel::Draw()
{
    ShownLayout().Draw();
    if (stage != Stage::ShowingError)
        waitingMark->Draw(SystemMenu::WideCanvas.width);
}

SystemMenu::CursorManager& WiiConnect24Channel::Hands()
{
    return *cursors;
}

EffectHands* WiiConnect24Channel::OwnHands()
{
    return ownHands.get();
}

WiiConnect24Channel::Stage WiiConnect24Channel::CurrentStage() const
{
    return stage;
}

const WaitingMark& WiiConnect24Channel::Mark() const
{
    return *waitingMark;
}

std::optional<int> WiiConnect24Channel::ShownMessage() const
{
    if (!failure)
        return std::nullopt;

    return failure->message;
}

std::u16string WiiConnect24Channel::ErrorCodeText() const
{
    if (!failure)
        return u"";

    std::u16string text(ErrorCodeLabel);
    text += AsciiToText(std::format("{:06}", failure->code));
    return text;
}

// While the PC is not connected the sockets do not start and the download fails after the time-out; else it fails with the error of
// the HTTP library as soon as the server could not be reached.
std::optional<WiiConnect24Channel::DownloadFailure> WiiConnect24Channel::FindDownloadFailure()
{
    if (!networkStart.isOnline)
    {
        if (services.clock.Seconds() < networkStart.secondsUntilFailure)
            return std::nullopt;

        return DownloadFailure{CannotConnectMessage, DownloadErrorCodeOfStart(networkStart.startupError)};
    }

    std::optional<SystemMenu::ServerAnswer> answer = server->Answer();
    if (!answer)
        return std::nullopt;

    int message = HttpErrorOf(*answer) == HttpNameNotFoundError ? CannotConnectMessage : ServiceUnavailableMessage;
    return DownloadFailure{message, DownloadErrorCodeOfServer(*answer)};
}

void WiiConnect24Channel::UpdateStage()
{
    if (stage == Stage::OneMoment)
        UpdateOneMoment();
    else if (stage == Stage::LeavingOneMoment)
        UpdateLeavingOneMoment();
    else
        UpdateError();
}

void WiiConnect24Channel::UpdateOneMoment()
{
    SystemMenu::Fader& fader = services.fader;
    if (fader.GetStatus() == SystemMenu::FaderStatus::Covered)
        fader.StartFadeIn();

    if (fader.GetStatus() != SystemMenu::FaderStatus::Clear)
        return;

    failure = FindDownloadFailure();
    if (!failure)
        return;

    ShowErrorLayout();
    fader.StartFadeOut();
    stage = Stage::LeavingOneMoment;
}

void WiiConnect24Channel::UpdateLeavingOneMoment()
{
    if (services.fader.GetStatus() != SystemMenu::FaderStatus::Covered)
        return;

    services.fader.StartFadeIn();
    services.sounds.Request(ErrorSound);
    stage = Stage::ShowingError;
}

void WiiConnect24Channel::UpdateError()
{
    if (services.fader.GetStatus() != SystemMenu::FaderStatus::Clear)
        return;

    if (!IsButtonPressed())
        return;

    services.sounds.Request(ButtonSound);
    services.wiiMenu.Request();
}

void WiiConnect24Channel::ShowErrorLayout()
{
    for (int message = 0; message < errorMessageCount; ++message)
    {
        Formats::Pane& pane = errorLayout->PaneNamed(std::string(EnglishMessagePrefix) + std::to_string(message));
        pane.isVisible = message == failure->message;
    }
    errorLayout->SetText(ErrorCodePane, ErrorCodeText());
}

bool WiiConnect24Channel::IsButtonPressed() const
{
    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid)
            continue;

        if (!services.input.HasPlayerPressed(playerIndex, SystemMenu::SelectButtons))
            continue;

        Formats::Vector2 pointedPlace = services.display.PointerToCanvas(player->state.pointer);
        if (errorLayout->IsPointInPane(ButtonPane, pointedPlace))
            return true;
    }
    return false;
}

SystemMenu::MenuLayout& WiiConnect24Channel::ShownLayout()
{
    if (stage == Stage::ShowingError)
        return *errorLayout;

    return *oneMomentLayout;
}

}
