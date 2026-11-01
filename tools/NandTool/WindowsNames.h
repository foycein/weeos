#pragma once

#include <string>
#include <string_view>

// The NAND allows names that Windows cannot keep as a file or folder, like "a:b", "name." and "con".
// The NAND folder of the IOS part stores such an entry under another name. This import does not: it refuses them.

// True when Windows can keep a file or folder with exactly this name. The two files the NAND folder uses for its index
// (Ios::IndexFileName) are not allowed either.
bool CanBeStoredOnWindows(std::string_view name);

// The name with all capital letters made small, which is how Windows compares names.
std::string LowerCaseName(std::string_view name);
