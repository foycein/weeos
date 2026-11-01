#pragma once

#include "Formats/Bytes.h"
#include "Ios/FileSystem/FileSystemService.h"
#include "Ios/Identity/ConsoleIdentityService.h"
#include "Ios/SdCard/SdCardService.h"
#include "Ios/Title/TitleService.h"

namespace SystemMenu
{

// What the work of Data Management uses of IOS: the titles, the NAND, the SD card and the identity of the console.
// The title service must have entered the System Menu, because the work reads and writes save data as the title it belongs to.
struct DataManagementServices
{
    Ios::TitleService& titles;
    Ios::FileSystemService& fileSystem;
    Ios::SdCardService& sdCard;
    const Ios::ConsoleIdentityService& identity;

    // The certificates that vouch for the console that signed save data: the System Menu keeps four in its program
    // (CA00000001, MS00000002, CA00000002, MS00000003; see MenuCertificates.h), one after the other.
    Formats::Bytes authorityCertificates;
};

}
