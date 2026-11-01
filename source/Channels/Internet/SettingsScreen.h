#pragma once

#include "Channels/Internet/OperaData.h"
#include "Channels/OperaLanguageFile.h"
#include "Channels/PagePainter.h"

#include "Formats/U8Archive.h"
#include "Graphics/GraphicsDevice.h"

#include <memory>
#include <optional>
#include <vector>

namespace Channels
{

// The settings of the Internet Channel that mean something on a PC (preferences.html with the widescreen prefernces.css, the manual
// page 7): Toolbar Display, Search Engine and Delete Cookies. Each setting is a button; a button with choices opens below itself and
// shows its choices as radio buttons. The settings that the PC decides are left out: the proxy (DECISIONS.md: no proxy), the width of
// the display (the picture of WeeOS is always whole) and the zoom type (the zoom of the page is the engine's).
class SettingsScreen
{
public:
    enum class Entry
    {
        ToolbarDisplay,
        SearchEngine,
        DeleteCookies,
    };

    // A setting, or one of its choices: the value that it stands for.
    struct Item
    {
        Entry entry = Entry::ToolbarDisplay;
        std::optional<int> choice;

        bool operator==(const Item& other) const = default;
    };

    // The device, the pages and the texts must outlive the screen.
    SettingsScreen(Graphics::GraphicsDevice& device, const Formats::U8Archive& pages, const OperaLanguageFile& texts);

    // Every setting closed.
    void Open();

    // A setting with choices opens, or closes when it is open.
    void ToggleEntry(Entry entry);

    std::optional<Item> ItemAt(Formats::Vector2 pagePoint, const PageFrame& frame) const;
    void Draw(PagePainter& painter, const PageFrame& frame, const OperaData& data, std::optional<Item> focusedItem) const;

private:
    struct PlacedItem
    {
        Item item;
        PageRectangle place;
    };

    std::vector<PlacedItem> PlaceItems(const PageFrame& frame) const;
    int ChosenValue(Entry entry, const OperaData& data) const;
    void DrawEntry(PagePainter& painter, const PlacedItem& placed, bool isFocused) const;
    void DrawChoice(PagePainter& painter, const PlacedItem& placed, bool isChosen, bool isFocused) const;

    const OperaLanguageFile& texts;
    std::unique_ptr<PagePicture> button;
    std::unique_ptr<PagePicture> arrow;
    std::unique_ptr<PagePicture> radio;
    std::unique_ptr<PagePicture> titleIcon;
    std::optional<Entry> openEntry;
};

}
