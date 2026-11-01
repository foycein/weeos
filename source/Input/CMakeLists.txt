add_library(Input STATIC
    InfraredPointer.cpp
    PointerSpace.cpp
    RemoteChannels.cpp
    SpeakerAdpcm.cpp
    VirtualRemote.cpp
    WiiRemote.cpp
    WiiRemoteReports.cpp
)

target_link_libraries(Input PUBLIC Formats Platform)
