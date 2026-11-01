add_library(Platform STATIC
    ErrorMessage.cpp
    FileLocations.cpp
    AspectLock.cpp
    FilePicker.cpp
    FitPicture.cpp
    KeyCodes.cpp
    MessagePump.cpp
    PreciseTimer.cpp
    RegionalSettings.cpp
    Window.cpp
)

target_compile_definitions(Platform PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_definitions(Platform PRIVATE UNICODE _UNICODE)
target_link_libraries(Platform PUBLIC user32 ole32)
