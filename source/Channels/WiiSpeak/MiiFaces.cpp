#include "Channels/WiiSpeak/MiiFaces.h"

#include "Mii/MiiIcon.h"

#include <format>

namespace Channels
{

MiiFaces::MiiFaces(Graphics::GraphicsDevice& graphicsDevice, Formats::ByteView facePartsFile)
    : device(graphicsDevice)
    , faceParts(Formats::Bytes(facePartsFile.begin(), facePartsFile.end()))
{
}

MiiFaces::~MiiFaces()
{
    for (const auto& [createId, face] : faces)
        device.DestroyTexture(face.handle);
}

Layout::LoadedTexture MiiFaces::FaceOf(const Mii::MiiRecord& mii)
{
    CreateId createId = {mii.miiId, mii.systemId};
    auto known = faces.find(createId);
    if (known != faces.end())
        return known->second;

    Layout::LoadedTexture face;
    face.handle = Mii::MakeMiiIcon(device, faceParts, mii, Mii::MenuIconSize, Mii::MenuIconSize);
    face.width = Mii::MenuIconSize;
    face.height = Mii::MenuIconSize;
    faces[createId] = face;
    return face;
}

std::string MiiFaces::TextureNameOf(const Mii::MiiRecord& mii)
{
    return std::format("Mii_{:08X}_{:08X}", mii.miiId, mii.systemId);
}

bool IsSameMii(const Mii::MiiRecord& first, const Mii::MiiRecord& second)
{
    return first.miiId == second.miiId && first.systemId == second.systemId;
}

}
