#include "SystemMenu/MenuSystem.h"

#include "SystemMenu/ConsoleImport.h"
#include "SystemMenu/FirstScreen.h"
#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuStateFile.h"
#include "SystemMenu/ScreenFactory.h"
#include "SystemMenu/WiiSettings/SettingsFlow.h"

namespace SystemMenu
{

MenuSystem::MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, NandFiles& nandFiles,
    Ios::RemoteService& remoteService, Ios::PowerService& powerService, Platform::ScreenShape screenShape, MenuConsole& wiiMenuConsole,
    SettingsFlowServices* wiiSettingsServices, ConsoleImport* consoleImport)
    : display(device, screenShape)
    , files(titleContents)
    , fonts(display.Device(), titleContents, files)
    , messages(files.OpenEnglishMessages())
    , fader(display)
    , input(remoteService)
    , cursors(display, files)
    , wiiMenu(wiiMenuConsole)
    , screens([this](ScreenId screen, int argument) { return MakeScreen(screen, argument, services); })
    , keyboard(display, files, messages, clock, input, sounds)
    , program(titleContents)
    , settings(nandFiles)
    , dialogs(display, files, sounds)
    , errorScreen(display, program, fonts, fader)
    , frontPanel(display, powerService, screens, nandFiles)
    , homeButton(display, files, input, remoteService, cursors, fader, ConnectHomeButton())
    , nand(nandFiles)
    , services{display, files, messages, clock, fader, input, cursors, screens, sounds, keyboard, program, nand, settings, dialogs,
          frontPanel, wiiSettingsServices, homeButton, wiiMenu, titleLaunch}
{
    display.SetTextFonts(fonts.Fonts());
    services.consoleImport = consoleImport;
    if (wiiSettingsServices != nullptr)
        homeButton.SetSpeakerVolumeSetting(wiiSettingsServices->settings.GetSpeakerVolume());

    StartFacts facts = ReadStartFacts(nand, settings);
    facts.offersConsoleImport = consoleImport != nullptr && consoleImport->TakeOfferAtStart();
    RecordMenuStart(nand);

    FirstScreen firstScreen = ChooseFirstScreen(facts);
    if (IsMenuReachedAtStart(firstScreen.screen))
        frontPanel.NoteMenuReached();

    screens.Start(firstScreen.screen, firstScreen.argument);
}

void MenuSystem::Update()
{
    sounds.StartNextStep();
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
    bool isHomeMenuOpen = homeButton.IsOpen();
    if (!isHomeMenuOpen)
        screens.Update();

    if (!frontPanel.IsWorking() && !isHomeMenuOpen)
        keyboard.Update();

    if (!isHomeMenuOpen)
        dialogs.Update(input, fader.GetStatus() == FaderStatus::Clear);

    if (frontPanel.IsWorking())
        homeButton.HoldForFrontPanel();

    homeButton.Update();

    // The HOME menu pauses the music and the sounds while it is open (the sound events 0 and 4 of 0x81347AD0).
    if (homeButton.IsOpen() && !isHomeMenuOpen)
        sounds.PauseAllSounds();
    if (!homeButton.IsOpen() && isHomeMenuOpen)
        sounds.ResumeAllSounds();
    sounds.ChangeConsoleVolume(homeButton.SoundVolume());

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
    display.ForgetDrawnLayouts();
    if (!errorScreen.IsShown())
    {
        screens.Draw();
        keyboard.Draw();
        dialogs.Draw();
        homeButton.Draw();
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

    sounds.StopAllSounds(ErrorStopFadeSteps);
    errorScreen.Show(reason);
}

std::optional<Ios::TitleId> MenuSystem::TakeTitleToLaunch()
{
    std::optional<Ios::TitleId> titleOfFrontPanel = frontPanel.TakeTitleToLaunch();
    if (titleOfFrontPanel)
        return titleOfFrontPanel;

    return titleLaunch.Take();
}

const std::optional<std::string>& MenuSystem::FatalErrorMessage() const
{
    return fatalErrorMessage;
}

void MenuSystem::UsePhysicalKeyboard(Ios::KeyboardService& keyboardService)
{
    keyboard.UsePhysicalKeyboard(keyboardService);
}

// The HOME menu plays its sounds, draws its texts and starts a reset with the parts of the menu. Its volume of the speakers and its
// rumble choice are settings of the console (BT.SPKV and BT.MOT); PC Settings shows the rumble too.
HomeButtonConnections MenuSystem::ConnectHomeButton()
{
    HomeButtonConnections connections;
    connections.playSound = [this](HomeMenuSound sound) { sounds.Request(NameOfHomeMenuSound(sound)); };
    connections.textFonts = &fonts.Fonts();
    // A reset from the HOME menu stops every sound at once (0x8136BC14).
    connections.startReset = [this]()
    {
        sounds.StopAllSounds(0);
        frontPanel.PressReset();
    };
    connections.saveRemoteSettings = [this](std::uint8_t speakerVolumeSetting, bool isRumbleOn)
    {
        SaveRemoteSettings(speakerVolumeSetting, isRumbleOn);
    };
    return connections;
}

// The original writes both at every close of the HOME menu (WPADSaveConfig 0x8157B274); WeeOS writes SYSCONF only when one changed.
void MenuSystem::SaveRemoteSettings(std::uint8_t speakerVolumeSetting, bool isRumbleOn)
{
    if (services.wiiSettings == nullptr)
        return;

    Settings::SystemSettings& systemSettings = services.wiiSettings->settings;
    bool isUnchanged = systemSettings.GetSpeakerVolume() == speakerVolumeSetting && systemSettings.IsRumbleOn() == isRumbleOn;
    if (isUnchanged)
        return;

    systemSettings.SetSpeakerVolume(speakerVolumeSetting);
    systemSettings.SetRumble(isRumbleOn);
    systemSettings.Save();
}

MenuServices& MenuSystem::Services()
{
    return services;
}

}
