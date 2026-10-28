#include "Ios/IosError.h"

#include <algorithm>
#include <iterator>

namespace Ios
{

namespace
{

struct Translation
{
    IosError error;
    int nandLibraryResultCode;
};

// The rows of the NAND library's table in the System Menu that belong to the errors this part can have (docs/audit/Ios.md).
constexpr Translation Translations[] =
{
    {IosError::InvalidArgument, -8},
    {IosError::AccessDenied, -1},
    {IosError::NoFileSystem, -64},
    {IosError::AlreadyExists, -6},
    {IosError::NotFound, -12},
    {IosError::NoFreeInodes, -11},
    {IosError::NoFreeClusters, -9},
    {IosError::TooManyOpenFiles, -10},
    {IosError::PathTooDeep, -16},
    {IosError::FileInUse, -14},
    {IosError::StorageFailure, -5},
    {IosError::NotEmpty, -13},
    {IosError::ShutDown, -128},
};

constexpr int UnexpectedErrorResultCode = -64;

}

int NandLibraryResultCode(IosError error)
{
    const Translation* translation = std::find_if(std::begin(Translations), std::end(Translations),
        [error](const Translation& candidate) { return candidate.error == error; });

    if (translation == std::end(Translations))
        return UnexpectedErrorResultCode;

    return translation->nandLibraryResultCode;
}

}
