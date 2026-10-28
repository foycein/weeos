#pragma once

#include <string_view>

// The names the NAND folder of Ios::NandStorage (source/Ios/FileSystem of the iosfiles branch) uses for its index.
// The index lies in the NAND folder, so no NAND entry may have one of these names. If that part changes, this file changes with it.
constexpr std::string_view IndexFileName = ".attributes";
constexpr std::string_view IndexTemporaryFileName = ".attributes.new";
