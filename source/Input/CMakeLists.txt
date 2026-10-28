add_library(Input STATIC
    PointerSpace.cpp
    RemoteChannels.cpp
    VirtualRemote.cpp
)

target_link_libraries(Input PUBLIC Formats Platform)
