#include "Channels/Photo/PhotoSession.h"

#include "Channels/ChannelFiles.h"

#include "Formats/Font.h"

#include <array>
#include <string>

namespace Channels
{

namespace
{

// Content 2 of the Photo Channel 1.1 holds the layouts, the textures and the sounds; content 3 is the shared HOME Button Menu.
constexpr std::uint16_t ResourceContentIndex = 2;
constexpr std::uint16_t HomeMenuContentIndex = 3;
constexpr std::string_view LayoutArchivePath = "./layout.arc.LZ";
constexpr std::string_view FontPath = "arc/font/font_photo.brfnt";
constexpr std::string_view FontName = "font_photo.brfnt";

// Every text and every picture with words is there once per language: the part of the pane, then T (text), I (picture with
// words) or M (the hints of the tips window), then the language. The program shows the English ones (US).
constexpr std::array<char, 3> LanguagePaneMarks = {'T', 'I', 'M'};
constexpr std::array<std::string_view, 6> OtherLanguages = {"JP", "GE", "FR", "IT", "SP", "DU"};
constexpr std::string_view ButtonPaneEnding = "B";

bool IsPaneOfAnotherLanguage(std::string_view paneName)
{
    for (char mark : LanguagePaneMarks)
    {
        for (std::string_view language : OtherLanguages)
        {
            std::string languagePart = std::string(1, mark) + std::string(language);
            if (paneName.find(languagePart) != std::string_view::npos)
                return true;
        }
    }
    return false;
}

}

PhotoSession::PhotoSession(ChannelServices& channelServices)
    : services(channelServices)
{
    Formats::U8Archive resources = ReadArchiveContent(services.contents, ResourceContentIndex);
    Formats::U8Archive layoutFiles = Formats::ReadU8Archive(ReadPackedFile(resources, LayoutArchivePath));

    fonts.AddFont(services.display.Device(), Formats::ReadFont(FindFileOrFail(layoutFiles, FontPath)), {FontName});
    services.display.SetTextFonts(fonts.Fonts());

    layoutArchive = std::make_unique<Layout::LayoutArchive>(std::move(layoutFiles));
    cursors = std::make_unique<SystemMenu::CursorManager>(services.display, ReadHandArchive(services.contents, HomeMenuContentIndex));
}

ChannelServices& PhotoSession::Services()
{
    return services;
}

SystemMenu::CursorManager& PhotoSession::Cursors()
{
    return *cursors;
}

std::unique_ptr<SystemMenu::MenuLayout> PhotoSession::MakeLayout(std::string_view layoutName)
{
    auto layout = std::make_unique<SystemMenu::MenuLayout>(services.display, *layoutArchive, std::string(layoutName));
    for (Formats::Pane* pane : layout->Live().AllPanes())
    {
        if (IsPaneOfAnotherLanguage(pane->name))
            pane->isVisible = false;
    }
    return layout;
}

bool PhotoSession::IsButtonPressed(SystemMenu::MenuLayout& layout, std::string_view buttonName)
{
    std::optional<Formats::Vector2> place = PlaceOfPress();
    if (!place)
        return false;

    Formats::Pane* button = layout.FindPane(buttonName);
    if (button == nullptr || !button->isVisible)
        return false;

    std::string areaName = std::string(buttonName) + std::string(ButtonPaneEnding);
    if (layout.FindPane(areaName) == nullptr)
        areaName = std::string(buttonName);

    return layout.IsPointInPane(areaName, *place);
}

bool PhotoSession::IsRemoteButtonPressed(Input::ButtonMask remoteButtons)
{
    return services.input.AnyPlayerPressed({remoteButtons, 0});
}

std::optional<Formats::Vector2> PhotoSession::PlaceOfPress()
{
    if (services.fader.GetStatus() != SystemMenu::FaderStatus::Clear)
        return std::nullopt;

    for (int playerIndex = 0; playerIndex < SystemMenu::PlayerCount; ++playerIndex)
    {
        const std::optional<SystemMenu::PlayerInput>& player = services.input.Player(playerIndex);
        if (!player || !player->state.pointerIsValid)
            continue;

        if (services.input.HasPlayerPressed(playerIndex, SystemMenu::SelectButtons))
            return services.display.PointerToCanvas(player->state.pointer);
    }
    return std::nullopt;
}

void PhotoSession::GoTo(std::unique_ptr<PhotoScreen> screen)
{
    nextScreen = std::move(screen);
}

std::unique_ptr<PhotoScreen> PhotoSession::TakeNextScreen()
{
    return std::move(nextScreen);
}

}
