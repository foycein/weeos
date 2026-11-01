#pragma once

#include "Formats/Bytes.h"
#include "Sound/WaveDecoder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Sound
{

// The sample rate of the sounds that the menu plays on the speaker of a Wii Remote. The files do not hold it. The HOME menu sends the
// speaker 40 samples every 1/150 second (0x81378f98), and the AX library makes 18 samples for the remotes in every sound frame of
// 3 milliseconds (0x815537e0): both are 6000 samples a second. The speaker is set to 4 bit ADPCM with the number 0x07D0 (0x81695840);
// the 6 000 000 divided by that number of WiiBrew is 3000 bytes a second, which hold two samples each.
constexpr std::uint32_t SpeakerSoundSampleRate = 6000;

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
