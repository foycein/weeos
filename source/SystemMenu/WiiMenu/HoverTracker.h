#pragma once

#include "SystemMenu/InputManager.h"

#include <array>
#include <optional>
#include <vector>

namespace SystemMenu
{

// What changed under the pointers since the last look: a pointer came onto an element, or went away from one.
struct HoverChange
{
    int playerIndex = 0;

    // The number that the screen gave the element: a place of the grid, a button of the bar.
    int element = 0;

    bool hasEntered = false;
};

// The element that lies under the pointer of each player, or nothing. A player whose remote does not point at the picture has nothing.
using ElementsUnderPointers = std::array<std::optional<int>, PlayerCount>;

// Remembers which element lay under the pointer of each player, and tells what changed. A screen asks it once in every step.
class HoverTracker
{
public:
    // The pointers that went away from an element come first, then the pointers that came onto one.
    // A pointer that moved from one element to another leaves the first and enters the second.
    std::vector<HoverChange> Update(const ElementsUnderPointers& elementsNow);

    // The element that lies under the pointer of the player as of the last Update.
    std::optional<int> ElementOf(int playerIndex) const;

private:
    ElementsUnderPointers elementsBefore;
};

}
