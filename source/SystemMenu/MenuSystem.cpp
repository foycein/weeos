#include "SystemMenu/MenuSystem.h"

#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/MenuError.h"
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
    , frontPanel(display, powerService)
    , nand(nandFiles)
    , textEntry(textEntryOfTheMenu)
    , services{display, files, messages, clock, fader, input, cursors, screens, program, nand, settings, dialogs, textEntry, parentalAnswer}
{
    display.SetTextFonts(&fonts.Fonts());

    FirstScreen firstScreen = ChooseFirstScreen(ReadStartFacts(nand, settings));
    screens.Start(firstScreen.screen, firstScreen.argument);
}

void MenuSystem::Update()
{
    clock.Advance();
    frontPanel.Update();

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
        errorScreen.Show(error.Reason());
    }
}

void MenuSystem::RunStep()
{
    input.Poll();
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

void MenuSystem::ShowFatalError(FatalError reason)
{
    errorScreen.Show(reason);
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
