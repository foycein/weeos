add_library(Ios STATIC
    IosError.cpp
    IosServices.cpp
)
target_link_libraries(Ios PUBLIC Formats)

# Every service has its own folder with its own list of files.
add_subdirectory(FileSystem)
