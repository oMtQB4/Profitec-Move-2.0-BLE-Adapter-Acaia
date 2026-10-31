#pragma once

/** Case-insensitive name matching for scale advertisements. */

#include <algorithm>
#include <cctype>
#include <string_view>

namespace scale {

inline bool equalsIgnoreCase(char a, char b)
{
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

inline bool startsWithIgnoreCase(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), text.begin(), equalsIgnoreCase);
}

inline bool containsIgnoreCase(std::string_view text, std::string_view part)
{
    return std::search(text.begin(), text.end(), part.begin(), part.end(), equalsIgnoreCase) != text.end();
}

} // namespace scale
