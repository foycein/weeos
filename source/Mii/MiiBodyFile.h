#pragma once

#include "Formats/Bytes.h"
#include "Formats/RgbaImage.h"
#include "Formats/Vectors.h"

#include <cstdint>
#include <map>
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

// What the Mii Channel's body model holds that a standing Mii needs (docs/research/Mii.md section 10): the bones, the shirt
// (with hands and neck), the pants (with shoes), the first frame of the standing animation and the lit sphere that shades both.
struct MiiBodyFile
{
    std::vector<BodyBone> bones;
    BodyPart shirt;
    BodyPart pants;

    // The bones' transforms in frame 0 of the animation bodyWait, by bone name.
    std::map<std::string, BoneTransform> standingPose;

    Formats::RgbaImage environmentTexture;
};

struct MiiBodyFiles
{
    MiiBodyFile male;
    MiiBodyFile female;
};

// Reads maleBody.brres or femaleBody.brres. Throws a FormatError when the file does not hold what the body needs.
MiiBodyFile ReadMiiBodyFile(Formats::ByteView brres);

// Reads both bodies out of the Mii Channel's Channel/Character.szs (Yaz0 packed U8 archive).
MiiBodyFiles ReadMiiBodyFiles(Formats::ByteView characterArchive);

}
