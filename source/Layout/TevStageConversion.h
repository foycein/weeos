#pragma once

#include "Formats/LayoutMaterial.h"
#include "Graphics/TevStage.h"

#include <optional>

namespace Layout
{

// Turns the 16 bytes of a stage in a layout file into a stage for the graphics device. The stage picks its raster and texture swap
// tables out of the material's four (a material without a table of its own has four plain ones).
// Throws a LayoutError for what the library does not support: an indirect texture other than the one form it draws, texture coordinates
// that do not belong to the texture, and numbers the graphics chip has no meaning for.
Graphics::TevStage ConvertTevStage(const Formats::TevStage& record, const std::optional<Formats::TevSwapTable>& swapTables = std::nullopt);

}
