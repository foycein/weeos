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
};

}
