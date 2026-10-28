#pragma once

#include "Ios/Profile/IosFeature.h"
#include "Ios/Profile/IosNumber.h"

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace Ios
{

// The value of a flag in the row that lists the IOS which have it.
constexpr std::uint32_t FlagIsSet = 1;

// One row of the feature table: a feature, its value, and the IOS versions that have the feature with this value.
struct FeatureRow
{
    // A flag. The IOS in the list have it and every other IOS does not.
    FeatureRow(Feature flag, std::vector<IosNumberRange> numbers)
        : feature(flag)
        , value(FlagIsSet)
        , iosNumbers(std::move(numbers))
    {
    }

    // A feature with a number as its value.
    FeatureRow(Feature numberFeature, std::uint32_t number, std::vector<IosNumberRange> numbers)
        : feature(numberFeature)
        , value(number)
        , iosNumbers(std::move(numbers))
    {
    }

    // A feature with one of the kinds (MemoryPlan, EsCommandLevel and so on) as its value.
    template <typename Kind>
        requires std::is_enum_v<Kind>
    FeatureRow(Feature kindFeature, Kind kind, std::vector<IosNumberRange> numbers)
        : FeatureRow(kindFeature, static_cast<std::uint32_t>(kind), std::move(numbers))
    {
    }

    // True when the IOS is one of the IOS of this row.
    bool Lists(IosNumber number) const;

    Feature feature;
    std::uint32_t value;
    std::vector<IosNumberRange> iosNumbers;
};

// What differs between the IOS versions, as a list of rows: "SdSupportsHighCapacityCards: IOS48, 56 to 58, 61, 62 and 80".
// A flag has one row, with the IOS that have it.
// A feature with values has one row per value, and every working IOS is in exactly one of them.
class IosFeatureTable
{
public:
    explicit IosFeatureTable(std::vector<FeatureRow> tableRows);

    // The table of the reference console, written by hand from docs/research/IosVersions.md (the matrix of section 7).
    static const IosFeatureTable& OfReferenceConsole();

    const std::vector<FeatureRow>& Rows() const;

    // True when a row of the flag lists the IOS.
    bool Has(Feature flag, IosNumber ios) const;

    // The value of a feature for an IOS. Throws a logic_error when no row of the feature lists the IOS:
    // the table must give every working IOS a value for every feature that has values.
    std::uint32_t ValueOf(Feature feature, IosNumber ios) const;

    template <typename Kind>
    Kind KindOf(Feature feature, IosNumber ios) const
    {
        return static_cast<Kind>(ValueOf(feature, ios));
    }

private:
    std::vector<FeatureRow> rows;
};

}
