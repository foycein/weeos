#include "Browser/DevToolsMessages.h"

#include <array>
#include <format>

namespace Browser
{

namespace
{

constexpr char16_t FirstPrintable = u' ';
constexpr char16_t LastPrintable = u'~';

// The name, the place on the keyboard and the Windows key number of each PageKey, in the order of the enumeration.
struct KeyNames
{
    std::string_view key;
    std::string_view code;
    int windowsKeyNumber;
};

constexpr std::array<KeyNames, 13> KeyTable = {{
    {"Enter", "Enter", 0x0D},
    {"Backspace", "Backspace", 0x08},
    {"Delete", "Delete", 0x2E},
    {"Tab", "Tab", 0x09},
    {"Escape", "Escape", 0x1B},
    {"ArrowLeft", "ArrowLeft", 0x25},
    {"ArrowRight", "ArrowRight", 0x27},
    {"ArrowUp", "ArrowUp", 0x26},
    {"ArrowDown", "ArrowDown", 0x28},
    {"PageUp", "PageUp", 0x21},
    {"PageDown", "PageDown", 0x22},
    {"Home", "Home", 0x24},
    {"End", "End", 0x23},
}};

constexpr std::u16string_view TextFieldMark = u"TextField";

// A click is a press and a release with the click count 1; a move has no button at all.
std::string_view EventTypeOf(MouseEventKind kind)
{
    switch (kind)
    {
    case MouseEventKind::Moved:
        return "mouseMoved";
    case MouseEventKind::Pressed:
        return "mousePressed";
    case MouseEventKind::Released:
        return "mouseReleased";
    }
    return "mouseMoved";
}

std::optional<TextFieldRequest::Kind> FieldKindOf(std::u16string_view name)
{
    if (name == u"Line")
        return TextFieldRequest::Kind::Line;
    if (name == u"Password")
        return TextFieldRequest::Kind::Password;
    if (name == u"Lines")
        return TextFieldRequest::Kind::Lines;
    return std::nullopt;
}

std::u16string_view TakeLine(std::u16string_view& text)
{
    std::size_t end = text.find(u'\n');
    if (end == std::u16string_view::npos)
    {
        std::u16string_view line = text;
        text = {};
        return line;
    }

    std::u16string_view line = text.substr(0, end);
    text.remove_prefix(end + 1);
    return line;
}

}

std::string JsonText(std::u16string_view text)
{
    std::string json = "\"";
    for (char16_t character : text)
    {
        bool isPlain = character >= FirstPrintable && character <= LastPrintable && character != u'"' && character != u'\\';
        if (isPlain)
            json += static_cast<char>(character);
        else
            json += std::format("\\u{:04x}", static_cast<unsigned>(character));
    }
    json += "\"";
    return json;
}

std::string MouseEventParameters(MouseEventKind kind, double pageX, double pageY)
{
    if (kind == MouseEventKind::Moved)
        return std::format(R"({{"type":"mouseMoved","x":{},"y":{},"button":"none"}})", pageX, pageY);

    return std::format(R"({{"type":"{}","x":{},"y":{},"button":"left","buttons":1,"clickCount":1}})", EventTypeOf(kind), pageX, pageY);
}

std::string WheelEventParameters(double pageX, double pageY, double pixelsRight, double pixelsDown)
{
    return std::format(R"({{"type":"mouseWheel","x":{},"y":{},"deltaX":{},"deltaY":{}}})", pageX, pageY, pixelsRight, pixelsDown);
}

std::string InsertTextParameters(std::u16string_view text)
{
    return std::format(R"({{"text":{}}})", JsonText(text));
}

// Enter types a line break as well, so that a form is sent; the other keys type nothing.
std::string KeyEventParameters(PageKey key, bool isDown)
{
    const KeyNames& names = KeyTable[static_cast<std::size_t>(key)];
    if (!isDown)
    {
        return std::format(R"({{"type":"keyUp","key":"{}","code":"{}","windowsVirtualKeyCode":{}}})", names.key, names.code,
            names.windowsKeyNumber);
    }

    if (key == PageKey::Enter)
    {
        return std::format(R"({{"type":"keyDown","key":"{}","code":"{}","windowsVirtualKeyCode":{},"text":"\r"}})", names.key, names.code,
            names.windowsKeyNumber);
    }

    return std::format(R"({{"type":"rawKeyDown","key":"{}","code":"{}","windowsVirtualKeyCode":{}}})", names.key, names.code,
        names.windowsKeyNumber);
}

std::string FocusEmulationParameters()
{
    return R"({"enabled":true})";
}

std::string FillTextFieldScript(std::u16string_view text)
{
    return std::format("(function(text) {{"
                       " var field = document.activeElement;"
                       " if (!field || !('value' in field)) return;"
                       " field.value = text;"
                       " field.dispatchEvent(new Event('input', {{bubbles: true}}));"
                       " field.dispatchEvent(new Event('change', {{bubbles: true}}));"
                       " }})({});",
        JsonText(text));
}

std::string SingleColumnScript(bool isSingleColumn)
{
    return std::format("(function(isOn) {{"
                       " var id = 'weeos-single-column';"
                       " var old = document.getElementById(id);"
                       " if (old) old.remove();"
                       " if (!isOn) return;"
                       " var style = document.createElement('style');"
                       " style.id = id;"
                       " style.textContent = '* {{ float: none !important; position: static !important; width: auto !important;"
                       " min-width: 0 !important; max-width: 100% !important; columns: auto !important; box-sizing: border-box !important; }}"
                       " table, thead, tbody, tfoot, tr, td, th {{ display: block !important; }}"
                       " img, video {{ height: auto !important; }}"
                       " body {{ font-size: 125% !important; }}';"
                       " (document.head || document.documentElement).appendChild(style);"
                       " }})({});",
        isSingleColumn ? "true" : "false");
}

std::u16string_view PageHelperScript()
{
    return u"(function() {"
           u" window.print = function() {};"
           u" var textInputTypes = ['', 'text', 'search', 'email', 'url', 'tel', 'number', 'password'];"
           u" document.addEventListener('click', function(event) {"
           u"  var field = event.target;"
           u"  if (!field || !field.tagName) return;"
           u"  var tag = field.tagName.toLowerCase();"
           u"  var type = (field.getAttribute('type') || '').toLowerCase();"
           u"  if (tag === 'input' && type === 'file') { event.preventDefault(); return; }"
           u"  var isLines = tag === 'textarea';"
           u"  var isLine = tag === 'input' && textInputTypes.indexOf(type) >= 0;"
           u"  if (!isLines && !isLine) return;"
           u"  if (field.disabled || field.readOnly) return;"
           u"  var kind = 'Line';"
           u"  if (isLines) kind = 'Lines';"
           u"  else if (type === 'password') kind = 'Password';"
           u"  window.chrome.webview.postMessage('TextField\\n' + kind + '\\n' + field.value);"
           u" }, true);"
           u"})();";
}

std::optional<TextFieldRequest> ReadTextFieldMessage(std::u16string_view message)
{
    if (TakeLine(message) != TextFieldMark)
        return std::nullopt;

    std::optional<TextFieldRequest::Kind> kind = FieldKindOf(TakeLine(message));
    if (!kind)
        return std::nullopt;

    return TextFieldRequest{*kind, std::u16string(message)};
}

}
