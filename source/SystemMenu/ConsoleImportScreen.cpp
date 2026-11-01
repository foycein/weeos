#include "SystemMenu/ConsoleImportScreen.h"

#include "SystemMenu/ConsoleImport.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/ScreenManager.h"

namespace SystemMenu
{

namespace
{

constexpr std::u16string_view QuestionText =
    u"Welcome to WeeOS!\n"
    u"Do you have your own Wii? Import it\n"
    u"from its BootMii backup to bring its\n"
    u"Miis, saves and channels.";
constexpr std::u16string_view ImportButtonText = u"Import your Wii";
constexpr std::u16string_view SkipButtonText = u"Skip";
constexpr std::u16string_view ImportingText = u"Importing your Wii...\nPlease wait.";
constexpr std::u16string_view OkButtonText = u"OK";
constexpr std::string_view UnknownFailureText = "The Wii could not be imported.";

std::u16string ToText(const std::string& text)
{
    return std::u16string(text.begin(), text.end());
}

}

ConsoleImportScreen::ConsoleImportScreen(MenuServices& menuServices)
    : Screen(ScreenId::ConsoleImport)
    , services(menuServices)
{
}

bool ConsoleImportScreen::IsReady()
{
    services.fader.SetClear();
    services.fader.Calculate();
    Ask();
    return true;
}

bool ConsoleImportScreen::RunMain()
{
    switch (state)
    {
    case State::Asking:
        return RunAsking();

    case State::Importing:
        RunImporting();
        return false;

    case State::ClosingWaitWindow:
        RunClosingWaitWindow();
        return false;

    case State::ShowingFailure:
        RunShowingFailure();
        return false;
    }
    return false;
}

// Health and Safety follows on a black picture, as it does after a cold start.
bool ConsoleImportScreen::RunClosing()
{
    if (!hasFadeOutStarted)
    {
        services.fader.StartFadeOut();
        hasFadeOutStarted = true;
    }

    if (services.fader.GetStatus() != FaderStatus::Covered)
        return false;

    services.screens.ReplaceScreen(*this, ScreenId::HealthAndSafety, 0);
    return true;
}

void ConsoleImportScreen::Ask()
{
    DialogRequest request;
    request.layout = DialogLayout::TwoButtons;
    request.message = QuestionText;
    request.buttonAText = ImportButtonText;
    request.buttonBText = SkipButtonText;
    request.cancelButton = DialogAnswer::ButtonB;
    services.dialogs.Open(request);
    state = State::Asking;
}

void ConsoleImportScreen::ShowThatTheConsoleWorks()
{
    DialogRequest request;
    request.layout = DialogLayout::Message;
    request.message = ImportingText;
    request.showsWaitIcon = true;
    services.dialogs.Open(request);
    state = State::Importing;
}

void ConsoleImportScreen::ShowFailure(const std::string& failure)
{
    DialogRequest request;
    request.layout = DialogLayout::OneButton;
    request.message = ToText(failure);
    request.buttonBText = OkButtonText;
    services.dialogs.Open(request);
    state = State::ShowingFailure;
}

bool ConsoleImportScreen::RunAsking()
{
    std::optional<DialogAnswer> answer = services.dialogs.TakeAnswer();
    if (!answer)
        return false;

    if (*answer != DialogAnswer::ButtonA)
        return true;

    if (services.consoleImport != nullptr && services.consoleImport->PickAndStartImport())
        ShowThatTheConsoleWorks();
    else
        Ask();

    return false;
}

// When the import has worked, the program starts the imported console and this screen is gone with the menu.
void ConsoleImportScreen::RunImporting()
{
    if (services.consoleImport->IsImporting())
        return;

    std::optional<std::string> failure = services.consoleImport->TakeFailure();
    pendingFailure = failure.value_or(std::string(UnknownFailureText));
    services.dialogs.Close();
    state = State::ClosingWaitWindow;
}

void ConsoleImportScreen::RunClosingWaitWindow()
{
    if (services.dialogs.IsOpen())
        return;

    services.dialogs.TakeAnswer();
    ShowFailure(pendingFailure);
}

void ConsoleImportScreen::RunShowingFailure()
{
    if (services.dialogs.TakeAnswer())
        Ask();
}

}
