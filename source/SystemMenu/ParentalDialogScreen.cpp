#include "SystemMenu/ParentalDialogScreen.h"

#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuSettings.h"
#include "SystemMenu/TextEntry.h"

#include "Text/MessageCatalog.h"

#include <array>
#include <vector>

namespace SystemMenu
{

namespace
{

constexpr std::string_view BundleName = "prntDlg";
constexpr std::string_view LayoutName = "my_ParentalWindow_a";

// The numbers of the messages that the dialog shows, in ipl_common.bmg.
constexpr std::uint16_t QuitMessage = 37;
constexpr std::uint16_t OkMessage = 46;
constexpr std::uint16_t RestrictedMessage = 327;
constexpr std::uint16_t InputPinMessage = 328;
constexpr std::uint16_t RestrictionsRemovedUntilMenuMessage = 329;
constexpr std::uint16_t RestrictionsRemovedUntilFinishedMessage = 330;
constexpr std::uint16_t WrongPinMessage = 331;

// The slots of the animations, in the order that Build binds them.
constexpr std::size_t DialogInSlot = 0;
constexpr std::size_t DialogOutSlot = 1;
constexpr std::size_t FirstButtonSlot = 2;
constexpr std::size_t SlotsPerButton = 3;
constexpr std::size_t BeforeAfterSlot = 11;

// The names of the next two animations are the ones of the files. The dialog plays OffBtn when a PIN is typed and OnBtn when the PIN is gone.
constexpr std::size_t OffButtonSlot = 12;
constexpr std::size_t OnButtonSlot = 13;

// The buttons, in the order of their numbers.
constexpr std::size_t QuitButton = 0;
constexpr std::size_t OkButton = 1;
constexpr std::size_t PinFieldButton = 2;
constexpr std::array<std::string_view, 3> ButtonLetters = {"A", "B", "C"};

constexpr std::size_t PinLength = 4;
constexpr int WrongPinsAllowed = 3;
constexpr std::uint32_t StepsOfTheRightPinMessage = 180;

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

}

ParentalDialogScreen::ParentalDialogScreen(MenuServices& menuServices, int messageToShow)
    : Screen(ScreenId::ParentalDialog)
    , services(menuServices)
    , messageChoice(messageToShow)
{
}

void ParentalDialogScreen::Load()
{
    archive = services.files.OpenLayoutBundle(BundleName);
}

void ParentalDialogScreen::Build()
{
    layout = std::make_unique<MenuLayout>(services.display, *archive, std::string(LayoutName));

    layout->BindAnimation("my_ParentalWindow_a_DialogIn", "G_InOut");
    layout->BindAnimation("my_ParentalWindow_a_DialogOut", "G_InOut");

    std::vector<std::string> buttonPaneNames;
    for (std::string_view letter : ButtonLetters)
    {
        std::string group = "G_FocusBtn" + std::string(letter);
        layout->BindAnimation("my_ParentalWindow_a_FocusBtn_on", group);
        layout->BindAnimation("my_ParentalWindow_a_FocusBtn_off", group);
        layout->BindAnimation("my_ParentalWindow_a_SelectBtn_Ac", group);
        buttonPaneNames.push_back("B_Btn" + std::string(letter));
    }

    layout->BindAnimation("my_ParentalWindow_a_BeforeAfter", "G_BrforeAfter");
    layout->BindAnimation("my_ParentalWindow_a_OffBtn", "G_OnOffBtnB");
    layout->BindAnimation("my_ParentalWindow_a_OnBtn", "G_OnOffBtnB");
    buttons = std::make_unique<ButtonGroup>(*layout, services.display, buttonPaneNames);

    layout->SetText("T_BtnA", services.messages.Find(QuitMessage));
    layout->SetText("T_BtnB", services.messages.Find(OkMessage));
    layout->SetText("T_Dialog", services.messages.Find(RestrictedMessage));
    layout->SetText("T_Before", services.messages.Find(InputPinMessage));

    // The window starts with the request for the PIN in the field and the OK button dimmed.
    layout->ShowAnimationFrame(BeforeAfterSlot, 0);
    layout->ShowAnimationFrame(OffButtonSlot, 0);

    layout->StartAnimation(DialogInSlot);
    layout->Calculate();

    services.parentalAnswer = ParentalAnswer::None;
}

void ParentalDialogScreen::Draw(DrawPass pass)
{
    if (pass == DrawPass::Main)
        layout->Draw();
}

bool ParentalDialogScreen::RunMain()
{
    layout->Calculate();

    switch (phase)
    {
    case Phase::WaitingForButton:
        for (const ButtonEvent& event : buttons->Update(services.input, services.fader.GetStatus() == FaderStatus::Clear))
        {
            ReactToButton(event);
            if (phase != Phase::WaitingForButton)
                break;
        }
        break;

    case Phase::TypingPin:
        ReadTypedPin();
        break;

    case Phase::WaitingForDialog:
        ReadAnswerOfDialog();
        break;

    case Phase::Quitting:
        if (!layout->IsAnimationPlaying(SelectSlot(QuitButton)))
            phase = Phase::Finished;
        break;

    case Phase::Finished:
        break;
    }

    return phase == Phase::Finished;
}

void ParentalDialogScreen::OnRunEnded()
{
    layout->StartAnimation(DialogOutSlot);
}

bool ParentalDialogScreen::RunClosing()
{
    layout->Calculate();
    if (layout->IsAnimationPlaying(DialogOutSlot))
        return false;

    services.parentalAnswer = answer;
    return true;
}

// The OK button reacts only when a PIN is typed.
void ParentalDialogScreen::ReactToButton(const ButtonEvent& event)
{
    bool isDimmedOkButton = event.buttonIndex == OkButton && !hasPin;

    if (event.kind == ButtonEventKind::Focused && !isDimmedOkButton)
        layout->StartAnimation(FocusOnSlot(event.buttonIndex));

    if (event.kind == ButtonEventKind::Unfocused && !isDimmedOkButton)
        layout->StartAnimation(FocusOffSlot(event.buttonIndex));

    if (event.kind == ButtonEventKind::Pressed && !isDimmedOkButton)
        Press(event.buttonIndex, event.playerIndex);
}

void ParentalDialogScreen::Press(std::size_t buttonIndex, int playerIndex)
{
    buttons->Reset();
    layout->StartAnimation(SelectSlot(buttonIndex));

    if (buttonIndex == QuitButton)
    {
        answer = ParentalAnswer::Cancelled;
        phase = Phase::Quitting;
    }

    if (buttonIndex == PinFieldButton)
    {
        services.textEntry.Begin(playerIndex, PinLength);
        phase = Phase::TypingPin;
    }

    if (buttonIndex == OkButton)
        CheckPin();
}

void ParentalDialogScreen::CheckPin()
{
    DialogRequest request;

    if (IsPinRight())
    {
        answer = ParentalAnswer::PinCorrect;
        request.layout = DialogLayout::Message;
        request.stepsBeforeClosing = StepsOfTheRightPinMessage;
        request.message = services.messages.Find(messageChoice == 0 ? RestrictionsRemovedUntilMenuMessage : RestrictionsRemovedUntilFinishedMessage);
    }
    else
    {
        ++wrongPinCount;
        request.layout = DialogLayout::OneButton;
        request.message = services.messages.Find(WrongPinMessage);
        request.buttonBText = services.messages.Find(OkMessage);

        layout->StartAnimation(OnButtonSlot);
        hasPin = false;
    }

    services.dialogs.Open(request);
    phase = Phase::WaitingForDialog;
}

bool ParentalDialogScreen::IsPinRight() const
{
    std::optional<ParentalControl> parentalControl = services.settings.ReadParentalControl();
    return parentalControl && parentalControl->pin.size() == PinLength && parentalControl->pin == pin;
}

void ParentalDialogScreen::ReadTypedPin()
{
    TextEntryState entryState = services.textEntry.State();
    if (entryState == TextEntryState::Typing)
        return;

    if (entryState == TextEntryState::Finished)
    {
        pin.clear();
        for (char16_t character : services.textEntry.Text().substr(0, PinLength))
            pin.push_back(static_cast<char>(character));

        hasPin = true;
        layout->StartAnimation(OffButtonSlot);
        layout->StartAnimation(BeforeAfterSlot);
    }

    services.textEntry.End();
    phase = Phase::WaitingForButton;
}

// After a wrong PIN the user goes back to the field, until the third wrong PIN. After the message of the right PIN the dialog is done.
void ParentalDialogScreen::ReadAnswerOfDialog()
{
    std::optional<DialogAnswer> dialogAnswer = services.dialogs.TakeAnswer();
    if (!dialogAnswer)
        return;

    bool wasWrongPin = answer != ParentalAnswer::PinCorrect;
    if (!wasWrongPin)
    {
        phase = Phase::Finished;
        return;
    }

    if (wrongPinCount < WrongPinsAllowed)
    {
        layout->ShowAnimationFrame(BeforeAfterSlot, 0);
        phase = Phase::WaitingForButton;
        return;
    }

    answer = ParentalAnswer::TooManyWrongPins;
    phase = Phase::Finished;
}

}
