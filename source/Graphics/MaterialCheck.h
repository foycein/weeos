#pragma once

#include "Graphics/GraphicsDevice.h"

namespace Graphics
{

// Throws a GraphicsError that says which rule the material breaks. The layout files can hold numbers
// that no stage can use, so the device checks every material before it draws with it.
void CheckMaterial(const Material& material);

}
