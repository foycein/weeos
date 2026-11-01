#include "Mii/MiiBodyFile.h"

namespace Mii
{

namespace
{

// The curve between two keys that starts at the first value with the first slope and ends at the second value with the second slope.
float HermiteValue(const AnimationKey& start, const AnimationKey& end, float frame)
{
    float span = end.frame - start.frame;
    if (span <= 0)
        return end.value;

    float t = (frame - start.frame) / span;
    float tSquared = t * t;
    float tCubed = tSquared * t;

    float startWeight = 2 * tCubed - 3 * tSquared + 1;
    float endWeight = -2 * tCubed + 3 * tSquared;
    float startSlopeWeight = tCubed - 2 * tSquared + t;
    float endSlopeWeight = tCubed - tSquared;

    return startWeight * start.value + endWeight * end.value + (startSlopeWeight * start.slope + endSlopeWeight * end.slope) * span;
}

Formats::Vector3 TracksValueAt(const AnimationTracks& tracks, float frame)
{
    return {TrackValueAt(tracks[0], frame), TrackValueAt(tracks[1], frame), TrackValueAt(tracks[2], frame)};
}

}

float TrackValueAt(const AnimationTrack& track, float frame)
{
    if (track.keys.empty())
        return track.fixedValue;

    if (frame <= track.keys.front().frame)
        return track.keys.front().value;

    for (std::size_t keyNumber = 1; keyNumber < track.keys.size(); ++keyNumber)
    {
        const AnimationKey& end = track.keys[keyNumber];
        if (frame <= end.frame)
            return HermiteValue(track.keys[keyNumber - 1], end, frame);
    }
    return track.keys.back().value;
}

BoneTransform AnimatedTransform(const BoneAnimation& animation, const BoneTransform& restTransform, float frame)
{
    BoneTransform transform = restTransform;
    transform.undoesParentScale = animation.undoesParentScale;

    if (animation.scale)
        transform.scale = TracksValueAt(*animation.scale, frame);

    if (animation.rotation)
        transform.rotationDegrees = TracksValueAt(*animation.rotation, frame);

    if (animation.translation)
        transform.translation = TracksValueAt(*animation.translation, frame);

    return transform;
}

}
