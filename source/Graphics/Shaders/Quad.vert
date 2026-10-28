#version 450
#extension GL_GOOGLE_include_directive : require

#include "DrawData.glsl"

// Half of the width and the height of the canvas, to bring canvas units into the range -1 to 1.
layout(push_constant) uniform CanvasSize
{
    vec2 halfCanvasSize;
} canvasSize;

layout(location = 0) out vec4 rasterColor;
layout(location = 1) out vec2 firstTextureCoordinates;
layout(location = 2) out vec2 secondTextureCoordinates;
layout(location = 3) out vec2 thirdTextureCoordinates;
layout(location = 4) flat out uint drawIndex;

// A quad is drawn as two triangles, with these corners. The corners are numbered left top, right top, right bottom, left bottom.
const uint cornerOfVertex[6] = uint[6](0u, 1u, 2u, 0u, 2u, 3u);

vec2 TransformedTextureCoordinates(uint drawNumber, uint textureNumber, uint corner)
{
    vec2 original = drawRecords[drawNumber].textureCoordinates[textureNumber * CornersPerQuad + corner];
    uint first = textureNumber * NumbersPerTextureMatrix;

    float s = drawRecords[drawNumber].textureMatrices[first + 0u] * original.x
        + drawRecords[drawNumber].textureMatrices[first + 1u] * original.y
        + drawRecords[drawNumber].textureMatrices[first + 2u];

    float t = drawRecords[drawNumber].textureMatrices[first + 3u] * original.x
        + drawRecords[drawNumber].textureMatrices[first + 4u] * original.y
        + drawRecords[drawNumber].textureMatrices[first + 5u];

    return vec2(s, t);
}

void main()
{
    // The draw number is the first instance of the draw call.
    uint drawNumber = uint(gl_InstanceIndex);
    uint corner = cornerOfVertex[gl_VertexIndex];

    vec2 position = drawRecords[drawNumber].cornerPositions[corner];

    // The canvas has y upward and the picture has y downward.
    gl_Position = vec4(position.x / canvasSize.halfCanvasSize.x, -position.y / canvasSize.halfCanvasSize.y, 0.0, 1.0);

    rasterColor = unpackUnorm4x8(drawRecords[drawNumber].cornerColors[corner]);
    firstTextureCoordinates = TransformedTextureCoordinates(drawNumber, 0u, corner);
    secondTextureCoordinates = TransformedTextureCoordinates(drawNumber, 1u, corner);
    thirdTextureCoordinates = TransformedTextureCoordinates(drawNumber, 2u, corner);
    drawIndex = drawNumber;
}
