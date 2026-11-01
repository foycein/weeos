#pragma once

#include "Mii/MiiRecord.h"

#include <cstddef>

namespace Mii
{

constexpr std::size_t DefaultMiiCount = 6;

// The six Miis built into the face library, which programs use when the console has no Mii of its own: three men (0 to 2) and
// three women (3 to 5), all named "no name" (docs/research/Mii.md section 3.1).
MiiRecord DefaultMii(std::size_t number);

}
