#pragma once

#include "Browser/WebPage.h"

#include <string>
#include <string_view>

namespace Browser
{

// The parameters of the DevToolsProtocol commands that bring the pointer, the wheel and the keys to the page, as JSON text
// (https://chromedevtools.github.io/devtools-protocol/tot/Input/). Places are in pixels of the page.

// The text as a JSON string with its quotes. Every character outside plain printable ASCII is written as \uXXXX.
std::string JsonText(std::u16string_view text);

enum class MouseEventKind
{
    Moved,
    Pressed,
    Released,
};

// Input.dispatchMouseEvent of the left button. A press and its release count as one click.
std::string MouseEventParameters(MouseEventKind kind, double pageX, double pageY);

// Input.dispatchMouseEvent of the wheel at the pointer: positive numbers scroll right and down.
std::string WheelEventParameters(double pageX, double pageY, double pixelsRight, double pixelsDown);

// Input.insertText.
std::string InsertTextParameters(std::u16string_view text);

// Input.dispatchKeyEvent, once with isDown true and once with false.
std::string KeyEventParameters(PageKey key, bool isDown);

// Emulation.setFocusEmulationEnabled: the page behaves as if its window had the focus of Windows.
std::string FocusEmulationParameters();

// A script for the page that puts the text into the text field that has the focus and tells the page's scripts, as typing would.
std::string FillTextFieldScript(std::u16string_view text);

// The script that runs in every page before its own scripts (WebPage): a click on a text field reports the field to the program, and
// the page cannot open the PC's file picker or print dialog.
std::u16string_view PageHelperScript();

// What the page helper script reports for a text field: "TextField", the kind and the text, one per line.
std::optional<TextFieldRequest> ReadTextFieldMessage(std::u16string_view message);

}
