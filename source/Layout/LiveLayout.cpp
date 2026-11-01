#include "Layout/LiveLayout.h"

#include "Layout/LayoutError.h"

#include <format>

namespace Layout
{

namespace
{

Formats::Pane* FindPaneInTree(Formats::Pane& pane, std::string_view paneName)
{
    if (pane.name == paneName)
        return &pane;

    for (Formats::Pane& child : pane.children)
    {
        Formats::Pane* found = FindPaneInTree(child, paneName);
        if (found)
            return found;
    }
    return nullptr;
}

void CollectPanes(Formats::Pane& pane, std::vector<Formats::Pane*>& panes)
{
    panes.push_back(&pane);
    for (Formats::Pane& child : pane.children)
        CollectPanes(child, panes);
}

const Formats::Group* FindGroupInTree(const Formats::Group& group, std::string_view groupName)
{
    if (group.name == groupName)
        return &group;

    for (const Formats::Group& child : group.children)
    {
        const Formats::Group* found = FindGroupInTree(child, groupName);
        if (found)
            return found;
    }
    return nullptr;
}

}

std::vector<Formats::Pane*> PaneWithDescendants(Formats::Pane& pane)
{
    std::vector<Formats::Pane*> panes;
    CollectPanes(pane, panes);
    return panes;
}

LiveLayout::LiveLayout(Graphics::GraphicsDevice& graphicsDevice, const PictureSource& pictureSource, Formats::Layout fileLayout)
    : device(graphicsDevice)
    , pictures(pictureSource)
    , width(fileLayout.width)
    , height(fileLayout.height)
    , fontNames(std::move(fileLayout.fontNames))
    , rootPane(std::move(fileLayout.rootPane))
    , rootGroup(std::move(fileLayout.rootGroup))
{
    if (!fileLayout.isCentered)
        throw LayoutError("Only layouts that are drawn around their middle are supported");

    for (Formats::Material& material : fileLayout.materials)
        materials.push_back(MakeLiveMaterial(std::move(material), fileLayout.textureNames));
}

LiveLayout::~LiveLayout()
{
    for (const auto& [key, texture] : loadedTextures)
        device.DestroyTexture(texture.handle);
}

float LiveLayout::Width() const
{
    return width;
}

float LiveLayout::Height() const
{
    return height;
}

Formats::Pane& LiveLayout::RootPane()
{
    return rootPane;
}

const Formats::Pane& LiveLayout::RootPane() const
{
    return rootPane;
}

Formats::Pane* LiveLayout::FindPane(std::string_view paneName)
{
    return FindPaneInTree(rootPane, paneName);
}

std::vector<Formats::Pane*> LiveLayout::AllPanes()
{
    return PaneWithDescendants(rootPane);
}

const std::optional<Formats::Group>& LiveLayout::RootGroup() const
{
    return rootGroup;
}

std::vector<Formats::Pane*> LiveLayout::PanesOfGroup(std::string_view groupName)
{
    const Formats::Group* group = rootGroup ? FindGroupInTree(*rootGroup, groupName) : nullptr;
    if (!group)
        throw LayoutError(std::format("The layout has no group {}", groupName));

    std::vector<Formats::Pane*> panes;
    for (const std::string& paneName : group->paneNames)
    {
        Formats::Pane* pane = FindPane(paneName);
        if (!pane)
            throw LayoutError(std::format("The group {} lists the pane {}, which the layout does not have", groupName, paneName));

        panes.push_back(pane);
    }
    return panes;
}

LiveMaterial& LiveLayout::MaterialAt(std::size_t materialIndex)
{
    if (materialIndex >= materials.size())
        throw LayoutError(std::format("The layout has no material {}, it has {}", materialIndex, materials.size()));

    return materials[materialIndex];
}

std::size_t LiveLayout::MaterialCount() const
{
    return materials.size();
}

const std::string& LiveLayout::FontNameAt(std::size_t fontIndex) const
{
    if (fontIndex >= fontNames.size())
        throw LayoutError(std::format("The layout has no font {}, it has {}", fontIndex, fontNames.size()));

    return fontNames[fontIndex];
}

LoadedTexture LiveLayout::ObtainTexture(const std::string& textureFileName, Graphics::WrapMode horizontalWrap, Graphics::WrapMode verticalWrap)
{
    auto provided = providedTextures.find(textureFileName);
    if (provided != providedTextures.end())
        return provided->second;

    TextureKey key(textureFileName, horizontalWrap, verticalWrap);
    auto alreadyLoaded = loadedTextures.find(key);
    if (alreadyLoaded != loadedTextures.end())
        return alreadyLoaded->second;

    std::optional<Formats::RgbaImage> picture = pictures.FindPicture(textureFileName);
    if (!picture)
        throw LayoutError(std::format("The picture source has no texture file {}", textureFileName));

    Graphics::TextureDescription description;
    description.width = picture->width;
    description.height = picture->height;
    description.pixels = picture->pixels;
    description.horizontalWrap = horizontalWrap;
    description.verticalWrap = verticalWrap;
    description.filter = Graphics::FilterMode::Linear;

    LoadedTexture texture;
    texture.handle = device.CreateTexture(description);
    texture.width = picture->width;
    texture.height = picture->height;

    loadedTextures.emplace(key, texture);
    return texture;
}

void LiveLayout::ProvideTexture(const std::string& textureFileName, LoadedTexture texture)
{
    providedTextures[textureFileName] = texture;
}

}
