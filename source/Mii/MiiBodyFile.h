#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Mii
{

// Scale, turn (in degrees, about x, then y, then z) and move of a bone.
struct BoneTransform
{
    Formats::Vector3 scale = {1, 1, 1};
    Formats::Vector3 rotationDegrees = {0, 0, 0};
    Formats::Vector3 translation = {0, 0, 0};

    // The bone takes back the scale of its parent before it turns (segment scale compensation).
    bool undoesParentScale = false;
};

struct BodyBone
{
    std::string name;

    // The bone before this one in the list that it hangs on, or -1 for the root.
    int parent = -1;

    // The number the draw lists use for the bone's matrix.
    int matrixId = 0;

    BoneTransform restTransform;
};

// One corner of a triangle of the body: the bone that carries it and its position and normal in that bone's space.
struct BodyCorner
{
    int matrixId;
    std::uint16_t position;
    std::uint16_t normal;
};

struct BodyPart
{
    std::vector<Formats::Vector3> positions;
    std::vector<Formats::Vector3> normals;

    // Three corners per triangle. Seen from outside, the corners go around clockwise.
    std::vector<BodyCorner> triangleCorners;
};

// One key of an animation track: at this frame the value is this, and it changes by the slope per frame.
struct AnimationKey
{
    float frame = 0;
    float value = 0;
    float slope = 0;
};

// One number of a bone over the frames: a fixed value when there are no keys, otherwise a smooth curve through the keys.
struct AnimationTrack
{
    float fixedValue = 0;
    std::vector<AnimationKey> keys;
};

// The x, y and z of a scale, a turn (in degrees) or a move.
using AnimationTracks = std::array<AnimationTrack, 3>;

// What an animation does to one bone. A kind that the animation leaves out keeps the value of the bone's rest pose.
struct BoneAnimation
{
    std::optional<AnimationTracks> scale;
    std::optional<AnimationTracks> rotation;
    std::optional<AnimationTracks> translation;
    bool undoesParentScale = false;
};

// A skeleton animation of the body (CHR0), for example bodyWalk: the bones it moves, by name.
struct BodyAnimation
{
    float frameCount = 0;
    bool isLooping = false;
    std::map<std::string, BoneAnimation> bones;
};

// What the Mii Channel's body model holds that a standing Mii needs (docs/research/Mii.md section 10): the bones, the shirt
// (with hands and neck), the pants (with shoes), the first frame of the standing animation and the lit sphere that shades both.
struct MiiBodyFile
{
    std::vector<BodyBone> bones;
    BodyPart shirt;
    BodyPart pants;

    // The bones' transforms in frame 0 of the animation bodyWait, by bone name.
    std::map<std::string, BoneTransform> standingPose;

    // Every animation of the file by its name: bodyWait, bodyWalk, bodyRun and the others of the Mii Plaza.
    std::map<std::string, BodyAnimation> animations;

    Formats::RgbaImage environmentTexture;
};

struct MiiBodyFiles
{
    MiiBodyFile male;
    MiiBodyFile female;
};

// The value of a track at a frame: between two keys the curve that meets both keys with their slopes (a Hermite curve), before the
// first key the first value and after the last key the last value.
float TrackValueAt(const AnimationTrack& track, float frame);

// The transform of a bone at a frame of an animation. The rest transform fills in what the animation leaves out.
BoneTransform AnimatedTransform(const BoneAnimation& animation, const BoneTransform& restTransform, float frame);

// Reads maleBody.brres or femaleBody.brres. Throws a FormatError when the file does not hold what the body needs.
MiiBodyFile ReadMiiBodyFile(Formats::ByteView brres);

// Reads both bodies out of the Mii Channel's Channel/Character.szs (Yaz0 packed U8 archive).
MiiBodyFiles ReadMiiBodyFiles(Formats::ByteView characterArchive);

}
