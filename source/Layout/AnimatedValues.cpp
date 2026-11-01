#include "Layout/AnimatedValues.h"

#include "Layout/LayoutError.h"

#include <algorithm>
#include <format>

namespace Layout
{

namespace
{

// The numbers of the properties in the animation files.
enum PaneTransformProperty : unsigned
{
    TranslationX,
    TranslationY,
    TranslationZ,
    RotationX,
    RotationY,
    RotationZ,
    ScaleX,
    ScaleY,
    Width,
    Height,
};

enum TextureTransformProperty : unsigned
{
    ShiftHorizontally,
    ShiftVertically,
    Rotation,
    ScaleHorizontally,
    ScaleVertically,
};

enum ColorChannel : unsigned
{
    Red,
    Green,
    Blue,
    Alpha,
};

enum ColorRegisterNumber : unsigned
{
    FirstRegister,
    SecondRegister,
};

constexpr unsigned ColorChannelCount = 4;
constexpr unsigned PaneAlphaElement = 16;
constexpr unsigned TopCornerCount = 2;

constexpr unsigned FirstRegisterElement = 4;
constexpr unsigned FirstConstantColorElement = 16;
constexpr unsigned LastMaterialColorElement = 31;

constexpr int LowestByte = 0;
constexpr int HighestByte = 255;

template <typename Color, typename Value>
void SetChannel(Color& color, unsigned channel, Value value)
{
    switch (channel)
    {
    case Red:
        color.red = value;
        return;
    case Green:
        color.green = value;
        return;
    case Blue:
        color.blue = value;
        return;
    default:
        color.alpha = value;
        return;
    }
}

std::uint8_t ClampToByte(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, LowestByte, HighestByte));
}

Formats::SignedColor& RegisterOf(Formats::Material& material, unsigned registerIndex)
{
    switch (registerIndex)
    {
    case FirstRegister:
        return material.foreColor;
    case SecondRegister:
        return material.backColor;
    default:
        return material.colorRegister3;
    }
}

}

void SetPaneTransformValue(Formats::Pane& pane, unsigned property, float value)
{
    switch (property)
    {
    case TranslationX:
        pane.translation.x = value;
        return;
    case TranslationY:
        pane.translation.y = value;
        return;
    case TranslationZ:
        pane.translation.z = value;
        return;
    case RotationX:
        pane.rotation.x = value;
        return;
    case RotationY:
        pane.rotation.y = value;
        return;
    case RotationZ:
        pane.rotation.z = value;
        return;
    case ScaleX:
        pane.scale.x = value;
        return;
    case ScaleY:
        pane.scale.y = value;
        return;
    case Width:
        pane.width = value;
        return;
    case Height:
        pane.height = value;
        return;
    default:
        throw LayoutError(std::format("The pane {} cannot be animated with the transform property {}", pane.name, property));
    }
}

void SetPaneColorElement(Formats::Pane& pane, unsigned element, std::uint8_t value)
{
    if (element > PaneAlphaElement)
        throw LayoutError(std::format("The pane {} cannot be animated with the color element {}", pane.name, element));

    if (element == PaneAlphaElement)
    {
        pane.alpha = value;
        return;
    }

    unsigned corner = element / ColorChannelCount;
    unsigned channel = element % ColorChannelCount;

    if (pane.kind == Formats::PaneKind::Picture && pane.picture)
        SetChannel(pane.picture->vertexColors[corner], channel, value);

    if (pane.kind == Formats::PaneKind::Window && pane.window)
        SetChannel(pane.window->content.vertexColors[corner], channel, value);

    if (pane.kind == Formats::PaneKind::TextBox && pane.textBox)
    {
        Formats::Rgba8Color& color = corner < TopCornerCount ? pane.textBox->topColor : pane.textBox->bottomColor;
        SetChannel(color, channel, value);
    }
}

void SetMaterialColorElement(Formats::Material& material, unsigned element, int value)
{
    if (element > LastMaterialColorElement)
        throw LayoutError(std::format("The material {} cannot be animated with the color element {}", material.name, element));

    if (element >= FirstConstantColorElement)
    {
        unsigned constantElement = element - FirstConstantColorElement;
        Formats::Rgba8Color& constantColor = material.constantColors[constantElement / ColorChannelCount];
        SetChannel(constantColor, constantElement % ColorChannelCount, ClampToByte(value));
        return;
    }

    if (element >= FirstRegisterElement)
    {
        unsigned registerElement = element - FirstRegisterElement;
        Formats::SignedColor& registerColor = RegisterOf(material, registerElement / ColorChannelCount);
        SetChannel(registerColor, registerElement % ColorChannelCount, static_cast<std::int16_t>(value));
        return;
    }

    if (material.materialColor)
        SetChannel(*material.materialColor, element, ClampToByte(value));
}

namespace
{

void SetTransformValue(const Formats::Material& material, Formats::TextureTransform& transform, unsigned property, float value)
{
    switch (property)
    {
    case ShiftHorizontally:
        transform.translation.x = value;
        return;
    case ShiftVertically:
        transform.translation.y = value;
        return;
    case Rotation:
        transform.rotation = value;
        return;
    case ScaleHorizontally:
        transform.scale.x = value;
        return;
    case ScaleVertically:
        transform.scale.y = value;
        return;
    default:
        throw LayoutError(std::format("The material {} cannot be animated with the texture transform property {}", material.name, property));
    }
}

}

void SetTextureTransformValue(Formats::Material& material, std::size_t transformIndex, unsigned property, float value)
{
    if (transformIndex < material.textureTransforms.size())
        SetTransformValue(material, material.textureTransforms[transformIndex], property, value);
}

void SetIndirectTransformValue(Formats::Material& material, std::size_t transformIndex, unsigned property, float value)
{
    if (transformIndex < material.indirectTransforms.size())
        SetTransformValue(material, material.indirectTransforms[transformIndex], property, value);
}

}
