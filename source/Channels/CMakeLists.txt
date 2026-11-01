add_library(Channels STATIC
    ChannelFiles.cpp
    ChannelSystem.cpp
    NativeChannels.cpp
    OfflineConnection.cpp
    WiiConnect24Channel.cpp
)

target_link_libraries(Channels PUBLIC SystemMenu)
