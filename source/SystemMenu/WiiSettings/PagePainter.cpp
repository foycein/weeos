#include "SystemMenu/WiiSettings/PagePainter.h"

#include "SystemMenu/WiiSettings/NumberText.h"
#include "SystemMenu/WiiSettings/PageLooks.h"

namespace SystemMenu
{

namespace
{

constexpr Formats::Rgba8Color White = {255, 255, 255, 255};
constexpr Formats::Rgba8Color DarkText = {51, 51, 51, 255};
constexpr Formats::Rgba8Color DisabledTint = {110, 110, 110, 255};

constexpr float MessageTextSize = 26;
constexpr float BigNumberTextSize = 60;

// The background is a strip of 8 points that is repeated from the left to the right.
constexpr std::string_view BackgroundPicturePath = "FIX/COMMON/BG/BG_common.gif";
constexpr int BackgroundStripWidth = 8;

// The tabs: small ones in front of the large one that holds the title. The pictures differ with the depth.
constexpr int FirstTabLeft = 24;
constexpr int TabDistance = 32;
constexpr int SmallTabTop = 35;
constexpr int LargeTabTop = 27;
constexpr PageRectangle LargeTabSize = {0, 0, 408, 36};
constexpr int TitleIndent = 8;
constexpr int TitleWidth = 384;

// The three squares for the page numbers, at the lower right.
constexpr int PageSquareLeft = 440;
constexpr int PageSquareDistance = 48;
constexpr int PageSquareTop = 376;
constexpr PageRectangle PageSquareSize = {0, 0, 40, 32};
constexpr float PageNumberTextSize = 24;

std::string TabPicture(const char* fileName)
{
    return std::string("FIX/COMMON/BG/") + fileName;
}

// The small tabs from the left. One tab in front of the title is light gray, two are medium and light, three are dark, medium and light.
std::vector<std::string> SmallTabPictures(int tabDepth)
{
    if (tabDepth == 1)
        return {TabPicture("Tab_gray.gif")};

    if (tabDepth == 2)
        return {TabPicture("Tab_Middle_gray.gif"), TabPicture("Tab_gray02.gif")};

    if (tabDepth >= 3)
        return {TabPicture("Tab_Darkgray.gif"), TabPicture("Tab_Middle_gray02.gif"), TabPicture("Tab_gray02.gif")};

    return {};
}

float PageToCanvasX(float x)
{
    return x - static_cast<float>(PageWidth) / 2;
}

float PageToCanvasY(float y)
{
    return static_cast<float>(PageHeight) / 2 - y;
}

Graphics::Quad MakeQuad(const PageRectangle& bounds, const Formats::Rgba8Color& tint)
{
    float left = PageToCanvasX(static_cast<float>(bounds.left));
    float right = PageToCanvasX(static_cast<float>(bounds.left + bounds.width));
    float top = PageToCanvasY(static_cast<float>(bounds.top));
    float bottom = PageToCanvasY(static_cast<float>(bounds.top + bounds.height));

    Graphics::Quad quad;
    quad.positions = {{{left, top}, {right, top}, {right, bottom}, {left, bottom}}};
    quad.colors = {{tint, tint, tint, tint}};
    quad.textureCoordinates[0] = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    return quad;
}

Graphics::Material MakeMaterial(const PageImage& image)
{
    Graphics::Material material;
    material.textures.push_back({image.texture, Graphics::TextureMatrix()});
    return material;
}

}

PagePainter::PagePainter(Graphics::GraphicsDevice& graphicsDevice, PageImages& pageImages, PageFont& pageFont)
    : device(graphicsDevice)
    , images(pageImages)
    , font(pageFont)
{
}

void PagePainter::Draw(const Page& page, std::optional<std::size_t> pointedButton, const Graphics::Canvas& canvas)
{
    DrawBackground(canvas);
    DrawTabs(page);

    for (const PagePicture& picture : page.pictures)
        DrawPictureAtItsOwnSizeInTheMiddle(picture.imagePath, picture.bounds, White);

    for (std::size_t buttonIndex = 0; buttonIndex < page.buttons.size(); ++buttonIndex)
    {
        const PageButton& button = page.buttons[buttonIndex];
        if (button.isShown)
            DrawButton(button, pointedButton == buttonIndex);
    }

    DrawTexts(page);
    DrawPageNumbers(page);
}

void PagePainter::DrawBackground(const Graphics::Canvas& canvas)
{
    const PageImage& strip = images.Find(std::string(BackgroundPicturePath));
    Graphics::Material material = MakeMaterial(strip);

    float firstLeft = -canvas.width / 2;
    for (float left = firstLeft; left < canvas.width / 2; left += BackgroundStripWidth)
    {
        PageRectangle bounds = {static_cast<int>(left) + PageWidth / 2, 0, BackgroundStripWidth, PageHeight};
        device.DrawQuad(MakeQuad(bounds, White), material);
    }
}

void PagePainter::DrawTabs(const Page& page)
{
    std::vector<std::string> smallTabs = SmallTabPictures(page.tabDepth);
    for (std::size_t tabNumber = 0; tabNumber < smallTabs.size(); ++tabNumber)
    {
        const PageImage& tab = images.Find(smallTabs[tabNumber]);
        PageRectangle bounds = {FirstTabLeft + static_cast<int>(tabNumber) * TabDistance, SmallTabTop, tab.width, tab.height};
        device.DrawQuad(MakeQuad(bounds, White), MakeMaterial(tab));
    }

    int largeTabLeft = FirstTabLeft + page.tabDepth * TabDistance;
    PageRectangle largeTab = {largeTabLeft, LargeTabTop, LargeTabSize.width, LargeTabSize.height};
    DrawPicture(TabPicture("Tab_White_L.gif"), largeTab, White);

    PageRectangle titleBounds = {largeTabLeft + TitleIndent, LargeTabTop, TitleWidth, LargeTabSize.height};
    font.Draw(page.title, titleBounds, {Text::HorizontalAlignment::Left, Text::VerticalAlignment::Middle}, DarkText, MessageTextSize);
}

void PagePainter::DrawPageNumbers(const Page& page)
{
    for (int number = 1; number <= page.pageCount; ++number)
    {
        std::string pictureName = number == page.pageNumber ? "Icon_Index_Page_on.gif" : "Icon_Index_Page_off.gif";
        PageRectangle bounds = {PageSquareLeft + (number - 1) * PageSquareDistance, PageSquareTop, PageSquareSize.width, PageSquareSize.height};
        DrawPicture("FIX/COMMON/List_Icon/" + pictureName, bounds, White);

        Formats::Rgba8Color color = number == page.pageNumber ? DarkText : White;
        font.Draw(NumberText(number), bounds, {Text::HorizontalAlignment::Center, Text::VerticalAlignment::Middle}, color, PageNumberTextSize);
    }
}

void PagePainter::DrawButton(const PageButton& button, bool isPointed)
{
    const LookPictures& look = PicturesOfLook(button.look);
    Formats::Rgba8Color tint = button.isEnabled ? White : DisabledTint;

    if (!look.background.empty())
        DrawPictureAtItsOwnSizeInTheMiddle(look.background, button.bounds, tint);

    bool showsPointedOverlay = isPointed && button.isEnabled;
    DrawPicture(showsPointedOverlay ? look.pointedOverlay : look.idleOverlay, button.bounds, tint);

    if (look.hasChosenFrame && button.isChosen)
    {
        const PageImage& leftFrame = images.Find(look.chosenFrameLeft.imagePath);
        PageRectangle leftBounds = {button.bounds.left + look.chosenFrameLeft.offsetFromLeft, button.bounds.top + look.chosenFrameLeft.offsetFromTop,
            leftFrame.width, leftFrame.height};
        DrawPicture(look.chosenFrameLeft.imagePath, leftBounds, White);

        const PageImage& rightFrame = images.Find(look.chosenFrameRight.imagePath);
        int rightLeft = button.bounds.left + button.bounds.width + look.chosenFrameRight.offsetFromLeft;
        PageRectangle rightBounds = {rightLeft, button.bounds.top + look.chosenFrameRight.offsetFromTop, rightFrame.width, rightFrame.height};
        DrawPicture(look.chosenFrameRight.imagePath, rightBounds, White);
    }

    font.Draw(button.text, button.bounds, {Text::HorizontalAlignment::Center, Text::VerticalAlignment::Middle}, DarkText, look.textSize);
}

void PagePainter::DrawTexts(const Page& page)
{
    for (const PageText& text : page.texts)
    {
        std::u16string joinedLines;
        for (const std::u16string& line : text.lines)
        {
            if (!joinedLines.empty())
                joinedLines += u'\n';

            joinedLines += line;
        }

        float size = text.role == TextRole::BigNumber ? BigNumberTextSize : MessageTextSize;
        Formats::Rgba8Color color = text.role == TextRole::Label ? DarkText : White;
        font.Draw(joinedLines, text.bounds, {Text::HorizontalAlignment::Center, Text::VerticalAlignment::Middle}, color, size);
    }
}

void PagePainter::DrawPicture(const std::string& imagePath, const PageRectangle& bounds, const Formats::Rgba8Color& tint)
{
    device.DrawQuad(MakeQuad(bounds, tint), MakeMaterial(images.Find(imagePath)));
}

void PagePainter::DrawPictureAtItsOwnSizeInTheMiddle(const std::string& imagePath, const PageRectangle& bounds, const Formats::Rgba8Color& tint)
{
    const PageImage& image = images.Find(imagePath);
    PageRectangle ownBounds = {bounds.left + (bounds.width - image.width) / 2, bounds.top + (bounds.height - image.height) / 2, image.width,
        image.height};
    device.DrawQuad(MakeQuad(ownBounds, tint), MakeMaterial(image));
}

}
