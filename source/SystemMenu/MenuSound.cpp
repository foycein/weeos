#include "SystemMenu/MenuSound.h"

#include "SystemMenu/MenuClock.h"

#include <optional>

namespace SystemMenu
{

namespace
{

constexpr std::string_view ErrorBeep = "WIPL_SE_ERROR";
constexpr std::string_view GrayButton = "WIPL_SE_GRAY_BUTTON";

double SecondsOf(int steps)
{
    return double(steps) * MenuClock::SecondsPerStep;
}

}

bool IsPlayedToItsEnd(std::string_view effectName)
{
    return effectName == ErrorBeep || effectName == GrayButton;
}

MenuSound::MenuSound(Sound::SoundSystem& soundSystem)
    : sounds(soundSystem)
{
    sounds.SetReverbEnabled(true);
}

void MenuSound::PlayRequests(const SoundRequests& requests)
{
    for (const SoundRequest& request : requests.AllRequestsInThisStep())
    {
        switch (request.action)
        {
        case SoundAction::PlayEffect: PlayEffect(request.effectName); break;
        case SoundAction::StartMusic: StartMusic(); break;
        case SoundAction::StopMusic: StopMusic(request.fadeSteps); break;
        case SoundAction::ChangeMusicVolume: ChangeMusicVolume(request.volume, request.fadeSteps); break;
        case SoundAction::PauseAllSounds: PauseAllSounds(request.fadeSteps); break;
        case SoundAction::ResumeAllSounds: ResumeAllSounds(request.fadeSteps); break;
        case SoundAction::StopAllSounds: StopAllSounds(request.fadeSteps); break;
        }
    }
}

Sound::SoundHandle MenuSound::MusicHandle() const
{
    return musicHandle;
}

// A name that the archive does not have plays nothing, as in the original.
void MenuSound::PlayEffect(const std::string& name)
{
    std::optional<std::size_t> soundNumber = sounds.Archive().FindSoundNumber(name);
    if (!soundNumber)
        return;

    if (IsPlayedToItsEnd(name))
    {
        effectHandles[name] = sounds.PlayIfNotPlaying(*soundNumber);
        return;
    }

    auto earlier = effectHandles.find(name);
    if (earlier != effectHandles.end())
        sounds.Stop(earlier->second);

    effectHandles[name] = sounds.Play(*soundNumber);
}

void MenuSound::StartMusic()
{
    std::optional<std::size_t> soundNumber = sounds.Archive().FindSoundNumber(SoundName::Music);
    if (!soundNumber)
        return;

    sounds.SetReverbEnabled(true);
    musicHandle = sounds.Play(*soundNumber);
    hasStartedMusic = true;
}

void MenuSound::StopMusic(int fadeSteps)
{
    sounds.Stop(musicHandle, SecondsOf(fadeSteps));
}

void MenuSound::ChangeMusicVolume(float volume, int fadeSteps)
{
    sounds.ChangeVolume(musicHandle, volume, SecondsOf(fadeSteps));
}

void MenuSound::PauseAllSounds(int fadeSteps)
{
    if (!hasStartedMusic)
        return;

    sounds.PauseRunningSounds(SecondsOf(fadeSteps));
    sounds.SetReverbEnabled(false);
}

void MenuSound::ResumeAllSounds(int fadeSteps)
{
    if (!hasStartedMusic)
        return;

    sounds.SetReverbEnabled(true);
    sounds.ResumePausedSounds(SecondsOf(fadeSteps));
}

void MenuSound::StopAllSounds(int fadeSteps)
{
    sounds.StopAll(SecondsOf(fadeSteps));
    sounds.SetReverbEnabled(false);
    effectHandles.clear();
}

}
