add_library(Channels STATIC
    ChannelFiles.cpp
    ChannelSystem.cpp
    InternetChannel.cpp
    NativeChannels.cpp
    OfflineConnection.cpp
    OperaLanguageFile.cpp
    PagePainter.cpp
    Photo/FunScreens.cpp
    Photo/MoodFilters.cpp
    Photo/PhotoChannel.cpp
    Photo/PhotoLibrary.cpp
    Photo/PhotoPictures.cpp
    Photo/PhotoSession.cpp
    Photo/PhotoStorage.cpp
    Photo/PostScreens.cpp
    Photo/PuzzleBoard.cpp
    Photo/PuzzleScreens.cpp
    Photo/SaveErrorScreen.cpp
    Photo/ShownPicture.cpp
    Photo/SlideSettingsScreen.cpp
    Photo/SlideShowMusic.cpp
    Photo/SlideShowScreen.cpp
    Photo/ThumbnailScreen.cpp
    Photo/TitleScreens.cpp
    Photo/ViewScreen.cpp
    ShopChannel.cpp
    WiiConnect24Channel.cpp
)

target_link_libraries(Channels PUBLIC SystemMenu Media Sound)
