#include "Ios/Profile/IosFeatureTable.h"

#include <algorithm>
#include <format>
#include <stdexcept>

namespace Ios
{

bool FeatureRow::Lists(IosNumber number) const
{
    return std::any_of(iosNumbers.begin(), iosNumbers.end(),
        [number](const IosNumberRange& range) { return range.Contains(number); });
}

IosFeatureTable::IosFeatureTable(std::vector<FeatureRow> tableRows)
    : rows(std::move(tableRows))
{
}

const std::vector<FeatureRow>& IosFeatureTable::Rows() const
{
    return rows;
}

bool IosFeatureTable::Has(Feature flag, IosNumber ios) const
{
    return std::any_of(rows.begin(), rows.end(),
        [flag, ios](const FeatureRow& row) { return row.feature == flag && row.Lists(ios); });
}

std::uint32_t IosFeatureTable::ValueOf(Feature feature, IosNumber ios) const
{
    for (const FeatureRow& row : rows)
    {
        if (row.feature == feature && row.Lists(ios))
            return row.value;
    }

    throw std::logic_error(std::format("No row of the feature table gives IOS{} a value for {}", ios, FeatureName(feature)));
}

}
