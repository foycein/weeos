#pragma once

#include <stdexcept>
#include <string>

namespace Formats
{

class FormatError : public std::runtime_error
{
public:
    explicit FormatError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
