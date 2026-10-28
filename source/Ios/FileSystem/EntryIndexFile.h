#pragma once

#include "Ios/FileSystem/EntryRecord.h"
#include "Ios/FileSystem/NandPath.h"
#include "Ios/Result.h"

#include <map>
#include <string>
#include <string_view>

namespace Ios
{

// Every entry of the NAND with its record, ordered by path.
using EntryMap = std::map<NandPath, EntryRecord>;

// The index is the file that keeps what a file or folder on the PC cannot keep: the owner, the group, the permissions
// and the flags of every NAND entry. It is plain text with one line per entry, so a person can open it and read it:
//
//     /title/00010001	directory	0	0	rw rw r-	0	12
//
// The columns are separated by tabs: the path, the kind, the owner id, the group id, the permissions of the owner, the group
// and everybody else, the flags, the link number, and last the name on the PC when it differs from the NAND name.
std::string WriteEntryIndex(const EntryMap& entries);

// Reads the text of an index. Gives back NoFileSystem when the text is not a complete and consistent index:
// the root must be there, and every other entry needs a parent directory that is there too.
// The lengths of the files are not in the index. They are 0 here.
Result<EntryMap> ReadEntryIndex(std::string_view text);

}
