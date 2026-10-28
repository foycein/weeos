add_library(SystemMenu STATIC
    ButtonController.cpp
    CursorManager.cpp
    Display.cpp
    Fader.cpp
    FirstScreen.cpp
    HealthAndSafetyScreen.cpp
    InputManager.cpp
    LauncherScreen.cpp
    MenuAnimation.cpp
    MenuFiles.cpp
    MenuFonts.cpp
    MenuLayout.cpp
    MenuSystem.cpp
    OwnTitleContents.cpp
    Screen.cpp
    ScreenFactory.cpp
    ScreenManager.cpp
    WiiOptionsBackgroundScreen.cpp
    WiiOptionsBarScreen.cpp
    WiiOptionsChoicesScreen.cpp
)

target_link_libraries(SystemMenu PUBLIC Formats Text Layout Input Ios)
