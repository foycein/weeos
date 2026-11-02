#pragma once

#include "SharpArt/VectorCanvas.h"

namespace SharpArt
{

// The Wii Remote beside the remote settings of the HOME menu. The menu tints these gray pictures.

// The body of the remote (96 by 328): a rounded white bar that turns light gray downwards, with a thin dark edge.
void DrawRemoteBody(VectorCanvas& canvas);

// The buttons and marks on the body (96 by 328): the outlines of POWER, the cross, A, minus, HOME, plus, 1 and 2, their gray signs and
// words, the holes of the speaker, the lights of the players and the word Wii.
void DrawRemoteButtons(VectorCanvas& canvas);

// The house on the HOME button and the sign on the POWER button (16 by 16), intensity pictures.
void DrawRemoteHomeSign(VectorCanvas& canvas);
void DrawRemotePowerSign(VectorCanvas& canvas);

// The holes of the speaker lit up while the remote plays a sound (16 by 32), an intensity picture.
void DrawRemoteSpeakerLit(VectorCanvas& canvas);

}
