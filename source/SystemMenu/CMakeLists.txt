add_library(SystemMenu STATIC
    ButtonController.cpp
    CursorManager.cpp
    Display.cpp
    Fader.cpp
    FirstScreen.cpp
    HealthAndSafetyScreen.cpp
    InputManager.cpp
    KeyboardView.cpp
    LauncherScreen.cpp
    MenuAnimation.cpp
    MenuFiles.cpp
    MenuFonts.cpp
    MenuLayout.cpp
    MenuSystem.cpp
    OwnTitleContents.cpp
    PhysicalKeyboard.cpp
    Screen.cpp
    ScreenFactory.cpp
    ScreenManager.cpp
    SoftKeyboard.cpp
    TextField.cpp
    UsKeyboardLayout.cpp
    WiiOptionsBackgroundScreen.cpp
    WiiOptionsBarScreen.cpp
    WiiOptionsChoicesScreen.cpp
)

add_subdirectory(DataManagement)
add_subdirectory(SdCardMenu)

target_link_libraries(SystemMenu PUBLIC Formats Text Layout Input Ios)
