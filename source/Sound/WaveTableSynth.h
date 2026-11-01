#pragma once

#include "Sound/MidiFile.h"
#include "Sound/WaveDecoder.h"
#include "Sound/WaveTable.h"

namespace Sound
{

// The rate the synth makes its samples at, the rate of the DSP, and the length of one of its frames: 96 samples, 3 milliseconds.
constexpr std::uint32_t SynthSampleRate = 32000;
constexpr std::uint32_t SynthFrameSamples = 96;

// Plays a MIDI file with the instruments of a wave table and gives the stereo samples it makes, the way the sequencer and synth of the SDK
// (SEQ and SYN) play the HOME menu's sounds in the Internet Channel. Read from its program (0x80823D80 to 0x80824BF0, 0x80821F50):
//
//  * A note takes the region of its key in the instrument of its channel's program. It plays the region's wave at the rate of the wave
//    times 2 to the power of (key - unity key + fine tune / 100) / 12.
//  * Its loudness in tenths of a decibel: the region's attenuation, the velocity (40 log10 of velocity / 127 decibels) and the envelope.
//    The channel's volume (controller 7, the same curve; 100 at the start) and pan (controller 10, the left side gets (127 - pan) / 127
//    of the power, the right side pan / 127) are added in the mix. A sum below -90.4 decibels is silence, the most is +5.9.
//  * The envelope moves once in every frame: the attack rises along 20 log10(step / 99) in steps of 100 / (frames of the attack time),
//    the decay falls 96 decibels in the decay time down to the sustain level, and after the key is let go the release takes its number
//    off in every frame. Below -72 decibels the note ends. A time counts in whole frames of 3 milliseconds.
//  * A note on a key that sounds lets the sounding one go first.
//
// The reverb send (controller 91) is not played: the program has no effect library (no AXFX) to put on the auxiliary bus.
DecodedWave RenderMidi(const MidiFile& file, const WaveTable& table);

}
