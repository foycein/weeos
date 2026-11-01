#pragma once

#include "Formats/Bytes.h"

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

// The wave player of the menu plays its files at this volume (0x8136B280 sets 0.9).
constexpr float WavePlayerVolume = 0.9f;

// The banner sound of the channel start window fades out in 28 steps when the window closes or a channel starts (0x813B76D8, 0x813B9488).
constexpr int BannerSoundStopFadeSteps = 28;

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
    ChangeConsoleVolume,
    PlayWaveFile,
    PlayBannerSound,
    StopWaveFile,
};

struct SoundRequest
{
    SoundAction action = SoundAction::PlayEffect;

    // The name of the effect that PlayEffect plays, or of the music that StartMusic starts.
    std::string effectName;

    // The new volume of ChangeMusicVolume and ChangeConsoleVolume: 1 is full, 0 is silence.
    float volume = 1.0f;

    // The file that PlayWaveFile or PlayBannerSound plays.
    Formats::Bytes waveFile;

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

    // Starts the music of the menu from its beginning (0x8136B3BC with WIPL_BGM_MENU), or another music of the archives, for example the
    // music of a channel. The reverb is set up again when a stop of all sounds had shut it down, as the screens of the original do before
    // they start the music again (0x8136B37C).
    void StartMusic(std::string_view musicName = SoundName::Music)
    {
        requests.push_back({.action = SoundAction::StartMusic, .effectName = std::string(musicName)});
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

    // The master volume of the console (0x81377118): the HOME menu fades it out while it fades the picture out to end the program under
    // it. The volume is asked for only when it is not the one asked for last.
    void ChangeConsoleVolume(float volume)
    {
        if (volume == consoleVolume)
            return;

        consoleVolume = volume;
        requests.push_back({.action = SoundAction::ChangeConsoleVolume, .volume = volume});
    }

    // The wave player of the menu (the object 0x81089F68) plays one file at a time, a WAV or AIFF file attached to a message of the
    // Message Board or the sound of a channel's banner, at WavePlayerVolume (0x8140E800). A new file stops the one that plays at once.
    void PlayWaveFile(Formats::ByteView file)
    {
        requests.push_back({.action = SoundAction::PlayWaveFile, .waveFile = Formats::Bytes(file.begin(), file.end())});
    }

    // The file of the wave player fades out in the steps given and stops (0x8136C1F8).
    void StopWaveFile(int fadeSteps)
    {
        requests.push_back({.action = SoundAction::StopWaveFile, .fadeSteps = fadeSteps});
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

    // The sound of a channel's banner, its file meta/sound.bin, which the channel start window plays while it shows the banner
    // (0x8136C198), on the wave player of the menu. The player decodes it with Sound::ReadBannerSound; it loops when the sound loops.
    // It replaces a file that still plays.
    void StartBannerSound(Formats::ByteView soundFile)
    {
        requests.push_back({.action = SoundAction::PlayBannerSound, .waveFile = Formats::Bytes(soundFile.begin(), soundFile.end())});
    }

    // The banner sound fades out in BannerSoundStopFadeSteps (0x8136C1F8 with 28).
    void StopBannerSound()
    {
        StopWaveFile(BannerSoundStopFadeSteps);
    }

    void StartNextStep()
    {
        effectNames.clear();
        requests.clear();
    }

private:
    std::vector<std::string> effectNames;
    std::vector<SoundRequest> requests;
    float consoleVolume = 1.0f;
};

}
