#include "Ios/Profile/IosVersionList.h"

namespace Ios
{

namespace
{

// Every IOS title of the reference console, written by hand from docs/research/IosVersions.md (sections 3 and 4.2).
// The columns are: the number, the title version, stub or working, stock or patched, and the code group.
// tests/IosProfileDumpTest.cpp reads all of it again from the dump.
std::vector<IosVersion> MakeReferenceVersions()
{
    return
    {
        {3,   PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   3},
        {4,   PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   3},
        {9,   1290,                    IosKind::Working, IosOrigin::Stock,   9},
        {10,  768,                     IosKind::Stub,    IosOrigin::Stock,   3},
        {11,  256,                     IosKind::Stub,    IosOrigin::Stock,   3},
        {12,  782,                     IosKind::Working, IosOrigin::Stock,   12},
        {13,  1288,                    IosKind::Working, IosOrigin::Stock,   13},
        {14,  1288,                    IosKind::Working, IosOrigin::Stock,   14},
        {15,  1288,                    IosKind::Working, IosOrigin::Stock,   15},
        {16,  512,                     IosKind::Stub,    IosOrigin::Stock,   16},
        {17,  1288,                    IosKind::Working, IosOrigin::Stock,   17},
        {20,  256,                     IosKind::Stub,    IosOrigin::Stock,   3},
        {21,  1295,                    IosKind::Working, IosOrigin::Stock,   21},
        {22,  1550,                    IosKind::Working, IosOrigin::Stock,   22},
        {28,  2063,                    IosKind::Working, IosOrigin::Stock,   28},
        {30,  2816,                    IosKind::Stub,    IosOrigin::Stock,   3},

        // The console runs a patched IOS31 (docs/research/IosVersions.md, 5.9). The profile is that of the stock IOS31.
        {31,  3608,                    IosKind::Working, IosOrigin::Patched, 31},

        {33,  3864,                    IosKind::Working, IosOrigin::Stock,   33},
        {34,  3864,                    IosKind::Working, IosOrigin::Stock,   33},
        {35,  3864,                    IosKind::Working, IosOrigin::Stock,   33},
        {36,  3864,                    IosKind::Working, IosOrigin::Stock,   33},
        {37,  5919,                    IosKind::Working, IosOrigin::Stock,   37},
        {38,  4380,                    IosKind::Working, IosOrigin::Stock,   38},
        {40,  3072,                    IosKind::Stub,    IosOrigin::Stock,   16},
        {41,  3863,                    IosKind::Working, IosOrigin::Stock,   41},
        {43,  3863,                    IosKind::Working, IosOrigin::Stock,   41},
        {45,  3863,                    IosKind::Working, IosOrigin::Stock,   41},
        {46,  3863,                    IosKind::Working, IosOrigin::Stock,   41},
        {48,  4380,                    IosKind::Working, IosOrigin::Stock,   48},
        {50,  5120,                    IosKind::Stub,    IosOrigin::Stock,   16},
        {51,  4864,                    IosKind::Stub,    IosOrigin::Stock,   16},
        {52,  5888,                    IosKind::Stub,    IosOrigin::Stock,   52},
        {53,  5919,                    IosKind::Working, IosOrigin::Stock,   37},
        {55,  5919,                    IosKind::Working, IosOrigin::Stock,   37},
        {56,  5918,                    IosKind::Working, IosOrigin::Stock,   56},
        {57,  6175,                    IosKind::Working, IosOrigin::Stock,   57},
        {58,  6432,                    IosKind::Working, IosOrigin::Stock,   58},
        {60,  6400,                    IosKind::Stub,    IosOrigin::Stock,   52},
        {61,  5918,                    IosKind::Working, IosOrigin::Stock,   56},
        {62,  6430,                    IosKind::Working, IosOrigin::Stock,   62},
        {70,  6912,                    IosKind::Stub,    IosOrigin::Stock,   70},
        {80,  7200,                    IosKind::Working, IosOrigin::Stock,   80},
        {222, PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   52},
        {223, PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   52},
        {249, PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   52},
        {250, PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   52},
        {254, PlaceholderTitleVersion, IosKind::Stub,    IosOrigin::Stock,   254},
    };
}

}

const IosVersionList& IosVersionList::OfReferenceConsole()
{
    static const IosVersionList referenceList(MakeReferenceVersions());
    return referenceList;
}

}
