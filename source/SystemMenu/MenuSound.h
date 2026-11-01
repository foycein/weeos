#pragma once

#include "SystemMenu/SoundRequests.h"

#include "Sound/SoundSystem.h"

#include <map>
#include <string>
#include <string_view>

namespace SystemMenu
{

// The sound archive of the menu inside its resource content.
constexpr std::string_view MenuSoundArchivePath = "sound/IplSound.brsar";

// The sound object of the menu (0x8108992C, its functions 0x8136B280 to 0x8136C7C8): it plays what the screens asked for in a step,
// through the sound system of the Sound part. It keeps a handle for every effect and one for the music, as the original does.
//
// The rules of the original that it follows:
// * An effect that is asked for again while it still plays stops at once and starts again (0x8136B46C). The error beep and the
//   sound of a gray button are not started again while they play.
// * The music starts from its beginning. Pausing and going on do nothing before the music has been started once (0x8136BD8C).
// * Stopping all sounds and pausing them shut the reverb of the menu down; going on and starting the music set it up again.
class MenuSound
{
public:
    // The sound system must outlive this object. The reverb of the menu is set up, as the start of the original does (0x8136B280).
    explicit MenuSound(Sound::SoundSystem& soundSystem);

    // Plays what the screens asked for in the step that has just run.
    void PlayRequests(const SoundRequests& requests);

    // The handle of the music, which names no sound before the music was started.
    Sound::SoundHandle MusicHandle() const;

private:
    void PlayEffect(const std::string& name);
    void StartMusic();
    void StopMusic(int fadeSteps);
    void ChangeMusicVolume(float volume, int fadeSteps);
    void PauseAllSounds(int fadeSteps);
    void ResumeAllSounds(int fadeSteps);
    void StopAllSounds(int fadeSteps);

    Sound::SoundSystem& sounds;
    std::map<std::string, Sound::SoundHandle, std::less<>> effectHandles;
    Sound::SoundHandle musicHandle;
    bool hasStartedMusic = false;
};

// The sounds that the original does not start again while they still play (the numbers 0x35 and 0x39 that 0x8136B46C compares).
bool IsPlayedToItsEnd(std::string_view effectName);

}
