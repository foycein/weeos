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

}
