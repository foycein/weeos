add_library(Boot STATIC
    BootInformation.cpp
    BootSelection.cpp
    ConsoleStartup.cpp
    IosBootContent.cpp
    IosStartup.cpp
    LaunchFile.cpp
    TitleLauncher.cpp
)
target_link_libraries(Boot PUBLIC Formats Ios)
