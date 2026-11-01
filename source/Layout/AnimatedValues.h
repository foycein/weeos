#pragma once

#include "Formats/Layout.h"

#include <cstddef>
#include <cstdint>

namespace Layout
{

// Each function writes one value that an animation changes. The properties are numbered like in the animation files.
// A property number that does not exist throws a LayoutError.

// 0 to 2 translation, 3 to 5 rotation in degrees, 6 and 7 scale, 8 width, 9 height.
void SetPaneTransformValue(Formats::Pane& pane, unsigned property, float value);

// The elements 0 to 15 are the colors of the four corners (left top, right top, left bottom, right bottom),
// each with red, green, blue and alpha. Element 16 is the alpha of the pane.
//
// A picture and the content of a window have the colors of four corners. A text box has two colors: its top color
// follows the two top corners and its bottom color follows the two bottom corners. Other panes have no colors,
// so the elements 0 to 15 change nothing for them.
void SetPaneColorElement(Formats::Pane& pane, unsigned element, std::uint8_t value);

// The elements 0 to 3 are the color of the material (red, green, blue, alpha) when the material has one,
// 4 to 15 are the registers 0, 1 and 2 (four numbers each) and 16 to 31 are the four constant colors.
// The value is in the range of the registers. The color of the material and the constant colors take it as far
// as it fits into 0 to 255.
void SetMaterialColorElement(Formats::Material& material, unsigned element, int value);

// 0 shift horizontally, 1 shift vertically, 2 rotation in degrees, 3 scale horizontally, 4 scale vertically.
// A material that has no texture transform with this number ignores the value.
void SetTextureTransformValue(Formats::Material& material, std::size_t transformIndex, unsigned property, float value);

// The same for an indirect matrix (the animation RLIM), whose numbers are the same five.
void SetIndirectTransformValue(Formats::Material& material, std::size_t transformIndex, unsigned property, float value);

}
