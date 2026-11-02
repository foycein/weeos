#pragma once

namespace Ios
{

// The commands of the title service that not every IOS has.
// IosProfile::EsCommands says which of them an IOS answers.
constexpr int GetStoredContentsCommand = 0x33;
constexpr int GetStoredTitleMetadataCommand = 0x35;
constexpr int GetSharedContentsCommand = 0x37;
constexpr int DeleteTitleContentCommand = 0x3E;
constexpr int CheckHasKoreanKeyCommand = 0x45;

}
