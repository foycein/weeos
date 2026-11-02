#pragma once

#include <string_view>

namespace SystemMenu
{

// The version of WeeOS that the welcome shows. It stays 0.x until the whole system is done; then it becomes 1.0.
constexpr std::u16string_view WeeOSVersion = u"0.8";

}
