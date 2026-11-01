#pragma once

#include <string>
#include <vector>

namespace Sound
{

// The names of the playback devices of the PC that are switched on and plugged in, as Windows shows them in its sound settings.
// The list is empty when Windows has no sound devices or cannot list them.
std::vector<std::u16string> ListPlaybackDeviceNames();

}
