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

// The words of a page fade out in 10 steps and the next ones fade in as fast (0x81346C14 gets 10, 0x81345880).
constexpr int FullAlpha = 255;
constexpr int PageFadeSteps = 10;
constexpr int AlphaPerPageStep = (FullAlpha + PageFadeSteps - 1) / PageFadeSteps;

constexpr std::size_t BackButtonIndex = 1;

// The animation of the picture of a page is the first animation of its layout.
constexpr std::size_t PictureAnimationSlot = 0;

struct WindowDescription
{
    std::string_view layoutName;
    std::size_t buttonCount;
};

constexpr std::array<WindowDescription, DialogLayoutCount> WindowDescriptions = {{
    {"my_DialogWindow_a0", 0},
    {"my_DialogWindow_a1", 1},
    {"my_DialogWindow_a2", 2},
    {"my_DialogWindow_a3", 3},
    {"my_DialogWindow_b", 2},
}};

// The buttons are numbered in this order: the first button of every window is B, the second is A.
constexpr std::array<std::string_view, MostButtonsOfADialog> ButtonLetters = {"B", "A", "C"};
constexpr std::array<DialogAnswer, MostButtonsOfADialog> ButtonAnswers = {DialogAnswer::ButtonB, DialogAnswer::ButtonA, DialogAnswer::ButtonC};

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

// Sets the alpha of every pane below the pane, as 0x81345718 does.
void SetAlphaOfPanesBelow(Formats::Pane& pane, std::uint8_t alpha)
{
    for (Formats::Pane& child : pane.children)
    {
        child.alpha = alpha;
        SetAlphaOfPanesBelow(child, alpha);
    }
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

DialogManager::DialogManager(Display& menuDisplay, const MenuFiles& files, SoundRequests& menuSounds)
    : display(menuDisplay)
    , sounds(menuSounds)
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
    window.buttons = std::make_unique<ButtonController>(*window.layout, display, buttonPaneNames);
}

bool DialogManager::IsOpen() const
{
    return state != State::Closed;
}

bool DialogManager::Open(const DialogRequest& request)
{
    if (state != State::Closed)
        return false;

    pages = request.pages;
    DialogLayout layout = pages.empty() ? request.layout : DialogLayout::TwoButtons;
    openWindow = &windows[static_cast<std::size_t>(layout)];
    answer.reset();
    closeRequested = false;
    stepsBeforeClosing = request.stepsBeforeClosing;
    stepsWaited = 0;
    showsProgress = request.showsProgress;
    soundsBusy = request.showsWaitIcon || request.showsProgress;
    cancelButton = request.cancelButton;
    targetPercentage = 0;
    shownPercentage = 0;
    sounds.Request(soundsBusy ? SoundName::Busy : SoundName::InfoWindow);

    isButtonFocused = {};
    if (openWindow->buttons)
        openWindow->buttons->ForgetPointers();

    if (layout == DialogLayout::TwoButtons)
        ShowBackButton(true);

    if (!pages.empty())
        OpenPage(0);
    else
        ShowTexts(request);

    if (layout == DialogLayout::Message)
        ShowMessageExtras(request);

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

void DialogManager::OpenPage(std::size_t pageIndex)
{
    shownPage = pageIndex;
    previousPage = pageIndex;
    pageAlpha = FullAlpha;

    const DialogPage& page = pages[pageIndex];
    ShowPageTexts(page);
    ShowBackButton(page.hasBackButton);
    FadePage(page, page, FullAlpha);
    if (page.picture != nullptr && page.playsPictureAnimation)
        page.picture->StartAnimation(PictureAnimationSlot);
}

void DialogManager::ShowPageTexts(const DialogPage& page)
{
    MenuLayout& layout = *openWindow->layout;
    layout.SetText("T_Dialog", page.message);
    layout.SetText("T_BtnB", page.nextText);
    layout.SetText("T_BtnA", page.backText);
}

// The button A of the window with two buttons, which a page of a dialog with pages may hide.
void DialogManager::ShowBackButton(bool isShown)
{
    MenuLayout& layout = *openWindow->layout;
    layout.PaneNamed("N_BtnA").isVisible = isShown;
    SetAlphaOfPanesBelow(layout.PaneNamed("N_BtnA_Pic"), FullAlpha);
    openWindow->buttons->SetEnabled("B_BtnA", isShown);
    if (!isShown)
        isButtonFocused[BackButtonIndex] = false;
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
            EndTheChoice();
        break;

    case State::TurningPage:
        UpdateTurningPage();
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
    {
        openWindow->layout->Calculate();
        UpdatePicture();
    }
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

    for (const ButtonEvent& event : openWindow->buttons->Poll(input, isActive))
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

    int percentageToReach = std::max(targetPercentage, shownPercentage);
    shownPercentage = std::min(shownPercentage + ProgressPerStep, percentageToReach);
    openWindow->layout->ShowAnimationFrame(ProgressSlot, static_cast<float>(shownPercentage));
    if (shownPercentage == FullProgress)
        StartClosing(DialogAnswer::ProgressDone);
}

void DialogManager::ReactToButton(const ButtonEvent& event)
{
    MenuLayout& layout = *openWindow->layout;

    if (event.kind == ButtonEventKind::FocusGained)
    {
        isButtonFocused[event.buttonIndex] = true;
        sounds.Request(SoundName::ButtonTargeted);
        layout.StartAnimation(FocusOnSlot(event.buttonIndex));
    }

    // A button that a page hid while a pointer was on it has lost its focus already.
    if (event.kind == ButtonEventKind::FocusLost && isButtonFocused[event.buttonIndex])
    {
        isButtonFocused[event.buttonIndex] = false;
        layout.StartAnimation(FocusOffSlot(event.buttonIndex));
    }

    if (event.kind == ButtonEventKind::Pressed)
    {
        closingAnswer = ButtonAnswers[event.buttonIndex];
        sounds.Request(SoundOfPress(closingAnswer));
        choiceSlot = SelectSlot(event.buttonIndex);
        layout.StartAnimation(choiceSlot);
        state = State::ShowingChoice;
    }
}

// The chosen button has flashed. A dialog with pages turns its page unless it was the last page or the first (0x8134753C); a button
// that a pointer is still on shows its focus again (0x81345778).
void DialogManager::EndTheChoice()
{
    if (pages.empty())
    {
        StartClosing(closingAnswer);
        return;
    }

    bool turnsForward = closingAnswer == DialogAnswer::ButtonB;
    bool isPastTheLastPage = turnsForward && shownPage + 1 == pages.size();
    bool isBeforeTheFirstPage = !turnsForward && shownPage == 0;
    if (isPastTheLastPage || isBeforeTheFirstPage)
    {
        StartClosing(closingAnswer);
        return;
    }

    const DialogPage& page = pages[shownPage];
    if (page.picture != nullptr && page.playsPictureAnimation)
        page.picture->StopAnimation(PictureAnimationSlot);

    previousPage = shownPage;
    shownPage = turnsForward ? shownPage + 1 : shownPage - 1;
    isFadingOut = true;
    state = State::TurningPage;

    for (std::size_t buttonIndex = 0; buttonIndex < openWindow->buttonCount; ++buttonIndex)
    {
        if (isButtonFocused[buttonIndex])
            openWindow->layout->StartAnimation(FocusOnSlot(buttonIndex));
    }
}

// The parts of the page that the next page changes fade out; at nothing the words change and fade in (0x81345880).
void DialogManager::UpdateTurningPage()
{
    const DialogPage& oldPage = pages[previousPage];
    const DialogPage& newPage = pages[shownPage];

    if (isFadingOut)
    {
        pageAlpha = std::max(pageAlpha - AlphaPerPageStep, 0);
        FadePage(oldPage, newPage, static_cast<std::uint8_t>(pageAlpha));
        if (pageAlpha > 0)
            return;

        isFadingOut = false;
        ShowPageTexts(newPage);
        if (!newPage.hasBackButton)
            ShowBackButton(false);
        return;
    }

    bool backButtonComesBack = newPage.hasBackButton && !oldPage.hasBackButton && pageAlpha == 0;
    if (backButtonComesBack)
        ShowBackButton(true);

    pageAlpha = std::min(pageAlpha + AlphaPerPageStep, FullAlpha);
    FadePage(newPage, oldPage, static_cast<std::uint8_t>(pageAlpha));
    if (pageAlpha < FullAlpha)
        return;

    if (newPage.picture != nullptr && newPage.playsPictureAnimation)
        newPage.picture->StartAnimation(PictureAnimationSlot);
    state = State::Waiting;
}

// The words of the page fade, and of its buttons and its picture what the other page does not share.
void DialogManager::FadePage(const DialogPage& page, const DialogPage& otherPage, std::uint8_t alpha)
{
    MenuLayout& layout = *openWindow->layout;
    layout.PaneNamed("T_Dialog").alpha = alpha;

    if (page.nextText != otherPage.nextText)
        layout.PaneNamed("T_BtnB").alpha = alpha;

    bool backButtonComesOrGoes = page.hasBackButton != otherPage.hasBackButton;
    if (backButtonComesOrGoes)
        SetAlphaOfPanesBelow(layout.PaneNamed("N_BtnA_Pic"), alpha);
    else if (page.backText != otherPage.backText)
        layout.PaneNamed("T_BtnA").alpha = alpha;

    if (page.picture != nullptr && page.picture != otherPage.picture)
        SetAlphaOfPanesBelow(page.picture->PaneNamed("RootPane"), alpha);
}

const DialogPage* DialogManager::PageOfPicture() const
{
    if (pages.empty())
        return nullptr;

    bool showsTheOldPage = state == State::TurningPage && isFadingOut;
    return &pages[showsTheOldPage ? previousPage : shownPage];
}

// The picture of the page lies over the middle of the dialog, raised by its height (0x81345EE8).
void DialogManager::UpdatePicture()
{
    const DialogPage* page = PageOfPicture();
    if (page == nullptr || page->picture == nullptr)
        return;

    const Formats::Vector3& middle = openWindow->layout->PaneNamed("N_Dialog").translation;
    page->picture->PaneNamed("RootPane").translation = {middle.x, middle.y + page->pictureHeight, middle.z};
    page->picture->Calculate();
}

// A window with one button has nothing to cancel.
std::string_view DialogManager::SoundOfPress(DialogAnswer pressed) const
{
    bool cancels = openWindow->buttonCount > 1 && pressed == cancelButton;
    return cancels ? SoundName::Cancel : SoundName::Decide;
}

void DialogManager::StartClosing(DialogAnswer reason)
{
    if (soundsBusy)
        sounds.Request(SoundName::BusyFinished);

    soundsBusy = false;
    closingAnswer = reason;
    openWindow->layout->StartAnimation(DialogOutSlot);
    state = State::Closing;
}

void DialogManager::Draw()
{
    if (state == State::Closed)
        return;

    openWindow->layout->Draw();

    const DialogPage* page = PageOfPicture();
    if (page != nullptr && page->picture != nullptr)
        page->picture->Draw();
}

}
