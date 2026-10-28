add_library(Settings STATIC
    SystemSettings.cpp
)

target_link_libraries(Settings PUBLIC Formats Ios)
