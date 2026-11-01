#include "SystemMenu/WiiMenu/HoverTracker.h"

namespace SystemMenu
{

std::vector<HoverChange> HoverTracker::Update(const ElementsUnderPointers& elementsNow)
{
    std::vector<HoverChange> changes;

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        if (elementsBefore[playerIndex] && elementsBefore[playerIndex] != elementsNow[playerIndex])
            changes.push_back({playerIndex, *elementsBefore[playerIndex], false});
    }

    for (int playerIndex = 0; playerIndex < PlayerCount; ++playerIndex)
    {
        if (elementsNow[playerIndex] && elementsBefore[playerIndex] != elementsNow[playerIndex])
            changes.push_back({playerIndex, *elementsNow[playerIndex], true});
    }

    elementsBefore = elementsNow;
    return changes;
}

std::optional<int> HoverTracker::ElementOf(int playerIndex) const
{
    return elementsBefore.at(playerIndex);
}

}
