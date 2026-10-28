#pragma once

#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"

#include "Graphics/GraphicsDevice.h"

namespace Layout
{

// Draws a window pane: first its content, then its frame on top.
//
// The frame is made of the textures of the frame materials. The size of the first texture of a frame material tells how
// thick the frame is. The content fills the rectangle inside the frame, stretched by the magnification of the window.
// Nothing is drawn for a frame whose material has no texture.
//
//     1 frame:   four bands around the content, all made of the one texture, which is flipped for each side
//     4 frames:  four bands around the content, one texture for each side: top, right, left, bottom
//     8 frames:  four corners and four edges: left top, right top, left bottom, right bottom, left, right, top, bottom
//
// A band or an edge repeats its texture along its length. Any other number of frames than 0, 1, 4 and 8 throws a LayoutError.
void DrawWindow(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement);

}
