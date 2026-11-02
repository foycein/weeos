#pragma once

#include "SystemMenu/CursorManager.h"
#include "SystemMenu/DialogManager.h"
#include "SystemMenu/Display.h"
#include "SystemMenu/ErrorScreen.h"
#include "SystemMenu/Fader.h"
#include "SystemMenu/FatalError.h"
#include "SystemMenu/FrontPanel.h"
#include "SystemMenu/HomeMenu/HomeButton.h"
#include "SystemMenu/InputManager.h"
#include "SystemMenu/MenuClock.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/MenuFonts.h"
#include "SystemMenu/MenuServices.h"
#include "SystemMenu/MenuSettings.h"
#include "SystemMenu/MessageBoard/FullBoard.h"
#include "SystemMenu/NandFiles.h"
#include "SystemMenu/ProgramImage.h"
#include "SystemMenu/ScreenManager.h"
#include "SystemMenu/SoftKeyboard.h"
#include "SystemMenu/SoundRequests.h"
#include "SystemMenu/TitleContents.h"
#include "SystemMenu/TurnOffQuestion.h"
#include "SystemMenu/WiiMenu/WiiMenuData.h"
#include "SystemMenu/TitleLaunchRequest.h"

#include "Ios/Power/PowerService.h"
#include "Ios/Remote/RemoteService.h"
#include "Text/MessageCatalog.h"

#include <memory>
#include <optional>
#include <string>

namespace SystemMenu
{

// The System Menu: it makes the managers that the screens work with and runs one step and one picture at a time.
// This is the System object of the original (iplSystem.cpp).
class MenuSystem
{
public:
    // Reads the resources of the menu from the contents of the System Menu title, chooses the first screen from the files of the NAND
    // and starts it. The device, the contents, the files, the services and the console of the Wii Menu must outlive the menu.
    // Wii Settings opens only when its services are given; the other screens work without them.
    MenuSystem(Graphics::GraphicsDevice& device, TitleContents& titleContents, NandFiles& nandFiles, Ios::RemoteService& remoteService,
        Ios::PowerService& powerService, MenuConsole& wiiMenuConsole,
        SettingsFlowServices* wiiSettingsServices = nullptr, ConsoleImport* consoleImport = nullptr);

    // One step of the console's picture, 1001/60000 of a second: reads the remotes, updates the screens, the hands and the fader.
    // A MenuError that a screen throws ends the menu with the error screen.
    void Update();

    // Starts a new picture of the screen and draws it. The program shows it with Present.
    void Draw();

    // Draws the picture into the picture that is open, for example an offscreen picture.
    void DrawScene();

    // The menu stops and shows the text of the reason, until the console is switched off.
    void ShowFatalError(FatalError reason);

    // What went wrong, when an error ended the menu. For the log of the program.
    const std::optional<std::string>& FatalErrorMessage() const;

    // The title that the console should start now: the menu again after RESET, or the channel that a screen asked for
    // (MenuServices::titleLaunch). The program ends the menu and launches it. Nothing most of the time.
    std::optional<Ios::TitleId> TakeTitleToLaunch();

    // Asks "Turn off WeeOS?" over everything; the menu stands still until the user answers, and Yes presses the POWER button.
    // The program asks when the user closes the window.
    void AskToTurnOff();
    bool IsAskingToTurnOff() const;

    // The keys of this USB keyboard type into the soft keyboard. The service must stay alive as long as the menu.
    void UsePhysicalKeyboard(Ios::KeyboardService& keyboardService);

    MenuServices& Services();

    // The work on a nearly full Message Board while it runs, for the checks of the tests. Nothing otherwise.
    const FullBoardProcedure* BoardMakingRoom() const;

private:
    void RunStep();
    void RunStepOfTheTurnOffQuestion();
    bool MustTheBoardMakeRoom() const;
    void RunStepOfMakingRoom();
    void DrawTheTurnOffQuestion();
    HomeButtonConnections ConnectHomeButton();
    void SaveRemoteSettings(std::uint8_t speakerVolumeSetting, bool isRumbleOn);

    Display display;
    MenuFiles files;
    MenuFonts fonts;
    Text::MessageCatalog messages;
    MenuClock clock;
    Fader fader;
    InputManager input;
    CursorManager cursors;
    WiiMenuData wiiMenu;
    ScreenManager screens;
    SoundRequests sounds;
    SoftKeyboard keyboard;
    ProgramImage program;
    MenuSettings settings;
    DialogManager dialogs;
    TurnOffQuestion turnOffQuestion;
    ErrorScreen errorScreen;
    FrontPanel frontPanel;
    HomeButton homeButton;
    NandFiles& nand;
    TitleLaunchRequest titleLaunch;
    MenuServices services;
    std::unique_ptr<FullBoardProcedure> fullBoard;
    std::optional<std::string> fatalErrorMessage;
};

}
