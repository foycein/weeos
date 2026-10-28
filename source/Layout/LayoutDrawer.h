#pragma once

#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"

#include "Graphics/GraphicsDevice.h"

namespace Layout
{

// Draws one pane that has been placed, with DrawQuad calls on the device.
//
//     picture:    one quad
//     window:     the content and the frame, see WindowDrawing.h
//     text box:   nothing. The glyphs of the text are the job of the Text part, which places them in the box of the pane.
//     plain pane, bounding pane:  nothing
//
// The pictures of the materials are sent to the device when they are needed for the first time.
void DrawPlacedPane(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement);

// Places all shown panes and draws them, the first pane at the back. The caller starts and ends the picture of the screen.
// A program that draws text between the panes places the panes itself with PlaceShownPanes,
// draws every one with DrawPlacedPane and puts the text in the right spot.
void DrawLayout(Graphics::GraphicsDevice& device, LiveLayout& layout, const PlacementSettings& settings = PlacementSettings());

}
