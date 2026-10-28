add_library(Settings STATIC
    ConsoleClock.cpp
    SystemSettings.cpp
)

target_link_libraries(Settings PUBLIC Formats Ios)
