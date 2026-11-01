#pragma once

#include "Channels/WiiSpeak/SceneLayout.h"
#include "Channels/WiiSpeak/WiiSpeakSession.h"
#include "Channels/WiiSpeak/WiiSpeakWater.h"

#include <memory>
#include <optional>

namespace Channels
{

// Wii Speak Settings (scene setMenu, 0x800A27F8 of the program): Sensitivity Settings, Headphone Settings, Performance Check and
// Precautions, with Back and Help, on the water of the menus.
class SettingsMenuScreen : public WiiSpeakScreen
{
public:
    explicit SettingsMenuScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

private:
    WiiSpeakSession& session;
    std::unique_ptr<WiiSpeakWater> water;
    SceneLayout layout;
};

// Sensitivity Settings (scene micSet, 0x80091A54): the meter of what the microphone hears, the sensitivity from 1 to 5 with the arrows
// or + and -, and OK, which saves it in the save file. The microphone takes every new step at once.
class SensitivityScreen : public WiiSpeakScreen
{
public:
    explicit SensitivityScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    int ShownSensitivity() const;
    float ShownMeterFrame() const;

private:
    void ChangeSensitivity(int step);
    void ShowSensitivity();

    WiiSpeakSession& session;
    SceneLayout layout;
    int sensitivity;
    float meterFrame = 0.0f;
};

// Headphone Settings (scene hpSet): Yes or No, then OK and the window that tells the new mode. The mode is not saved.
class HeadphoneScreen : public WiiSpeakScreen
{
public:
    explicit HeadphoneScreen(WiiSpeakSession& session);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

private:
    void ShowChoice();

    WiiSpeakSession& session;
    SceneLayout layout;
    bool choosesHeadphones;
    bool isTellingTheMode = false;
};

// Wii Speak Precautions (scene attention): eight pages with Back and Next. The last Next, or Back on the first page, leaves.
class PrecautionsScreen : public WiiSpeakScreen
{
public:
    PrecautionsScreen(WiiSpeakSession& session, WiiSpeakScreenName screenAfterwards);

    std::optional<WiiSpeakScreenName> Update() override;
    void Draw() override;
    std::vector<SceneLayout*> ShownLayouts() override;

    int ShownPage() const;

private:
    void ShowPage();

    WiiSpeakSession& session;
    SceneLayout layout;
    WiiSpeakScreenName nextScreen;
    int page = 1;
};

}
