#pragma once

#include "Channels/Manual/HtmlDocument.h"

#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// One property of a style rule, for example "font-size" with the value "18px".
struct StyleDeclaration
{
    std::string property;
    std::string value;

    // The folder that a url() of the value is read from: the folder of the style sheet, or of the page for a style attribute.
    std::string baseFolder;
};

// The style rules of a page: its style sheets and <style> elements, in the order the page names them. A rule applies when one of
// its selectors names the element by its tag, its class, its id or a tag with a class. Selectors with anything more (a:hover,
// td[width]>a, descendants) never apply: the pages use them only for links, whose look the browser draws itself.
class PageStyleSheet
{
public:
    void AddStyleSheet(std::string_view cssText, std::string_view baseFolder);

    // The declarations that apply to the element, the weakest first: by the specificity of the selector (tag, class, id), then by the
    // order of the rules, and the element's own style attribute last.
    std::vector<StyleDeclaration> DeclarationsFor(const HtmlNode& element, std::string_view pageFolder) const;

private:
    struct SimpleSelector
    {
        std::string tag;
        std::string className;
        std::string id;
        int specificity = 0;
    };

    struct StyleRule
    {
        std::vector<SimpleSelector> selectors;
        std::vector<StyleDeclaration> declarations;
    };

    static std::vector<SimpleSelector> ReadSelectors(std::string_view selectorText);
    static bool Matches(const SimpleSelector& selector, const HtmlNode& element);

    std::vector<StyleRule> rules;
};

// The declarations of a style attribute or of the inside of a rule: "padding: 18px 1px 0 10px; color: #333333".
std::vector<StyleDeclaration> ReadDeclarations(std::string_view declarationText, std::string_view baseFolder);

}
