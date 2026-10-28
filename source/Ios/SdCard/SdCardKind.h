#pragma once

namespace Ios
{

// A card of up to 2 GiB is a standard capacity card. A bigger card is a high capacity card (SDHC), which starts only
// on the IOS with support for it (IosProfile::sdSupportsHighCapacityCards).
enum class SdCardKind
{
    Standard,
    HighCapacity,
};

}
