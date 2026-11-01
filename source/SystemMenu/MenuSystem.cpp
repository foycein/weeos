#include "SystemMenu/MenuSystem.h"

#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuStateFile.h"
#include "SystemMenu/ScreenFactory.h"

namespace SystemMenu
{

MenuSystem::MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, NandFiles& nandFiles,
    Ios::RemoteService& remoteService, Ios::PowerService& powerService, TextEntry& textEntryOfTheMenu, Platform::ScreenShape screenShape)
    : display(device, screenShape)
    , files(titleContents)
    , messages(files.OpenEnglishMessages())
    , fader(display)
    , input(remoteService)
    , cursors(display, files)
    , screens([this](ScreenId screen, int argument) { return MakeScreen(screen, argument, services); })
    , program(titleContents)
    , settings(nandFiles)
    , fonts(device, titleContents, files)
    , dialogs(display, files)
    , errorScreen(display, program, fonts, fader)
    , frontPanel(display, powerService, screens, nandFiles)
    , nand(nandFiles)
    , textEntry(textEntryOfTheMenu)
    , services{display, files, messages, clock, fader, input, cursors, screens, program, nand, settings, dialogs, textEntry, parentalAnswer, frontPanel}
{
    display.SetTextFonts(&fonts.Fonts());

    StartFacts facts = ReadStartFacts(nand, settings);
    RecordMenuStart(nand);

    FirstScreen firstScreen = ChooseFirstScreen(facts);
    if (IsMenuReachedAtStart(firstScreen.screen))
        frontPanel.NoteMenuReached();

    screens.Start(firstScreen.screen, firstScreen.argument);
}

void MenuSystem::Update()
{
    clock.Advance();
    frontPanel.Update(errorScreen.IsRequested());

    if (errorScreen.IsRequested())
    {
        errorScreen.Update();
        fader.Calculate();
        return;
    }

    try
    {
        RunStep();
    }
    catch (const MenuError& error)
    {
        fatalErrorMessage = error.what();
        ShowFatalError(error.Reason());
    }
}

// While the front panel works, the original runs its own loop (0x81335348), which updates everything but the soft keyboard.
void MenuSystem::RunStep()
{
    input.Poll();
    if (!frontPanel.IsWorking())
        textEntry.Update();

    screens.Update();
    dialogs.Update(input, fader.GetStatus() == FaderStatus::Clear);
    cursors.Update(input);
    fader.Calculate();
}

void MenuSystem::Draw()
{
    display.BeginPicture();
    DrawScene();
}

void MenuSystem::DrawScene()
{
    if (!errorScreen.IsShown())
    {
        screens.Draw();
        dialogs.Draw();
        cursors.Draw();
    }

    errorScreen.Draw();
    fader.Draw();
    frontPanel.Draw();
}

// A button of the front panel that was pressed first wins: the error screen does not come then (0x81334F0C).
void MenuSystem::ShowFatalError(FatalError reason)
{
    if (frontPanel.IsWorking())
        return;

    errorScreen.Show(reason);
}

std::optional<Ios::TitleId> MenuSystem::TakeTitleToLaunch()
{
    return frontPanel.TakeTitleToLaunch();
}

const std::optional<std::string>& MenuSystem::FatalErrorMessage() const
{
    return fatalErrorMessage;
}

MenuServices& MenuSystem::Services()
{
    return services;
}

}
