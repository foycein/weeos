#pragma once

#include "Formats/Bytes.h"

#include <optional>

namespace Media
{

// How the camera was held, as the Orientation field of Exif says it (TIFF tag 274). The numbers are the field's values.
enum class ExifOrientation
{
    Upright = 1,
    MirroredLeftRight = 2,
    UpsideDown = 3,
    MirroredTopBottom = 4,
    MirroredAndTurnedLeft = 5,
    TurnedRight = 6,
    MirroredAndTurnedRight = 7,
    TurnedLeft = 8,
};

// The date and time a photo was taken, as the camera's clock showed it.
struct ExifDateTime
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    bool operator==(const ExifDateTime&) const = default;
};

// What the Exif segment (APP1) of a camera's JPEG file says.
struct ExifInformation
{
    // What the field says; Upright when the file has no Orientation field.
    int orientation = static_cast<int>(ExifOrientation::Upright);

    // DateTimeOriginal, or DateTime when the camera wrote only that; nothing when neither is a date of the years 1900 to 2099.
    std::optional<ExifDateTime> dateTaken;

    // The small JPEG picture of the second image directory (IFD1). It points into the file's bytes.
    std::optional<Formats::ByteView> thumbnail;
};

// Reads the Exif segment of a JPEG file. A file without one, or with one that cannot be read, gives the defaults.
ExifInformation ReadExifInformation(Formats::ByteView jpegBytes);

}
