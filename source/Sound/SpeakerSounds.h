#pragma once

#include "Formats/Bytes.h"
#include "Sound/WaveDecoder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sound
{

// The sample rate of the sounds that the menu plays on the speaker of a Wii Remote. The files do not hold it. In its standard mode the
// speaker of the remote plays 4 bit ADPCM at 3000 Hz (WiiBrew: the rate is 6 000 000 divided by the number 0x07D0), and the highest
// tone of the five sounds is a few percent below half of that rate, where a sound made for this speaker would put it.
constexpr std::uint32_t SpeakerSoundSampleRate = 3000;

struct SpeakerSound
{
    // The name of the file in the archive, for example "connect1.bwav".
    std::string name;

    DecodedWave wave;
};

// Reads the sounds of homebutton/SpeakerSe.arc, which are the sounds of the connect and volume beeps of the HOME menu. Each file is
// the samples of one mono sound, 16 bit big endian numbers without a header. The bytes are those of the archive (a U8 archive).
std::vector<SpeakerSound> ReadSpeakerSounds(Formats::ByteView archiveBytes);

}
