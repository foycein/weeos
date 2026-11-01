#include "Channels/Manual/HtmlDocument.h"

#include "Channels/ChannelText.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <iterator>
#include <span>

namespace Channels
{

namespace
{

// Elements that never hold anything and have no closing tag.
constexpr std::array<std::string_view, 7> EmptyElements = {"img", "br", "meta", "link", "frame", "input", "hr"};

// Elements whose content is not HTML: it is kept as one text.
constexpr std::array<std::string_view, 2> RawTextElements = {"script", "style"};

// The named character references the pages use, and the characters they stand for.
struct NamedCharacter
{
    std::string_view name;
    char16_t character;
};

constexpr std::array<NamedCharacter, 11> NamedCharacters = {{
    {"nbsp", u' '},
    {"amp", u'&'},
    {"lt", u'<'},
    {"gt", u'>'},
    {"quot", u'"'},
    {"apos", u'\''},
    {"copy", u'©'},
    {"reg", u'®'},
    {"trade", u'™'},
    {"deg", u'°'},
    {"middot", u'·'},
}};

// Longer than any reference, so an ampersand far before a semicolon is a plain ampersand.
constexpr std::size_t LongestReference = 10;

constexpr int DecimalBase = 10;
constexpr int HexadecimalBase = 16;

bool IsOneOf(std::string_view name, std::span<const std::string_view> names)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::string ToLowerCase(std::string_view text)
{
    std::string lower(text);
    for (char& character : lower)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    return lower;
}

bool IsSpace(char character)
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}

bool IsNameCharacter(char character)
{
    return !IsSpace(character) && character != '>' && character != '/' && character != '=';
}

// "&amp;" gives "&", "&#169;" and "&#xA9;" give the character of that number. Anything else stays as it was written.
std::u16string ResolveReference(std::string_view reference)
{
    std::string_view name = reference.substr(1, reference.size() - 2);
    if (name.starts_with('#'))
    {
        bool isHexadecimal = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
        std::string_view digits = name.substr(isHexadecimal ? 2 : 1);
        unsigned int number = 0;
        std::from_chars(digits.data(), digits.data() + digits.size(), number, isHexadecimal ? HexadecimalBase : DecimalBase);
        return std::u16string(1, static_cast<char16_t>(number));
    }

    for (const NamedCharacter& named : NamedCharacters)
    {
        if (named.name == name)
            return std::u16string(1, named.character);
    }
    return Utf8ToText(reference);
}

std::u16string DecodeText(std::string_view utf8Text)
{
    std::u16string text;
    std::size_t index = 0;
    while (index < utf8Text.size())
    {
        std::size_t ampersand = utf8Text.find('&', index);
        std::size_t semicolon = ampersand == std::string_view::npos ? ampersand : utf8Text.find(';', ampersand);
        if (semicolon == std::string_view::npos)
        {
            text += Utf8ToText(utf8Text.substr(index));
            break;
        }

        if (semicolon - ampersand > LongestReference)
        {
            text += Utf8ToText(utf8Text.substr(index, ampersand + 1 - index));
            index = ampersand + 1;
            continue;
        }

        text += Utf8ToText(utf8Text.substr(index, ampersand - index));
        text += ResolveReference(utf8Text.substr(ampersand, semicolon - ampersand + 1));
        index = semicolon + 1;
    }
    return text;
}

class HtmlReader
{
public:
    explicit HtmlReader(std::string_view utf8Page)
        : page(utf8Page)
    {
        openElements.push_back(&root);
    }

    HtmlNode Read()
    {
        while (position < page.size())
        {
            if (page[position] == '<')
                ReadMarkup();
            else
                ReadText();
        }
        return std::move(root);
    }

private:
    void ReadText()
    {
        std::size_t end = std::min(page.find('<', position), page.size());
        AddText(DecodeText(page.substr(position, end - position)));
        position = end;
    }

    void AddText(std::u16string text)
    {
        if (text.empty())
            return;

        HtmlNode textNode;
        textNode.text = std::move(text);
        openElements.back()->children.push_back(std::move(textNode));
    }

    void ReadMarkup()
    {
        if (page.substr(position).starts_with("<!--"))
        {
            SkipPast("-->");
            return;
        }

        bool isDeclaration = position + 1 < page.size() && page[position + 1] == '!';
        bool isClosingTag = position + 1 < page.size() && page[position + 1] == '/';
        if (isDeclaration)
            SkipPast(">");
        else if (isClosingTag)
            ReadClosingTag();
        else
            ReadOpeningTag();
    }

    void SkipPast(std::string_view end)
    {
        std::size_t found = page.find(end, position);
        position = found == std::string_view::npos ? page.size() : found + end.size();
    }

    std::string ReadName()
    {
        std::size_t start = position;
        while (position < page.size() && IsNameCharacter(page[position]))
            ++position;

        return ToLowerCase(page.substr(start, position - start));
    }

    void SkipSpaces()
    {
        while (position < page.size() && IsSpace(page[position]))
            ++position;
    }

    std::string ReadAttributeValue()
    {
        SkipSpaces();
        if (position >= page.size())
            return {};

        char quote = page[position];
        if (quote == '"' || quote == '\'')
        {
            std::size_t end = std::min(page.find(quote, position + 1), page.size());
            std::string value(page.substr(position + 1, end - position - 1));
            position = std::min(end + 1, page.size());
            return value;
        }

        std::size_t start = position;
        while (position < page.size() && !IsSpace(page[position]) && page[position] != '>')
            ++position;

        return std::string(page.substr(start, position - start));
    }

    void ReadAttributes(HtmlNode& element)
    {
        while (true)
        {
            SkipSpaces();
            if (position >= page.size() || page[position] == '>' || page[position] == '/')
                return;

            std::string name = ReadName();
            SkipSpaces();
            std::string value;
            if (position < page.size() && page[position] == '=')
            {
                ++position;
                value = ReadAttributeValue();
            }

            if (!name.empty())
                element.attributes[name] = value;
            else
                ++position;
        }
    }

    void ReadOpeningTag()
    {
        ++position;
        HtmlNode element;
        element.tag = ReadName();
        ReadAttributes(element);
        SkipPast(">");

        CloseImplicitly(element.tag);
        if (IsOneOf(element.tag, RawTextElements))
        {
            element.text = ReadRawText(element.tag);
            openElements.back()->children.push_back(std::move(element));
            return;
        }

        std::vector<HtmlNode>& siblings = openElements.back()->children;
        siblings.push_back(std::move(element));
        if (!IsOneOf(siblings.back().tag, EmptyElements))
            openElements.push_back(&siblings.back());
    }

    std::u16string ReadRawText(std::string_view tag)
    {
        std::string closingTag = "</" + std::string(tag);
        std::size_t end = page.find(closingTag, position);
        if (end == std::string_view::npos)
            end = page.size();

        std::u16string text = Utf8ToText(page.substr(position, end - position));
        position = end;
        SkipPast(">");
        return text;
    }

    void ReadClosingTag()
    {
        position += 2;
        std::string name = ReadName();
        SkipPast(">");
        CloseUpTo(name);
    }

    // A new cell ends the cell before it, and a new row the row before it, as the browser does for pages that leave them open.
    void CloseImplicitly(std::string_view tag)
    {
        bool isCell = tag == "td" || tag == "th";
        if (isCell && IsOpenInsideRow("td"))
            CloseUpTo("td");
        if (isCell && IsOpenInsideRow("th"))
            CloseUpTo("th");
        if (tag == "tr" && IsOpenInsideTable("tr"))
            CloseUpTo("tr");
    }

    bool IsOpenInsideRow(std::string_view tag) const
    {
        return IsOpenBelow(tag, "tr");
    }

    bool IsOpenInsideTable(std::string_view tag) const
    {
        return IsOpenBelow(tag, "table");
    }

    // True when an element with the tag is open and no element named boundary is open inside it.
    bool IsOpenBelow(std::string_view tag, std::string_view boundary) const
    {
        for (auto element = openElements.rbegin(); element != openElements.rend(); ++element)
        {
            if ((*element)->tag == tag)
                return true;
            if ((*element)->tag == boundary)
                return false;
        }
        return false;
    }

    void CloseUpTo(std::string_view tag)
    {
        auto found = std::find_if(openElements.rbegin(), openElements.rend(), [&](const HtmlNode* element) { return element->tag == tag; });
        if (found == openElements.rend() || *found == &root)
            return;

        openElements.erase(std::prev(found.base()), openElements.end());
    }

    std::string_view page;
    std::size_t position = 0;
    HtmlNode root;

    // The elements that are open, the root first. The pointers stay valid: only the innermost element gets new children.
    std::vector<HtmlNode*> openElements;
};

}

bool HtmlNode::IsText() const
{
    return tag.empty() && children.empty() && !text.empty();
}

std::string HtmlNode::Attribute(std::string_view name) const
{
    auto found = attributes.find(std::string(name));
    if (found == attributes.end())
        return {};

    return found->second;
}

bool HtmlNode::HasAttribute(std::string_view name) const
{
    return attributes.contains(std::string(name));
}

HtmlNode ReadHtmlDocument(std::string_view utf8Page)
{
    return HtmlReader(utf8Page).Read();
}

const HtmlNode* FindElement(const HtmlNode& root, std::string_view tag)
{
    for (const HtmlNode& child : root.children)
    {
        if (child.tag == tag)
            return &child;

        const HtmlNode* found = FindElement(child, tag);
        if (found != nullptr)
            return found;
    }
    return nullptr;
}

}
