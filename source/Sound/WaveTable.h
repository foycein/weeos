#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace Sound
{

// How a note sounds over time (an articulation of the table): the parts of its loudness, in tenths of a decibel.
struct WaveTableEnvelope
{
    // How long the loudness takes to rise from silence and to fall to the sustain level; nothing is at once.
    std::optional<double> attackSeconds;
    std::optional<double> decaySeconds;

    // The loudness while the key is held, 0 is full.
    double sustainTenthsOfDecibel = 0;

    // How much softer the note gets in every frame of the synth after the key is let go.
    double releaseTenthsOfDecibelPerFrame = 0;
};

// The samples that a region plays. A wave without a loop plays once.
struct WaveTableWave
{
    std::uint32_t sampleRate = 0;
    std::vector<float> samples;
    bool loops = false;
    std::uint32_t loopStartSample = 0;
};

// The part of an instrument that plays a key: its wave, the key at which the wave plays at its own rate, and its loudness.
struct WaveTableRegion
{
    std::uint8_t unityKey = 0;
    std::int16_t fineTuneCents = 0;
    double attenuationTenthsOfDecibel = 0;
    WaveTableEnvelope envelope;
    std::shared_ptr<const WaveTableWave> wave;
};

// The instruments of the synth of the SDK (SYN), as the HOME menu of the Internet Channel keeps them: a table file (.wt) and the samples
// it points into (.pcm). The table starts with the offsets of the percussive instruments, the melodic instruments, the regions, the
// articulations, the samples and the ADPCM parameters. A melodic instrument is the region of each of the 128 keys; a program change
// chooses the instrument (0x80823D80, 0x80823B50 of the Internet Channel).
//
// The articulations of DLS have more than this synth reads: the LFO, the second envelope and the scaling of the times by velocity and
// key. A table that uses one of them is refused, so that nothing is played wrong without a word.
class WaveTable
{
public:
    static constexpr std::size_t KeyCount = 128;

    // Throws a FormatError for a broken table, for samples beyond the end of the sample file and for articulations it cannot play.
    WaveTable(Formats::ByteView tableFile, Formats::ByteView sampleFile);

    // The region that plays a key of a program. Nothing when the program has no sound on that key.
    const WaveTableRegion* FindRegion(std::uint8_t program, std::uint8_t key) const;

    std::size_t InstrumentCount() const;

private:
    static constexpr std::uint16_t NoRegion = 0xFFFF;

    std::vector<std::vector<std::uint16_t>> instruments;
    std::vector<WaveTableRegion> regions;
};

}
