add_library(SharpArt STATIC
    ChannelBannerPictures.cpp
    ClockPictures.cpp
    CursorPictures.cpp
    DataManagementPictures.cpp
    DialogPictures.cpp
    HomeMenuPictures.cpp
    InternetChannelPictures.cpp
    Paint.cpp
    RemotePictures.cpp
    SharpPictures.cpp
    SmallWords.cpp
    VectorCanvas.cpp
    VectorPath.cpp
    WiiLogoPictures.cpp
    WiiMenuPictures.cpp
    WiiOptionsPictures.cpp
)

target_link_libraries(SharpArt PUBLIC Formats)
