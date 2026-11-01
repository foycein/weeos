#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace Mii
{

// The rows of most tables: gender x 9 + age x 3 + look (two genders, three ages, three looks).
constexpr std::size_t LookCombinationCount = 18;

// The rows of the skin table: gender x 3 + look.
constexpr std::size_t GenderLookCount = 6;

// The rows of the hair color table: look x 3 + age.
constexpr std::size_t LookAgeCount = 9;

constexpr std::size_t LookCount = 3;

extern const std::array<std::array<int, 10>, LookCombinationCount> FaceShapeChoices;
extern const std::array<std::array<int, 20>, LookCombinationCount> FacialFeatureChoices;
extern const std::array<std::array<int, 10>, GenderLookCount> SkinColorChoices;
extern const std::array<std::array<int, 20>, LookAgeCount> HairColorChoices;
extern const std::array<std::array<int, 10>, LookCount> EyeColorChoices;

extern const std::array<std::vector<int>, LookCombinationCount> HairTypeChoices;
extern const std::array<std::vector<int>, LookCombinationCount> EyeTypeChoices;
extern const std::array<std::vector<int>, LookCombinationCount> EyebrowTypeChoices;
extern const std::array<std::vector<int>, LookCombinationCount> NoseTypeChoices;
extern const std::array<std::vector<int>, LookCombinationCount> MouthTypeChoices;

}
