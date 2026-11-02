#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The big buttons with gray drawings of the console, an SD card, a memory card and a channel (256 by 200), in the frame of the
// buttons of Wii Options.

// Data Management on the page of Wii Options: the console, a memory card, an SD card and a channel.
void DrawDataManagementButton(VectorCanvas& canvas);

// Save Data in Data Management: the console, a memory card and an SD card.
void DrawSaveDataButton(VectorCanvas& canvas);

// Wii in Save Data: the console and an SD card.
void DrawWiiSaveDataButton(VectorCanvas& canvas);

// Channels in Data Management: a channel among faint floating frames.
void DrawChannelsButton(VectorCanvas& canvas);

}
