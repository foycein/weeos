#pragma once

#include "SystemMenu/DataManagement/DataItems.h"
#include "SystemMenu/DataManagement/DataResult.h"

#include <cstdint>
#include <optional>

namespace SystemMenu
{

// The three buttons of the window of an entry. Each works from the side that is shown to the other side, or erases the entry there.
enum class DataAction
{
    Move,
    Copy,
    Erase,
};

// The state of the SD card as the editors see it (the worker's states of the card, iplNandSDWorker.cpp 0x8134A938).
enum class CardState
{
    NoCard,

    // The card cannot be used, for example a high capacity card on an IOS that does not know it.
    Unusable,

    Ready,
};

CardState ReadCardState(const DataManagementServices& services);

// How the work of a button ended, with what the save data left behind.
struct DataOutcome
{
    DataResult result = DataResult::Done;
    bool hasNoCopyFiles = false;
    bool hasNoTransferFiles = false;
    bool hasLeftOutNoTransferFiles = false;
};

// The message that refuses the button before the question is asked, or nothing when the question may come
// (screens 0x0c 0x813A6908 and 0x0d 0x813C932C).
std::optional<std::uint16_t> RefusalMessage(DataManagementServices& services, DataKind kind, DataPlace place, DataAction action, const DataItem& item);

// The question, the message while the work runs, and the message at its end.
std::uint16_t QuestionMessage(DataPlace place, DataAction action);
std::uint16_t ProgressMessage(DataPlace place, DataAction action);
std::uint16_t OutcomeMessage(DataKind kind, DataPlace place, DataAction action, const DataOutcome& outcome);

// Does the work. It may take long: the screens run it beside the menu.
DataOutcome RunDataAction(DataManagementServices& services, DataKind kind, DataPlace place, DataAction action, const DataItem& item);

}
