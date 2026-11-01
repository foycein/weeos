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
};

}
