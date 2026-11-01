#pragma once

#include "SystemMenu/MessageBoard/BoardPosting.h"

#include "Layout/LiveLayout.h"
#include "Mii/MiiDatabase.h"
#include "Mii/MiiResources.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SystemMenu
{

class MenuFiles;
class NandFiles;

// The Mii database of the console, which the Mii Channel keeps.
constexpr std::string_view MiiDatabasePath = "/shared2/menu/FaceLib/RFL_DB.dat";

// The faces of the Miis of the Message Board: 76 x 76 icons that the Mii part draws, as the board asks the face library for them
// (0x8133EA94, 0x8133EB24). A face is drawn once and kept until the portraits are destroyed.
class MiiPortraits
{
public:
    // The device and the files must outlive the portraits.
    MiiPortraits(Graphics::GraphicsDevice& graphicsDevice, const MenuFiles& menuFiles);
    ~MiiPortraits();

    MiiPortraits(const MiiPortraits&) = delete;
    MiiPortraits& operator=(const MiiPortraits&) = delete;

    // The face of the Mii, drawn the first time it is asked for. Nothing when the data is not a Mii that may be shown (0x8133ECC0).
    // It must not be asked for while the picture of the screen is drawn.
    std::optional<Layout::LoadedTexture> FaceOf(const MiiData& mii);

    // The name of the Mii, or nothing when the data is not a Mii that may be shown.
    static std::optional<std::u16string> NameOf(const MiiData& mii);

    // The Miis of the database of the console that may be shown, in the order of their slots, as the Mii chooser lists them.
    // None when the console has no database or it cannot be read.
    static std::vector<MiiData> MiisOfTheConsole(NandFiles& nand);

private:
    void LoadTheResources();

    Graphics::GraphicsDevice& device;
    const MenuFiles& files;
    std::unique_ptr<Mii::MiiResources> resources;
    std::map<MiiData, Layout::LoadedTexture> faces;
};

}
