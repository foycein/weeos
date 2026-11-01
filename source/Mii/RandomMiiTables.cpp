#include "Mii/RandomMiiTables.h"

// The tables of the random generator of the face library, as the Mii Channel and the System Menu hold them
// (docs/research/Mii.md section 4). Each row lists the choices for one kind of Mii; a value that is listed more often is
// drawn more often. The comment names the kind of Mii of the row.

namespace Mii
{

const std::array<std::array<int, 10>, LookCombinationCount> FaceShapeChoices = {{
    {0, 0, 0, 1, 1, 1, 2, 3, 4, 5}, // male, child, dark skin
    {0, 0, 0, 1, 1, 1, 2, 3, 4, 5}, // male, child, light skin
    {0, 0, 0, 1, 1, 1, 2, 3, 4, 5}, // male, child, East Asian
    {0, 0, 1, 2, 2, 3, 4, 5, 6, 7}, // male, adult, dark skin
    {0, 1, 2, 2, 3, 4, 5, 6, 6, 7}, // male, adult, light skin
    {0, 0, 1, 2, 2, 3, 4, 5, 6, 7}, // male, adult, East Asian
    {0, 0, 1, 2, 2, 3, 4, 5, 6, 7}, // male, elder, dark skin
    {0, 1, 2, 2, 3, 4, 5, 6, 7, 7}, // male, elder, light skin
    {0, 0, 1, 2, 2, 3, 4, 5, 6, 7}, // male, elder, East Asian
    {0, 0, 0, 1, 1, 1, 2, 3, 4, 5}, // female, child, dark skin
    {0, 0, 0, 1, 1, 1, 2, 3, 4, 5}, // female, child, light skin
    {0, 0, 0, 1, 1, 1, 2, 3, 4, 5}, // female, child, East Asian
    {0, 0, 0, 1, 1, 2, 2, 3, 4, 5}, // female, adult, dark skin
    {0, 0, 0, 1, 1, 2, 2, 3, 4, 5}, // female, adult, light skin
    {0, 0, 0, 1, 1, 2, 2, 3, 4, 5}, // female, adult, East Asian
    {0, 0, 0, 1, 1, 2, 2, 3, 4, 5}, // female, elder, dark skin
    {0, 0, 0, 1, 1, 2, 2, 3, 4, 5}, // female, elder, light skin
    {0, 0, 0, 1, 1, 2, 2, 3, 4, 5}, // female, elder, East Asian
}};

const std::array<std::array<int, 20>, LookCombinationCount> FacialFeatureChoices = {{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3}, // male, child, dark skin
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 8, 8}, // male, child, light skin
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3}, // male, child, East Asian
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 5, 6, 7, 8, 9, 10}, // male, adult, dark skin
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 5, 6, 7, 8, 9, 10}, // male, adult, light skin
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 4, 5, 6, 7, 9, 10}, // male, adult, East Asian
    {10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11}, // male, elder, dark skin
    {10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11}, // male, elder, light skin
    {10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11}, // male, elder, East Asian
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 4, 4}, // female, child, dark skin
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 4, 4}, // female, child, light skin
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 4, 4}, // female, child, East Asian
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 4, 4, 8, 8}, // female, adult, dark skin
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 8, 8}, // female, adult, light skin
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4}, // female, adult, East Asian
    {10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11}, // female, elder, dark skin
    {10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11}, // female, elder, light skin
    {10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11}, // female, elder, East Asian
}};

const std::array<std::array<int, 10>, GenderLookCount> SkinColorChoices = {{
    {2, 2, 4, 4, 4, 4, 5, 5, 5, 5}, // male, dark skin
    {0, 0, 0, 0, 1, 1, 2, 3, 3, 3}, // male, light skin
    {0, 0, 1, 1, 1, 1, 1, 1, 1, 2}, // male, East Asian
    {2, 2, 4, 4, 4, 4, 5, 5, 5, 5}, // female, dark skin
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 3}, // female, light skin
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1}, // female, East Asian
}};

const std::array<std::array<int, 20>, LookAgeCount> HairColorChoices = {{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // dark skin, child
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // dark skin, adult
    {0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}, // dark skin, elder
    {2, 3, 3, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7}, // light skin, child
    {2, 3, 3, 3, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7}, // light skin, adult
    {2, 3, 3, 4, 4, 4, 4, 4, 4, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7}, // light skin, elder
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // East Asian, child
    {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 3, 3, 3, 3}, // East Asian, adult
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4}, // East Asian, elder
}};

const std::array<std::array<int, 10>, LookCount> EyeColorChoices = {{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // dark skin
    {0, 1, 1, 2, 3, 3, 4, 4, 4, 5}, // light skin
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, // East Asian
}};

const std::array<std::vector<int>, LookCombinationCount> HairTypeChoices = {{
    {13, 23, 30, 31, 32, 33, 34, 35, 36, 37, 38, 40, 43, 44, 45, 47, 48, 49, 50, 51, 52, 54, 56, 57, 64, 66}, // male, child, dark skin
    {13, 23, 30, 31, 32, 33, 34, 35, 36, 37, 38, 40, 43, 44, 45, 47, 48, 49, 50, 51, 52, 54, 56, 57, 64, 66}, // male, child, light skin
    {13, 23, 30, 31, 32, 33, 34, 35, 36, 37, 38, 40, 43, 44, 45, 47, 48, 49, 50, 51, 52, 54, 56, 57, 64, 66}, // male, child, East Asian
    {13, 23, 30, 31, 32, 33, 34, 36, 37, 38, 40, 42, 43, 44, 45, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59, 60, 64, 65, 66, 67, 68, 70}, // male, adult, dark skin
    {13, 23, 30, 31, 32, 33, 34, 36, 37, 38, 39, 40, 43, 44, 45, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59, 60, 64, 65, 66, 67, 68, 70}, // male, adult, light skin
    {13, 23, 30, 31, 32, 33, 34, 36, 37, 38, 39, 40, 43, 44, 45, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59, 60, 64, 65, 66, 67, 68, 70}, // male, adult, East Asian
    {13, 23, 30, 36, 37, 41, 45, 47, 51, 53, 54, 55, 58, 59, 65, 67}, // male, elder, dark skin
    {13, 23, 30, 36, 37, 39, 41, 45, 47, 51, 53, 54, 55, 58, 59, 65, 67}, // male, elder, light skin
    {13, 23, 30, 36, 37, 39, 41, 45, 47, 51, 53, 54, 55, 58, 59, 65, 67}, // male, elder, East Asian
    {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 28, 46, 50, 61, 62, 63, 64, 69}, // female, child, dark skin
    {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 28, 46, 50, 61, 62, 63, 64, 69}, // female, child, light skin
    {0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 28, 46, 50, 61, 62, 63, 64, 69}, // female, child, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 42, 50, 58, 60, 62, 63, 64, 69, 71}, // female, adult, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 50, 58, 60, 62, 63, 64, 69, 71}, // female, adult, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 25, 26, 27, 29, 50, 58, 60, 62, 63, 64, 69, 71}, // female, adult, East Asian
    {0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 16, 17, 18, 20, 21, 24, 25, 58, 62, 69}, // female, elder, dark skin
    {0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 16, 17, 18, 20, 21, 24, 25, 58, 62, 69}, // female, elder, light skin
    {0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14, 16, 17, 18, 20, 21, 24, 25, 58, 62, 69}, // female, elder, East Asian
}};

const std::array<std::vector<int>, LookCombinationCount> EyeTypeChoices = {{
    {2, 3, 5, 7, 8, 9, 11, 12, 13, 15, 16, 18, 27, 29, 32, 34, 36, 38, 39, 41, 43, 47}, // male, child, dark skin
    {2, 3, 5, 7, 8, 9, 11, 12, 13, 15, 16, 18, 27, 29, 32, 34, 36, 38, 39, 41, 43, 47}, // male, child, light skin
    {2, 3, 5, 7, 8, 9, 11, 12, 13, 15, 16, 18, 26, 27, 29, 32, 34, 36, 38, 39, 41, 43, 47}, // male, child, East Asian
    {2, 3, 5, 6, 7, 8, 9, 11, 12, 13, 15, 16, 17, 18, 21, 22, 27, 29, 31, 32, 34, 36, 37, 38, 39, 41, 43, 44, 47}, // male, adult, dark skin
    {2, 3, 5, 6, 7, 8, 9, 11, 12, 13, 15, 16, 17, 18, 21, 22, 27, 29, 31, 32, 34, 36, 37, 38, 39, 41, 43, 44, 47}, // male, adult, light skin
    {2, 3, 5, 6, 7, 8, 9, 11, 12, 13, 15, 16, 18, 21, 22, 26, 27, 29, 31, 32, 34, 36, 37, 38, 39, 41, 43, 44, 47}, // male, adult, East Asian
    {2, 3, 5, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 21, 22, 31, 32, 34, 36, 37, 39, 41, 44}, // male, elder, dark skin
    {2, 3, 5, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 21, 22, 31, 32, 34, 36, 37, 39, 41, 44}, // male, elder, light skin
    {2, 3, 5, 7, 8, 9, 11, 12, 13, 14, 15, 16, 18, 21, 22, 26, 31, 32, 34, 36, 37, 39, 41, 44}, // male, elder, East Asian
    {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 19, 23, 24, 25, 27, 28, 29, 32, 33, 34, 35, 38, 39, 40, 41, 42, 45, 46, 47}, // female, child, dark skin
    {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 19, 23, 24, 25, 27, 28, 29, 32, 33, 34, 35, 38, 39, 40, 41, 42, 45, 46, 47}, // female, child, light skin
    {0, 1, 2, 4, 5, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 19, 23, 24, 25, 26, 27, 28, 29, 32, 33, 34, 35, 38, 39, 40, 41, 42, 45, 46, 47}, // female, child, East Asian
    {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 27, 28, 29, 30, 32, 33, 34, 35, 37, 38, 39, 40, 41, 42, 45, 46, 47}, // female, adult, dark skin
    {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 27, 28, 29, 30, 32, 33, 34, 35, 37, 38, 39, 40, 41, 42, 45, 46, 47}, // female, adult, light skin
    {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 18, 19, 20, 21, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35, 37, 38, 39, 40, 41, 42, 45, 46, 47}, // female, adult, East Asian
    {1, 2, 5, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 21, 32, 34, 37, 39, 41}, // female, elder, dark skin
    {1, 2, 5, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 21, 32, 34, 37, 39, 41}, // female, elder, light skin
    {1, 2, 5, 7, 8, 9, 11, 12, 13, 14, 15, 16, 18, 21, 26, 32, 34, 37, 39, 41}, // female, elder, East Asian
}};

const std::array<std::vector<int>, LookCombinationCount> EyebrowTypeChoices = {{
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18, 20}, // male, child, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18, 20}, // male, child, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18, 20}, // male, child, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22}, // male, adult, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22}, // male, adult, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22}, // male, adult, East Asian
    {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 22}, // male, elder, dark skin
    {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 22}, // male, elder, light skin
    {0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 21, 22}, // male, elder, East Asian
    {0, 1, 3, 7, 8, 9, 10, 11, 13}, // female, child, dark skin
    {0, 1, 3, 7, 8, 9, 10, 11, 13}, // female, child, light skin
    {0, 1, 3, 7, 8, 9, 10, 11, 13}, // female, child, East Asian
    {0, 1, 3, 7, 8, 9, 10, 11, 13, 15, 19}, // female, adult, dark skin
    {0, 1, 3, 7, 8, 9, 10, 11, 13, 15, 19}, // female, adult, light skin
    {0, 1, 3, 7, 8, 9, 10, 11, 13, 15, 19}, // female, adult, East Asian
    {0, 3, 7, 8, 9, 10, 11, 13, 15}, // female, elder, dark skin
    {0, 3, 7, 8, 9, 10, 11, 13, 15}, // female, elder, light skin
    {0, 3, 7, 8, 9, 10, 11, 13, 15}, // female, elder, East Asian
}};

const std::array<std::vector<int>, LookCombinationCount> NoseTypeChoices = {{
    {0, 1, 2, 3, 4, 5, 7, 8, 10}, // male, child, dark skin
    {0, 1, 2, 3, 4, 5, 7, 8, 10}, // male, child, light skin
    {0, 1, 2, 3, 4, 5, 7, 8, 10}, // male, child, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // male, adult, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // male, adult, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11}, // male, adult, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // male, elder, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // male, elder, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11}, // male, elder, East Asian
    {0, 1, 3, 4, 8, 10}, // female, child, dark skin
    {0, 1, 3, 4, 8, 10}, // female, child, light skin
    {0, 1, 3, 4, 8, 10}, // female, child, East Asian
    {0, 1, 3, 4, 6, 8, 9, 10, 11}, // female, adult, dark skin
    {0, 1, 3, 4, 6, 8, 9, 10, 11}, // female, adult, light skin
    {0, 1, 3, 4, 6, 8, 10, 11}, // female, adult, East Asian
    {0, 1, 3, 4, 6, 8, 9, 10, 11}, // female, elder, dark skin
    {0, 1, 3, 4, 6, 8, 9, 10, 11}, // female, elder, light skin
    {0, 1, 3, 4, 6, 8, 10, 11}, // female, elder, East Asian
}};

const std::array<std::vector<int>, LookCombinationCount> MouthTypeChoices = {{
    {0, 2, 3, 6, 7, 8, 9, 10, 12, 14, 15, 17, 18, 19, 21, 22, 23}, // male, child, dark skin
    {0, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23}, // male, child, light skin
    {0, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23}, // male, child, East Asian
    {0, 2, 3, 6, 7, 8, 9, 10, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}, // male, adult, dark skin
    {0, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}, // male, adult, light skin
    {0, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}, // male, adult, East Asian
    {0, 2, 3, 6, 7, 8, 9, 10, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}, // male, elder, dark skin
    {0, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}, // male, elder, light skin
    {0, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23}, // male, elder, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 14, 15, 17, 18, 19, 21, 22, 23}, // female, child, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23}, // female, child, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23}, // female, child, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 14, 15, 17, 18, 19, 21, 22, 23}, // female, adult, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23}, // female, adult, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 17, 18, 19, 21, 22, 23}, // female, adult, East Asian
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 14, 15, 16, 17, 18, 19, 21, 22, 23}, // female, elder, dark skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23}, // female, elder, light skin
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23}, // female, elder, East Asian
}};

}
