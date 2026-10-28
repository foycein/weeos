#pragma once

#include <cstddef>
#include <span>

namespace Sound
{

// A stereo frame has a left and a right sample.
constexpr std::size_t StereoChannelCount = 2;

// Turns frames of left and right samples into frames for a device with the number of channels given. The left sample goes to the
// first channel and the right sample to the second, which are the front speakers. Every other channel of the device is silent.
// A device with one channel plays the mean of left and right.
//
// The device gets numbers between -1 and 1: louder samples are cut off at the limit, and a sample that is not a number
// becomes silence. Nothing is rounded or dithered, the device's own format is a 32 bit float number like the mix.
void PlaceStereoOnDevice(std::span<const float> stereoFrames, std::size_t deviceChannelCount, std::span<float> deviceFrames);

}
