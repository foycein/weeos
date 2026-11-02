#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace Channels
{

// What the Wii Speak Performance Check finds (0x801BCD58 of the Wii Speak program, the numbers stored at +0x58).
enum class CheckResult
{
    // How well the echo is reduced, from 0 (badly) to 5 (well). The screen shows 1 to 6 lights.
    ScoreZero,
    ScoreOne,
    ScoreTwo,
    ScoreThree,
    ScoreFour,
    ScoreFive,

    // "The sound was too low to test accurately." (4915208)
    TooQuiet,

    // "The volume was too high to test accurately." (4915209)
    TooLoud,
};

// The limits of the check, read in 0x801BCD58: the heard level below 551.7 or the reduced level below 27.6 is too quiet, the heard
// level above 2758.6 too loud. Otherwise the reduction in decibels gives the score at 10, 18.5, 20, 21.5 and 27 decibels.
constexpr float QuietHeardLevel = 551.7f;
constexpr float QuietReducedLevel = 27.6f;
constexpr float LoudHeardLevel = 2758.6f;
constexpr std::array<float, 5> ScoreLimitsInDecibels = {10.0f, 18.5f, 20.0f, 21.5f, 27.0f};

// The original reads the levels from the signal processor of the accessory, in units that are not known. WeeOS takes the mean size
// of the 16 bit samples (Assumed).
constexpr float LevelUnitsPerSampleUnit = 1.0f;

// Sorts the two levels of the check: the level of what the microphone heard while the music played and the level that is left after
// the echo of the music was taken out.
CheckResult JudgeLevels(float heardLevel, float reducedLevel);

// The mean size of the samples, in the units of the check.
float LevelOf(std::span<const std::int16_t> samples);

// WeeOS's echo reduction for the check: the music that was played is the far end. The delay at which the heard sound looks most like
// the music is searched up to the longest delay, and a short filter around it, fitted by least squares, takes the echo away; what is
// left is the reduced sound. The original does this in the signal processor of the accessory.
std::vector<std::int16_t> RemoveEcho(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t longestDelay);

}
