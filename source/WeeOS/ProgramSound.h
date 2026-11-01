#pragma once

#include "Formats/Bytes.h"
#include "Settings/PcSettings.h"
#include "Settings/SystemSettings.h"
#include "Sound/AudioOutput.h"
#include "Sound/SoundSystem.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace WeeOS
{

// The loudness of a volume of PC Settings, from 0 to 100 percent: the square of the fraction, the same square law that the Wii
// uses for the volumes of its sequences. 50 percent is a quarter of the amplitude (12 decibels softer), 0 is silence.
float GainOfVolume(int volumePercent);

// The sound of WeeOS.exe: the output to the playback device that PC Settings chooses, at the volume it sets, and the sound system
// that plays the sound archive of the System Menu into it. It lives as long as the program; every session of the menu plays through
// the same sound system.
//
// A silent program (the option --silent of WeeOS.exe, for the runs of development) opens the output as usual and gives it only
// silence: every sample is exactly 0, whatever the menu plays.
class ProgramSound
{
public:
    explicit ProgramSound(bool silent);

    ProgramSound(const ProgramSound&) = delete;
    ProgramSound& operator=(const ProgramSound&) = delete;

    // The device and the volume of PC Settings. They count at once, also while sound plays: a new device takes over after a short gap.
    void ApplyPcSettings(const Settings::PcSettings& settings);

    // The first time: makes the sound system for the archive and starts the output. Gives the sound system, or nothing when the PC
    // cannot play sound; the program says why on its console once, and the menu runs without sound.
    Sound::SoundSystem* Start(Formats::ByteView menuSoundArchive);

    // The sound mode of the console, IPL.SND. The sounds follow it from the next sound frame on.
    void SetSoundMode(Settings::SoundMode mode);

    // Every sound stops at once, for example when the console starts again.
    void StopAllSounds();

    bool IsSilent() const;

    // Nothing before Start, or when the PC cannot play sound.
    Sound::SoundSystem* Sounds();
    Sound::AudioOutput* Output();

    // The number of samples that the sound system made, and that the device got, that were not exactly 0.
    std::uint64_t RenderedSoundingSamples() const;
    std::uint64_t DeliveredSoundingSamples() const;

private:
    void Render(std::span<float> interleavedStereo);

    bool isSilent;
    std::u16string deviceName;
    float volumeGain = 1.0f;
    bool hasTriedToStart = false;
    std::atomic<std::uint64_t> renderedSoundingSamples{0};
    std::atomic<std::uint64_t> deliveredSoundingSamples{0};

    // The output comes last, so that it goes first: its audio thread stops before the sound system that it renders goes.
    std::unique_ptr<Sound::SoundSystem> sounds;
    std::unique_ptr<Sound::AudioOutput> output;
};

}
