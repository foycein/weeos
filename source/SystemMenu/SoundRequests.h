#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The names of the sounds that the screens ask for, as they are named in the sound archive IplSound.brsar
// (docs/research/SystemMenuStructure.md 6.5).
namespace SoundName
{

constexpr std::string_view ButtonTargeted = "WIPL_SE_BT_TARGETTING";
constexpr std::string_view ButtonPushed = "WIPL_SE_BT_PUSH";
constexpr std::string_view Decide = "WIPL_SE_DECIDE";
constexpr std::string_view Cancel = "WIPL_SE_CANCEL";

// A window with a message opens; the menu is busy (it goes on until another sound of its player takes its place); the work is done.
constexpr std::string_view InfoWindow = "WIPL_SE_INFO_WINDOW";
constexpr std::string_view Busy = "WIPL_SE_COPYING";
constexpr std::string_view BusyFinished = "WIPL_SE_COPY_FINISH";

// The music of the menu, the one long sound of the archive. It goes on until it is stopped.
constexpr std::string_view Music = "WIPL_BGM_MENU";

}

// The steps that the original gives the fades of its music and its sounds (a step is 1001/60000 of a second).
// The music falls silent on the Sound page of Wii Settings, and comes back when the page is left (0x813F20AC, 90 steps).
constexpr int MusicFadeStepsOfTheSoundPage = 90;

// A channel that is pressed in the Wii Menu stops the music in 5 steps (0x813ADA94); the music starts again from its beginning when the
// channel start window has closed (0x813AC7D8).
constexpr int MusicStopStepsOfAChannel = 5;

// The music and the sounds are paused, and go on, in 5 steps (0x8136BD8C and 0x8136BE34).
constexpr int PauseFadeSteps = 5;

// The error screen stops every sound in 5 steps (0x81334F0C). Before a title starts and before the menu starts again, the
// screens stop them in 20 steps (0x813B5130, 0x813F7DA4, 0x81409988).
constexpr int ErrorStopFadeSteps = 5;
constexpr int LeavingStopFadeSteps = 20;

enum class SoundAction
{
    PlayEffect,
    StartMusic,
    StopMusic,
    ChangeMusicVolume,
    PauseAllSounds,
    ResumeAllSounds,
    StopAllSounds,
};

struct SoundRequest
{
    SoundAction action = SoundAction::PlayEffect;

    // The name of the effect that PlayEffect plays.
    std::string effectName;

    // The new volume of ChangeMusicVolume: 1 is the volume of the archive, 0 is silence.
    float volume = 1.0f;

    // In how many steps the stop or the change of the volume happens. 0 is at once.
    int fadeSteps = 0;
};

// What the screens want to hear in one step of the menu: the sound effects, which play once, and what happens to the music and to
// all the sounds. The sound object of the menu takes the requests after every step and plays them, in the order of the requests.
// These are the calls of the original's sound object 0x8108992C, which the screens make by name.
class SoundRequests
{
public:
    // Plays a sound effect of the archive by its name (0x8136B46C). The same effect again stops the one that still plays.
    void Request(std::string_view soundName)
    {
        effectNames.emplace_back(soundName);
        requests.push_back({.action = SoundAction::PlayEffect, .effectName = std::string(soundName)});
    }

    // Starts the music of the menu from its beginning (0x8136B3BC with WIPL_BGM_MENU). The reverb of the menu is set up again
    // when a stop of all sounds had shut it down, as the screens of the original do before they start the music again (0x8136B37C).
    void StartMusic()
    {
        requests.push_back({.action = SoundAction::StartMusic});
    }

    // Fades the music out in the steps given and stops it (0x8136BA34).
    void StopMusic(int fadeSteps)
    {
        requests.push_back({.action = SoundAction::StopMusic, .fadeSteps = fadeSteps});
    }

    // Moves the volume of the music to the new volume in the steps given (0x8136BD24 to 0, 0x8136BD58 to 1).
    void ChangeMusicVolume(float volume, int fadeSteps)
    {
        requests.push_back({.action = SoundAction::ChangeMusicVolume, .volume = volume, .fadeSteps = fadeSteps});
    }

    // The music and the effects that play now fall silent in PauseFadeSteps and are held; the reverb is shut down (0x8136BD8C).
    // The HOME menu does this when it opens. Effects that start later play.
    void PauseAllSounds()
    {
        requests.push_back({.action = SoundAction::PauseAllSounds, .fadeSteps = PauseFadeSteps});
    }

    // The held sounds go on and the reverb is set up again (0x8136BE34).
    void ResumeAllSounds()
    {
        requests.push_back({.action = SoundAction::ResumeAllSounds, .fadeSteps = PauseFadeSteps});
    }

    // Fades the music and every effect out in the steps given and stops them, and shuts the reverb down (0x8136BB10).
    void StopAllSounds(int fadeSteps)
    {
        requests.push_back({.action = SoundAction::StopAllSounds, .fadeSteps = fadeSteps});
    }

    // The names of the sound effects requested since the last call of StartNextStep, in the order in which they were requested.
    const std::vector<std::string>& RequestedInThisStep() const
    {
        return effectNames;
    }

    // Everything requested since the last call of StartNextStep, in order.
    const std::vector<SoundRequest>& AllRequestsInThisStep() const
    {
        return requests;
    }

    void StartNextStep()
    {
        effectNames.clear();
        requests.clear();
    }

private:
    std::vector<std::string> effectNames;
    std::vector<SoundRequest> requests;
};

}
