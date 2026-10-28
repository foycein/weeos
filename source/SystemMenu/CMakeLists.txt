add_library(SystemMenu STATIC
    BackToWiiMenuScreen.cpp
    ButtonGroup.cpp
    ContinueWatcher.cpp
    CursorManager.cpp
    DialogManager.cpp
    Display.cpp
    ErrorScreen.cpp
    FatalError.cpp
    Fader.cpp
    FirstScreen.cpp
    FrontPanel.cpp
    HealthAndSafetyScreen.cpp
    InputManager.cpp
    IosNandFiles.cpp
    KeyboardDigitEntry.cpp
    LauncherScreen.cpp
    LimitOverScreen.cpp
    MenuAnimation.cpp
    MenuFiles.cpp
    MenuFonts.cpp
    MenuLayout.cpp
    MenuSettings.cpp
    MenuSystem.cpp
    OwnTitleContents.cpp
    ParentalDialogScreen.cpp
    ProgramImage.cpp
    Screen.cpp
    ScreenFactory.cpp
    ScreenManager.cpp
    StartupRouterScreen.cpp
)

target_link_libraries(SystemMenu PUBLIC Formats Text Layout Input Ios)
