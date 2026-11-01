#include "Channels/WiiConnect24Channel.h"

#include "Channels/ChannelFiles.h"
#include "Channels/ChannelText.h"

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

// The hands are the ones of the HOME Button Menu: the channels' own hand is a NintendoWare effect (nw4r_defcursor_all01) that WeeOS
// cannot draw yet (docs/audit/OpenPoints/Channels.md CHN-26).
constexpr std::string_view HandArchivePath = "HomeButton2/LZ77_homeBtn.arc";

// The label in front of the code is a text of the program (0x8000B9C8 of the Forecast, "%ls %ls%06d").
constexpr std::u16string_view ErrorCodeLabel = u"Error Code: ";

// The sounds of the error, by their names in rev_wtr.brsar and rev_news.brsar (CHN-26, Assumed).
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

std::unique_ptr<Layout::LayoutArchive> ReadHandArchive(SystemMenu::TitleContents& contents, std::uint16_t homeMenuContentIndex)
{
    Formats::U8Archive homeMenuFiles = ReadArchiveContent(contents, homeMenuContentIndex);
    Formats::Bytes handArchive = ReadPackedFile(homeMenuFiles, HandArchivePath);
    return std::make_unique<Layout::LayoutArchive>(Formats::ReadU8Archive(handArchive));
}

}

WiiConnect24Channel::WiiConnect24Channel(ChannelServices& channelServices, const WiiConnect24ChannelFiles& files)
    : services(channelServices)
    , failure(TryToConnect(channelServices.network))
    , errorMessageCount(files.errorMessageCount)
{
    Formats::U8Archive resources = ReadArchiveContent(services.contents, files.resourceContentIndex);
    Formats::U8Archive layoutFiles = Formats::ReadU8Archive(ReadPackedFile(resources, files.layoutArchivePath));

    Graphics::GraphicsDevice& device = services.display.Device();
    fonts.AddFont(device, Formats::ReadFont(FindFileOrFail(layoutFiles, files.fontPath)), {files.fontName});
    services.display.SetTextFonts(fonts.Fonts());

    layoutArchive = std::make_unique<Layout::LayoutArchive>(std::move(layoutFiles));
    oneMomentLayout = std::make_unique<SystemMenu::MenuLayout>(services.display, *layoutArchive, std::string(OneMomentLayoutName));
    errorLayout = std::make_unique<SystemMenu::MenuLayout>(services.display, *layoutArchive, std::string(ErrorLayoutName));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, ReadHandArchive(services.contents, files.homeMenuContentIndex));

    HideOtherLanguages(*oneMomentLayout);
    Formats::Pane* downloadingMessage = oneMomentLayout->FindPane(DownloadingMessagePane);
    if (downloadingMessage != nullptr)
        downloadingMessage->isVisible = false;

    HideOtherLanguages(*errorLayout);
    ShowErrorLayout();
}

void WiiConnect24Channel::Update()
{
    if (!services.wiiMenu.IsRequested())
        UpdateStage();

    ShownLayout().Calculate();
    cursors->Update(services.input);
}

void WiiConnect24Channel::Draw()
{
    ShownLayout().Draw();
    cursors->Draw();
}

WiiConnect24Channel::Stage WiiConnect24Channel::CurrentStage() const
{
    return stage;
}

std::u16string WiiConnect24Channel::ErrorCodeText() const
{
    std::u16string text(ErrorCodeLabel);
    text += AsciiToText(std::format("{:06}", DownloadErrorCodeOf(failure.startupError)));
    return text;
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

    bool hasDownloadFailed = services.clock.Seconds() >= failure.secondsUntilFailure;
    if (fader.GetStatus() != SystemMenu::FaderStatus::Clear || !hasDownloadFailed)
        return;

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
        pane.isVisible = message == CannotConnectMessage;
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
