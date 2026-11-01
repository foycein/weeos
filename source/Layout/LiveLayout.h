#pragma once

#include "Layout/LiveMaterial.h"
#include "Layout/PictureSource.h"

#include "Formats/Layout.h"
#include "Graphics/GraphicsDevice.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace Layout
{

// A picture that has been sent to the graphics device, and its size in pixels.
struct LoadedTexture
{
    Graphics::TextureHandle handle;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

// The pane and all the panes below it, in drawing order.
std::vector<Formats::Pane*> PaneWithDescendants(Formats::Pane& pane);

// A layout that lives: the panes and materials of the file, which the animations change while time goes by,
// and the pictures that it has sent to the graphics device.
//
// The pictures are sent when a quad needs them for the first time. They stay on the device until the live layout is destroyed.
// The graphics device and the picture source must stay alive as long as the live layout does.
class LiveLayout
{
public:
    // Throws a LayoutError when the layout uses something that is not supported.
    LiveLayout(Graphics::GraphicsDevice& graphicsDevice, const PictureSource& pictureSource, Formats::Layout fileLayout);
    ~LiveLayout();

    LiveLayout(const LiveLayout&) = delete;
    LiveLayout& operator=(const LiveLayout&) = delete;

    // The size of the layout in layout units. The origin lies in the middle, x grows to the right and y grows upward.
    float Width() const;
    float Height() const;

    Formats::Pane& RootPane();
    const Formats::Pane& RootPane() const;

    // The first pane with this name, looking through the tree from the root pane in drawing order. Nothing if there is none.
    Formats::Pane* FindPane(std::string_view paneName);

    // Every pane of the tree in drawing order.
    std::vector<Formats::Pane*> AllPanes();

    // The panes that the group lists. Throws a LayoutError when the layout has no such group or lists a pane that does not exist.
    std::vector<Formats::Pane*> PanesOfGroup(std::string_view groupName);

    // The groups of the file, as it lists them. Nothing when the file has none.
    const std::optional<Formats::Group>& RootGroup() const;

    // Throws a LayoutError when the layout has no material with this number.
    LiveMaterial& MaterialAt(std::size_t materialIndex);
    std::size_t MaterialCount() const;

    // The name of a font file, for example "RevoIpl_RodinNTLGPro_DB_32_I4.brfnt". Throws a LayoutError when there is no such font.
    const std::string& FontNameAt(std::size_t fontIndex) const;

    // The picture of the texture file on the graphics device. The device keeps one copy for every combination of
    // texture file and wrap modes, because the device fixes the wrap modes when it receives a picture.
    // Throws a LayoutError when the picture source does not have the file.
    LoadedTexture ObtainTexture(const std::string& textureFileName, Graphics::WrapMode horizontalWrap, Graphics::WrapMode verticalWrap);

    // A texture that the program made (the face of a Mii, for example) is shown by the materials that name this texture file, whatever
    // their wrap modes. The layout does not destroy it; the caller keeps it alive as long as the layout.
    void UseMadeTexture(const std::string& textureFileName, LoadedTexture texture);

private:
    using TextureKey = std::tuple<std::string, Graphics::WrapMode, Graphics::WrapMode>;

    Graphics::GraphicsDevice& device;
    const PictureSource& pictures;

    float width;
    float height;
    std::vector<std::string> fontNames;
    std::vector<LiveMaterial> materials;
    Formats::Pane rootPane;
    std::optional<Formats::Group> rootGroup;
    std::map<TextureKey, LoadedTexture> loadedTextures;
    std::map<std::string, LoadedTexture> madeTextures;
};

}
