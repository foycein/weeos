add_library(Input STATIC
    GamepadRemote.cpp
    InfraredPointer.cpp
    JoyCon.cpp
    JoyConRemote.cpp
    JoyConReports.cpp
    PointerSpace.cpp
    RealControllers.cpp
    RemoteChannels.cpp
    SharedRemote.cpp
    SpeakerAdpcm.cpp
    StickPointer.cpp
    VirtualRemote.cpp
    WiiRemote.cpp
    WiiRemoteReports.cpp
)

target_link_libraries(Input PUBLIC Formats Platform)
