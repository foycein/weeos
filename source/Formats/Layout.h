#pragma once

#include "Formats/Bytes.h"
#include "Formats/LayoutMaterial.h"
#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Formats
{

enum class PaneKind
{
    Null,
    Boundary,
    Picture,
    TextBox,
    Window,
};

enum class HorizontalOrigin
{
    Left,
    Center,
    Right,
};

enum class VerticalOrigin
{
    Top,
    Center,
    Bottom,
};

// The four corners of a picture: top left, top right, bottom left, bottom right.
constexpr std::size_t CornerCount = 4;

struct TextureCoordinateSet
{
    std::array<Vector2, CornerCount> corners;
};

// What a picture shows, and what lies inside a window.
struct PaneContent
{
    std::array<Rgba8Color, CornerCount> vertexColors;
    std::uint16_t materialIndex;
    std::vector<TextureCoordinateSet> textureCoordinateSets;
};

struct TextBox
{
    std::u16string text;

    // The most bytes the text may have, as the layout says.
    std::uint16_t maximumTextSize;

    std::uint16_t materialIndex;
    std::uint16_t fontIndex;
    std::uint8_t textOrigin;
    std::uint8_t lineAlignment;
    Rgba8Color topColor;
    Rgba8Color bottomColor;
    Vector2 fontSize;
    float characterSpace;
    float lineSpace;
};

struct WindowFrame
{
    std::uint16_t materialIndex;

    // How the texture of the frame is turned: 0 not, 1 flipped horizontally, 2 flipped vertically,
    // 3, 4 and 5 turned by 90, 180 and 270 degrees.
    std::uint8_t textureFlip;
};

struct Window
{
    float magnificationLeft;
    float magnificationRight;
    float magnificationUp;
    float magnificationDown;
    PaneContent content;
    std::vector<WindowFrame> frames;
};

// A pane is one part of a layout: a group of other panes, a picture, a text, a window or an area that reacts to the pointer.
// The panes of a layout form a tree.
struct Pane
{
    PaneKind kind;

    std::string name;
    std::string userInformation;
    bool isVisible;
    bool inheritsAlpha;
    bool adjustsLocationForWideScreen;
    HorizontalOrigin horizontalOrigin;
    VerticalOrigin verticalOrigin;
    std::uint8_t alpha;
    Vector3 translation;
    Vector3 rotation;
    Vector2 scale;
    float width;
    float height;

    std::optional<PaneContent> picture;
    std::optional<TextBox> textBox;
    std::optional<Window> window;

    std::vector<Pane> children;
};

// Groups of panes, so that an animation can address many panes at once. Groups can contain groups.
struct Group
{
    std::string name;
    std::vector<std::string> paneNames;
    std::vector<Group> children;
};

// A layout (BRLYT file) describes how a screen is built from panes.
struct Layout
{
    bool isCentered;
    float width;
    float height;
    std::vector<std::string> textureNames;
    std::vector<std::string> fontNames;
    std::vector<Material> materials;
    Pane rootPane;
    std::optional<Group> rootGroup;
};

Layout ReadLayout(ByteView bytes);

}
