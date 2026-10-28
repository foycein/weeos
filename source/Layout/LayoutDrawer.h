#pragma once

#include "Layout/LiveLayout.h"
#include "Layout/PanePlacement.h"
#include "Layout/TextFonts.h"

#include "Graphics/GraphicsDevice.h"

namespace Layout
{

// Draws one pane that has been placed, with DrawQuad calls on the device.
//
//     picture:    one quad
//     window:     the content and the frame, see WindowDrawing.h
//     text box:   its text, one quad for every glyph, see TextBoxDrawing.h. Without text fonts a text box draws nothing.
//     plain pane, bounding pane:  nothing
//
// The pictures of the materials are sent to the device when they are needed for the first time.
void DrawPlacedPane(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement,
    const TextFonts* textFonts = nullptr);

// Places all shown panes and draws them, the first pane at the back. The caller starts and ends the picture of the screen.
void DrawLayout(Graphics::GraphicsDevice& device, LiveLayout& layout, const PlacementSettings& settings = PlacementSettings(),
    const TextFonts* textFonts = nullptr);

}
