add_library(Channels STATIC
    ChannelFiles.cpp
    ChannelSystem.cpp
    InternetChannel.cpp
    NativeChannels.cpp
    OfflineConnection.cpp
    OperaLanguageFile.cpp
    PagePainter.cpp
    ShopChannel.cpp
    WiiConnect24Channel.cpp
)

target_link_libraries(Channels PUBLIC SystemMenu)
