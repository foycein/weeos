#include "Channels/Internet/SettingsScreen.h"

#include "Channels/ChannelFiles.h"

#include "Formats/PngImage.h"

#include <array>
#include <string_view>

namespace Channels
{

namespace
{

constexpr std::string_view PictureFolder = "./preferences/stylesWideScreen/";

// The places of the widescreen prefernces.css: the title with its icon, and the buttons of the settings 461 by 45 pixels in the middle.
constexpr float ListWidth = 461;
constexpr float HeaderTop = 22;
constexpr float HeaderHeight = 30;
constexpr float TitleIconSize = 23;
constexpr float TitleIconDrop = 3;
constexpr float TitleIndent = 33;
constexpr float ListTop = 64;
constexpr float RowHeight = 45;
constexpr float RowGap = 7;
constexpr float TextIndent = 79;
constexpr float TextTop = 12;
constexpr float ArrowLeft = 33;
constexpr float ArrowWidth = 23;
constexpr float ChoiceIndent = 22;
constexpr float RadioWidth = 67;

// The states in the sprites of the css: the button as it is and pointed at (side by side), the arrow closed and open, the radio buttons.
constexpr float ButtonPointedAtLeft = 472;
constexpr float ArrowClosedTop = 0;
constexpr float ArrowClosedPointedAtTop = 90;
constexpr float ArrowOpenPointedAtTop = 135;
constexpr float ArrowOpenTop = 225;
constexpr float RadioChosenTop = 0;
constexpr float RadioChosenPointedAtTop = 45;
constexpr float RadioTop = 90;
constexpr float RadioPointedAtTop = 135;

constexpr Formats::Rgba8Color PageColor = {255, 255, 255, 255};
constexpr Formats::Rgba8Color ChoicesColor = {0xf0, 0xf0, 0xf1, 255};
constexpr PageTextStyle TitleStyle = {22, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};
constexpr PageTextStyle RowStyle = {18, {0, 0, 0, 255}, Text::HorizontalAlignment::Left};

constexpr std::string_view SettingsTitle = "S_HOKKAIDO_LABEL_SETTINGS";

// "Delete Cookies" is asked for by its id.
constexpr std::int32_t DeleteCookiesTextId = -1038172511;

struct EntryTexts
{
    SettingsScreen::Entry entry;
    std::string_view title;
    std::array<std::string_view, 3> choices;
    std::array<int, 3> values;
};

// The settings in the order of preferences/menu.js, with the values of their choices.
constexpr std::array<EntryTexts, 3> Entries = {{
    {SettingsScreen::Entry::ToolbarDisplay, "S_SETTING_TOOLBAR", {"S_SETTING_TOOLBAR_ON", "S_SETTING_TOOLBAR_AUTO", "S_SETTING_TOOLBAR_TOGGLE"},
        {
            static_cast<int>(ToolbarMode::AlwaysShown),
            static_cast<int>(ToolbarMode::HiddenUntilPointedAt),
            static_cast<int>(ToolbarMode::ToggledWithOne),
        }},
    {SettingsScreen::Entry::SearchEngine, "S_SE_PREF", {"S_HOKKAIDO_GOOGLE", "S_HOKKAIDO_YAHOO", ""},
        {static_cast<int>(SearchEngine::Google), static_cast<int>(SearchEngine::Yahoo), 0}},
    {SettingsScreen::Entry::DeleteCookies, "", {"", "", ""}, {0, 0, 0}},
}};

float ArrowTopOf(bool isOpen, bool isFocused)
{
    if (isOpen)
        return isFocused ? ArrowOpenPointedAtTop : ArrowOpenTop;

    return isFocused ? ArrowClosedPointedAtTop : ArrowClosedTop;
}

float RadioTopOf(bool isChosen, bool isFocused)
{
    if (isChosen)
        return isFocused ? RadioChosenPointedAtTop : RadioChosenTop;

    return isFocused ? RadioPointedAtTop : RadioTop;
}

const EntryTexts& TextsOf(SettingsScreen::Entry entry)
{
    return Entries[static_cast<std::size_t>(entry)];
}

std::unique_ptr<PagePicture> ReadPicture(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, std::string_view name)
{
    std::string path = std::string(PictureFolder) + std::string(name);
    return std::make_unique<PagePicture>(device, Formats::ReadPngImage(FindFileOrFail(pages, path)));
}

}

SettingsScreen::SettingsScreen(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, const OperaLanguageFile& languageTexts)
    : texts(languageTexts)
    , button(ReadPicture(device, pages, "button.png"))
    , arrow(ReadPicture(device, pages, "arrows_2.png"))
    , radio(ReadPicture(device, pages, "radio.png"))
    , titleIcon(ReadPicture(device, pages, "tools.png"))
{
}

void SettingsScreen::Open()
{
    openEntry.reset();
}

void SettingsScreen::ToggleEntry(Entry entry)
{
    if (openEntry == entry)
        openEntry.reset();
    else
        openEntry = entry;
}

std::optional<SettingsScreen::Item> SettingsScreen::ItemAt(Formats::Vector2 pagePoint, const PageFrame& frame) const
{
    for (const PlacedItem& placed : PlaceItems(frame))
    {
        if (placed.place.Contains(pagePoint))
            return placed.item;
    }
    return std::nullopt;
}

void SettingsScreen::Draw(PagePainter& painter, const PageFrame& frame, const OperaData& data, std::optional<Item> focusedItem) const
{
    painter.FillRectangle({0, 0, frame.width, frame.height}, PageColor);

    float left = (frame.width - ListWidth) / 2;
    painter.DrawPicture(*titleIcon, {left, HeaderTop + TitleIconDrop, TitleIconSize, TitleIconSize});
    painter.DrawText(texts.Find(SettingsTitle), TitleStyle, {left + TitleIndent, HeaderTop, ListWidth, HeaderHeight});

    for (const PlacedItem& placed : PlaceItems(frame))
    {
        bool isFocused = focusedItem == placed.item;
        if (!placed.item.choice)
        {
            DrawEntry(painter, placed, isFocused);
            continue;
        }

        bool isChosen = ChosenValue(placed.item.entry, data) == *placed.item.choice;
        DrawChoice(painter, placed, isChosen, isFocused);
    }
}

// The buttons one under the other; the choices of the open setting lie between it and the next button.
std::vector<SettingsScreen::PlacedItem> SettingsScreen::PlaceItems(const PageFrame& frame) const
{
    float left = (frame.width - ListWidth) / 2;
    float top = ListTop;

    std::vector<PlacedItem> placed;
    for (const EntryTexts& entry : Entries)
    {
        placed.push_back({{entry.entry, std::nullopt}, {left, top, ListWidth, RowHeight}});
        top += RowHeight + RowGap;
        if (openEntry != entry.entry)
            continue;

        for (std::size_t index = 0; index < entry.choices.size(); ++index)
        {
            if (entry.choices[index].empty())
                continue;

            placed.push_back({{entry.entry, entry.values[index]}, {left, top, ListWidth, RowHeight}});
            top += RowHeight;
        }
        top += RowGap;
    }
    return placed;
}

int SettingsScreen::ChosenValue(Entry entry, const OperaData& data) const
{
    if (entry == Entry::ToolbarDisplay)
        return static_cast<int>(data.GetToolbarMode());
    if (entry == Entry::SearchEngine)
        return static_cast<int>(data.GetSearchEngine());
    return 0;
}

void SettingsScreen::DrawEntry(PagePainter& painter, const PlacedItem& placed, bool isFocused) const
{
    const PageRectangle& place = placed.place;
    float buttonLeft = isFocused ? ButtonPointedAtLeft : 0;
    painter.DrawPicturePart(*button, place, {buttonLeft, 0, ListWidth, RowHeight});

    bool isDeleteCookies = placed.item.entry == Entry::DeleteCookies;
    if (!isDeleteCookies)
    {
        bool isOpen = openEntry == placed.item.entry;
        float arrowTop = ArrowTopOf(isOpen, isFocused);
        painter.DrawPicturePart(*arrow, {place.left + ArrowLeft, place.top, ArrowWidth, RowHeight}, {0, arrowTop, ArrowWidth, RowHeight});
    }

    const std::u16string& title = isDeleteCookies ? texts.FindById(DeleteCookiesTextId) : texts.Find(TextsOf(placed.item.entry).title);
    painter.DrawText(title, RowStyle, {place.left + TextIndent, place.top + TextTop, place.width - TextIndent, RowHeight});
}

void SettingsScreen::DrawChoice(PagePainter& painter, const PlacedItem& placed, bool isChosen, bool isFocused) const
{
    const PageRectangle& place = placed.place;
    painter.FillRectangle(place, ChoicesColor);

    float radioTop = RadioTopOf(isChosen, isFocused);
    painter.DrawPicturePart(*radio, {place.left + ChoiceIndent, place.top, radio->Width(), RowHeight}, {0, radioTop, radio->Width(), RowHeight});

    const EntryTexts& entry = TextsOf(placed.item.entry);
    for (std::size_t index = 0; index < entry.values.size(); ++index)
    {
        if (entry.values[index] != *placed.item.choice || entry.choices[index].empty())
            continue;

        float textLeft = place.left + ChoiceIndent + RadioWidth;
        float textWidth = place.width - textLeft + place.left;
        painter.DrawText(texts.Find(entry.choices[index]), RowStyle, {textLeft, place.top + TextTop, textWidth, RowHeight});
    }
}

}
