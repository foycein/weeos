add_library(SystemMenu STATIC
    CursorManager.cpp
    Display.cpp
    Fader.cpp
    FirstScreen.cpp
    HealthAndSafetyScreen.cpp
    InputManager.cpp
    IosMenuConsole.cpp
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
)

add_subdirectory(WiiMenu)

target_link_libraries(SystemMenu PUBLIC Formats Text Layout Input Ios)
