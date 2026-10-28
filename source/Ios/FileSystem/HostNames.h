#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Ios
{

// The NAND allows names that Windows does not, for example "a:b", "name.", "con", or two names that differ only in case.
// A NAND entry is stored on the PC under its NAND name whenever Windows can store that name. Otherwise it gets another name,
// its host name, and the index remembers it.

// The file that keeps the index. It lies in the NAND folder, so no NAND entry may use its name.
constexpr std::string_view IndexFileName = ".attributes";
constexpr std::string_view IndexTemporaryFileName = ".attributes.new";

// True when Windows can store a file or folder with this name, and the name is not one of the index files.
bool IsValidHostName(std::string_view name);

// True when Windows takes the name for a device: "con", "nul", "com1", also with an extension like "con.txt".
bool IsReservedDeviceName(std::string_view name);

// Picks the name for a new entry. It is the NAND name when that is a valid host name and none of the siblings
// has the same name, ignoring the difference between capital and small letters. Otherwise it is a changed name:
// every character Windows cannot store becomes ^ and its code in hexadecimal, for example "a:b" becomes "a^3Ab",
// and a number is added at the end when the changed name is taken too.
std::string ChooseHostName(std::string_view nandName, const std::vector<std::string>& siblingHostNames);

}
