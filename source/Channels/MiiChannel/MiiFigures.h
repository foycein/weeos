#pragma once

#include "Channels/MiiChannel/MiiPlaza.h"
#include "Channels/MiiChannel/PlazaGround.h"

#include "Graphics/GraphicsDevice.h"
#include "Mii/MiiBody.h"
#include "Mii/MiiHead.h"
#include "Mii/MiiResources.h"

#include <cstddef>
#include <map>
#include <memory>
#include <vector>

namespace Channels
{

// A rectangle of the picture in layout units: its middle and half its width and height.
struct PictureRectangle
{
    Formats::Vector2 middle;
    Formats::Vector2 halfSize;
};

// A projection that draws into a rectangle of the picture what the projection would draw into the whole picture.
Graphics::Matrix4 ProjectIntoRectangle(const Graphics::Matrix4& projection, const PictureRectangle& rectangle, Graphics::Canvas canvas);

// The heads of the Miis, made once for every Mii and made again when the Mii changes. Making a head draws its face texture, so the heads
// are made in a step and never while a picture is drawn.
class MiiHeads
{
public:
    MiiHeads(Graphics::GraphicsDevice& graphicsDevice, const Mii::MiiResources& faceParts);

    // Makes the head of every Mii that has none or whose record changed, and forgets the heads of the keys that are not there.
    void Prepare(const std::vector<std::pair<std::size_t, Mii::MiiRecord>>& miis);

    // Nothing for a key that Prepare did not see.
    const Mii::MiiHead* Find(std::size_t key) const;

private:
    struct KeptHead
    {
        std::vector<std::uint8_t> recordBytes;
        std::unique_ptr<Mii::MiiHead> head;
    };

    Graphics::GraphicsDevice& device;
    const Mii::MiiResources& parts;
    std::map<std::size_t, KeptHead> heads;
};

// Draws whole Miis: the shared bodies posed in an animation, with their heads on them.
class MiiFigures
{
public:
    MiiFigures(Graphics::GraphicsDevice& graphicsDevice, const Mii::MiiBodyFiles& bodyFiles, std::uint32_t consoleSystemId);

    // The ground and the Miis of the plaza, as its camera sees them, over the whole picture.
    void DrawPlaza(const MiiPlaza& plaza, const PlazaGround& ground, const MiiHeads& heads) const;

    // One Mii standing upright, seen from the front, filling the rectangle: the Mii of the editor.
    void DrawStandingMii(const Mii::MiiRecord& record, const Mii::MiiHead& head, const PictureRectangle& rectangle, Graphics::Canvas canvas,
        float waitFrame) const;

    // One Mii in any stance, seen as the editor sees it, filling the rectangle.
    void DrawPosedMii(const Mii::MiiRecord& record, const Mii::MiiHead& head, const Mii::BodyStance& stance, const PictureRectangle& rectangle,
        Graphics::Canvas canvas) const;

    // A head alone, seen from the front, filling the rectangle: the faces to choose from in "Choose a look-alike".
    void DrawHead(const Mii::MiiHead& head, const PictureRectangle& rectangle, Graphics::Canvas canvas) const;

    const Mii::MiiBodyFiles& Bodies() const;

private:
    void DrawFigure(const Mii::MiiRecord& record, const Mii::MiiHead& head, const Mii::BodyStance& stance, const Graphics::Matrix4& view,
        const Graphics::Matrix4& projection) const;

    Graphics::GraphicsDevice& device;
    const Mii::MiiBodyFiles& bodies;
    Mii::MiiBodyModels bodyModels;
    std::uint32_t systemId;
};

}
