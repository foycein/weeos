#include "SystemMenu/BackgroundServices/SharedFlagsFile.h"

#include "Formats/ByteReader.h"
#include "Formats/ByteWriter.h"
#include "Formats/FormatError.h"
#include "Formats/SystemConfiguration.h"

namespace SystemMenu
{

namespace
{

constexpr std::string_view SharedFlagsPath = "/shared2/sys/flags.dat";
constexpr std::string_view SystemConfigurationPath = "/shared2/sys/SYSCONF";

constexpr std::string_view UserAgreementSetting = "IPL.EULA";
constexpr std::size_t FlagsWordOffset = 4;
constexpr std::uint32_t UserAgreementBit = 0x1;

// A missing or damaged SYSCONF, or one without the setting, counts as "not agreed", as the SC library answers.
bool HasUserAgreed(NandFileAccess& files)
{
    Ios::Result<Formats::Bytes> bytes = files.Read(MenuPath(SystemConfigurationPath));
    if (bytes.Failed())
        return false;

    try
    {
        Formats::SystemConfiguration configuration = Formats::ReadSystemConfiguration(bytes.Value());
        return configuration.FindNumber(UserAgreementSetting).value_or(0) != 0;
    }
    catch (const Formats::FormatError&)
    {
        return false;
    }
}

Ios::Result<void> MakeFlagsFileIfNeeded(NandFileAccess& files, const Ios::NandPath& path)
{
    Ios::Result<Ios::EntryAttributes> attributes = files.Attributes(path);
    if (attributes.Succeeded())
        return Ios::Result<void>();

    if (attributes.Error() != Ios::IosError::NotFound)
    {
        Ios::Result<void> deleted = files.Delete(path);
        if (deleted.Failed())
            return deleted;
    }

    Ios::Result<void> created = files.Create(path, OthersOnlyRead);
    if (created.Failed())
        return created;

    Formats::Bytes zeros(SharedFlagsFileSize, 0);
    return files.Write(path, zeros);
}

Formats::Bytes WithUserAgreementBit(Formats::ByteView flags, bool hasAgreed)
{
    Formats::ByteReader reader(flags, "Shared flags");
    reader.MoveTo(FlagsWordOffset);
    std::uint32_t word = reader.ReadUint32();

    if (hasAgreed)
        word = word | UserAgreementBit;
    else
        word = word & ~UserAgreementBit;

    Formats::ByteWriter newFlags;
    newFlags.WriteBytes(flags.first(FlagsWordOffset));
    newFlags.WriteUint32(word);
    newFlags.WriteBytes(flags.subspan(FlagsWordOffset + sizeof(word)));
    return newFlags.GetBytes();
}

}

void UpdateSharedFlagsFile(NandFileAccess& files)
{
    Ios::NandPath path = MenuPath(SharedFlagsPath);
    if (MakeFlagsFileIfNeeded(files, path).Failed())
        return;

    Ios::Result<Formats::Bytes> flags = files.Read(path);
    if (flags.Failed() || flags.Value().size() < SharedFlagsFileSize)
        return;

    Formats::ByteView firstFlags = Formats::ByteView(flags.Value()).first(SharedFlagsFileSize);
    files.Write(path, WithUserAgreementBit(firstFlags, HasUserAgreed(files)));
}

}
