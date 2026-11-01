#pragma once

namespace SystemMenu
{

// The pages of Wii Settings. The original has them as files of a web site (docs/research/SystemMenuStructure.md 4.9 and Appendix B);
// WeeOS keeps the pages that belong to the Wii and adds the settings of the PC (docs/DECISIONS.md, Settings).
enum class PageId
{
    // The page that leads to all the others.
    Index,

    // Console Nickname.
    NicknameSet,

    // Sensor Bar.
    SensorBarIndex,
    SensorBarPositionSet,
    SensitivityIntroduction,
    SensitivitySet,

    // Internet: the connection of the PC and its test.
    ConnectionStatus,
    ConnectionTesting,
    ConnectionTestPassed,
    ConnectionTestFailed,

    // PC Settings: the window, the sound and the controls of WeeOS on the PC.
    PcIndex,
    PcWindow,
    PcWindowSize,
    PcSound,
    PcSoundDevice,
    PcControls,
    PcKeys,
    PcMouse,
    PcControllers,
    PcGamepadButtons,

    // PC Settings: the import of the person's own Wii.
    PcImport,
    PcImporting,
    PcImportFailed,

    // The sound mode of the console: mono, stereo or surround. It is a page of PC Settings.
    SoundSet,
};

}
