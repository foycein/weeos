#pragma once

#include "Browser/WebPage.h"

#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace Browser
{

// What the thread of the engine tells the thread of the program. Every field is read and written while the lock is held.
struct PageReport
{
    std::mutex lock;

    bool isReady = false;
    std::optional<std::string> startError;
    PageState state;
    std::optional<Formats::RgbaImage> newPicture;
    std::deque<LoadFailure> failures;
    std::optional<ScriptDialog> dialog;
    std::optional<TextFieldRequest> textField;
};

}
