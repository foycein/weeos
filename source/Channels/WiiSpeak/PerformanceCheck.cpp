#include "Channels/WiiSpeak/PerformanceCheck.h"

#include <algorithm>
#include <cmath>

namespace Channels
{

namespace
{

constexpr float DecibelsPerAmplitudeDecade = 20.0f;

// The delay is searched on this many samples of the heard sound (one second of the microphone); the gain uses all of them.
constexpr std::size_t DelaySearchSamples = 16000;

constexpr float LargestSampleValue = 32767.0f;
constexpr float SmallestSampleValue = -32768.0f;

// How much of the played sound, at this delay, is in the heard sound: its correlation with the heard sound over its own energy.
double MatchAtDelay(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t delay, std::size_t count)
{
    double correlation = 0.0;
    double energy = 0.0;
    for (std::size_t sample = delay; sample < count; ++sample)
    {
        double playedSample = played[sample - delay];
        correlation += double(heard[sample]) * playedSample;
        energy += playedSample * playedSample;
    }

    if (energy <= 0.0)
        return 0.0;

    return correlation * correlation / energy;
}

// The delay at which the heard sound looks most like the played one.
std::size_t FindEchoDelay(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t longestDelay)
{
    std::size_t searchCount = std::min({heard.size(), played.size(), DelaySearchSamples});
    std::size_t bestDelay = 0;
    double bestMatch = 0.0;
    for (std::size_t delay = 0; delay <= longestDelay && delay < searchCount; ++delay)
    {
        double match = MatchAtDelay(heard, played, delay, searchCount);
        if (match > bestMatch)
        {
            bestMatch = match;
            bestDelay = delay;
        }
    }
    return bestDelay;
}

// The path from the speakers to the microphone is a short filter of a few taps around the delay. The taps that best explain the heard
// sound by the played one are found by least squares over the whole sound (the normal equations, with the autocorrelation of the
// played sound), and the echo they explain is taken away.
constexpr std::size_t FilterTaps = 32;
constexpr std::size_t TapsBeforeTheDelay = 8;

// A tiny addition to the diagonal keeps the equations solvable when the played sound lacks some frequencies.
constexpr double DiagonalLoading = 1e-6;

using Matrix = std::vector<std::vector<double>>;

double PlayedAt(std::span<const std::int16_t> played, std::size_t sample, std::size_t lag)
{
    if (sample < lag || sample - lag >= played.size())
        return 0.0;

    return played[sample - lag];
}

// Solves the equations by Gaussian elimination with the largest pivot of each column.
std::vector<double> Solve(Matrix matrix, std::vector<double> rightSide)
{
    std::size_t size = rightSide.size();
    for (std::size_t column = 0; column < size; ++column)
    {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < size; ++row)
        {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                pivot = row;
        }
        std::swap(matrix[column], matrix[pivot]);
        std::swap(rightSide[column], rightSide[pivot]);
        if (matrix[column][column] == 0.0)
            continue;

        for (std::size_t row = column + 1; row < size; ++row)
        {
            double factor = matrix[row][column] / matrix[column][column];
            for (std::size_t entry = column; entry < size; ++entry)
                matrix[row][entry] -= factor * matrix[column][entry];
            rightSide[row] -= factor * rightSide[column];
        }
    }

    std::vector<double> solution(size, 0.0);
    for (std::size_t rowsLeft = size; rowsLeft > 0; --rowsLeft)
    {
        std::size_t row = rowsLeft - 1;
        if (matrix[row][row] == 0.0)
            continue;

        double sum = rightSide[row];
        for (std::size_t entry = row + 1; entry < size; ++entry)
            sum -= matrix[row][entry] * solution[entry];
        solution[row] = sum / matrix[row][row];
    }
    return solution;
}

std::size_t DistanceBetween(std::size_t first, std::size_t second)
{
    if (first > second)
        return first - second;

    return second - first;
}

std::vector<double> FindEchoPath(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t firstLag)
{
    std::vector<double> autocorrelation(FilterTaps, 0.0);
    for (std::size_t distance = 0; distance < FilterTaps; ++distance)
    {
        for (std::size_t sample = distance; sample < played.size(); ++sample)
            autocorrelation[distance] += double(played[sample]) * played[sample - distance];
    }

    Matrix equations(FilterTaps, std::vector<double>(FilterTaps, 0.0));
    for (std::size_t row = 0; row < FilterTaps; ++row)
    {
        for (std::size_t column = 0; column < FilterTaps; ++column)
            equations[row][column] = autocorrelation[DistanceBetween(row, column)];
        equations[row][row] += DiagonalLoading * autocorrelation[0];
    }

    std::vector<double> crossCorrelation(FilterTaps, 0.0);
    for (std::size_t tap = 0; tap < FilterTaps; ++tap)
    {
        for (std::size_t sample = 0; sample < heard.size(); ++sample)
            crossCorrelation[tap] += double(heard[sample]) * PlayedAt(played, sample, firstLag + tap);
    }
    return Solve(equations, crossCorrelation);
}

}

CheckResult JudgeLevels(float heardLevel, float reducedLevel)
{
    if (heardLevel < QuietHeardLevel || reducedLevel < QuietReducedLevel)
        return CheckResult::TooQuiet;

    if (heardLevel > LoudHeardLevel)
        return CheckResult::TooLoud;

    float reductionInDecibels = DecibelsPerAmplitudeDecade * std::log10(heardLevel / reducedLevel);
    int score = 0;
    for (float limit : ScoreLimitsInDecibels)
    {
        if (reductionInDecibels >= limit)
            ++score;
    }
    return static_cast<CheckResult>(score);
}

float LevelOf(std::span<const std::int16_t> samples)
{
    if (samples.empty())
        return 0.0f;

    double sum = 0.0;
    for (std::int16_t sample : samples)
        sum += std::abs(double(sample));

    return float(sum / double(samples.size())) * LevelUnitsPerSampleUnit;
}

std::vector<std::int16_t> RemoveEcho(std::span<const std::int16_t> heard, std::span<const std::int16_t> played, std::size_t longestDelay)
{
    std::size_t delay = FindEchoDelay(heard, played, longestDelay);
    std::size_t firstLag = delay > TapsBeforeTheDelay ? delay - TapsBeforeTheDelay : 0;
    std::vector<double> path = FindEchoPath(heard, played, firstLag);

    std::vector<std::int16_t> reduced;
    reduced.reserve(heard.size());
    for (std::size_t sample = 0; sample < heard.size(); ++sample)
    {
        double echo = 0.0;
        for (std::size_t tap = 0; tap < FilterTaps; ++tap)
            echo += path[tap] * PlayedAt(played, sample, firstLag + tap);

        double left = std::round(double(heard[sample]) - echo);
        double leftInRange = std::clamp(left, double(SmallestSampleValue), double(LargestSampleValue));
        reduced.push_back(static_cast<std::int16_t>(leftInRange));
    }
    return reduced;
}

}
