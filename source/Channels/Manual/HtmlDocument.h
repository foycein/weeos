#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Channels
{

// One part of a web page: a text, or an element such as <table width="608"> with the parts inside it.
struct HtmlNode
{
    // The name of the element in lower case, for example "td". Empty for a text.
    std::string tag;

    // The attributes by their names in lower case. The values are as the page wrote them, in UTF-8.
    std::map<std::string, std::string> attributes;

    // The text of a text node with its character references resolved, in UTF-16. A <script> or <style> element keeps its
    // whole content here, unchanged.
    std::u16string text;

    std::vector<HtmlNode> children;

    bool IsText() const;

    // The value of the attribute, or an empty text when the element has none.
    std::string Attribute(std::string_view name) const;
    bool HasAttribute(std::string_view name) const;
};

// Reads a page of HTML into its tree. The root is an element without a name that holds the <html> element. Comments, the
// document type and unknown closing tags are left out; an element that the page does not close ends with its parent.
HtmlNode ReadHtmlDocument(std::string_view utf8Page);

// The first element with this name in the tree, depth first, or nothing.
const HtmlNode* FindElement(const HtmlNode& root, std::string_view tag);

}
