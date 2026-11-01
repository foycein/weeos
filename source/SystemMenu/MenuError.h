#pragma once

#include "SystemMenu/FatalError.h"

#include <stdexcept>
#include <string>

namespace SystemMenu
{

// Thrown when the menu cannot go on: a file of the System Menu title is missing, or a pane that a screen needs is not in its layout.
// The error screen shows the reason that goes with it.
class MenuError : public std::runtime_error
{
public:
    explicit MenuError(const std::string& message, FatalError reasonToShow = FatalError::SystemFilesCorrupted)
        : std::runtime_error(message)
        , reason(reasonToShow)
    {
    }

    FatalError Reason() const
    {
        return reason;
    }

private:
    FatalError reason;
};

}
