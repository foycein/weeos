#include "Layout/AnimationPlayer.h"

#include "Layout/AnimatedValues.h"
#include "Layout/CurveEvaluation.h"
#include "Layout/LayoutError.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace Layout
{

namespace
{

// A color value is rounded to the nearest whole number by adding a half and cutting off the fraction.
constexpr float RoundingOffset = 0.5f;
constexpr float LowestColorByte = 0.0f;
constexpr float HighestColorByte = 255.0f;

// The registers of the materials hold numbers from -1024 to 1023.
constexpr float LowestRegisterValue = -1024.0f;
constexpr float HighestRegisterValue = 1023.0f;

// The only property of a texture pattern animation: which texture of the animation's list a texture map shows.
constexpr unsigned TextureImageProperty = 0;

std::uint8_t ToColorByte(float value)
{
    return static_cast<std::uint8_t>(std::clamp(value + RoundingOffset, LowestColorByte, HighestColorByte));
}

int ToRegisterValue(float value)
{
    return static_cast<int>(std::clamp(value + RoundingOffset, LowestRegisterValue, HighestRegisterValue));
}

// The materials that a pane draws with: the material of a picture or a text box, and the materials of the content
// and of the frames of a window.
std::vector<std::uint16_t> MaterialIndicesOf(const Formats::Pane& pane)
{
    std::vector<std::uint16_t> materialIndices;

    if (pane.picture)
        materialIndices.push_back(pane.picture->materialIndex);

    if (pane.textBox)
        materialIndices.push_back(pane.textBox->materialIndex);

    if (pane.window)
    {
        materialIndices.push_back(pane.window->content.materialIndex);
        for (const Formats::WindowFrame& frame : pane.window->frames)
            materialIndices.push_back(frame.materialIndex);
    }
    return materialIndices;
}

Formats::Pane* FindPaneNamed(const std::vector<Formats::Pane*>& panes, const std::string& paneName)
{
    for (Formats::Pane* pane : panes)
    {
        if (pane->name == paneName)
            return pane;
    }
    return nullptr;
}

LiveMaterial* FindMaterialNamed(LiveLayout& layout, const std::vector<Formats::Pane*>& panes, const std::string& materialName)
{
    for (const Formats::Pane* pane : panes)
    {
        for (std::uint16_t materialIndex : MaterialIndicesOf(*pane))
        {
            LiveMaterial& material = layout.MaterialAt(materialIndex);
            if (material.description.name == materialName)
                return &material;
        }
    }
    return nullptr;
}

void ApplyPaneTag(Formats::Pane& pane, const Formats::AnimationTag& tag, float frame)
{
    switch (tag.kind)
    {
    case Formats::AnimationTagKind::PaneTransform:
        for (const Formats::AnimationTarget& target : tag.targets)
            SetPaneTransformValue(pane, target.target, EvaluateCurve(target, frame));
        return;

    case Formats::AnimationTagKind::VertexColor:
        for (const Formats::AnimationTarget& target : tag.targets)
            SetPaneColorElement(pane, target.target, ToColorByte(EvaluateCurve(target, frame)));
        return;

    case Formats::AnimationTagKind::Visibility:
        for (const Formats::AnimationTarget& target : tag.targets)
            pane.isVisible = EvaluateCurve(target, frame) != 0.0f;
        return;

    case Formats::AnimationTagKind::TextureTransform:
    case Formats::AnimationTagKind::MaterialColor:
    case Formats::AnimationTagKind::TexturePattern:
    case Formats::AnimationTagKind::IndirectTransform:
        return;
    }
}

// The animation lists texture files, and the curve says which of them a texture map of the material shows.
void ShowTextureOfPattern(LiveMaterial& material, const Formats::LayoutAnimation& animation, const Formats::AnimationTarget& target, float frame)
{
    bool isTextureOfMaterial = target.index < material.textureNames.size() && target.target == TextureImageProperty;
    if (!isTextureOfMaterial)
        return;

    std::size_t textureNumber = static_cast<std::size_t>(EvaluateCurve(target, frame));
    if (textureNumber >= animation.textureNames.size())
    {
        throw LayoutError(std::format("The animation shows the texture {} for the material {}, but it lists only {} textures",
            textureNumber, material.description.name, animation.textureNames.size()));
    }

    material.textureNames[target.index] = animation.textureNames[textureNumber];
}

void ApplyMaterialTag(LiveMaterial& material, const Formats::LayoutAnimation& animation, const Formats::AnimationTag& tag, float frame)
{
    switch (tag.kind)
    {
    case Formats::AnimationTagKind::MaterialColor:
        for (const Formats::AnimationTarget& target : tag.targets)
            SetMaterialColorElement(material.description, target.target, ToRegisterValue(EvaluateCurve(target, frame)));
        return;

    case Formats::AnimationTagKind::TextureTransform:
        for (const Formats::AnimationTarget& target : tag.targets)
            SetTextureTransformValue(material.description, target.index, target.target, EvaluateCurve(target, frame));
        return;

    case Formats::AnimationTagKind::TexturePattern:
        for (const Formats::AnimationTarget& target : tag.targets)
            ShowTextureOfPattern(material, animation, target, frame);
        return;

    // The indirect stages are not drawn (docs/audit/WiiSpeak.md), so their matrix has nothing to move.
    case Formats::AnimationTagKind::IndirectTransform:
    case Formats::AnimationTagKind::PaneTransform:
    case Formats::AnimationTagKind::Visibility:
    case Formats::AnimationTagKind::VertexColor:
        return;
    }
}

}

AnimationPlayer::AnimationPlayer(LiveLayout& layoutToAnimate, Formats::LayoutAnimation animationToPlay, PlaybackMode playbackMode,
    const std::vector<Formats::Pane*>& panes)
    : layout(layoutToAnimate)
    , animation(std::move(animationToPlay))
    , mode(playbackMode)
{
    BindObjects(panes);
    ApplyCurrentFrame();
}

void AnimationPlayer::BindObjects(const std::vector<Formats::Pane*>& panes)
{
    std::vector<Formats::Pane*> panesToBind;
    for (Formats::Pane* pane : panes)
    {
        std::vector<Formats::Pane*> paneAndDescendants = PaneWithDescendants(*pane);
        panesToBind.insert(panesToBind.end(), paneAndDescendants.begin(), paneAndDescendants.end());
    }

    for (std::size_t objectIndex = 0; objectIndex < animation.objects.size(); ++objectIndex)
    {
        const Formats::AnimatedObject& object = animation.objects[objectIndex];

        if (object.isMaterial)
        {
            LiveMaterial* material = FindMaterialNamed(layout, panesToBind, object.name);
            if (material)
                materialBindings.push_back({material, objectIndex});
        }
        else
        {
            Formats::Pane* pane = FindPaneNamed(panesToBind, object.name);
            if (pane)
                paneBindings.push_back({pane, objectIndex});
        }
    }
}

void AnimationPlayer::ApplyCurrentFrame()
{
    for (const PaneBinding& binding : paneBindings)
    {
        for (const Formats::AnimationTag& tag : animation.objects[binding.objectIndex].tags)
            ApplyPaneTag(*binding.pane, tag, frame);
    }

    for (const MaterialBinding& binding : materialBindings)
    {
        for (const Formats::AnimationTag& tag : animation.objects[binding.objectIndex].tags)
            ApplyMaterialTag(*binding.material, animation, tag, frame);
    }
}

void AnimationPlayer::Step()
{
    Advance(1.0f);
}

void AnimationPlayer::Advance(float frameCount)
{
    if (isFinished)
        return;

    float animationLength = animation.frameCount;
    frame += frameCount;

    if (frame < animationLength)
    {
        ApplyCurrentFrame();
        return;
    }

    if (mode == PlaybackMode::PlayOnce)
    {
        isFinished = true;
        return;
    }

    frame = animationLength > 0 ? std::fmod(frame, animationLength) : 0.0f;
    ApplyCurrentFrame();
}

float AnimationPlayer::CurrentFrame() const
{
    return frame;
}

bool AnimationPlayer::IsFinished() const
{
    return isFinished;
}

}
