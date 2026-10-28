#pragma once

#include "Formats/RgbaImage.h"

#include <optional>
#include <string>

namespace Layout
{

// Gives the pictures that layouts show. A picture is asked for by the name of its texture file, for example "my_Balloon_a.tpl".
class PictureSource
{
public:
    PictureSource() = default;
    PictureSource(const PictureSource&) = delete;
    PictureSource& operator=(const PictureSource&) = delete;
    virtual ~PictureSource() = default;

    // Gives the first image of the texture file, or nothing when the source has no such file.
    virtual std::optional<Formats::RgbaImage> FindPicture(const std::string& textureFileName) const = 0;
};

}
