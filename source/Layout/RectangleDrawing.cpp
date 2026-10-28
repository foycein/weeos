#include "Layout/RectangleDrawing.h"

#include "Layout/LayoutError.h"
#include "Layout/MaterialBuilder.h"
#include "Layout/QuadCorners.h"

#include <format>

namespace Layout
{

namespace
{

void AssignTextureCoordinates(Graphics::Quad& quad, const Formats::Material& material, const RectangleAppearance& appearance,
    const Formats::Pane& pane)
{
    for (std::size_t textureMapIndex = 0; textureMapIndex < material.textures.size(); ++textureMapIndex)
    {
        std::size_t setIndex = TextureCoordinateSetOf(material, textureMapIndex);
        if (setIndex >= appearance.textureCoordinateSets.size())
        {
            throw LayoutError(std::format("The material {} reads the set {} of texture coordinates, but the pane {} has only {}",
                material.name, setIndex, pane.name, appearance.textureCoordinateSets.size()));
        }

        quad.textureCoordinates[textureMapIndex] = InQuadOrder(appearance.textureCoordinateSets[setIndex].corners);
    }
}

}

void DrawRectangle(Graphics::GraphicsDevice& device, LiveLayout& layout, const PanePlacement& placement,
    const PaneRectangle& rectangle, const RectangleAppearance& appearance)
{
    LiveMaterial& liveMaterial = layout.MaterialAt(appearance.materialIndex);
    DrawableMaterial drawable = BuildDrawableMaterial(layout, liveMaterial, placement.alpha, appearance.vertexColors);

    Graphics::Quad quad{};
    quad.positions = CornersOf(rectangle, placement.matrix);
    quad.colors = drawable.cornerColors;
    AssignTextureCoordinates(quad, liveMaterial.description, appearance, *placement.pane);

    device.DrawQuad(quad, drawable.material);
}

}
