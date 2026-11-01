#include "SystemMenu/WiiMenu/ChannelTitleIds.h"

#include <algorithm>

namespace SystemMenu
{

bool IsKindShownInTheGrid(std::uint32_t kind)
{
    return std::find(KindsShownInTheGrid.begin(), KindsShownInTheGrid.end(), kind) != KindsShownInTheGrid.end();
}

bool AreInOneFamily(Ios::TitleId first, Ios::TitleId second)
{
    return first.highPart == second.highPart && (first.lowPart & FamilyMask) == (second.lowPart & FamilyMask);
}

bool IsFirstVersion(Ios::TitleId titleId)
{
    return (titleId.lowPart & ~FamilyMask) == FirstVersionLetter;
}

bool IsInFamily(Ios::TitleId titleId, std::uint32_t kind, std::uint32_t family)
{
    return titleId.highPart == kind && (titleId.lowPart & FamilyMask) == family;
}

}
