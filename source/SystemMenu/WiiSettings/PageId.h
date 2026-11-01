#pragma once

namespace SystemMenu
{

// The pages of Wii Settings that are built. The original has them as files of a web site (docs/research/SystemMenuStructure.md 4.9 and Appendix B).
enum class PageId
{
    // The three pages that lead to all the others.
    FirstIndex,
    SecondIndex,
    ThirdIndex,

    // Calendar.
    CalendarIndex,
    DateSet,
    TimeSet,

    // Screen.
    DisplayIndex,
    ScreenPosition,
    WidescreenSet,
    TvResolution,
    BurnInReduction,

    // Sound.
    SoundSet,

    // Sensor Bar.
    SensorBarIndex,
    SensorBarPositionSet,
    SensitivityIntroduction,
    SensitivitySet,

    // Console Nickname.
    NicknameSet,

    // Language.
    LanguageSet,

    // Country.
    CountrySelect,

    // Format Wii System Memory.
    FormatWarning,
    FormatAccountWarning,
    FormatConfirm,
    FormatWorking,
    FormatDone,
    FormatFailed,

    // Wii System Update.
    UpdateIndex,
    UpdateNeedsConnection,

    // The test of a connection, for Internet and for the update.
    ConnectionTesting,
    ConnectionTestFailed,

    // Internet.
    InternetIndex,
    ConnectionList,
    ConnectionKind,
    WirelessSetup,
    AccessPointSearch,
    NoAccessPointFound,
    AccessPointList,
    SecurityKeyEntry,
    ConnectionSaved,
    UsbConnectorIntroduction,
    UsbConnectorWaiting,
    AossWaiting,
    ConnectionActions,
    ClearConnectionConfirm,
    ConnectionInUse,
    ConnectionTestStart,
    ConsoleInformation,

    // Parental Controls: the setup, one step after the other.
    ParentalUseQuestion,
    ParentalDescription,
    ParentalGuardianNote,
    ParentalPinChoice,
    ParentalPinRepeat,
    ParentalPinMismatch,
    ParentalQuestionChoice,
    ParentalAnswerChoice,

    // Parental Controls: what can be changed, once the PIN is set.
    ParentalSettingsMenu,
    ParentalSettingsOverview,
    ParentalRatingChoice,
    ParentalRestriction,
    ParentalClearQuestion,
    ParentalCleared,

    // Parental Controls that are on: the PIN, the answer or the master key before anything can be changed.
    ParentalChangeQuestion,
    ParentalPinCheck,
    ParentalPinWrong,
    ParentalAnswerCheck,
    ParentalAnswerWrong,
    ParentalMasterKeyInformation,
    ParentalMasterKeyEntry,
    ParentalMasterKeyWrong,
    ParentalDisabled,

    // PC Settings: the window, the sound and the keys of WeeOS on the PC.
    PcIndex,
    PcScreen,
    PcWindowSize,
    PcSound,
    PcSoundDevice,
    PcKeys,
    PcMouse,
};

}
