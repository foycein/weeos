#version 450
#extension GL_GOOGLE_include_directive : require

#include "DrawData.glsl"

// Gives the corners of a mesh to the same fragment program as the quads: the raster color is the light at the corner,
// every texture reads the corner's texture coordinates through its own texture matrix.

layout(location = 0) out vec4 rasterColor;
layout(location = 1) out vec2 firstTextureCoordinates;
layout(location = 2) out vec2 secondTextureCoordinates;
layout(location = 3) out vec2 thirdTextureCoordinates;
layout(location = 4) flat out uint drawIndex;

const vec4 OpaqueWhite = vec4(1.0);

vec3 TransformByRows(vec4 rows[3], vec4 point)
{
    return vec3(dot(rows[0], point), dot(rows[1], point), dot(rows[2], point));
}

vec2 TransformedTextureCoordinates(uint drawNumber, uint textureNumber, vec2 original)
{
    uint first = textureNumber * NumbersPerTextureMatrix;

    float s = drawRecords[drawNumber].textureMatrices[first + 0u] * original.x
        + drawRecords[drawNumber].textureMatrices[first + 1u] * original.y
        + drawRecords[drawNumber].textureMatrices[first + 2u];

    float t = drawRecords[drawNumber].textureMatrices[first + 3u] * original.x
        + drawRecords[drawNumber].textureMatrices[first + 4u] * original.y
        + drawRecords[drawNumber].textureMatrices[first + 5u];

    return vec2(s, t);
}

// The light of the Wii's graphics chip at one corner: ambient plus the light times the cosine toward it, limited to 1.
// The normal is not made unit length again after the normal matrix, like on the Wii.
vec4 LightAtCorner(MeshRecord mesh, vec3 viewPosition, vec3 viewNormal)
{
    vec3 towardLight = normalize(mesh.lightPosition.xyz - viewPosition);
    float diffuse = max(dot(viewNormal, towardLight), 0.0);
    vec3 color = mesh.ambientColor.rgb + diffuse * mesh.lightColor.rgb;
    return vec4(clamp(color, 0.0, 1.0), 1.0);
}

void main()
{
    // The draw number is the first instance of the draw call, and the corner number counts on from the mesh's first corner.
    uint drawNumber = uint(gl_InstanceIndex);
    MeshRecord mesh = meshRecords[drawRecords[drawNumber].meshNumber];
    MeshCorner corner = meshCorners[gl_VertexIndex];

    vec3 viewPosition = TransformByRows(mesh.modelView, vec4(corner.positionAndS.xyz, 1.0));
    vec3 viewNormal = TransformByRows(mesh.normalMatrix, vec4(corner.normalAndT.xyz, 0.0));

    vec4 viewPoint = vec4(viewPosition, 1.0);
    vec4 picturePosition = vec4(dot(mesh.projection[0], viewPoint), dot(mesh.projection[1], viewPoint),
        dot(mesh.projection[2], viewPoint), dot(mesh.projection[3], viewPoint));

    // The view has y upward and the picture has y downward.
    gl_Position = vec4(picturePosition.x, -picturePosition.y, picturePosition.z, picturePosition.w);

    rasterColor = OpaqueWhite;
    if (mesh.hasLight != 0u)
        rasterColor = LightAtCorner(mesh, viewPosition, viewNormal);

    vec2 coordinates = vec2(corner.positionAndS.w, corner.normalAndT.w);
    if (mesh.textureCoordinatesFromNormal != 0u)
        coordinates = normalize(viewNormal).xy;

    firstTextureCoordinates = TransformedTextureCoordinates(drawNumber, 0u, coordinates);
    secondTextureCoordinates = TransformedTextureCoordinates(drawNumber, 1u, coordinates);
    thirdTextureCoordinates = TransformedTextureCoordinates(drawNumber, 2u, coordinates);
    drawIndex = drawNumber;
}
