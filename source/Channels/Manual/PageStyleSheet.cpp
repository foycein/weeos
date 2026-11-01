#include "Channels/Manual/PageStyleSheet.h"

#include <algorithm>
#include <cctype>

namespace Channels
{

namespace
{

// CSS weighs an id above a class and a class above a tag.
constexpr int TagSpecificity = 1;
constexpr int ClassSpecificity = 10;
constexpr int IdSpecificity = 100;

// A selector with one of these is more than a tag, a class or an id.
constexpr std::string_view UnsupportedSelectorCharacters = ":[]>+~ \t\n\r";

constexpr std::string_view UniversalSelector = "*";

std::string_view Trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);

    return text;
}

std::string ToLowerCase(std::string_view text)
{
    std::string lower(text);
    for (char& character : lower)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

    return lower;
}

std::string WithoutComments(std::string_view cssText)
{
    std::string text;
    std::size_t position = 0;
    while (position < cssText.size())
    {
        std::size_t commentStart = cssText.find("/*", position);
        if (commentStart == std::string_view::npos)
        {
            text += cssText.substr(position);
            break;
        }

        text += cssText.substr(position, commentStart - position);
        std::size_t commentEnd = cssText.find("*/", commentStart + 2);
        position = commentEnd == std::string_view::npos ? cssText.size() : commentEnd + 2;
    }
    return text;
}

// The pages wrap their <style> content in an HTML comment, which CSS ignores.
std::string WithoutHtmlCommentMarks(std::string text)
{
    for (std::string_view mark : {std::string_view("<!--"), std::string_view("-->")})
    {
        std::size_t found = text.find(mark);
        while (found != std::string::npos)
        {
            text.replace(found, mark.size(), " ");
            found = text.find(mark);
        }
    }
    return text;
}

std::vector<std::string_view> Split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = std::min(text.find(separator, start), text.size());
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

std::vector<std::string> ClassNamesOf(const HtmlNode& element)
{
    std::string classes = element.Attribute("class");
    std::vector<std::string> names;
    for (std::string_view name : Split(classes, ' '))
    {
        if (!Trim(name).empty())
            names.emplace_back(Trim(name));
    }
    return names;
}

}

std::vector<StyleDeclaration> ReadDeclarations(std::string_view declarationText, std::string_view baseFolder)
{
    std::vector<StyleDeclaration> declarations;
    for (std::string_view part : Split(declarationText, ';'))
    {
        std::size_t colon = part.find(':');
        if (colon == std::string_view::npos)
            continue;

        StyleDeclaration declaration;
        declaration.property = ToLowerCase(Trim(part.substr(0, colon)));
        declaration.value = std::string(Trim(part.substr(colon + 1)));
        declaration.baseFolder = std::string(baseFolder);
        if (!declaration.property.empty())
            declarations.push_back(std::move(declaration));
    }
    return declarations;
}

void PageStyleSheet::AddStyleSheet(std::string_view cssText, std::string_view baseFolder)
{
    std::string text = WithoutHtmlCommentMarks(WithoutComments(cssText));
    std::size_t position = 0;
    while (position < text.size())
    {
        std::size_t open = text.find('{', position);
        if (open == std::string::npos)
            return;

        std::size_t close = text.find('}', open);
        if (close == std::string::npos)
            close = text.size();

        StyleRule rule;
        rule.selectors = ReadSelectors(std::string_view(text).substr(position, open - position));
        rule.declarations = ReadDeclarations(std::string_view(text).substr(open + 1, close - open - 1), baseFolder);
        if (!rule.selectors.empty())
            rules.push_back(std::move(rule));

        position = close + 1;
    }
}

std::vector<StyleDeclaration> PageStyleSheet::DeclarationsFor(const HtmlNode& element, std::string_view pageFolder) const
{
    struct WeighedDeclaration
    {
        int specificity;
        std::size_t order;
        const StyleDeclaration* declaration;
    };

    std::vector<WeighedDeclaration> weighed;
    for (const StyleRule& rule : rules)
    {
        int bestSpecificity = -1;
        for (const SimpleSelector& selector : rule.selectors)
        {
            if (Matches(selector, element))
                bestSpecificity = std::max(bestSpecificity, selector.specificity);
        }

        if (bestSpecificity < 0)
            continue;

        for (const StyleDeclaration& declaration : rule.declarations)
            weighed.push_back({bestSpecificity, weighed.size(), &declaration});
    }

    std::stable_sort(weighed.begin(), weighed.end(),
        [](const WeighedDeclaration& first, const WeighedDeclaration& second) { return first.specificity < second.specificity; });

    std::vector<StyleDeclaration> declarations;
    for (const WeighedDeclaration& entry : weighed)
        declarations.push_back(*entry.declaration);

    std::vector<StyleDeclaration> ownStyle = ReadDeclarations(element.Attribute("style"), pageFolder);
    declarations.insert(declarations.end(), ownStyle.begin(), ownStyle.end());
    return declarations;
}

std::vector<PageStyleSheet::SimpleSelector> PageStyleSheet::ReadSelectors(std::string_view selectorText)
{
    std::vector<SimpleSelector> selectors;
    for (std::string_view part : Split(selectorText, ','))
    {
        std::string_view text = Trim(part);
        if (text.empty() || text.find_first_of(UnsupportedSelectorCharacters) != std::string_view::npos)
            continue;

        SimpleSelector selector;
        if (text == UniversalSelector)
        {
            selectors.push_back(selector);
            continue;
        }

        std::size_t mark = text.find_first_of(".#");
        selector.tag = ToLowerCase(text.substr(0, mark));
        if (!selector.tag.empty())
            selector.specificity += TagSpecificity;

        if (mark != std::string_view::npos && text[mark] == '.')
        {
            selector.className = std::string(text.substr(mark + 1));
            selector.specificity += ClassSpecificity;
        }
        if (mark != std::string_view::npos && text[mark] == '#')
        {
            selector.id = std::string(text.substr(mark + 1));
            selector.specificity += IdSpecificity;
        }
        selectors.push_back(selector);
    }
    return selectors;
}

bool PageStyleSheet::Matches(const SimpleSelector& selector, const HtmlNode& element)
{
    if (!selector.tag.empty() && selector.tag != element.tag)
        return false;

    if (!selector.id.empty() && selector.id != element.Attribute("id"))
        return false;

    if (selector.className.empty())
        return true;

    std::vector<std::string> classNames = ClassNamesOf(element);
    return std::find(classNames.begin(), classNames.end(), selector.className) != classNames.end();
}

}
