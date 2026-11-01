#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Sound
{

// The names of the playback devices of the PC that are switched on and plugged in, as Windows shows them in its sound settings.
// The list is empty when Windows has no sound devices or cannot list them.
std::vector<std::u16string> ListPlaybackDeviceNames();

// The identifier that Windows gives the playback device with this name, or nothing when no device that is switched on and plugged in
// has the name. The identifier is what AudioOutput opens.
std::optional<std::wstring> FindPlaybackDeviceId(std::u16string_view deviceName);

}
