#include "SystemMenu/MenuSound.h"

#include "SystemMenu/MenuClock.h"

#include "Formats/FormatError.h"
#include "Sound/AudioFile.h"
#include "Sound/BannerSound.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view ErrorBeep = "WIPL_SE_ERROR";
constexpr std::string_view GrayButton = "WIPL_SE_GRAY_BUTTON";

double SecondsOf(int steps)
{
    return static_cast<double>(steps) * MenuClock::SecondsPerStep;
}

}

bool IsPlayedToItsEnd(std::string_view effectName)
{
    return effectName == ErrorBeep || effectName == GrayButton;
}

MenuSound::MenuSound(Sound::SoundSystem& soundSystem)
    : MenuSound(std::vector<Sound::SoundSystem*>{&soundSystem})
{
}

MenuSound::MenuSound(std::vector<Sound::SoundSystem*> soundSystems)
    : systems(std::move(soundSystems))
{
    for (Sound::SoundSystem* system : systems)
        system->SetReverbEnabled(true);
}

void MenuSound::PlayRequests(const SoundRequests& requests)
{
    for (const SoundRequest& request : requests.AllRequestsInThisStep())
    {
        switch (request.action)
        {
        case SoundAction::PlayEffect: PlayEffect(request.effectName); break;
        case SoundAction::StartMusic: StartMusic(request.effectName); break;
        case SoundAction::StopMusic: StopMusic(request.fadeSteps); break;
        case SoundAction::ChangeMusicVolume: ChangeMusicVolume(request.volume, request.fadeSteps); break;
        case SoundAction::PauseAllSounds: PauseAllSounds(request.fadeSteps); break;
        case SoundAction::ResumeAllSounds: ResumeAllSounds(request.fadeSteps); break;
        case SoundAction::StopAllSounds: StopAllSounds(request.fadeSteps); break;
        case SoundAction::ChangeConsoleVolume: ChangeConsoleVolume(request.volume); break;
        case SoundAction::PlayWaveFile: PlayWaveFile(request.waveFile); break;
        case SoundAction::PlayBannerSound: PlayBannerSound(request.waveFile); break;
        case SoundAction::StopWaveFile: StopWaveFile(request.fadeSteps); break;
        }
    }
}

Sound::SoundHandle MenuSound::MusicHandle() const
{
    return music.handle;
}

Sound::SoundHandle MenuSound::WaveFileHandle() const
{
    return waveFile.handle;
}

// A name that no archive has plays nothing, as in the original.
std::optional<MenuSound::FoundSound> MenuSound::FindSound(std::string_view name) const
{
    for (Sound::SoundSystem* system : systems)
    {
        std::optional<std::size_t> number = system->Archive().FindSoundNumber(name);
        if (number)
            return FoundSound{system, *number};
    }
    return std::nullopt;
}

void MenuSound::Stop(const StartedSound& sound, int fadeSteps)
{
    if (sound.system != nullptr)
        sound.system->Stop(sound.handle, SecondsOf(fadeSteps));
}

void MenuSound::PlayEffect(const std::string& name)
{
    std::optional<FoundSound> found = FindSound(name);
    if (!found)
        return;

    if (IsPlayedToItsEnd(name))
    {
        effects[name] = {found->system, found->system->PlayIfNotPlaying(found->number)};
        return;
    }

    auto earlier = effects.find(name);
    if (earlier != effects.end())
        Stop(earlier->second, 0);

    effects[name] = {found->system, found->system->Play(found->number)};
}

void MenuSound::StartMusic(const std::string& name)
{
    std::optional<FoundSound> found = FindSound(name);
    if (!found)
        return;

    found->system->SetReverbEnabled(true);
    music = {found->system, found->system->Play(found->number)};
    hasStartedMusic = true;
}

void MenuSound::StopMusic(int fadeSteps)
{
    Stop(music, fadeSteps);
}

void MenuSound::ChangeMusicVolume(float volume, int fadeSteps)
{
    if (music.system != nullptr)
        music.system->ChangeVolume(music.handle, volume, SecondsOf(fadeSteps));
}

void MenuSound::PauseAllSounds(int fadeSteps)
{
    if (!hasStartedMusic)
        return;

    for (Sound::SoundSystem* system : systems)
    {
        system->PauseRunningSounds(SecondsOf(fadeSteps));
        system->SetReverbEnabled(false);
    }
}

void MenuSound::ResumeAllSounds(int fadeSteps)
{
    if (!hasStartedMusic)
        return;

    for (Sound::SoundSystem* system : systems)
    {
        system->SetReverbEnabled(true);
        system->ResumePausedSounds(SecondsOf(fadeSteps));
    }
}

void MenuSound::StopAllSounds(int fadeSteps)
{
    for (Sound::SoundSystem* system : systems)
    {
        system->StopAll(SecondsOf(fadeSteps));
        system->SetReverbEnabled(false);
    }
    effects.clear();
}

void MenuSound::ChangeConsoleVolume(float volume)
{
    for (Sound::SoundSystem* system : systems)
        system->SetConsoleVolume(volume);
}

// A file that the wave player cannot read plays nothing, as in the original.
void MenuSound::PlayWaveFile(const Formats::Bytes& file)
{
    try
    {
        PlayDecodedWave(Sound::ReadAudioFile(file));
    }
    catch (const Formats::FormatError&)
    {
        PlayDecodedWave(std::nullopt);
    }
}

void MenuSound::PlayBannerSound(const Formats::Bytes& soundFile)
{
    try
    {
        PlayDecodedWave(Sound::ReadBannerSound(soundFile));
    }
    catch (const Formats::FormatError&)
    {
        PlayDecodedWave(std::nullopt);
    }
}

// The wave player stops the file that plays at once and plays the new one (0x8140E800).
void MenuSound::PlayDecodedWave(std::optional<Sound::DecodedWave> wave)
{
    Stop(waveFile, 0);
    waveFile = {};
    waveOfTheFile.reset();
    if (!wave)
        return;

    waveOfTheFile = std::make_shared<const Sound::DecodedWave>(std::move(*wave));
    Sound::SoundSystem* system = systems.front();
    waveFile = {system, system->PlayWave(waveOfTheFile, WavePlayerVolume)};
}

void MenuSound::StopWaveFile(int fadeSteps)
{
    Stop(waveFile, fadeSteps);
}

}
