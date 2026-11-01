#include "SystemMenu/WiiSettings/SettingsScreen.h"

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/ParentalAnswer.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoftKeyboard.h"
#include "SystemMenu/SoundRequests.h"

namespace SystemMenu
{

namespace
{

// The pages that the argument of the screen names.
// The pages that the argument of the screen names (the tables 0x81610A30 and 0x81610AA4 of the original). The arguments 2 and 5 open
// the first-time setup, which WeeOS does not have (docs/DECISIONS.md: no first-run setup), so they open the first index page.
constexpr int FirstIndexArgument = 0;
constexpr int InternetArgument = 1;
constexpr int SystemUpdateArgument = 3;
constexpr int SecondIndexArgument = 4;
constexpr int CountryArgument = 6;

// The parental dialog tells, after a right PIN, that the restriction is lifted until the user has finished (SystemMenuStructure.md 4.10).
constexpr int ParentalDialogArgument = 1;

// A button that is held down presses again after 400 milliseconds, then every 150 milliseconds. A step lasts 1001/60000 of a second.
constexpr int StepsBeforeRepeat = 24;
constexpr int StepsBetweenRepeats = 9;

// The sounds of the pages: the numbers that their scripts give wii.se, and the names that the original plays for them (0x813F9834):
// 1 the push of an arrow, 2 the pointer on a button, 3 decide, 4 cancel, 5 another choice, 6 an error, 10 to 12 a new sound mode.
std::string_view SoundNameOf(PageSound sound)
{
    switch (sound)
    {
    case PageSound::Decide:
        return "WIPL_SE_DECIDE";

    case PageSound::Back:
        return "WIPL_SE_CANCEL";

    case PageSound::Choose:
        return "WIPL_SE_CHOICE_CHG";

    case PageSound::Scroll:
        return "WIPL_SE_BT_PUSH";

    case PageSound::Error:
        return "WIPL_SE_CHAR_DELETE_ERROR";

    case PageSound::Surround:
        return "WIPL_SE_OUTPUT_MODE_SELECT";

    default:
        return {};
    }
}

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

std::unique_ptr<Screen> MakeWiiSettingsScreen(MenuServices& services, int argument)
{
    if (services.wiiSettings == nullptr)
        return nullptr;

    return std::make_unique<SettingsScreen>(services, argument);
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
    switch (argument)
    {
    case InternetArgument:
        return PageId::InternetIndex;

    case SystemUpdateArgument:
        return PageId::UpdateIndex;

    case SecondIndexArgument:
        return PageId::SecondIndex;

    case CountryArgument:
        return PageId::CountrySelect;

    default:
        return PageId::FirstIndex;
    }
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
    if (IsWaitingForAnswer())
        return false;

    std::optional<std::size_t> pointedBefore = pointedButton;
    HandlePlayers();
    if (pointedButton && pointedButton != pointedBefore)
        services.sounds.Request(SoundName::ButtonTargeted);

    RepeatHeldButton();
    flow->Step();
    RequestSoundsOfThePages();
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

void SettingsScreen::RequestSoundsOfThePages()
{
    for (PageSound sound : flow->TakeSounds())
    {
        if (sound != PageSound::None)
            services.sounds.Request(SoundNameOf(sound));
    }
}

// A page that asks for text or digits gets the soft keyboard, a page that needs the PIN of the parental controls gets their dialog
// (screen 0x1b, opened with the argument 1 as the original does, 0x813F20AC). While one of them is open the pages wait, and its
// answer goes to the page. The dialog is a child of this screen, so this screen stands still until the dialog is gone.
bool SettingsScreen::IsWaitingForAnswer()
{
    if (isParentalDialogOpen)
    {
        TakeAnswerOfParentalDialog();
        return false;
    }

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
    if (!request)
        return false;

    if (request->kind == InputKind::ParentalPassword)
        OpenParentalDialog();
    else
        OpenKeyboard(*request);

    return true;
}

void SettingsScreen::OpenKeyboard(const InputRequest& request)
{
    KeyboardRequest keyboardRequest;
    keyboardRequest.title = request.title;
    keyboardRequest.initialText = request.initialText;
    keyboardRequest.rules.maximumLength = request.maximumLength;
    if (request.kind == InputKind::Digits)
        keyboardRequest.rules.allowedCharacters = IsDigit;

    keyboardRequest.playerIndex = pressingPlayer;
    services.keyboard.Open(keyboardRequest);
}

void SettingsScreen::OpenParentalDialog()
{
    services.parentalAnswer = ParentalAnswer::None;
    services.screens.OpenChild(ScreenId::ParentalDialog, ScreenId::WiiSettings, ParentalDialogArgument);
    isParentalDialogOpen = true;
}

// Only a right PIN lets the page go on; three wrong PINs and Quit leave the user where he was.
void SettingsScreen::TakeAnswerOfParentalDialog()
{
    isParentalDialogOpen = false;
    if (services.parentalAnswer == ParentalAnswer::PinCorrect)
        flow->AnswerInput(u"");
    else
        flow->CancelInput();
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
