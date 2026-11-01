#pragma once

#include "Graphics/GraphicsDevice.h"
#include "Layout/LiveLayout.h"
#include "Mii/MiiRecord.h"
#include "Mii/MiiResources.h"

#include "Formats/Bytes.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>

namespace Channels
{

// The faces of Miis as the layouts of the Wii Speak Channel show them, drawn by the face library of the Mii part the first time a
// screen asks for a Mii and kept until the channel ends.
class MiiFaces
{
public:
    // The device must outlive the faces.
    MiiFaces(Graphics::GraphicsDevice& device, Formats::ByteView faceParts);
    MiiFaces(const MiiFaces&) = delete;
    MiiFaces& operator=(const MiiFaces&) = delete;
    ~MiiFaces();

    // The face of the Mii, and the name a layout knows it by.
    Layout::LoadedTexture FaceOf(const Mii::MiiRecord& mii);
    static std::string TextureNameOf(const Mii::MiiRecord& mii);

private:
    using CreateId = std::pair<std::uint32_t, std::uint32_t>;

    Graphics::GraphicsDevice& device;
    Mii::MiiResources faceParts;
    std::map<CreateId, Layout::LoadedTexture> faces;
};

// Two records are the same Mii when their create ids are the same.
bool IsSameMii(const Mii::MiiRecord& first, const Mii::MiiRecord& second);

}
