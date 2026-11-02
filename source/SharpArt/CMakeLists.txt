add_library(SharpArt STATIC
    CardPerspective.cpp
    ChannelBannerPictures.cpp
    ClockPictures.cpp
    CursorPictures.cpp
    DataManagementPictures.cpp
    DialogPictures.cpp
    HomeMenuPictures.cpp
    InternetChannelPictures.cpp
    KeyboardPictures.cpp
    MessageBoardPictures.cpp
    Paint.cpp
    RemotePictures.cpp
    SettingsPictures.cpp
    SharpPictures.cpp
    ShopChannelPictures.cpp
    SmallWords.cpp
    VectorCanvas.cpp
    VectorPath.cpp
    WaitingMarkPictures.cpp
    WiiLogoPictures.cpp
    WiiMenuPictures.cpp
    WiiOptionsPictures.cpp
)

target_link_libraries(SharpArt PUBLIC Formats)
