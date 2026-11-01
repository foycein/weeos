add_library(Channels STATIC
    ChannelFiles.cpp
    ChannelSystem.cpp
    InternetChannel.cpp
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
    WiiSpeak/ChatRoomScreen.cpp
    WiiSpeak/CreateMessageScreen.cpp
    WiiSpeak/MicrophoneLevel.cpp
    WiiSpeak/PerformanceCheck.cpp
    WiiSpeak/PerformanceCheckScreen.cpp
    WiiSpeak/SceneLayout.cpp
    WiiSpeak/SettingsScreens.cpp
    WiiSpeak/VoiceRecording.cpp
    WiiSpeak/WiiSpeakChannel.cpp
    WiiSpeak/WiiSpeakDialog.cpp
    WiiSpeak/WiiSpeakFiles.cpp
    WiiSpeak/WiiSpeakMessages.cpp
    WiiSpeak/WiiSpeakSaveData.cpp
    WiiSpeak/WiiSpeakSession.cpp
)

target_link_libraries(Channels PUBLIC SystemMenu Media Mii Sound)
