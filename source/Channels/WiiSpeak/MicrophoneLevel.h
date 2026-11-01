#pragma once

#include <cstdint>
#include <span>

namespace Channels
{

// The loudness of what the microphone heard, in decibels below full scale, from the mean size of the 16 bit samples.
// Silence (or no sample) is SilenceLevel.
constexpr float SilenceLevel = -100.0f;
float HeardLevelInDecibels(std::span<const std::int16_t> samples);

// The level meter of the sensitivity screen has 28 bars, the frames 0 to 28 of its animation mic_level. The original counts the
// level of the signal processor; WeeOS lights no bar at QuietestMeterLevel and all at the full scale (CHN-10, Assumed).
constexpr float QuietestMeterLevel = -60.0f;
constexpr float MeterBarCount = 28.0f;
float MeterFrame(float levelInDecibels);

// The chat room lights the circle of the player while a voice is heard: louder than this (CHN-10, Assumed).
constexpr float VoiceLevel = -45.0f;
bool IsVoiceHeard(float levelInDecibels);

}
