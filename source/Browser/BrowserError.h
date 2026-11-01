#pragma once

#include <stdexcept>
#include <string>

namespace Browser
{

// Thrown when the browser engine or the capture of its pictures cannot work, for example when the WebView2 Runtime is missing.
class BrowserError : public std::runtime_error
{
public:
    explicit BrowserError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

}
