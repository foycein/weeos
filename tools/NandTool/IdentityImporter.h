#pragma once

#include <filesystem>

// Copies the identity of the console in a dump into a folder: keys.txt, device.cert and seeprom.bin. These are the three files that the
// identity folder of the IOS part holds (Ios::IosFolders::identityFolder). Nothing else of the dump is needed there: the one-time
// programmable memory and the key blocks hold the same keys again, and boot0 and boot1 are not rebuilt.
// The files are read with our readers first, and the device certificate must carry the console id of the keys, as the IOS part checks.
// Throws a runtime_error when a file is missing or wrong or when the target folder is not empty. Nothing in it is ever deleted.
void WriteIdentity(const std::filesystem::path& dumpFolder, const std::filesystem::path& targetFolder);
