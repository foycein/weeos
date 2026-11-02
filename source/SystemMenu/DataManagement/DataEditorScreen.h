#pragma once

#include "SystemMenu/ButtonController.h"
#include "SystemMenu/DataManagement/ChannelIconLayout.h"
#include "SystemMenu/DataManagement/DataActions.h"
#include "SystemMenu/DataManagement/DataItems.h"
#include "SystemMenu/MenuLayout.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/Screen.h"
#include "SystemMenu/WiiMenu/Balloon.h"

#include "Layout/LayoutArchive.h"

#include <array>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

// An editor of Data Management: the channels (screen 0x0c, chanEdit.ash) or the save data of the Wii (screen 0x0d, wiiMem.ash).
// Wii Options opens it below its buttons, and the Back button of the bar of Wii Options leads out of it.
//
// The list shows 15 entries on a page, of the Wii System Memory or of the SD card; the two buttons at the top switch between them.
// The arrows and the buttons Plus and Minus turn the pages. An entry opens a window with its name, its size in blocks and the buttons
// Move, Copy and Erase. A button asks first, then the work runs beside the menu while a message says what happens, and a message
// tells how it ended.
class DataEditorScreen : public Screen
{
public:
    DataEditorScreen(MenuServices& menuServices, DataKind editorKind);
    ~DataEditorScreen() override;

    void Load() override;
    void Build() override;
    void Draw(DrawPass pass) override;

protected:
    bool IsReady() override;
    bool RunMain() override;
    bool RunClosing() override;

private:
    enum class Phase
    {
        List,
        TurningPage,
        Window,
        Asking,
        Working,
        ShowingOutcome,
        ClosingWindow,
    };

    // One of the 15 places of a page, with the entry it shows.
    struct Slot
    {
        std::size_t itemIndex = 0;
        std::unique_ptr<SaveIconPictures> pictures;
        std::unique_ptr<MenuLayout> layout;
        std::unique_ptr<ButtonController> buttons;
        std::unique_ptr<ChannelIconLayout> icon;
        std::unique_ptr<Balloon> balloon;
        std::size_t focusInSlot = 0;
        std::size_t focusOutSlot = 0;
    };

    struct WindowAnimations
    {
        std::size_t seenIn = 0;
        std::size_t seenOut = 0;
        std::array<std::size_t, 3> focusIn = {};
        std::array<std::size_t, 3> focusOut = {};
        std::array<std::size_t, 3> flash = {};
    };

    void BuildList();
    void BuildWindow(const std::optional<Formats::RgbaImage>& savePicture);
    std::size_t BindListAnimation(std::string_view ending, std::string_view group);
    std::size_t BindWindowAnimation(std::string_view ending, std::string_view group);

    void ShowPlace(DataPlace newPlace);
    void ReadItems();
    void ShowPage();
    void MakeSlot(std::size_t itemIndex);
    void PlaceSlots();
    void PlaceBalloon(Balloon& balloon, std::size_t slotIndex);
    std::unique_ptr<ChannelIconLayout> MakeIcon(Formats::ByteView iconFile);
    void PlaceIcon(ChannelIconLayout& icon, MenuLayout& layout, std::string_view placePane);
    void ShowFreeBlocks();
    void ShowCardProblem();
    void ShowArrows();

    void RunList();
    void HandleListButtons(const std::vector<ButtonEvent>& events);
    void HandleSlotButtons();
    void TurnPage(int direction);
    void FinishTurningPage();
    bool WasBackPressed();

    void OpenWindow(std::size_t itemIndex);
    void RunWindow();
    void PressAction(DataAction action);
    void ReadAnswer();
    void StartWork();
    void FinishWork();
    void ReadOutcomeDialog();
    void CloseWindow();
    void ShowMessage(std::uint16_t messageNumber);

    MenuServices& services;
    DataKind kind;
    DataPlace place = DataPlace::Wii;
    Phase phase = Phase::List;

    std::unique_ptr<Layout::LayoutArchive> archive;
    std::unique_ptr<Layout::LayoutArchive> balloonArchive;
    std::unique_ptr<MenuLayout> listLayout;
    std::unique_ptr<SaveIconPictures> windowPictures;
    std::unique_ptr<MenuLayout> windowLayout;
    std::unique_ptr<ChannelIconLayout> windowIcon;
    std::unique_ptr<ButtonController> listButtons;
    std::unique_ptr<ButtonController> windowButtons;

    std::size_t dataInSlot = 0;
    std::size_t dataOutSlot = 0;
    std::size_t selectInSlot = 0;
    std::size_t errorInSlot = 0;
    std::size_t errorOutSlot = 0;
    std::size_t pageLeftSlot = 0;
    std::size_t pageRightSlot = 0;
    std::size_t toCardFlashSlot = 0;
    std::size_t toWiiFlashSlot = 0;
    WindowAnimations windowAnimations;

    std::vector<DataItem> items;
    std::vector<Slot> slots;
    std::size_t firstItemOfPage = 0;
    bool isErrorShown = false;
    bool hasSelectionAppeared = false;
    bool isCardReady = false;

    std::optional<std::size_t> openItem;
    std::optional<DataAction> chosenAction;
    std::future<DataOutcome> work;
};

}
