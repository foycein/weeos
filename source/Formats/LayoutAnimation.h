#pragma once

#include "Formats/Bytes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Formats
{

// The kinds of animation in a layout animation file. The tag of four letters in the file is written in brackets.
enum class AnimationTagKind
{
    PaneTransform,
    TextureTransform,
    Visibility,
    VertexColor,
    MaterialColor,
    TexturePattern,

    // (RLIM) The matrix of an indirect texture: it moves the water of the Wii Speak Channel's chat room.
    IndirectTransform,
};

// A step key keeps its value until the next key. A hermite key goes smoothly to the next one, using its slope.
enum class KeyFrameKind : std::uint8_t
{
    Step = 1,
    Hermite = 2,
};

struct KeyFrame
{
    float frame;

    // A step key stores a whole number, which is given here as a float.
    float value;

    // Zero for a step key.
    float slope;
};

// One property that is animated, for example the width of a pane.
struct AnimationTarget
{
    // Which of the textures of a material is animated. Zero for the other kinds of animation.
    std::uint8_t index;

    // The number of the property. The numbers depend on the kind of the tag. For a pane transform they are:
    // 0 to 2 translation, 3 to 5 rotation, 6 and 7 scale, 8 width, 9 height. For a vertex color: 0 to 15 the
    // four colors of the corners (red, green, blue, alpha each) and 16 the alpha of the pane.
    std::uint8_t target;

    KeyFrameKind keyFrameKind;
    std::vector<KeyFrame> keyFrames;
};

struct AnimationTag
{
    AnimationTagKind kind;
    std::vector<AnimationTarget> targets;
};

// A pane or a material that is animated.
struct AnimatedObject
{
    std::string name;
    bool isMaterial;
    std::vector<AnimationTag> tags;
};

// A layout animation (BRLAN file) changes the panes and materials of a layout over a number of frames.
struct LayoutAnimation
{
    std::uint16_t frameCount;
    bool isLooping;

    // The textures that texture pattern animations switch between.
    std::vector<std::string> textureNames;

    std::vector<AnimatedObject> objects;
};

LayoutAnimation ReadLayoutAnimation(ByteView bytes);

}
