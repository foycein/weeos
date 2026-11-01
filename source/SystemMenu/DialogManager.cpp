#include "SystemMenu/DialogManager.h"

#include <algorithm>
#include <format>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "dlgWdw";

// Every window has these two animations first.
constexpr std::size_t DialogInSlot = 0;
constexpr std::size_t DialogOutSlot = 1;

// The message window then has the animations of its turning icon and of its progress bar.
constexpr std::size_t WaitSlot = 2;
constexpr std::size_t ProgressSlot = 3;

// The other windows have three animations for each button: the pointer comes onto it, the pointer goes away, and it is pressed.
constexpr std::size_t FirstButtonSlot = 2;
constexpr std::size_t SlotsPerButton = 3;

constexpr int ProgressPerStep = 2;
constexpr int FullProgress = 100;

struct WindowDescription
{
    std::string_view layoutName;
    std::size_t buttonCount;
};

constexpr std::array<WindowDescription, 5> WindowDescriptions = {{
    {"my_DialogWindow_a0", 0},
    {"my_DialogWindow_a1", 1},
    {"my_DialogWindow_a2", 2},
    {"my_DialogWindow_a3", 3},
    {"my_DialogWindow_b", 2},
}};

// The buttons are numbered in this order: the first button of every window is B, the second is A.
constexpr std::array<std::string_view, 3> ButtonLetters = {"B", "A", "C"};
constexpr std::array<DialogAnswer, 3> ButtonAnswers = {DialogAnswer::ButtonB, DialogAnswer::ButtonA, DialogAnswer::ButtonC};

std::size_t FocusOnSlot(std::size_t buttonIndex)
{
    return FirstButtonSlot + buttonIndex * SlotsPerButton;
}

std::size_t FocusOffSlot(std::size_t buttonIndex)
{
    return FocusOnSlot(buttonIndex) + 1;
}

std::size_t SelectSlot(std::size_t buttonIndex)
{
    return FocusOnSlot(buttonIndex) + 2;
}

const std::u16string& TextOfButton(const DialogRequest& request, std::size_t buttonIndex)
{
    if (buttonIndex == 0)
        return request.buttonBText;

    if (buttonIndex == 1)
        return request.buttonAText;

    return request.buttonCText;
}

}

DialogManager::DialogManager(Display& menuDisplay, const MenuFiles& files)
    : display(menuDisplay)
    , archive(files.OpenLayoutBundle(BundleName))
{
    for (std::size_t kind = 0; kind < windows.size(); ++kind)
        MakeWindow(static_cast<DialogLayout>(kind));
}

// The animations of a window are bound in the order of the slot numbers above.
void DialogManager::MakeWindow(DialogLayout kind)
{
    const WindowDescription& description = WindowDescriptions[static_cast<std::size_t>(kind)];
    std::string layoutName(description.layoutName);

    DialogWindow& window = windows[static_cast<std::size_t>(kind)];
    window.layout = std::make_unique<MenuLayout>(display, *archive, layoutName);
    window.buttonCount = description.buttonCount;

    window.layout->BindAnimation(layoutName + "_DialogIn", "G_InOut");
    window.layout->BindAnimation(layoutName + "_DialogOut", "G_InOut");

    if (kind == DialogLayout::Message)
    {
        window.layout->BindAnimation(layoutName + "_Wait", "G_Wait");
        window.layout->BindAnimation(layoutName + "_Progres", "G_Prog");
        return;
    }

    std::vector<std::string> buttonPaneNames;
    for (std::size_t buttonIndex = 0; buttonIndex < description.buttonCount; ++buttonIndex)
    {
        std::string letter(ButtonLetters[buttonIndex]);
        window.layout->BindAnimation(layoutName + "_FocusBtn_on", "G_FocusBtn" + letter);
        window.layout->BindAnimation(layoutName + "_FocusBtn_off", "G_FocusBtn" + letter);
        window.layout->BindAnimation(layoutName + "_SelectBtn_Ac", "G_SelectBtn" + letter);
        buttonPaneNames.push_back("B_Btn" + letter);
    }
    window.buttons = std::make_unique<ButtonGroup>(*window.layout, display, buttonPaneNames);
}

bool DialogManager::IsOpen() const
{
    return state != State::Closed;
}

bool DialogManager::Open(const DialogRequest& request)
{
    if (state != State::Closed)
        return false;

    openWindow = &windows[static_cast<std::size_t>(request.layout)];
    answer.reset();
    closeRequested = false;
    stepsBeforeClosing = request.stepsBeforeClosing;
    stepsWaited = 0;
    showsProgress = request.showsProgress;
    targetPercentage = 0;
    shownPercentage = 0;

    ShowTexts(request);
    if (request.layout == DialogLayout::Message)
        ShowMessageExtras(request);

    if (openWindow->buttons)
        openWindow->buttons->Reset();

    openWindow->layout->StartAnimation(DialogInSlot);
    openWindow->layout->Calculate();
    state = State::Opening;
    return true;
}

void DialogManager::ShowTexts(const DialogRequest& request)
{
    openWindow->layout->SetText("T_Dialog", request.message);

    for (std::size_t buttonIndex = 0; buttonIndex < openWindow->buttonCount; ++buttonIndex)
        openWindow->layout->SetText(std::format("T_Btn{}", ButtonLetters[buttonIndex]), TextOfButton(request, buttonIndex));
}

void DialogManager::ShowMessageExtras(const DialogRequest& request)
{
    MenuLayout& layout = *openWindow->layout;
    layout.PaneNamed("Wait_00").isVisible = request.showsWaitIcon;
    layout.PaneNamed("N_Prog").isVisible = request.showsProgress;
    layout.PaneNamed("Shade").isVisible = !request.showsProgress;

    if (request.showsWaitIcon)
        layout.StartAnimation(WaitSlot);

    if (request.showsProgress)
        layout.ShowAnimationFrame(ProgressSlot, 0);
}

void DialogManager::SetProgress(int percentage)
{
    targetPercentage = std::clamp(percentage, 0, FullProgress);
}

void DialogManager::Close()
{
    if (state != State::Closed)
        closeRequested = true;
}

std::optional<DialogAnswer> DialogManager::TakeAnswer()
{
    std::optional<DialogAnswer> taken = answer;
    answer.reset();
    return taken;
}

void DialogManager::Update(const InputManager& input, bool isActive)
{
    switch (state)
    {
    case State::Closed:
        return;

    case State::Opening:
        if (!openWindow->layout->IsAnimationPlaying(DialogInSlot))
            state = State::Waiting;
        break;

    case State::Waiting:
        UpdateWaiting(input, isActive);
        break;

    case State::ShowingChoice:
        if (!openWindow->layout->IsAnimationPlaying(choiceSlot))
            StartClosing(closingAnswer);
        break;

    case State::Closing:
        if (!openWindow->layout->IsAnimationPlaying(DialogOutSlot))
        {
            answer = closingAnswer;
            state = State::Closed;
        }
        break;
    }

    if (state != State::Closed)
        openWindow->layout->Calculate();
}

void DialogManager::UpdateWaiting(const InputManager& input, bool isActive)
{
    if (closeRequested)
    {
        StartClosing(DialogAnswer::ClosedByProgram);
        return;
    }

    if (!openWindow->buttons)
    {
        UpdateMessageWindow();
        return;
    }

    for (const ButtonGroupEvent& event : openWindow->buttons->Update(input, isActive))
    {
        ReactToButton(event);
        if (state != State::Waiting)
            return;
    }
}

void DialogManager::UpdateMessageWindow()
{
    if (stepsBeforeClosing != 0)
    {
        ++stepsWaited;
        if (stepsWaited > stepsBeforeClosing)
        {
            StartClosing(DialogAnswer::TimedOut);
            return;
        }
    }

    if (!showsProgress)
        return;

    shownPercentage = std::min(shownPercentage + ProgressPerStep, std::max(targetPercentage, shownPercentage));
    openWindow->layout->ShowAnimationFrame(ProgressSlot, static_cast<float>(shownPercentage));
    if (shownPercentage == FullProgress)
        StartClosing(DialogAnswer::ProgressDone);
}

void DialogManager::ReactToButton(const ButtonGroupEvent& event)
{
    MenuLayout& layout = *openWindow->layout;

    if (event.kind == ButtonGroupEventKind::Focused)
        layout.StartAnimation(FocusOnSlot(event.buttonIndex));

    if (event.kind == ButtonGroupEventKind::Unfocused)
        layout.StartAnimation(FocusOffSlot(event.buttonIndex));

    if (event.kind == ButtonGroupEventKind::Pressed)
    {
        closingAnswer = ButtonAnswers[event.buttonIndex];
        choiceSlot = SelectSlot(event.buttonIndex);
        layout.StartAnimation(choiceSlot);
        state = State::ShowingChoice;
    }
}

void DialogManager::StartClosing(DialogAnswer reason)
{
    closingAnswer = reason;
    openWindow->layout->StartAnimation(DialogOutSlot);
    state = State::Closing;
}

void DialogManager::Draw()
{
    if (state != State::Closed)
        openWindow->layout->Draw();
}

}
