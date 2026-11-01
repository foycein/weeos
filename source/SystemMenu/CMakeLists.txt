add_library(SystemMenu STATIC
    CursorManager.cpp
    Display.cpp
    Fader.cpp
    FirstScreen.cpp
    HealthAndSafetyScreen.cpp
    InputManager.cpp
    LauncherScreen.cpp
    MenuAnimation.cpp
    MenuFiles.cpp
    MenuLayout.cpp
    MenuSystem.cpp
    OwnTitleContents.cpp
    Screen.cpp
    ScreenFactory.cpp
    ScreenManager.cpp
)

target_link_libraries(SystemMenu PUBLIC Formats Text Layout Input Ios)

add_subdirectory(BackgroundServices)
