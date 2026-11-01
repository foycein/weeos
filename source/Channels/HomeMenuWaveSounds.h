#pragma once

#include "SystemMenu/TitleContents.h"

#include "Ios/Title/TitleId.h"
#include "Sound/WaveDecoder.h"

#include <map>
#include <memory>
#include <string>

namespace Channels
{

using RenderedSounds = std::map<std::string, std::shared_ptr<const Sound::DecodedWave>>;

// The HOME menu of the Internet Channel keeps its sounds as MIDI files with a wave table (homebutton/HomeButtonSe.arc of content 5,
// CHN-30), not in a sound archive. They are played once through the synth of the SDK (Sound/WaveTableSynth) when the channel starts,
// by the names the HOME menu asks for (HOMESE_FOCUS, ...), so that they play as waves. Empty for every other channel.
RenderedSounds RenderHomeMenuSounds(Ios::TitleId titleId, SystemMenu::TitleContents& contents);

}
