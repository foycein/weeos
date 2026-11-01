#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// What a link or a button of a manual page does when the pointer comes, leaves or presses A. The pages say it with small scripts
// (onclick="jump('ukv/page_01/page_01.html');wiiSelectSound()"); WeeOS does not run them but reads which of these things they do.
enum class PageActionKind
{
    // Show the page at path.
    OpenPage,

    // Show the picture at path in the <img> named elementName, until RestorePictures (Dreamweaver's MM_swapImage).
    SwapPicture,
    RestorePictures,

    // Give the element named elementName the background picture at path (the contents buttons).
    SwapBackground,

    // The host's sounds of the page (the object vcJavaScriptExt): the pointer comes to a button, a button is chosen.
    PlayFocusSound,
    PlaySelectSound,

    // The next page comes from the left (Back) or from the right (Next); both play the select sound (ext.LeftScroll, ext.RightScroll).
    SlideFromLeft,
    SlideFromRight,
};

struct PageAction
{
    PageActionKind kind;
    std::string elementName;

    // As the page wrote it, relative to the page.
    std::string path;
};

// The functions that the scripts of a page define, read so that the handlers that call them can be understood.
class PageScript
{
public:
    void AddScript(std::string_view code);

    // What a handler such as an onmouseover attribute does, in order. Calls of functions that do nothing that is seen (preloading
    // pictures) give nothing.
    std::vector<PageAction> ActionsOf(std::string_view handler) const;

private:
    struct ScriptFunction
    {
        std::vector<std::string> parameters;
        std::string body;
    };

    void ActionsOfCall(std::string_view name, const std::vector<std::string>& arguments, std::vector<PageAction>& actions) const;
    void ActionsOfBody(const ScriptFunction& function, const std::vector<std::string>& arguments, std::vector<PageAction>& actions) const;

    std::map<std::string, ScriptFunction, std::less<>> functions;
};

}
