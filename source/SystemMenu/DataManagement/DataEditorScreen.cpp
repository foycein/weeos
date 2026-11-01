#include "SystemMenu/DataManagement/DataEditorScreen.h"

#include "SystemMenu/DataManagement/NandRoom.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/WiiOptionsBarScreen.h"

#include "Formats/FormatError.h"
#include "Input/RemoteButtons.h"
#include "Layout/LayoutError.h"
#include "Text/MessageCatalog.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <string_view>

namespace SystemMenu
{

namespace
{

// The names that bind an editor to its layouts (chanEdit.ash for the channels, wiiMem.ash for the save data).
struct EditorNames
{
    std::string_view bundle;
    std::string_view listLayout;
    std::string_view slotLayout;
    std::string_view windowLayout;

    // The pane of a slot that is moved onto its place of the list.
    std::string_view slotAnchor;
};

constexpr EditorNames ChannelEditorNames = {"chanEdit", "it_ObjChannelEdit_a", "it_ObjChannelEdit_b", "mn_ChannelDetail_a", "N_All"};
constexpr EditorNames SaveDataEditorNames = {"wiiMem", "it_ObjCubeEdit_a", "it_ObjDataEdit_b", "081210_sys4_mn_DataDetail_a", "N_Data_00"};

const EditorNames& NamesOf(DataKind kind)
{
    return kind == DataKind::Channels ? ChannelEditorNames : SaveDataEditorNames;
}

constexpr std::size_t SlotsPerPage = 15;

// The meter of the SD card has room for six digits.
constexpr std::uint64_t MostCardBlocksShown = 999999;

// The numbers of the messages in the message file of the menu.
constexpr std::uint16_t WiiMessage = 0x13D;
constexpr std::uint16_t SdCardMessage = 0x13E;
constexpr std::uint16_t MoveMessage = 0xA7;
constexpr std::uint16_t CopyMessage = 0xB2;
constexpr std::uint16_t EraseMessage = 0xBD;
constexpr std::uint16_t BlocksOpenMessage = 0x9C;
constexpr std::uint16_t YesMessage = 0x141;
constexpr std::uint16_t NoMessage = 0x142;
constexpr std::uint16_t OkMessage = 0x2E;
constexpr std::uint16_t NoCardMessage = 0x121;
constexpr std::uint16_t UnusableCardMessage = 0x38;

constexpr std::string_view PushSound = "WIPL_SE_BT_PUSH";
constexpr std::string_view PageSound = "WSD_SELECT";
constexpr std::string_view BalloonSound = "WIPL_SE_BALLOON";

// The balloon with the name of an entry (0x813A0680, 0x813A50F8): 55 units below the place of its balloon in the list, at least 90 units
// from the edges of a 16:9 picture and 60 of a 4:3 one; it appears as the balloons of the bar do (0x813D140C).
constexpr std::string_view BalloonBundleName = "balloon";
constexpr int StepsBeforeABalloonAppears = 17;
constexpr float BalloonDepthBelowItsPlace = 55;
constexpr float BalloonMarginOnAWideScreen = 90;
constexpr float BalloonMarginOnANarrowScreen = 60;

// The buttons of the list and of the window, in the order of their numbers in the button groups.
constexpr std::size_t WiiButton = 0;
constexpr std::size_t SdCardButton = 1;
constexpr std::size_t NextPageButton = 2;
constexpr std::size_t PreviousPageButton = 3;

constexpr std::array<DataAction, 3> WindowActions = {DataAction::Move, DataAction::Copy, DataAction::Erase};

constexpr ButtonSet NextPageButtons = {Input::RemoteButtons::Plus, Input::ClassicButtons::Plus};
constexpr ButtonSet PreviousPageButtons = {Input::RemoteButtons::Minus, Input::ClassicButtons::Minus};

// The panes of the window that the editors of the original never show.
constexpr std::array<std::string_view, 4> UnusedWindowPanes = {"T_Block_01", "T_Move_off", "T_Move_off_00", "N_Wait"};

// The window of a channel has a part for each shape of the screen.
struct ChannelWindowPanes
{
    std::string_view shownPart;
    std::string_view hiddenPart;
    std::string_view firstLine;
    std::string_view secondLine;
    std::string_view blocks;
    std::string_view iconPlace;
};

constexpr ChannelWindowPanes StandardChannelWindow = {"N_Mask4x3", "N_Mask16x9", "T_Title_00", "T_Title_01", "T_Block_00", "N_Atari4x3"};
constexpr ChannelWindowPanes WideChannelWindow = {"N_Mask16x9", "N_Mask4x3", "T_Title_02", "T_Title_03", "T_Block_02", "N_Atari16x9"};

// A slot of the channels has a frame for each shape of the screen, with its own button and its own place for the icon.
struct ChannelSlotPanes
{
    std::string_view shownFrame;
    std::string_view hiddenFrame;
    std::string_view button;
    std::string_view iconPlace;
};

constexpr ChannelSlotPanes StandardChannelSlot = {"N_Data4x3", "N_Data16x9", "B_Data_00", "N_Atari4x3"};
constexpr ChannelSlotPanes WideChannelSlot = {"N_Data16x9", "N_Data4x3", "B_Data_01", "N_Atari16x9"};
constexpr std::string_view SaveSlotButton = "B_Data_00";

constexpr std::u16string_view DamagedName = u"???";

// The places of the slots of the page that shows, which are also the places of their balloons, in the layout of the list. The original
// takes N_Data_a_xx, which its slide animation has moved into the middle; WeeOS keeps the list at rest, where N_Data_b_xx is the middle.
std::string SlotPaneName(std::size_t slotIndex)
{
    return std::format("N_Data_b_{:02}", slotIndex);
}

std::u16string NumberText(std::uint64_t number)
{
    std::string digits = std::to_string(number);
    return std::u16string(digits.begin(), digits.end());
}

}

DataEditorScreen::DataEditorScreen(MenuServices& menuServices, DataKind editorKind)
    : Screen(editorKind == DataKind::Channels ? ScreenId::ChannelDataEditor : ScreenId::WiiSaveDataEditor)
    , services(menuServices)
    , kind(editorKind)
{
}

DataEditorScreen::~DataEditorScreen()
{
    if (work.valid())
        work.wait();
}

void DataEditorScreen::Load()
{
    archive = services.files.OpenLayoutBundle(NamesOf(kind).bundle);
    balloonArchive = services.files.OpenLayoutBundle(BalloonBundleName);
}

void DataEditorScreen::Build()
{
    BuildList();
    BuildWindow(std::nullopt);
    ShowPlace(DataPlace::Wii);

    listLayout->StartAnimation(dataInSlot);
    listLayout->Calculate();
    windowLayout->Calculate();
}

void DataEditorScreen::BuildList()
{
    const EditorNames& names = NamesOf(kind);
    listLayout = std::make_unique<MenuLayout>(services.display, *archive, std::string(names.listLayout));
    listLayout->SetText("T_SelectWii_00", services.messages.Find(WiiMessage));
    listLayout->SetText("T_SelectWii_01", services.messages.Find(WiiMessage));
    listLayout->SetText("T_SelectSd_00", services.messages.Find(SdCardMessage));
    listLayout->SetText("T_SelectSd_01", services.messages.Find(SdCardMessage));

    dataInSlot = BindListAnimation("DataIn", "G_DataAll");
    dataOutSlot = BindListAnimation("DataOut", "G_DataAll");
    selectInSlot = BindListAnimation("SelectIn", "G_Select");
    errorInSlot = BindListAnimation("ErrorTxtIn", "G_ErrorTxt");
    errorOutSlot = BindListAnimation("ErrorTxtOut", "G_ErrorTxt");
    pageLeftSlot = BindListAnimation("ArwL1", "G_Arw");
    pageRightSlot = BindListAnimation("AwrR1", "G_Arw");

    // The original plays the flash that is named after the other button (0x813A1A20).
    toCardFlashSlot = BindListAnimation("SelectWiiFlash", "G_Select");
    toWiiFlashSlot = BindListAnimation("SelectSdFlash", "G_Select");

    // The list of the save data shares its layout with the memory cards of the GameCube and switches it to the Wii once.
    if (kind == DataKind::SaveData)
        listLayout->ShowLastFrameOfAnimation(BindListAnimation("WiiSwitch", "G_Switch"));

    listLayout->PaneNamed("N_Error").isVisible = false;
    listButtons = std::make_unique<ButtonController>(*listLayout, services.display, std::vector<std::string>{"B_SelectWii_00", "B_SelectSd_00", "B_ArwR", "B_ArwL"});
}

// The window of save data shows the picture of its banner where the layout has an empty picture.
void DataEditorScreen::BuildWindow(const std::optional<Formats::RgbaImage>& savePicture)
{
    windowAnimations = WindowAnimations();
    windowPictures = std::make_unique<SaveIconPictures>(*archive, savePicture);
    windowLayout = std::make_unique<MenuLayout>(services.display, *archive, std::string(NamesOf(kind).windowLayout), *windowPictures);
    windowLayout->SetText("T_Move_00", services.messages.Find(MoveMessage));
    windowLayout->SetText("T_Copy_00", services.messages.Find(CopyMessage));
    windowLayout->SetText("T_Del_00", services.messages.Find(EraseMessage));
    windowLayout->SetText("T_Message_00", u"");
    for (std::string_view pane : UnusedWindowPanes)
        windowLayout->PaneNamed(pane).isVisible = false;

    windowAnimations.seenIn = BindWindowAnimation("SeenIn", "G_Mask");
    windowAnimations.seenOut = BindWindowAnimation("SeenOut", "G_Mask");

    constexpr std::array<std::string_view, 3> ButtonStems = {"Move", "Copy", "Del"};
    for (std::size_t index = 0; index < ButtonStems.size(); ++index)
    {
        std::string group = std::format("G_{}", ButtonStems[index]);
        windowAnimations.focusIn[index] = BindWindowAnimation(std::format("{}FoucusIn", ButtonStems[index]), group);
        windowAnimations.focusOut[index] = BindWindowAnimation(std::format("{}FoucusOut", ButtonStems[index]), group);
        windowAnimations.flash[index] = BindWindowAnimation(std::format("{}Flash", ButtonStems[index]), group + "Flash");
    }
    windowButtons = std::make_unique<ButtonController>(*windowLayout, services.display, std::vector<std::string>{"B_Move_00", "B_Copy_00", "B_Del_00"});
}

std::size_t DataEditorScreen::BindListAnimation(std::string_view ending, std::string_view group)
{
    std::string name = std::format("{}_{}", NamesOf(kind).listLayout, ending);
    return listLayout->BindAnimation(name, group, Layout::PlaybackMode::PlayOnce);
}

std::size_t DataEditorScreen::BindWindowAnimation(std::string_view ending, std::string_view group)
{
    std::string name = std::format("{}_{}", NamesOf(kind).windowLayout, ending);
    return windowLayout->BindAnimation(name, group, Layout::PlaybackMode::PlayOnce);
}

// The list of the Wii System Memory or of the SD card, from its first page (0x813A1A20).
void DataEditorScreen::ShowPlace(DataPlace newPlace)
{
    place = newPlace;
    firstItemOfPage = 0;
    ReadItems();
    ShowPage();
    ShowFreeBlocks();
    ShowCardProblem();
}

void DataEditorScreen::ReadItems()
{
    items.clear();
    isCardReady = services.dataManagement != nullptr && ReadCardState(*services.dataManagement) == CardState::Ready;
    if (services.dataManagement == nullptr)
        return;

    if (place == DataPlace::SdCard && !isCardReady)
        return;

    items = ReadDataItems(*services.dataManagement, kind, place);
}

void DataEditorScreen::ShowPage()
{
    slots.clear();
    for (std::size_t slotIndex = 0; slotIndex < SlotsPerPage; ++slotIndex)
    {
        std::size_t itemIndex = firstItemOfPage + slotIndex;
        if (itemIndex >= items.size())
            break;

        MakeSlot(itemIndex);
    }
    ShowArrows();
}

void DataEditorScreen::MakeSlot(std::size_t itemIndex)
{
    const DataItem& item = items[itemIndex];
    const EditorNames& names = NamesOf(kind);
    bool isWide = services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
    const ChannelSlotPanes& channelPanes = isWide ? WideChannelSlot : StandardChannelSlot;

    Slot slot;
    slot.itemIndex = itemIndex;
    slot.pictures = std::make_unique<SaveIconPictures>(*archive, item.saveIcon);
    slot.layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(names.slotLayout), *slot.pictures);
    slot.focusInSlot = slot.layout->BindAnimation(std::format("{}_SaveDataFoucusIn", names.slotLayout), "G_Data", Layout::PlaybackMode::PlayOnce);
    slot.focusOutSlot = slot.layout->BindAnimation(std::format("{}_SaveDataFoucusOut", names.slotLayout), "G_Data", Layout::PlaybackMode::PlayOnce);
    std::size_t appearSlot = slot.layout->BindAnimation(std::format("{}_SaveDataIn", names.slotLayout), "G_Data", Layout::PlaybackMode::PlayOnce);
    slot.layout->StartAnimation(appearSlot);

    std::string_view button = SaveSlotButton;
    if (kind == DataKind::Channels)
    {
        slot.layout->PaneNamed(channelPanes.shownFrame).isVisible = true;
        slot.layout->PaneNamed(channelPanes.hiddenFrame).isVisible = false;
        button = channelPanes.button;
    }
    slot.buttons = std::make_unique<ButtonController>(*slot.layout, services.display, std::vector<std::string>{std::string(button)});
    slot.balloon = std::make_unique<Balloon>(services.display, *balloonArchive, StepsBeforeABalloonAppears);
    slot.balloon->SetText(item.firstLine);

    bool hasChannelIcon = kind == DataKind::Channels && !item.isDamaged && !item.channelIcon.empty();
    if (hasChannelIcon)
        slot.icon = MakeIcon(item.channelIcon);

    slots.push_back(std::move(slot));
}

// A channel whose icon cannot be read shows its empty frame.
std::unique_ptr<ChannelIconLayout> DataEditorScreen::MakeIcon(Formats::ByteView iconFile)
{
    try
    {
        return std::make_unique<ChannelIconLayout>(services.display.Device(), iconFile);
    }
    catch (const Formats::FormatError&)
    {
        return nullptr;
    }
    catch (const Layout::LayoutError&)
    {
        return nullptr;
    }
}

// The icon sits in its place of a layout, as big as that place.
void DataEditorScreen::PlaceIcon(ChannelIconLayout& icon, MenuLayout& layout, std::string_view placePane)
{
    const Formats::Pane& iconPlace = layout.PaneNamed(placePane);
    Formats::Vector2 stretch = services.display.LayoutStretch();
    std::optional<Formats::Vector2> point = layout.PointOfPaneInPicture(placePane, {0, 0});
    if (point)
        icon.Place(*point, {iconPlace.scale.x * stretch.x, iconPlace.scale.y * stretch.y}, {iconPlace.width, iconPlace.height});

    icon.Calculate();
}

// Every slot sits on its place of the middle page of the list, which moves when the pages turn. As in the original (0x813A0C34,
// 0x813A7568), the place in the list becomes the place of the slot below its root pane, which the wide screen
// stretches once more: on a 16:9 screen the slots spread wider than the places of the list, which keep their 4:3 shape.
void DataEditorScreen::PlaceSlots()
{
    bool isWide = services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
    const ChannelSlotPanes& channelPanes = isWide ? WideChannelSlot : StandardChannelSlot;

    if (windowIcon)
        PlaceIcon(*windowIcon, *windowLayout, isWide ? WideChannelWindow.iconPlace : StandardChannelWindow.iconPlace);

    for (std::size_t slotIndex = 0; slotIndex < slots.size(); ++slotIndex)
    {
        Slot& slot = slots[slotIndex];
        std::optional<Formats::Vector2> point = listLayout->PointOfPaneInPicture(SlotPaneName(slotIndex), {0, 0});
        if (!point)
            continue;

        slot.layout->PaneNamed(NamesOf(kind).slotAnchor).translation = {point->x, point->y, 0};
        slot.layout->Calculate();

        if (slot.icon)
            PlaceIcon(*slot.icon, *slot.layout, channelPanes.iconPlace);

        PlaceBalloon(*slot.balloon, slotIndex);
    }
}

void DataEditorScreen::PlaceBalloon(Balloon& balloon, std::size_t slotIndex)
{
    std::optional<Formats::Vector2> place = listLayout->PointOfPaneInPicture(SlotPaneName(slotIndex), {0, 0});
    if (!place)
        return;

    bool isWide = services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
    float margin = isWide ? BalloonMarginOnAWideScreen : BalloonMarginOnANarrowScreen;
    balloon.PlaceAt({place->x, place->y - BalloonDepthBelowItsPlace}, margin);

    bool wasShown = balloon.IsShown();
    balloon.Calculate();
    if (!wasShown && balloon.IsShown())
        services.sounds.Request(BalloonSound);
}

// The number of free blocks: of the user area of the NAND, or of the SD card (0x813A100C, 0x813A1234).
void DataEditorScreen::ShowFreeBlocks()
{
    bool canShow = services.dataManagement != nullptr && (place == DataPlace::Wii || isCardReady);
    listLayout->PaneNamed("N_Capa_00").isVisible = canShow;
    if (!canShow)
        return;

    std::uint64_t freeBlocks = 0;
    if (place == DataPlace::Wii)
    {
        freeBlocks = FreeUserBlocks(MeasureNandRoom(services.dataManagement->fileSystem));
    }
    else
    {
        Ios::Result<std::uint64_t> freeBytes = services.dataManagement->sdCard.GetFreeBytes();
        freeBlocks = freeBytes.Succeeded() ? std::min(freeBytes.Value() / BlockBytes, MostCardBlocksShown) : 0;
    }

    std::u16string text = services.messages.Find(BlocksOpenMessage);
    text += NumberText(freeBlocks);
    listLayout->SetText("T_Capa_00", text);
}

// The list of the SD card says when there is no card or the card cannot be used.
void DataEditorScreen::ShowCardProblem()
{
    bool hasProblem = place == DataPlace::SdCard && !isCardReady;
    if (hasProblem == isErrorShown)
        return;

    isErrorShown = hasProblem;
    if (!hasProblem)
    {
        listLayout->StartAnimation(errorOutSlot);
        return;
    }

    bool isCardInSlot = services.dataManagement != nullptr && services.dataManagement->sdCard.GetStatus().isInserted;
    listLayout->SetText("T_Error_00", services.messages.Find(isCardInSlot ? UnusableCardMessage : NoCardMessage));
    listLayout->PaneNamed("N_Error").isVisible = true;
    listLayout->StartAnimation(errorInSlot);
}

void DataEditorScreen::ShowArrows()
{
    listLayout->PaneNamed("N_ArwR").isVisible = items.size() > firstItemOfPage + SlotsPerPage;
    listLayout->PaneNamed("N_ArwL").isVisible = firstItemOfPage > 0;
}

void DataEditorScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main)
        return;

    listLayout->Draw();
    if (phase != Phase::TurningPage)
    {
        for (Slot& slot : slots)
        {
            slot.layout->Draw();
            if (slot.icon)
                slot.icon->Draw();
        }

        for (Slot& slot : slots)
            slot.balloon->Draw();
    }

    if (!openItem)
        return;

    windowLayout->Draw();
    if (windowIcon)
        windowIcon->Draw();
}

// The list comes in first, then the two buttons of the Wii and the SD card (0x813A2118).
bool DataEditorScreen::IsReady()
{
    listLayout->Calculate();
    PlaceSlots();
    if (listLayout->IsAnimationPlaying(dataInSlot))
        return false;

    if (!hasSelectionAppeared)
    {
        hasSelectionAppeared = true;
        listLayout->StartAnimation(selectInSlot);
    }
    return !listLayout->IsAnimationPlaying(selectInSlot);
}

bool DataEditorScreen::RunMain()
{
    bool isLeaving = false;
    switch (phase)
    {
    case Phase::List:
        isLeaving = WasBackPressed();
        if (!isLeaving)
            RunList();
        break;
    case Phase::TurningPage:
        FinishTurningPage();
        break;
    case Phase::Window:
        RunWindow();
        break;
    case Phase::Asking:
        ReadAnswer();
        break;
    case Phase::Working:
        FinishWork();
        break;
    case Phase::ShowingOutcome:
        ReadOutcomeDialog();
        break;
    case Phase::ClosingWindow:
        if (!windowLayout->IsAnimationPlaying(windowAnimations.seenOut))
        {
            openItem.reset();
            windowIcon.reset();
            phase = Phase::List;
        }
        break;
    }

    if (isLeaving)
    {
        listLayout->StartAnimation(dataOutSlot);
        if (isErrorShown)
            listLayout->StartAnimation(errorOutSlot);
        slots.clear();
    }

    listLayout->Calculate();
    windowLayout->Calculate();
    PlaceSlots();
    return isLeaving;
}

bool DataEditorScreen::RunClosing()
{
    listLayout->Calculate();
    return !listLayout->IsAnimationPlaying(dataOutSlot);
}

void DataEditorScreen::RunList()
{
    HandleListButtons(listButtons->Poll(services.input, true));
    HandleSlotButtons();

    if (services.input.AnyPlayerPressed(NextPageButtons))
        TurnPage(1);
    else if (services.input.AnyPlayerPressed(PreviousPageButtons))
        TurnPage(-1);

    // A card that comes or goes changes the list of the card at once (0x813A0DC4).
    bool isCardReadyNow = services.dataManagement != nullptr && ReadCardState(*services.dataManagement) == CardState::Ready;
    if (place == DataPlace::SdCard && isCardReadyNow != isCardReady)
        ShowPlace(DataPlace::SdCard);
}

void DataEditorScreen::HandleListButtons(const std::vector<ButtonEvent>& events)
{
    for (const ButtonEvent& event : events)
    {
        if (event.kind == ButtonEventKind::FocusGained)
            services.sounds.Request(SoundName::ButtonTargeted);

        if (event.kind != ButtonEventKind::Pressed)
            continue;

        if (event.buttonIndex == WiiButton && place != DataPlace::Wii)
        {
            services.sounds.Request(PushSound);
            listLayout->StartAnimation(toWiiFlashSlot);
            ShowPlace(DataPlace::Wii);
        }
        else if (event.buttonIndex == SdCardButton && place != DataPlace::SdCard)
        {
            services.sounds.Request(PushSound);
            listLayout->StartAnimation(toCardFlashSlot);
            ShowPlace(DataPlace::SdCard);
        }
        else if (event.buttonIndex == NextPageButton)
        {
            TurnPage(1);
        }
        else if (event.buttonIndex == PreviousPageButton)
        {
            TurnPage(-1);
        }
    }
}

void DataEditorScreen::HandleSlotButtons()
{
    std::optional<std::size_t> pressedItem;
    for (Slot& slot : slots)
    {
        for (const ButtonEvent& event : slot.buttons->Poll(services.input, true))
        {
            if (event.kind == ButtonEventKind::FocusGained)
            {
                slot.layout->StartAnimation(slot.focusInSlot);
                slot.balloon->PointerArrived();
            }
            else if (event.kind == ButtonEventKind::FocusLost)
            {
                slot.layout->StartAnimation(slot.focusOutSlot);
                slot.balloon->PointerLeft();
            }
            else
            {
                pressedItem = slot.itemIndex;
            }
        }
    }

    if (pressedItem)
        OpenWindow(*pressedItem);
}

// The other pages are empty frames that slide in; the entries of the new page appear when the slide is over (0x813A2268, 0x813A2334).
void DataEditorScreen::TurnPage(int direction)
{
    bool canGoForward = direction > 0 && items.size() > firstItemOfPage + SlotsPerPage;
    bool canGoBack = direction < 0 && firstItemOfPage > 0;
    if (!canGoForward && !canGoBack)
        return;

    services.sounds.Request(PageSound);
    listLayout->StartAnimation(canGoForward ? pageLeftSlot : pageRightSlot);
    firstItemOfPage = canGoForward ? firstItemOfPage + SlotsPerPage : firstItemOfPage - SlotsPerPage;
    phase = Phase::TurningPage;
}

void DataEditorScreen::FinishTurningPage()
{
    if (listLayout->IsAnimationPlaying(pageLeftSlot) || listLayout->IsAnimationPlaying(pageRightSlot))
        return;

    listLayout->ShowAnimationFrame(pageLeftSlot, 0);
    ShowPage();
    phase = Phase::List;
}

bool DataEditorScreen::WasBackPressed()
{
    auto* bar = static_cast<WiiOptionsBarScreen*>(services.screens.FindScreen(ScreenId::WiiOptionsBar));
    return bar != nullptr && bar->WasBackButtonPressed();
}

// The window grows out of the slot and shows the name and the size of the entry (0x813A5EB8, 0x813C8BA0).
void DataEditorScreen::OpenWindow(std::size_t itemIndex)
{
    for (Slot& slot : slots)
        slot.balloon->PointerLeft();

    const DataItem& item = items[itemIndex];
    BuildWindow(item.savePicture);
    windowIcon.reset();
    if (kind == DataKind::Channels && !item.isDamaged && !item.channelIcon.empty())
        windowIcon = MakeIcon(item.channelIcon);

    std::u16string firstLine = item.isDamaged ? std::u16string(DamagedName) : item.firstLine;
    std::u16string secondLine = item.isDamaged ? std::u16string() : item.secondLine;

    if (kind == DataKind::Channels)
    {
        bool isWide = services.display.GetScreenShape() == Platform::ScreenShape::SixteenByNine;
        const ChannelWindowPanes& panes = isWide ? WideChannelWindow : StandardChannelWindow;
        windowLayout->PaneNamed(panes.shownPart).isVisible = true;
        windowLayout->PaneNamed(panes.hiddenPart).isVisible = false;
        windowLayout->SetText(panes.firstLine, firstLine);
        windowLayout->SetText(panes.secondLine, secondLine);
        windowLayout->SetText(panes.blocks, NumberText(item.blocks));
    }
    else
    {
        windowLayout->SetText("T_Title_00", firstLine);
        windowLayout->SetText("T_Title_01", secondLine);
        windowLayout->SetText("T_Block_00", NumberText(item.blocks));
    }

    bool isMoveOff = item.mayNotBeCopied;
    windowLayout->PaneNamed("BaseMove_off").isVisible = isMoveOff;
    windowLayout->PaneNamed("BaseMove_off_00").isVisible = isMoveOff;

    openItem = itemIndex;
    windowButtons->ForgetPointers();
    windowLayout->StartAnimation(windowAnimations.seenIn);
    services.sounds.Request(SoundName::Decide);
    phase = Phase::Window;
}

void DataEditorScreen::RunWindow()
{
    if (WasBackPressed())
    {
        CloseWindow();
        return;
    }

    if (windowLayout->IsAnimationPlaying(windowAnimations.seenIn))
        return;

    for (const ButtonEvent& event : windowButtons->Poll(services.input, true))
    {
        if (event.kind == ButtonEventKind::FocusGained)
        {
            windowLayout->StartAnimation(windowAnimations.focusIn[event.buttonIndex]);
            services.sounds.Request(SoundName::ButtonTargeted);
        }
        else if (event.kind == ButtonEventKind::FocusLost)
        {
            windowLayout->StartAnimation(windowAnimations.focusOut[event.buttonIndex]);
        }
        else
        {
            windowLayout->StartAnimation(windowAnimations.flash[event.buttonIndex]);
            PressAction(WindowActions[event.buttonIndex]);
            return;
        }
    }
}

// The menu refuses the button at once, or asks whether to go on.
void DataEditorScreen::PressAction(DataAction action)
{
    services.sounds.Request(SoundName::Decide);
    chosenAction = action;

    if (services.dataManagement == nullptr)
        return;

    std::optional<std::uint16_t> refusal = RefusalMessage(*services.dataManagement, kind, place, action, items[*openItem]);
    if (refusal)
    {
        chosenAction.reset();
        ShowMessage(*refusal);
        return;
    }

    DialogRequest request;
    request.layout = DialogLayout::TwoButtons;
    request.message = services.messages.Find(QuestionMessage(place, action));
    request.buttonAText = services.messages.Find(YesMessage);
    request.buttonBText = services.messages.Find(NoMessage);
    request.cancelButton = DialogAnswer::ButtonB;
    services.dialogs.Open(request);
    phase = Phase::Asking;
}

void DataEditorScreen::ReadAnswer()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return;

    if (answer == DialogAnswer::ButtonA)
        StartWork();
    else
        phase = Phase::Window;
}

void DataEditorScreen::StartWork()
{
    DialogRequest request;
    request.layout = DialogLayout::Message;
    request.message = services.messages.Find(ProgressMessage(place, *chosenAction));
    request.showsWaitIcon = true;
    services.dialogs.Open(request);

    work = std::async(std::launch::async, RunDataAction, std::ref(*services.dataManagement), kind, place, *chosenAction, items[*openItem]);
    phase = Phase::Working;
}

// When the work is done the message of the work closes, and the message of how it ended opens.
void DataEditorScreen::FinishWork()
{
    bool isDone = work.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    if (!isDone)
        return;

    if (services.dialogs.IsOpen())
    {
        services.dialogs.Close();
        return;
    }
    services.dialogs.TakeAnswer();

    DataOutcome outcome = work.get();
    ShowMessage(OutcomeMessage(kind, place, *chosenAction, outcome));
}

void DataEditorScreen::ShowMessage(std::uint16_t messageNumber)
{
    DialogRequest request;
    request.layout = DialogLayout::OneButton;
    request.message = services.messages.Find(messageNumber);
    request.buttonBText = services.messages.Find(OkMessage);
    services.dialogs.Open(request);
    phase = Phase::ShowingOutcome;
}

// After the work the window closes and the list shows what is there now. After a refusal the window stays.
void DataEditorScreen::ReadOutcomeDialog()
{
    if (!services.dialogs.TakeAnswer())
        return;

    if (!chosenAction)
    {
        phase = Phase::Window;
        return;
    }

    chosenAction.reset();
    std::size_t firstItem = firstItemOfPage;
    ReadItems();
    firstItemOfPage = firstItem >= items.size() && firstItem >= SlotsPerPage ? firstItem - SlotsPerPage : firstItem;
    ShowPage();
    ShowFreeBlocks();
    CloseWindow();
}

void DataEditorScreen::CloseWindow()
{
    windowLayout->StartAnimation(windowAnimations.seenOut);
    services.sounds.Request(SoundName::Cancel);
    phase = Phase::ClosingWindow;
}

}
