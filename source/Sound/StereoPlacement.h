#pragma once

#include <cstdint>
#include <span>

namespace Sound
{

// The bits of a channel mask for the two front speakers.
constexpr std::uint32_t FrontLeftSpeakerBit = 0x1;
constexpr std::uint32_t FrontRightSpeakerBit = 0x2;

// How the channels of an audio device are ordered in its frames. A frame holds one sample for each channel.
struct DeviceChannelLayout
{
    std::uint32_t channelCount;

    // The places of the front left and the front right speaker inside a frame. Not used by a device with one channel.
    std::uint32_t leftChannel;
    std::uint32_t rightChannel;
};

// Turns frames of left and right samples into frames for the device. Every other channel of the device is silent.
// A device with one channel plays the mean of left and right.
//
// The device gets numbers between -1 and 1: louder samples are cut off at the limit, and a sample that is not a number
// becomes silence. Nothing is rounded or dithered, the device's own format is a 32 bit float number like the mix.
void PlaceStereoOnDevice(std::span<const float> stereoFrames, const DeviceChannelLayout& layout, std::span<float> deviceFrames);

// The place of the front left and front right speaker among the channels that a channel mask lists. The channels of a frame come in
// the order of the bits in the mask (a mask is one bit per speaker, front left is bit 0 and front right is bit 1).
// A mask without the two front speakers gives the first two channels.
DeviceChannelLayout LayoutOfChannelMask(std::uint32_t channelCount, std::uint32_t channelMask);

}
