#include "Channels/Manual/PageLayouter.h"

#include "Channels/ChannelError.h"

#include <algorithm>
#include <format>

namespace Channels
{

namespace
{

// The attributes that make an element do something under the pointer.
constexpr std::string_view LinkAddress = "href";
constexpr std::string_view PressHandler = "onclick";
constexpr std::string_view ComingHandler = "onmouseover";
constexpr std::string_view LeavingHandler = "onmouseout";

// Links to a place on the same page or to a script do not open a page.
constexpr std::string_view PlaceOnThisPage = "#";
constexpr std::string_view ScriptAddress = "javascript:";

// A frameset row or column of "*" takes the room that the others leave.
constexpr std::string_view RemainingRoom = "*";

bool IsCollapsibleSpace(char16_t character)
{
    return character == u' ' || character == u'\t' || character == u'\n' || character == u'\r';
}

// Scripts and style sheets are plain ASCII; anything beyond it becomes a question mark.
constexpr char16_t FirstCharacterBeyondAscii = 0x80;

std::string AsciiOf(std::u16string_view text)
{
    std::string ascii;
    for (char16_t character : text)
    {
        bool isAscii = character < FirstCharacterBeyondAscii;
        ascii += isAscii ? static_cast<char>(character) : '?';
    }
    return ascii;
}

std::string NameOf(const HtmlNode& element)
{
    if (element.HasAttribute("name"))
        return element.Attribute("name");

    return element.Attribute("id");
}

std::vector<std::string_view> SplitAtCommas(std::string_view text)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = std::min(text.find(',', start), text.size());
        parts.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return parts;
}

// "71,30,*,63" in a room of 456 gives 71, 30, 292 and 63.
std::vector<float> FrameSizes(std::string_view sizesText, float room)
{
    std::vector<std::string_view> sizeTexts = SplitAtCommas(sizesText);
    std::vector<float> sizes(sizeTexts.size(), 0);
    float fixedRoom = 0;
    std::size_t remainingCount = 0;
    for (std::size_t index = 0; index < sizeTexts.size(); ++index)
    {
        if (sizeTexts[index].find(RemainingRoom) != std::string_view::npos)
        {
            ++remainingCount;
            continue;
        }

        sizes[index] = ReadLength(sizeTexts[index], 0).value_or(PageLength{}).Of(room);
        fixedRoom += sizes[index];
    }

    for (std::size_t index = 0; index < sizeTexts.size(); ++index)
    {
        if (sizeTexts[index].find(RemainingRoom) != std::string_view::npos)
            sizes[index] = std::max(room - fixedRoom, 0.0f) / static_cast<float>(remainingCount);
    }
    return sizes;
}

PageRectangle Union(const PageRectangle& first, const PageRectangle& second)
{
    float left = std::min(first.left, second.left);
    float top = std::min(first.top, second.top);
    float right = std::max(first.left + first.width, second.left + second.width);
    float bottom = std::max(first.top + first.height, second.top + second.height);
    return {left, top, right - left, bottom - top};
}

}

std::string FolderOf(std::string_view path)
{
    std::size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string_view::npos)
        return {};

    return std::string(path.substr(0, lastSlash + 1));
}

std::string ResolvePath(std::string_view pagePath, std::string_view relativePath)
{
    std::size_t end = std::min(relativePath.find_first_of("#?"), relativePath.size());
    std::string joined = FolderOf(pagePath) + std::string(relativePath.substr(0, end));

    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= joined.size())
    {
        std::size_t slash = std::min(joined.find('/', start), joined.size());
        std::string part = joined.substr(start, slash - start);
        start = slash + 1;

        if (part == "..")
        {
            if (!parts.empty())
                parts.pop_back();
            continue;
        }
        if (part != "." && !part.empty())
            parts.push_back(part);
    }

    std::string resolved;
    for (const std::string& part : parts)
        resolved += resolved.empty() ? part : "/" + part;

    return resolved;
}

PageLayouter::PageLayouter(const PageFiles& pageFiles, std::string_view pagePath, const Text::TextLayouter& textFont, LaidOutPage& page)
    : files(pageFiles)
    , path(pagePath)
    , folder(FolderOf(pagePath))
    , font(textFont)
    , output(&page)
{
    std::optional<Formats::ByteView> bytes = files.readFile(path);
    if (!bytes)
        throw ChannelError(std::format("The manual has no page {}", path));

    std::string_view pageText(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    document = ReadHtmlDocument(pageText);
    ReadHead(document);
}

float PageLayouter::LayOut(const PageRectangle& screen)
{
    const HtmlNode* frameset = FindElement(document, "frameset");
    if (frameset != nullptr)
        return LayOutFrameset(*frameset, screen);

    const HtmlNode* body = FindElement(document, "body");
    BoxStyle rootStyle;
    if (body == nullptr)
        return LayOutChildren(document, rootStyle, screen.left, screen.top, screen.width);

    BoxStyle bodyStyle = StyleOf(*body, rootStyle);
    std::size_t backgroundPart = output->parts.size();
    float contentLeft = screen.left + bodyStyle.paddingLeft;
    float contentTop = screen.top + bodyStyle.paddingTop;
    float contentWidth = screen.width - bodyStyle.paddingLeft - bodyStyle.paddingRight;
    float pageHeight = bodyStyle.paddingTop + LayOutChildren(*body, bodyStyle, contentLeft, contentTop, contentWidth);

    DrawBackground(backgroundPart, *body, bodyStyle, {screen.left, screen.top, screen.width, std::max(screen.height, pageHeight)});
    return pageHeight;
}

// The style sheets, the <style> elements and the scripts of the page, wherever they are.
void PageLayouter::ReadHead(const HtmlNode& node)
{
    for (const HtmlNode& child : node.children)
    {
        bool isStyleSheetLink = child.tag == "link" && child.Attribute("rel") == "stylesheet";
        if (isStyleSheetLink)
        {
            std::string sheetPath = ResolvePath(path, child.Attribute("href"));
            std::optional<Formats::ByteView> sheet = files.readFile(sheetPath);
            if (sheet)
                styleSheet.AddStyleSheet(std::string_view(reinterpret_cast<const char*>(sheet->data()), sheet->size()), FolderOf(sheetPath));
        }

        if (child.tag == "style")
            styleSheet.AddStyleSheet(AsciiOf(child.text), folder);

        if (child.tag == "script" && child.HasAttribute("src"))
        {
            std::optional<Formats::ByteView> code = files.readFile(ResolvePath(path, child.Attribute("src")));
            if (code)
                script.AddScript(std::string_view(reinterpret_cast<const char*>(code->data()), code->size()));
        }

        if (child.tag == "script")
            script.AddScript(AsciiOf(child.text));

        ReadHead(child);
    }
}

// Each frame shows its own document in its part of the screen, one under the other for rows and side by side for columns.
float PageLayouter::LayOutFrameset(const HtmlNode& frameset, const PageRectangle& screen)
{
    bool isRows = frameset.HasAttribute("rows");
    std::string sizesText = isRows ? frameset.Attribute("rows") : frameset.Attribute("cols");
    float room = isRows ? screen.height : screen.width;
    std::vector<float> sizes = FrameSizes(sizesText, room);

    std::size_t frameIndex = 0;
    float offset = 0;
    for (const HtmlNode& child : frameset.children)
    {
        if (child.tag != "frame" && child.tag != "frameset")
            continue;
        if (frameIndex >= sizes.size())
            break;

        PageRectangle frameScreen = screen;
        if (isRows)
        {
            frameScreen.top += offset;
            frameScreen.height = sizes[frameIndex];
        }
        else
        {
            frameScreen.left += offset;
            frameScreen.width = sizes[frameIndex];
        }

        if (child.tag == "frameset")
        {
            LayOutFrameset(child, frameScreen);
        }
        else
        {
            PageFrameArea frame;
            frame.place = frameScreen;
            frame.firstPart = output->parts.size();
            frame.firstLink = output->links.size();
            frame.contentHeight = PageLayouter(files, ResolvePath(path, child.Attribute("src")), font, *output).LayOut(frameScreen);
            frame.endPart = output->parts.size();
            frame.endLink = output->links.size();
            output->frames.push_back(frame);
        }

        offset += sizes[frameIndex];
        ++frameIndex;
    }
    return screen.height;
}

// The children one under the other: runs of text and inline elements become lines, and every block starts on a line of its own.
float PageLayouter::LayOutChildren(const HtmlNode& node, const BoxStyle& style, float left, float top, float width)
{
    float y = top;
    std::vector<InlineItem> lineItems;
    for (const HtmlNode& child : node.children)
    {
        if (!child.IsText() && IsHiddenElement(child.tag))
            continue;

        if (child.IsText() || !IsBlockElement(child.tag))
        {
            CollectInlineItems(child, style, {}, lineItems);
            continue;
        }

        y += LayOutLines(lineItems, style, left, y, width);
        lineItems.clear();
        y += LayOutBlock(child, style, left, y, width);
    }

    y += LayOutLines(lineItems, style, left, y, width);
    return y - top;
}

float PageLayouter::LayOutBlock(const HtmlNode& element, const BoxStyle& parentStyle, float left, float top, float width)
{
    if (element.tag == "table")
        return LayOutTable(element, parentStyle, left, top, width);

    BoxStyle style = StyleOf(element, parentStyle);
    float boxTop = top + style.marginTop;
    float contentWidth = style.width ? style.width->Of(width) : width - style.paddingLeft - style.paddingRight;
    float boxWidth = contentWidth + style.paddingLeft + style.paddingRight;

    // The old way of centering: a block of a given width inside <center> or an element with align="center".
    float boxLeft = left;
    if (style.width && parentStyle.textAlignment == TextAlignment::Center)
        boxLeft = left + (width - boxWidth) / 2;

    std::size_t backgroundPart = output->parts.size();
    std::optional<std::size_t> link = AddLink(element);
    float contentHeight = LayOutChildren(element, style, boxLeft + style.paddingLeft, boxTop + style.paddingTop, contentWidth);
    if (style.height)
        contentHeight = std::max(contentHeight, style.height->Of(0));

    PageRectangle box = {boxLeft, boxTop, boxWidth, contentHeight + style.paddingTop + style.paddingBottom};
    DrawBackground(backgroundPart, element, style, box);
    if (link)
        GrowLink(*link, box);

    return style.marginTop + box.height + style.marginBottom;
}

void PageLayouter::CollectInlineItems(const HtmlNode& node, const BoxStyle& style, std::vector<std::size_t> links, std::vector<InlineItem>& items)
{
    if (node.IsText())
    {
        AddWords(node.text, style.look, links, items);
        return;
    }

    if (IsHiddenElement(node.tag))
        return;

    if (node.tag == "br")
    {
        InlineItem lineBreak;
        lineBreak.kind = InlineItem::Kind::LineBreak;
        lineBreak.look = style.look;
        lineBreak.height = TextLineHeight(style.look);
        items.push_back(lineBreak);
        return;
    }

    std::optional<std::size_t> ownLink = AddLink(node);
    if (ownLink)
        links.push_back(*ownLink);

    if (node.tag == "img")
    {
        items.push_back(PictureItem(node, links));
        return;
    }

    BoxStyle childStyle = StyleOf(node, style);
    for (const HtmlNode& child : node.children)
        CollectInlineItems(child, childStyle, links, items);
}

// The browser joins every run of spaces and line ends into one space; a no-break space is part of its word.
void PageLayouter::AddWords(std::u16string_view text, const PageTextLook& look, const std::vector<std::size_t>& links, std::vector<InlineItem>& items) const
{
    std::u16string word;
    auto finishWord = [&]() {
        if (word.empty())
            return;

        InlineItem item;
        item.kind = InlineItem::Kind::Word;
        item.text = word;
        item.look = look;
        item.width = TextWidth(word, look);
        item.height = TextLineHeight(look);
        item.links = links;
        items.push_back(item);
        word.clear();
    };

    for (char16_t character : text)
    {
        if (!IsCollapsibleSpace(character))
        {
            word += character;
            continue;
        }

        finishWord();
        bool followsSpace = !items.empty() && items.back().kind == InlineItem::Kind::Space;
        if (followsSpace)
            continue;

        InlineItem space;
        space.kind = InlineItem::Kind::Space;
        space.look = look;
        space.width = TextWidth(u" ", look);
        items.push_back(space);
    }
    finishWord();
}

PageLayouter::InlineItem PageLayouter::PictureItem(const HtmlNode& image, const std::vector<std::size_t>& links) const
{
    InlineItem item;
    item.kind = InlineItem::Kind::Picture;
    item.path = ResolvePath(path, image.Attribute("src"));
    item.elementName = NameOf(image);
    item.links = links;

    std::optional<Formats::Vector2> size = PictureSizeOf(image);
    if (size)
    {
        item.width = size->x;
        item.height = size->y;
    }

    std::string alignment = image.Attribute("align");
    item.isCenteredInLine = alignment == "middle" || alignment == "center" || alignment == "absmiddle";
    if (alignment == "left")
        item.floatSide = TextAlignment::Left;
    if (alignment == "right")
        item.floatSide = TextAlignment::Right;
    item.verticalSpace = ReadLength(image.Attribute("vspace"), 0).value_or(PageLength{}).value;
    return item;
}

float PageLayouter::LayOutLines(const std::vector<InlineItem>& items, const BoxStyle& style, float left, float top, float width)
{
    float y = top;
    std::vector<InlineItem> line;
    float lineWidth = 0;
    bool isFirstLine = true;
    std::vector<FloatingPicture> floats;

    // The room of a line beside the pictures that stand at the sides where the line starts.
    auto lineLeft = [&]() {
        float floatsWidth = 0;
        for (const FloatingPicture& picture : floats)
        {
            if (picture.side == TextAlignment::Left && picture.bottom > y)
                floatsWidth += picture.width;
        }
        return left + floatsWidth;
    };
    auto lineWidthRoom = [&]() {
        float floatsWidth = 0;
        for (const FloatingPicture& picture : floats)
        {
            if (picture.bottom > y)
                floatsWidth += picture.width;
        }
        return width - floatsWidth;
    };
    auto finishLine = [&]() {
        float lineHeight = PlaceLine(line, style, lineLeft(), y, lineWidthRoom(), isFirstLine);
        line.clear();
        lineWidth = 0;
        isFirstLine = false;
        return lineHeight;
    };

    for (const InlineItem& item : items)
    {
        if (item.floatSide)
        {
            PlaceFloatingPicture(item, left, y, width, floats);
            continue;
        }

        if (item.kind == InlineItem::Kind::LineBreak)
        {
            bool isEmptyLine = line.empty();
            float lineHeight = finishLine();
            y += isEmptyLine ? item.height : lineHeight;
            continue;
        }

        if (item.kind == InlineItem::Kind::Space && line.empty())
            continue;

        float room = isFirstLine ? lineWidthRoom() - style.textIndent : lineWidthRoom();
        bool overflows = item.kind != InlineItem::Kind::Space && !line.empty() && lineWidth + item.width > room;
        if (overflows)
            y += finishLine();

        line.push_back(item);
        lineWidth += item.width;
    }

    y += finishLine();
    for (const FloatingPicture& picture : floats)
        y = std::max(y, picture.bottom);

    return y - top;
}

void PageLayouter::PlaceFloatingPicture(const InlineItem& item, float left, float top, float width, std::vector<FloatingPicture>& floats)
{
    float pictureLeft = left;
    for (const FloatingPicture& picture : floats)
    {
        if (picture.side == item.floatSide && picture.bottom > top)
            pictureLeft += item.floatSide == TextAlignment::Left ? picture.width : -picture.width;
    }
    if (item.floatSide == TextAlignment::Right)
        pictureLeft += width - item.width;

    PagePart picture;
    picture.kind = PagePartKind::Picture;
    picture.place = {pictureLeft, top + item.verticalSpace, item.width, item.height};
    picture.path = item.path;
    picture.elementName = item.elementName;
    output->parts.push_back(picture);

    for (std::size_t link : item.links)
        GrowLink(link, picture.place);

    floats.push_back({*item.floatSide, item.width, top + item.height + 2 * item.verticalSpace});
}

// Places one line and gives its height. The words stand on the bottom of the line, as on a base line; a picture with align="middle"
// sits in the middle of the line, and the words of its line too.
float PageLayouter::PlaceLine(const std::vector<InlineItem>& line, const BoxStyle& style, float left, float top, float width, bool isFirstLine)
{
    std::size_t end = line.size();
    while (end > 0 && line[end - 1].kind == InlineItem::Kind::Space)
        --end;
    if (end == 0)
        return 0;

    float lineWidth = 0;
    float lineHeight = 0;
    bool hasCenteredPicture = false;
    for (std::size_t index = 0; index < end; ++index)
    {
        const InlineItem& item = line[index];
        lineWidth += item.width;
        lineHeight = std::max(lineHeight, item.height + 2 * item.verticalSpace);
        hasCenteredPicture = hasCenteredPicture || (item.kind == InlineItem::Kind::Picture && item.isCenteredInLine);
    }

    float indent = isFirstLine ? style.textIndent : 0;
    float x = left + indent;
    if (style.textAlignment == TextAlignment::Center)
        x = left + (width - lineWidth) / 2;
    else if (style.textAlignment == TextAlignment::Right)
        x = left + width - lineWidth;

    for (std::size_t index = 0; index < end; ++index)
    {
        const InlineItem& item = line[index];
        bool isCentered = hasCenteredPicture && (item.kind == InlineItem::Kind::Word || item.isCenteredInLine);
        float itemTop = isCentered ? top + (lineHeight - item.height) / 2 : top + lineHeight - item.verticalSpace - item.height;
        PageRectangle place = {x, itemTop, item.width, item.height};

        if (item.kind == InlineItem::Kind::Word)
        {
            PagePart text;
            text.kind = PagePartKind::Text;
            text.place = place;
            text.text = item.text;
            text.look = item.look;
            output->parts.push_back(text);
        }

        if (item.kind == InlineItem::Kind::Picture)
        {
            PagePart picture;
            picture.kind = PagePartKind::Picture;
            picture.place = place;
            picture.path = item.path;
            picture.elementName = item.elementName;
            output->parts.push_back(picture);
        }

        for (std::size_t link : item.links)
            GrowLink(link, place);

        x += item.width;
    }
    return lineHeight;
}

// The background goes in front of the parts of the element's content, which were laid out first to learn the size of the box.
void PageLayouter::DrawBackground(std::size_t partIndex, const HtmlNode& element, const BoxStyle& style, const PageRectangle& box)
{
    std::vector<PagePart> background;
    if (style.backgroundColor)
    {
        PagePart fill;
        fill.kind = PagePartKind::Fill;
        fill.place = box;
        fill.color = *style.backgroundColor;
        background.push_back(fill);
    }

    if (!style.backgroundPath.empty())
    {
        PagePart picture;
        picture.kind = PagePartKind::Picture;
        picture.place = box;
        picture.path = style.backgroundPath;
        picture.elementName = NameOf(element);
        picture.isTiled = true;
        background.push_back(picture);
    }

    auto where = output->parts.begin() + static_cast<std::ptrdiff_t>(std::min(partIndex, output->parts.size()));
    output->parts.insert(where, background.begin(), background.end());
}

std::optional<std::size_t> PageLayouter::AddLink(const HtmlNode& element)
{
    bool hasHandlers = element.HasAttribute(PressHandler) || element.HasAttribute(ComingHandler) || element.HasAttribute(LeavingHandler);
    bool hasAddress = element.tag == "a" && element.HasAttribute(LinkAddress);
    if (!hasHandlers && !hasAddress)
        return std::nullopt;

    auto resolved = [&](std::vector<PageAction> actions) {
        for (PageAction& action : actions)
        {
            if (!action.path.empty())
                action.path = ResolvePath(path, action.path);
        }
        return actions;
    };

    PageLink link;
    link.whenPointerComes = resolved(script.ActionsOf(element.Attribute(ComingHandler)));
    link.whenPointerLeaves = resolved(script.ActionsOf(element.Attribute(LeavingHandler)));
    link.whenPressed = resolved(script.ActionsOf(element.Attribute(PressHandler)));

    std::string address = element.Attribute(LinkAddress);
    bool opensPage = hasAddress && !address.empty() && !address.starts_with(PlaceOnThisPage) && !address.starts_with(ScriptAddress);
    if (opensPage)
        link.whenPressed.push_back({PageActionKind::OpenPage, {}, ResolvePath(path, address)});

    output->links.push_back(link);
    return output->links.size() - 1;
}

void PageLayouter::GrowLink(std::size_t link, const PageRectangle& place)
{
    PageRectangle& linkPlace = output->links[link].place;
    bool isEmpty = linkPlace.width == 0 && linkPlace.height == 0;
    linkPlace = isEmpty ? place : Union(linkPlace, place);
}

LaidOutPage LayOutPage(const PageFiles& files, std::string_view path, const Text::TextLayouter& font, float screenWidth, float screenHeight)
{
    LaidOutPage page;
    page.path = std::string(path);
    PageLayouter layouter(files, path, font, page);
    page.height = layouter.LayOut({0, 0, screenWidth, screenHeight});
    return page;
}

}
