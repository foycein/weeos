#include "SystemMenu/WiiSettings/SettingsScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/SoftKeyboard.h"

namespace SystemMenu
{

namespace
{

// The pages that the argument of the screen names.
constexpr int FirstIndexArgument = 0;
constexpr int SecondIndexArgument = 4;

// A button that is held down presses again after 400 milliseconds, then every 150 milliseconds. A step lasts 1001/60000 of a second.
constexpr int StepsBeforeRepeat = 24;
constexpr int StepsBetweenRepeats = 9;

bool HasPressedA(const PlayerInput& player)
{
    if ((player.state.buttonsPressed & Input::RemoteButtons::A) != 0)
        return true;

    bool hasClassicController = player.deviceKind == Input::DeviceKind::RemoteWithClassicController;
    return hasClassicController && (player.state.classic.buttonsPressed & Input::ClassicButtons::A) != 0;
}

bool IsHoldingA(const PlayerInput& player)
{
    if ((player.state.buttonsHeld & Input::RemoteButtons::A) != 0)
        return true;

    bool hasClassicController = player.deviceKind == Input::DeviceKind::RemoteWithClassicController;
    return hasClassicController && (player.state.classic.buttonsHeld & Input::ClassicButtons::A) != 0;
}

}

SettingsScreen::SettingsScreen(MenuServices& menuServices, int argumentOfTheScreen)
    : Screen(ScreenId::WiiSettings)
    , services(menuServices)
    , argument(argumentOfTheScreen)
{
    if (services.wiiSettings == nullptr)
        throw MenuError("Wii Settings cannot open: the menu has no settings to work with");
}

PageId SettingsScreen::FirstPageOf(int argument)
{
    if (argument == SecondIndexArgument)
        return PageId::SecondIndex;

    return PageId::FirstIndex;
}

void SettingsScreen::Build()
{
    images = std::make_unique<PageImages>(services.display.Device(), services.files);
    font = std::make_unique<PageFont>(services.display.Device(), services.files);
    painter = std::make_unique<PagePainter>(services.display.Device(), *images, *font);
    flow = std::make_unique<SettingsFlow>(*services.wiiSettings, services.messages, FirstPageOf(argument));

    services.cursors.SetVisible(true);
    services.fader.StartFadeIn();
}

void SettingsScreen::Draw(DrawPass pass)
{
    if (pass != DrawPass::Main)
        return;

    painter->Draw(flow->CurrentPage(), pointedButton, services.display.GetCanvas());
}

bool SettingsScreen::DrawsWithChildren() const
{
    return true;
}

SettingsFlow& SettingsScreen::Flow()
{
    return *flow;
}

bool SettingsScreen::RunMain()
{
    if (IsWaitingForKeyboard())
        return false;

    HandlePlayers();
    RepeatHeldButton();
    flow->Step();
    return flow->IsFinished();
}

bool SettingsScreen::RunClosing()
{
    services.fader.StartFadeOut();
    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    GoToRoot(ScreenId::WiiOptionsBackground, FirstIndexArgument);
    return true;
}

// A page that asks for text or digits gets the soft keyboard. While it is open the pages wait, and its answer goes to the page.
// The parental dialog (screen 0x1b) is not in this menu yet, so a request for it stays open and the page stays where it is.
bool SettingsScreen::IsWaitingForKeyboard()
{
    if (services.keyboard.IsOpen())
        return true;

    std::optional<KeyboardAnswer> answer = services.keyboard.TakeAnswer();
    if (answer)
    {
        if (answer->wasConfirmed)
            flow->AnswerInput(answer->text);
        else
            flow->CancelInput();

        return false;
    }

    const std::optional<InputRequest>& request = flow->PendingInput();
    if (!request || request->kind == InputKind::ParentalPassword)
        return false;

    KeyboardRequest keyboardRequest;
    keyboardRequest.title = request->title;
    keyboardRequest.initialText = request->initialText;
    keyboardRequest.rules.maximumLength = request->maximumLength;
    if (request->kind == InputKind::Digits)
        keyboardRequest.rules.allowedCharacters = IsDigit;

    keyboardRequest.playerIndex = pressingPlayer;
    services.keyboard.Open(keyboardRequest);
    return true;
}

// Every remote that points at the screen points at a button or at nothing. The highlight follows the first of them, and a press of A
// on a button is a press of that button.
void SettingsScreen::HandlePlayers()
{
    pointedButton.reset();

    for (int player = 0; player < PlayerCount; ++player)
    {
        const std::optional<PlayerInput>& playerInput = services.input.Player(player);
        if (!playerInput || !playerInput->state.pointerIsValid)
            continue;

        // The page is in the middle of the picture, which can be wider than the page.
        Formats::Vector2 canvasPoint = services.display.PointerToCanvas(playerInput->state.pointer);
        float pageX = canvasPoint.x + static_cast<float>(PageWidth) / 2;
        float pageY = static_cast<float>(PageHeight) / 2 - canvasPoint.y;
        std::optional<std::size_t> button = FindButtonAt(pageX, pageY);

        if (!pointedButton)
            pointedButton = button;

        if (button && HasPressedA(*playerInput))
            PressWithPlayer(player, *button);
    }
}

std::optional<std::size_t> SettingsScreen::FindButtonAt(float pageX, float pageY) const
{
    const Page& page = flow->CurrentPage();
    for (std::size_t buttonIndex = 0; buttonIndex < page.buttons.size(); ++buttonIndex)
    {
        const PageButton& button = page.buttons[buttonIndex];
        if (button.isShown && button.bounds.Contains(static_cast<int>(pageX), static_cast<int>(pageY)))
            return buttonIndex;
    }

    return std::nullopt;
}

void SettingsScreen::PressWithPlayer(int player, std::size_t button)
{
    pressingPlayer = player;
    heldButton.reset();
    stepsHeld = 0;

    if (flow->CurrentPage().buttons[button].repeatsWhenHeld)
    {
        heldButton = button;
        heldPlayer = player;
    }

    flow->PressButton(button);
}

// An arrow that changes a number presses again while A stays down on it.
void SettingsScreen::RepeatHeldButton()
{
    if (!heldButton)
        return;

    const std::optional<PlayerInput>& playerInput = services.input.Player(heldPlayer);
    bool isStillHeld = pointedButton == heldButton && playerInput && IsHoldingA(*playerInput);
    if (!isStillHeld)
    {
        heldButton.reset();
        return;
    }

    ++stepsHeld;
    bool isTimeToRepeat = stepsHeld >= StepsBeforeRepeat && (stepsHeld - StepsBeforeRepeat) % StepsBetweenRepeats == 0;
    if (isTimeToRepeat)
        flow->PressButton(*heldButton);
}

}
