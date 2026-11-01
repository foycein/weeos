#include "SystemMenu/WiiOptionsChoicesScreen.h"

#include "SystemMenu/Fader.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoundRequests.h"

#include "Text/MessageCatalog.h"

#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "setupSel";
constexpr std::string_view LayoutName = "it_ObjSetUp_a";

// The texts of the buttons are messages of the menu.
constexpr std::uint16_t DataManagementMessage = 253;
constexpr std::uint16_t SaveDataMessage = 254;
constexpr std::uint16_t ChannelsMessage = 255;
constexpr std::uint16_t WiiMessage = 256;
constexpr std::uint16_t WiiSettingsMessage = 316;

struct TextOfButton
{
    std::string_view textPane;
    std::uint16_t message;
};

constexpr std::array<TextOfButton, 9> TextsOfButtons = {{
    {"T_Datamanage0_00", DataManagementMessage},
    {"T_DataManage_01", DataManagementMessage},
    {"T_Setting_00", WiiSettingsMessage},
    {"T_SaveData_00", SaveDataMessage},
    {"T_SaveData_01", SaveDataMessage},
    {"T_Channel_00", ChannelsMessage},
    {"T_Channel_01", ChannelsMessage},
    {"T_Wii_00", WiiMessage},
    {"T_Wii_01", WiiMessage},
}};

constexpr int OptionsPageArgument = 0;

}

WiiOptionsChoicesScreen::WiiOptionsChoicesScreen(MenuServices& menuServices, int screenArgument)
    : Screen(ScreenId::WiiOptionsChoices)
    , services(menuServices)
    , argument(screenArgument)
{
}

const WiiOptionsChoicesScreen::ChoiceNames& WiiOptionsChoicesScreen::NamesOf(Choice choice)
{
    static const std::array<ChoiceNames, ChoiceCount> names = {{
        {Page::Options, "B_DataManage_00", "G_DataManage_00", "G_DataManage_01", "SetUp"},
        {Page::Options, "B_Setting_00", "G_Setting_00", "G_Setting_01", "SetUp"},
        {Page::DataManagement, "B_SaveData_00", "G_SaveData_00", "G_SaveData_01", "DataChannel"},
        {Page::DataManagement, "B_Channel_00", "G_Channel_00", "G_Channel_01", "DataChannel"},
        {Page::SaveData, "B_Wii_00", "G_Wii_00", "G_Wii_01", "SaveData"},
    }};
    return names[static_cast<std::size_t>(choice)];
}

const WiiOptionsChoicesScreen::ButtonAnimations& WiiOptionsChoicesScreen::AnimationsOf(Choice choice) const
{
    return animations[static_cast<std::size_t>(choice)];
}

std::optional<WiiOptionsChoicesScreen::Choice> WiiOptionsChoicesScreen::OtherChoiceOfPage(Choice choice)
{
    switch (choice)
    {
    case Choice::DataManagement:
        return Choice::WiiSettings;
    case Choice::WiiSettings:
        return Choice::DataManagement;
    case Choice::SaveData:
        return Choice::Channels;
    case Choice::Channels:
        return Choice::SaveData;
    case Choice::Wii:
        return std::nullopt;
    }
    return std::nullopt;
}

void WiiOptionsChoicesScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void WiiOptionsChoicesScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));
    buttons = std::make_unique<ButtonController>(*layout, services.display);

    for (std::size_t index = 0; index < ChoiceCount; ++index)
    {
        Choice choice = static_cast<Choice>(index);
        BindAnimationsOfButton(choice);
        buttons->AddButton(std::string(NamesOf(choice).buttonPane));
    }

    SetTextsOfButtons();
    EnableButtonsOfPage(argument == OptionsPageArgument ? Page::Options : Page::DataManagement);
    layout->Calculate();
}

void WiiOptionsChoicesScreen::BindAnimationsOfButton(Choice choice)
{
    const ChoiceNames& names = NamesOf(choice);
    ButtonAnimations& slots = animations[static_cast<std::size_t>(choice)];

    slots.appear = BindAnimation(names, "In", names.buttonGroup);
    slots.focusIn = BindAnimation(names, "FoucusIn", names.buttonGroup);
    slots.focusOut = BindAnimation(names, "FoucusOut", names.buttonGroup);
    slots.flashOfButton = BindAnimation(names, "FoucusFlash", names.buttonGroup);
    slots.flashOfShade = BindAnimation(names, "FoucusFlash", names.shadeGroup);
    slots.disappear = BindAnimation(names, "Out", names.buttonGroup);
    slots.comeBack = BindAnimation(names, "Back", names.buttonGroup);
}

std::size_t WiiOptionsChoicesScreen::BindAnimation(const ChoiceNames& names, std::string_view ending, std::string_view group)
{
    std::string animationName = std::format("it_ObjSetUp_a_{}{}", names.animationStem, ending);
    std::size_t slot = layout->BindAnimation(animationName, group, Layout::PlaybackMode::PlayOnce);
    allSlots.push_back(slot);
    return slot;
}

void WiiOptionsChoicesScreen::SetTextsOfButtons()
{
    for (const TextOfButton& text : TextsOfButtons)
        layout->SetText(text.textPane, services.messages.Find(text.message));
}

void WiiOptionsChoicesScreen::EnableButtonsOfPage(Page page)
{
    for (std::size_t index = 0; index < ChoiceCount; ++index)
    {
        const ChoiceNames& names = NamesOf(static_cast<Choice>(index));
        buttons->SetEnabled(names.buttonPane, names.page == page);
    }
}

void WiiOptionsChoicesScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

bool WiiOptionsChoicesScreen::UpdatesWithChildren() const
{
    return true;
}

bool WiiOptionsChoicesScreen::DrawsWithChildren() const
{
    return true;
}

// The state machine of the original (0x81408294), one step of it.
bool WiiOptionsChoicesScreen::RunMain()
{
    bool hasLeft = false;

    switch (state)
    {
    case State::WaitingForTheBar:
        StartFirstPage();
        break;
    case State::OptionsAppearing:
        WaitForAnimations(State::OptionsReady);
        break;
    case State::DataManagementAppearing:
        WaitForAnimations(State::DataManagementReady);
        break;
    case State::SaveDataAppearing:
        WaitForAnimations(State::SaveDataReady);
        break;
    case State::OptionsReady:
    case State::DataManagementReady:
    case State::SaveDataReady:
        RunPageReady();
        break;
    case State::OptionsLeaving:
        FinishLeavingOptionsPage();
        break;
    case State::DataManagementLeaving:
        FinishLeavingDataManagementPage();
        break;
    case State::SaveDataLeaving:
        FinishLeavingSaveDataPage();
        break;
    case State::ReturningToOptions:
        WaitForAnimations(State::OptionsReady);
        break;
    case State::ReturningToDataManagement:
        WaitForAnimations(State::DataManagementReady);
        break;
    case State::BackButtonPressed:
        ReactToBackButton();
        break;
    case State::FadingOut:
        hasLeft = services.fader.GetStatus() == FaderStatus::Covered;
        break;
    case State::WaitingForEditor:
        WaitForEditorToClose();
        break;
    case State::EditorEnded:
        FinishReturningFromEditor();
        break;
    }

    layout->Calculate();
    return hasLeft;
}

void WiiOptionsChoicesScreen::MoveToState(State newState)
{
    previousState = state;
    state = newState;
}

void WiiOptionsChoicesScreen::StartAnimation(std::size_t slot)
{
    layout->StartAnimation(slot);
}

bool WiiOptionsChoicesScreen::AreAnimationsFinished() const
{
    for (std::size_t slot : allSlots)
    {
        if (layout->IsAnimationPlaying(slot))
            return false;
    }
    return true;
}

void WiiOptionsChoicesScreen::WaitForAnimations(State nextState)
{
    if (AreAnimationsFinished())
        MoveToState(nextState);
}

WiiOptionsBarScreen* WiiOptionsChoicesScreen::FindBar() const
{
    return static_cast<WiiOptionsBarScreen*>(services.screens.FindScreen(ScreenId::WiiOptionsBar));
}

// The buttons come in as soon as the picture has faded in and the bar has slid in.
void WiiOptionsChoicesScreen::StartFirstPage()
{
    WiiOptionsBarScreen* bar = FindBar();
    bool hasBarStarted = bar != nullptr && bar->GetState() != ScreenState::WaitingToStart;
    if (services.fader.GetStatus() != FaderStatus::Clear || !hasBarStarted)
        return;

    if (argument == OptionsPageArgument)
    {
        StartAnimation(AnimationsOf(Choice::DataManagement).appear);
        StartAnimation(AnimationsOf(Choice::WiiSettings).appear);
        MoveToState(State::OptionsAppearing);
        return;
    }

    // The Data Management page opens with the button of Data Management already pressed.
    layout->ShowLastFrameOfAnimation(AnimationsOf(Choice::DataManagement).flashOfButton);
    StartAnimation(AnimationsOf(Choice::SaveData).appear);
    StartAnimation(AnimationsOf(Choice::Channels).appear);
    MoveToState(State::DataManagementAppearing);
}

void WiiOptionsChoicesScreen::RunPageReady()
{
    State stateBeforePolling = state;
    if (services.fader.GetStatus() == FaderStatus::Clear)
        HandleButtonEvents(buttons->Poll(services.input));

    if (state != stateBeforePolling)
        return;

    WiiOptionsBarScreen* bar = FindBar();
    if (bar != nullptr && bar->WasBackButtonPressed())
        MoveToState(State::BackButtonPressed);
}

void WiiOptionsChoicesScreen::HandleButtonEvents(const std::vector<ButtonEvent>& events)
{
    for (const ButtonEvent& event : events)
    {
        std::optional<Choice> choice = ChoiceOfPane(event.buttonName);
        if (!choice || !IsButtonOnCurrentPage(*choice))
            continue;

        const ButtonAnimations& choiceAnimations = AnimationsOf(*choice);
        if (event.kind == ButtonEventKind::FocusGained)
        {
            services.sounds.Request(SoundName::ButtonTargeted);
            StartAnimation(choiceAnimations.focusIn);
        }
        else if (event.kind == ButtonEventKind::FocusLost)
        {
            StartAnimation(choiceAnimations.focusOut);
        }
        else
        {
            PressButton(*choice);
        }
    }
}

std::optional<WiiOptionsChoicesScreen::Choice> WiiOptionsChoicesScreen::ChoiceOfPane(std::string_view buttonPane)
{
    for (std::size_t index = 0; index < ChoiceCount; ++index)
    {
        if (NamesOf(static_cast<Choice>(index)).buttonPane == buttonPane)
            return static_cast<Choice>(index);
    }
    return std::nullopt;
}

bool WiiOptionsChoicesScreen::IsButtonOnCurrentPage(Choice choice) const
{
    Page page = NamesOf(choice).page;
    return (page == Page::Options && state == State::OptionsReady) || (page == Page::DataManagement && state == State::DataManagementReady)
        || (page == Page::SaveData && state == State::SaveDataReady);
}

// The pressed button flashes, the other button of its page goes away, and the page, the screen or the editor that the button leads to
// is asked for at once, so that it can load while the animations play.
void WiiOptionsChoicesScreen::PressButton(Choice choice)
{
    pressedChoice = choice;

    switch (choice)
    {
    case Choice::DataManagement:
        MoveToState(State::OptionsLeaving);
        break;
    case Choice::WiiSettings:
        destination = ScreenId::WiiSettings;
        GoToRoot(ScreenId::WiiSettings, 0);
        MoveToState(State::OptionsLeaving);
        break;
    case Choice::SaveData:
        MoveToState(State::DataManagementLeaving);
        break;
    case Choice::Channels:
        OpenEditor(ScreenId::ChannelDataEditor);
        MoveToState(State::DataManagementLeaving);
        break;
    case Choice::Wii:
        OpenEditor(ScreenId::WiiSaveDataEditor);
        MoveToState(State::SaveDataLeaving);
        break;
    }

    const ButtonAnimations& pressed = AnimationsOf(choice);
    StartAnimation(pressed.flashOfButton);
    StartAnimation(pressed.flashOfShade);

    std::optional<Choice> otherChoice = OtherChoiceOfPage(choice);
    if (otherChoice)
        StartAnimation(AnimationsOf(*otherChoice).disappear);

    services.sounds.Request(SoundName::Decide);
}

void WiiOptionsChoicesScreen::OpenEditor(ScreenId editor)
{
    destination = editor;
    services.screens.OpenChild(editor, Id(), 0);
    FindBar()->HideWiiIcon();
}

void WiiOptionsChoicesScreen::FinishLeavingOptionsPage()
{
    if (!AreAnimationsFinished())
        return;

    if (destination)
    {
        services.fader.StartFadeOut();
        MoveToState(State::FadingOut);
        return;
    }

    ShowPage(Page::DataManagement);
    MoveToState(State::DataManagementAppearing);
}

void WiiOptionsChoicesScreen::FinishLeavingDataManagementPage()
{
    if (!AreAnimationsFinished())
        return;

    if (destination)
    {
        buttons->ForgetPointers();
        MoveToState(State::WaitingForEditor);
        return;
    }

    ShowPage(Page::SaveData);
    MoveToState(State::SaveDataAppearing);
}

void WiiOptionsChoicesScreen::FinishLeavingSaveDataPage()
{
    if (!AreAnimationsFinished())
        return;

    buttons->ForgetPointers();
    MoveToState(State::WaitingForEditor);
}

// The buttons of the page can be pressed from now on, and appear.
void WiiOptionsChoicesScreen::ShowPage(Page page)
{
    EnableButtonsOfPage(page);
    buttons->ForgetPointers();

    for (std::size_t index = 0; index < ChoiceCount; ++index)
    {
        if (NamesOf(static_cast<Choice>(index)).page == page)
            StartAnimation(animations[index].appear);
    }
}

void WiiOptionsChoicesScreen::ReactToBackButton()
{
    WiiOptionsBarScreen* bar = FindBar();
    if (bar != nullptr && bar->IsBackButtonFlashing())
        return;

    if (previousState == State::OptionsReady)
        LeaveWiiOptions();
    else if (previousState == State::DataManagementReady)
        ReturnToPage(Page::Options);
    else if (previousState == State::SaveDataReady)
        ReturnToPage(Page::DataManagement);
}

void WiiOptionsChoicesScreen::LeaveWiiOptions()
{
    services.fader.StartFadeOut();
    GoToRoot(ScreenId::MainFrame, 0);
    MoveToState(State::FadingOut);
}

// The button that led to the page they come from comes back, the other button of that page appears again, and the buttons of the page they leave go away.
void WiiOptionsChoicesScreen::ReturnToPage(Page page)
{
    EnableButtonsOfPage(page);

    if (page == Page::Options)
    {
        StartAnimation(AnimationsOf(Choice::DataManagement).comeBack);
        StartAnimation(AnimationsOf(Choice::WiiSettings).appear);
        StartAnimation(AnimationsOf(Choice::SaveData).disappear);
        StartAnimation(AnimationsOf(Choice::Channels).disappear);
        MoveToState(State::ReturningToOptions);
        return;
    }

    StartAnimation(AnimationsOf(Choice::SaveData).comeBack);
    StartAnimation(AnimationsOf(Choice::Channels).appear);
    StartAnimation(AnimationsOf(Choice::Wii).disappear);
    MoveToState(State::ReturningToDataManagement);
}

// The editor has opened and closed again, or was never built. The button that opened it comes back.
void WiiOptionsChoicesScreen::WaitForEditorToClose()
{
    if (HasChildren() || services.screens.IsLoadingScreen())
        return;

    if (pressedChoice == Choice::Channels)
    {
        StartAnimation(AnimationsOf(Choice::Channels).comeBack);
        StartAnimation(AnimationsOf(Choice::SaveData).appear);
    }
    else
    {
        StartAnimation(AnimationsOf(Choice::Wii).comeBack);
    }
    MoveToState(State::EditorEnded);
}

void WiiOptionsChoicesScreen::FinishReturningFromEditor()
{
    if (!AreAnimationsFinished())
        return;

    FindBar()->ShowWiiIcon();
    MoveToState(pressedChoice == Choice::Channels ? State::DataManagementReady : State::SaveDataReady);
    destination.reset();
}

}
