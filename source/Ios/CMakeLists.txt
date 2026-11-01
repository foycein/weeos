add_library(Ios STATIC
    IosError.cpp
    IosServices.cpp
)
target_link_libraries(Ios PUBLIC Formats Input)

# Every service has its own folder with its own list of files.
add_subdirectory(DiscDrive)
add_subdirectory(FileSystem)
add_subdirectory(Identity)
add_subdirectory(Keyboard)
add_subdirectory(Microphone)
add_subdirectory(Network)
add_subdirectory(Power)
add_subdirectory(Profile)
add_subdirectory(Remote)
add_subdirectory(SdCard)
add_subdirectory(Sockets)
add_subdirectory(Title)
