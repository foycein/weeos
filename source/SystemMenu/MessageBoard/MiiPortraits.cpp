#include "SystemMenu/MessageBoard/MiiPortraits.h"

#include "SystemMenu/MenuError.h"
#include "SystemMenu/MenuFiles.h"
#include "SystemMenu/NandFiles.h"

#include "Formats/FormatError.h"
#include "Formats/U8Archive.h"
#include "Mii/MiiIcon.h"
#include "Mii/MiiRecord.h"

namespace SystemMenu
{

namespace
{

// RFL_Res.dat, the shapes and textures of the faces, is the only file of the shared content 0000000b of the System Menu title,
// which has the index 4.
constexpr std::uint16_t MiiResourceContentIndex = 4;
constexpr std::string_view MiiResourceFileName = "RFL_Res.dat";

// The slots of RFL_DB.dat follow its four letters "RNOD". A memo takes the bytes of the slot as they are.
constexpr std::size_t FirstMiiSlotOffset = 4;

std::optional<Mii::MiiRecord> UsableRecordOf(const MiiData& mii)
{
    Mii::MiiRecord record = Mii::ReadMiiRecord(std::span<const Formats::Byte, Mii::MiiRecordSize>(mii));
    if (!Mii::IsUsableMii(record))
        return std::nullopt;
    return record;
}

}

MiiPortraits::MiiPortraits(Graphics::GraphicsDevice& graphicsDevice, const MenuFiles& menuFiles)
    : device(graphicsDevice)
    , files(menuFiles)
{
}

MiiPortraits::~MiiPortraits()
{
    for (const auto& [mii, face] : faces)
        device.DestroyTexture(face.handle);
}

std::optional<Layout::LoadedTexture> MiiPortraits::FaceOf(const MiiData& mii)
{
    auto drawnBefore = faces.find(mii);
    if (drawnBefore != faces.end())
        return drawnBefore->second;

    std::optional<Mii::MiiRecord> record = UsableRecordOf(mii);
    if (!record)
        return std::nullopt;

    if (!resources)
        LoadTheResources();

    Layout::LoadedTexture face;
    try
    {
        face.handle = Mii::MakeMiiIcon(device, *resources, *record, Mii::MenuIconSize, Mii::MenuIconSize);
    }
    catch (const Formats::FormatError&)
    {
        return std::nullopt;
    }
    face.width = Mii::MenuIconSize;
    face.height = Mii::MenuIconSize;
    faces.emplace(mii, face);
    return face;
}

void MiiPortraits::LoadTheResources()
{
    try
    {
        Formats::U8Archive archive = Formats::ReadU8Archive(files.ReadSharedContent(MiiResourceContentIndex));
        std::optional<Formats::ByteView> resourceFile = archive.FindFile(std::string(MiiResourceFileName));
        if (!resourceFile)
            throw MenuError("The content 4 of the System Menu has no RFL_Res.dat");
        resources = std::make_unique<Mii::MiiResources>(Formats::Bytes(resourceFile->begin(), resourceFile->end()));
    }
    catch (const Formats::FormatError& error)
    {
        throw MenuError(std::string("RFL_Res.dat cannot be read: ") + error.what());
    }
}

std::optional<std::u16string> MiiPortraits::NameOf(const MiiData& mii)
{
    std::optional<Mii::MiiRecord> record = UsableRecordOf(mii);
    if (!record)
        return std::nullopt;
    return record->name;
}

std::vector<MiiData> MiiPortraits::MiisOfTheConsole(NandFiles& nand)
{
    std::optional<Formats::Bytes> databaseFile = nand.ReadFile(MiiDatabasePath);
    if (!databaseFile)
        return {};

    std::vector<MiiData> miis;
    try
    {
        Mii::MiiDatabase database = Mii::MiiDatabase::Read(*databaseFile);
        for (std::size_t slot = 0; slot < Mii::MiiSlotCount; ++slot)
        {
            std::optional<Mii::MiiRecord> record = database.FindMii(slot);
            if (!record)
                continue;

            MiiData data{};
            auto slotStart = databaseFile->begin() + FirstMiiSlotOffset + slot * Mii::MiiRecordSize;
            std::copy(slotStart, slotStart + Mii::MiiRecordSize, data.begin());
            miis.push_back(data);
        }
    }
    catch (const Formats::FormatError&)
    {
        return {};
    }
    return miis;
}

}
