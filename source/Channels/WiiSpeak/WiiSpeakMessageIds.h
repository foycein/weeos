#pragma once

#include "Channels/WiiSpeak/WiiSpeakMessages.h"

namespace Channels::WiiSpeakText
{

// The messages that WeeOS shows, by the layout number (or message group) and the number in it, as the program asks for them.

// Group 1030: the words of the message windows.
constexpr std::uint32_t ReturningToWiiMenu = WiiSpeakMessageId(1030, 0);
constexpr std::uint32_t Ok = WiiSpeakMessageId(1030, 1);
constexpr std::uint32_t Yes = WiiSpeakMessageId(1030, 2);
constexpr std::uint32_t No = WiiSpeakMessageId(1030, 3);
constexpr std::uint32_t ConnectingToWiFiConnection = WiiSpeakMessageId(1030, 20);
constexpr std::uint32_t SaveDataCreated = WiiSpeakMessageId(1030, 21);
constexpr std::uint32_t WiiMenu = WiiSpeakMessageId(1030, 80);
constexpr std::uint32_t Next = WiiSpeakMessageId(2010, 1);

// Groups 502, 504 and 507: the errors of the system, of Nintendo Wi-Fi Connection and of the accessory.
constexpr std::uint32_t SaveDataCorrupted = WiiSpeakMessageId(502, 5);
constexpr std::uint32_t UnableToConnectToTheInternet = WiiSpeakMessageId(504, 6);
constexpr std::uint32_t AccessoryProblem = WiiSpeakMessageId(507, 2);
constexpr std::uint32_t AccessoryNotConnected = WiiSpeakMessageId(507, 4);

// Layout 2000: the buttons of the chat room; layout 34000: its help.
constexpr std::uint32_t ChatRoomButtonWiiMenu = WiiSpeakMessageId(2000, 0);
constexpr std::uint32_t ChatRoomButtonChooseMiis = WiiSpeakMessageId(2000, 1);
constexpr std::uint32_t ChatRoomButtonShowPhotos = WiiSpeakMessageId(2000, 2);
constexpr std::uint32_t ChatRoomButtonHelp = WiiSpeakMessageId(2000, 3);
constexpr std::uint32_t ChatRoomButtonWiiNumber = WiiSpeakMessageId(2000, 4);
constexpr std::uint32_t ChatRoomButtonAddressBook = WiiSpeakMessageId(2000, 8);
constexpr std::uint32_t ChatRoomButtonSettings = WiiSpeakMessageId(2000, 5);
constexpr std::uint32_t ChatRoomButtonCreateMessage = WiiSpeakMessageId(2000, 6);
constexpr std::uint32_t ChatRoomButtonLobby = WiiSpeakMessageId(2000, 7);
constexpr std::uint32_t Mute = WiiSpeakMessageId(2000, 15);
constexpr std::uint32_t Volume = WiiSpeakMessageId(2000, 16);
constexpr std::uint32_t ChangeVoice = WiiSpeakMessageId(2000, 17);
constexpr std::uint32_t Morning = WiiSpeakMessageId(2001, 0);
constexpr std::uint32_t Afternoon = WiiSpeakMessageId(2001, 1);
constexpr std::uint32_t January = WiiSpeakMessageId(2001, 4);
constexpr std::uint32_t Monday = WiiSpeakMessageId(2001, 16);
constexpr std::uint32_t ChatRoomGuide = WiiSpeakMessageId(34000, 13);

// Group 37000: the first start.
constexpr std::uint32_t Welcome = WiiSpeakMessageId(37000, 1);
constexpr std::uint32_t AboutChatting = WiiSpeakMessageId(37000, 2);
constexpr std::uint32_t AboutVoiceMessages = WiiSpeakMessageId(37000, 3);
constexpr std::uint32_t MiiChannelDataUnreadable = WiiSpeakMessageId(37000, 4);

// Layout 13100 and group 40000: a voice message.
constexpr std::uint32_t CreateMessageTitle = WiiSpeakMessageId(13100, 4);
constexpr std::uint32_t AddMii = WiiSpeakMessageId(13100, 5);
constexpr std::uint32_t CommentTitle = WiiSpeakMessageId(13100, 6);
constexpr std::uint32_t PhotoTitle = WiiSpeakMessageId(13100, 7);
constexpr std::uint32_t RecordVoiceHint = WiiSpeakMessageId(13100, 8);
constexpr std::uint32_t AddPhotoHint = WiiSpeakMessageId(13100, 9);
constexpr std::uint32_t Send = WiiSpeakMessageId(13100, 10);
constexpr std::uint32_t Back = WiiSpeakMessageId(13100, 11);
constexpr std::uint32_t Help = WiiSpeakMessageId(13100, 12);
constexpr std::uint32_t Addressees = WiiSpeakMessageId(13100, 20);
constexpr std::uint32_t CreateMessageHelp = WiiSpeakMessageId(40000, 2);
constexpr std::uint32_t QuitCreatingMessage = WiiSpeakMessageId(40000, 4);
constexpr std::uint32_t NoWiiFriends = WiiSpeakMessageId(40000, 5);
constexpr std::uint32_t NoMiisForMessage = WiiSpeakMessageId(40000, 6);
constexpr std::uint32_t ChooseAddressees = WiiSpeakMessageId(40000, 12);
constexpr std::uint32_t MutedCannotRecord = WiiSpeakMessageId(40000, 15);

// Layout 16000: the comment.
constexpr std::uint32_t Comment = WiiSpeakMessageId(16000, 0);
constexpr std::uint32_t Redo = WiiSpeakMessageId(16000, 1);
constexpr std::uint32_t BeginRecording = WiiSpeakMessageId(16000, 2);
constexpr std::uint32_t Cancel = WiiSpeakMessageId(16000, 3);
constexpr std::uint32_t Recording = WiiSpeakMessageId(16000, 4);
constexpr std::uint32_t CommentChangeVoice = WiiSpeakMessageId(16000, 5);
constexpr std::uint32_t CommentOk = WiiSpeakMessageId(16000, 20);

// Layout 19000: the sensitivity.
constexpr std::uint32_t SensitivityTitle = WiiSpeakMessageId(19000, 0);
constexpr std::uint32_t Low = WiiSpeakMessageId(19000, 1);
constexpr std::uint32_t High = WiiSpeakMessageId(19000, 2);
constexpr std::uint32_t OkZone = WiiSpeakMessageId(19000, 3);
constexpr std::uint32_t SensitivityHowTo = WiiSpeakMessageId(19000, 4);
constexpr std::uint32_t SensitivityOk = WiiSpeakMessageId(19000, 5);

// Layout 19010: the settings menu.
constexpr std::uint32_t SettingsSensitivity = WiiSpeakMessageId(19010, 0);
constexpr std::uint32_t SettingsHeadphones = WiiSpeakMessageId(19010, 1);
constexpr std::uint32_t SettingsCheck = WiiSpeakMessageId(19010, 2);
constexpr std::uint32_t SettingsPrecautions = WiiSpeakMessageId(19010, 3);
constexpr std::uint32_t SettingsTitle = WiiSpeakMessageId(19010, 4);
constexpr std::uint32_t SettingsBack = WiiSpeakMessageId(19010, 11);
constexpr std::uint32_t SettingsHelp = WiiSpeakMessageId(19010, 12);

// Layout 19100: the headphones.
constexpr std::uint32_t HeadphoneTitle = WiiSpeakMessageId(19100, 0);
constexpr std::uint32_t HeadphoneNo = WiiSpeakMessageId(19100, 1);
constexpr std::uint32_t HeadphoneYes = WiiSpeakMessageId(19100, 2);
constexpr std::uint32_t HeadphoneQuestion = WiiSpeakMessageId(19100, 4);
constexpr std::uint32_t HeadphoneOk = WiiSpeakMessageId(19100, 5);

// Layout 19200: the performance check.
constexpr std::uint32_t CheckTitle = WiiSpeakMessageId(19200, 0);
constexpr std::uint32_t CheckCancel = WiiSpeakMessageId(19200, 1);
constexpr std::uint32_t CheckBegin = WiiSpeakMessageId(19200, 2);
constexpr std::uint32_t CheckHowTo = WiiSpeakMessageId(19200, 4);
constexpr std::uint32_t CheckNotFunctioning = WiiSpeakMessageId(19200, 5);
constexpr std::uint32_t CheckFunctioningFineTune = WiiSpeakMessageId(19200, 6);
constexpr std::uint32_t CheckFunctioning = WiiSpeakMessageId(19200, 7);
constexpr std::uint32_t CheckTooQuiet = WiiSpeakMessageId(19200, 8);
constexpr std::uint32_t CheckTooLoud = WiiSpeakMessageId(19200, 9);
constexpr std::uint32_t CheckTryAgain = WiiSpeakMessageId(19200, 20);
constexpr std::uint32_t CheckFinish = WiiSpeakMessageId(19200, 21);
constexpr std::uint32_t CheckRefusedHeadphones = WiiSpeakMessageId(19200, 23);
constexpr std::uint32_t CheckRefusedMuted = WiiSpeakMessageId(19200, 24);
constexpr std::uint32_t CheckIntroduction = WiiSpeakMessageId(19200, 25);
constexpr std::uint32_t CheckRunning = WiiSpeakMessageId(19200, 26);
constexpr std::uint32_t HeadphonesOn = WiiSpeakMessageId(19200, 28);
constexpr std::uint32_t HeadphonesOff = WiiSpeakMessageId(19200, 29);
constexpr std::uint32_t SettingsHelpText = WiiSpeakMessageId(19200, 30);

// Layout 5000 and group 41000: choosing the Miis; group 37000: the Mii that the Mii Channel deleted.
constexpr std::uint32_t ChooseMiisPrompt = WiiSpeakMessageId(5000, 4);
constexpr std::uint32_t ChooseMiisHelp = WiiSpeakMessageId(5000, 3);
constexpr std::uint32_t ChooseMiisOk = WiiSpeakMessageId(5000, 20);
constexpr std::uint32_t NoMiisToChoose = WiiSpeakMessageId(41000, 1);
constexpr std::uint32_t ChooseMiisHelpText = WiiSpeakMessageId(41000, 3);
constexpr std::uint32_t ChosenMiiDeleted = WiiSpeakMessageId(37000, 6);

// The Wii number and the address book: layout 6000, groups 506 (what to type), 1030 (what the number is not), 34000 (the Wii number),
// 38000 (the help) and 503 (WiiConnect24 cannot reach the Internet).
constexpr std::uint32_t AddressBookTitle = WiiSpeakMessageId(6000, 0);
constexpr std::uint32_t AddWiiFriend = WiiSpeakMessageId(6000, 1);
constexpr std::uint32_t AddressBookOk = WiiSpeakMessageId(6000, 2);
constexpr std::uint32_t AddressBookHelp = WiiSpeakMessageId(6000, 3);
constexpr std::uint32_t AddressBookHelpText = WiiSpeakMessageId(38000, 3);
constexpr std::uint32_t EnterWiiNumber = WiiSpeakMessageId(506, 2);
constexpr std::uint32_t EnterNickname = WiiSpeakMessageId(506, 3);
constexpr std::uint32_t WiiNumberIncorrect = WiiSpeakMessageId(1030, 22);
constexpr std::uint32_t AddressBookFull = WiiSpeakMessageId(1030, 26);
constexpr std::uint32_t WiiNumberAlreadyRegistered = WiiSpeakMessageId(1030, 28);
constexpr std::uint32_t OwnWiiNumberCannotBeRegistered = WiiSpeakMessageId(1030, 29);
constexpr std::uint32_t KeyboardBack = WiiSpeakMessageId(1030, 82);
constexpr std::uint32_t YourWiiNumber = WiiSpeakMessageId(34000, 12);
constexpr std::uint32_t WiiConnect24CannotReachTheInternet = WiiSpeakMessageId(503, 5);

// Group 50000: Show Photos.
constexpr std::uint32_t PhotoNotSupported = WiiSpeakMessageId(50000, 3);
constexpr std::uint32_t ZoomOut = WiiSpeakMessageId(50000, 7);
constexpr std::uint32_t ZoomIn = WiiSpeakMessageId(50000, 8);
constexpr std::uint32_t ChangePhotoLocation = WiiSpeakMessageId(50000, 9);
constexpr std::uint32_t FinishThumbnails = WiiSpeakMessageId(50000, 10);
constexpr std::uint32_t ChoosePhotoLocation = WiiSpeakMessageId(50000, 23);
constexpr std::uint32_t PhotoBack = WiiSpeakMessageId(50000, 24);
constexpr std::uint32_t DigitalCameraOrCellPhone = WiiSpeakMessageId(50000, 25);
constexpr std::uint32_t SdCardsOnly = WiiSpeakMessageId(50000, 26);
constexpr std::uint32_t WiiMessageBoardPhotos = WiiSpeakMessageId(50000, 72);
constexpr std::uint32_t NoPhotosOnTheMessageBoard = WiiSpeakMessageId(50000, 82);
constexpr std::uint32_t PhotoThumbnails = WiiSpeakMessageId(50000, 86);
constexpr std::uint32_t FinishPhotoSelection = WiiSpeakMessageId(50000, 14);
constexpr std::uint32_t InsertTheSdCard = WiiSpeakMessageId(50000, 30);
constexpr std::uint32_t SdCardUnreadable = WiiSpeakMessageId(50000, 34);
constexpr std::uint32_t NoPhotosOnTheSdCard = WiiSpeakMessageId(50000, 42);
constexpr std::uint32_t PhotoPrevious = WiiSpeakMessageId(50000, 84);
constexpr std::uint32_t PhotoNext = WiiSpeakMessageId(50000, 85);
constexpr std::uint32_t ShowThisPhoto = WiiSpeakMessageId(50000, 87);
constexpr std::uint32_t PhotoFinish = WiiSpeakMessageId(50000, 90);

// Layout 20001: the precautions.
constexpr std::uint32_t PrecautionsBack = WiiSpeakMessageId(20001, 1);
constexpr std::uint32_t PrecautionsNext = WiiSpeakMessageId(20001, 2);
constexpr std::uint32_t PrecautionsClose = WiiSpeakMessageId(20001, 80);

}
