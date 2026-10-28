#include "Ios/Profile/IosFeature.h"

#include <stdexcept>

namespace Ios
{

std::string_view FeatureName(Feature feature)
{
    switch (feature)
    {
    case Feature::KernelBuildDate: return "KernelBuildDate";
    case Feature::KernelMemoryPlan: return "KernelMemoryPlan";
    case Feature::KernelAppliesTmdIpcRights: return "KernelAppliesTmdIpcRights";
    case Feature::CryptoHasHmacDevice: return "CryptoHasHmacDevice";
    case Feature::EsCommandLevel: return "EsCommandLevel";
    case Feature::EsReadsVersionOneTickets: return "EsReadsVersionOneTickets";
    case Feature::EsCreatesWfsFolder: return "EsCreatesWfsFolder";
    case Feature::FileSystemFlavor: return "FileSystemFlavor";
    case Feature::FileSystemChecksNameLengthInLookUp: return "FileSystemChecksNameLengthInLookUp";
    case Feature::FileSystemRecreatesTmpAtEveryStart: return "FileSystemRecreatesTmpAtEveryStart";
    case Feature::DiscDriveHasNoDiscCommands: return "DiscDriveHasNoDiscCommands";
    case Feature::SdSupportsHighCapacityCards: return "SdSupportsHighCapacityCards";
    case Feature::UsbHostGeneration: return "UsbHostGeneration";
    case Feature::KeyboardDriver: return "KeyboardDriver";
    case Feature::NetworkConfiguration: return "NetworkConfiguration";
    }

    throw std::logic_error("FeatureName was asked for a feature that does not exist");
}

}
