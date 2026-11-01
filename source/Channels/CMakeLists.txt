add_library(Channels STATIC
    ChannelFiles.cpp
    ChannelSystem.cpp
    InternetChannel.cpp
    MiiChannel/ButtonFocus.cpp
    MiiChannel/EditorScreen.cpp
    MiiChannel/LookAlikeScreen.cpp
    MiiChannel/MessageWindow.cpp
    MiiChannel/MiiChannel.cpp
    MiiChannel/MiiChannelData.cpp
    MiiChannel/MiiChannelFiles.cpp
    MiiChannel/MiiEditor.cpp
    MiiChannel/MiiFigures.cpp
    MiiChannel/MiiPlaza.cpp
    MiiChannel/ParadeScreen.cpp
    MiiChannel/PlazaGround.cpp
    MiiChannel/PlazaScreen.cpp
    MiiChannel/ProfileDialog.cpp
    NativeChannels.cpp
    OfflineConnection.cpp
    OperaLanguageFile.cpp
    PagePainter.cpp
    Photo/PhotoChannel.cpp
    Photo/PhotoLibrary.cpp
    Photo/PhotoPictures.cpp
    Photo/PhotoSession.cpp
    Photo/ShownPicture.cpp
    Photo/SlideShowScreen.cpp
    Photo/ThumbnailScreen.cpp
    Photo/TitleScreens.cpp
    Photo/ViewScreen.cpp
    ShopChannel.cpp
    WiiConnect24Channel.cpp
)

target_link_libraries(Channels PUBLIC SystemMenu Mii Media)
