#pragma once

#include "Formats/LayoutMaterial.h"
#include "Graphics/TevStage.h"

namespace Layout
{

// Turns the 16 bytes of a stage in a layout file into a stage for the graphics device.
// Throws a LayoutError for what the library does not support: indirect textures, swap tables other than the plain one,
// texture coordinates that do not belong to the texture, and numbers the graphics chip has no meaning for.
Graphics::TevStage ConvertTevStage(const Formats::TevStage& record);

}
