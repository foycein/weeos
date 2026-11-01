#pragma once

#include "SystemMenu/SoundRequests.h"

#include "Sound/SoundSystem.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SystemMenu
{

// The sound archive of the menu inside its resource content.
constexpr std::string_view MenuSoundArchivePath = "sound/IplSound.brsar";

// The sound object of the menu (0x8108992C, its functions 0x8136B280 to 0x8136C7C8): it plays what the screens asked for in a step,
// through the sound systems of the Sound part. It keeps a handle for every effect, one for the music and one for the file of the wave
// player (0x81089F68), as the original does. A channel plays its sounds through one of its own, over the sound systems of its archives:
// a name plays from the first archive that has it.
//
// The rules of the original that it follows:
// * An effect that is asked for again while it still plays stops at once and starts again (0x8136B46C). The error beep and the
//   sound of a gray button are not started again while they play.
// * The music starts from its beginning. Pausing and going on do nothing before the music has been started once (0x8136BD8C).
// * Stopping all sounds and pausing them shut the reverb down; going on and starting the music set it up again.
// * The wave player plays one file at a time; pausing, going on and stopping all sounds take it along (0x8140E9C8, 0x8140E918).
class MenuSound
{
public:
    // The sound systems must outlive this object. Their reverb is set up, as the start of the original does (0x8136B280).
    explicit MenuSound(Sound::SoundSystem& soundSystem);
    explicit MenuSound(std::vector<Sound::SoundSystem*> soundSystems);

    // Plays what the screens asked for in the step that has just run.
    void PlayRequests(const SoundRequests& requests);

    // The handles of the music and of the file of the wave player, which name no sound before they were started.
    Sound::SoundHandle MusicHandle() const;
    Sound::SoundHandle WaveFileHandle() const;

private:
    // A sound that was started, and the sound system that plays it.
    struct StartedSound
    {
        Sound::SoundSystem* system = nullptr;
        Sound::SoundHandle handle;
    };

    struct FoundSound
    {
        Sound::SoundSystem* system;
        std::size_t number;
    };

    std::optional<FoundSound> FindSound(std::string_view name) const;
    void Stop(const StartedSound& sound, int fadeSteps);
    void PlayEffect(const std::string& name);
    void StartMusic(const std::string& name);
    void StopMusic(int fadeSteps);
    void ChangeMusicVolume(float volume, int fadeSteps);
    void PauseAllSounds(int fadeSteps);
    void ResumeAllSounds(int fadeSteps);
    void StopAllSounds(int fadeSteps);
    void ChangeConsoleVolume(float volume);
    void PlayWaveFile(const Formats::Bytes& file);
    void StopWaveFile(int fadeSteps);

    std::vector<Sound::SoundSystem*> systems;
    std::map<std::string, StartedSound, std::less<>> effects;
    StartedSound music;
    StartedSound waveFile;

    // The wave of the file that plays. The menu holds it too, so that the audio thread seldom has to free it.
    std::shared_ptr<const Sound::DecodedWave> waveOfTheFile;
    bool hasStartedMusic = false;
};

// The sounds that the original does not start again while they still play (the numbers 0x35 and 0x39 that 0x8136B46C compares).
bool IsPlayedToItsEnd(std::string_view effectName);

}
