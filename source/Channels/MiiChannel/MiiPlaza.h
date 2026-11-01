#pragma once

#include "Channels/MiiChannel/MiiChannelData.h"

#include "Formats/Vectors.h"
#include "Graphics/Matrix4.h"

#include <cstddef>
#include <optional>
#include <random>
#include <vector>

namespace Channels
{

// What a Mii of the plaza is doing.
enum class PlazaActivity
{
    // Standing in bodyWait until its wait is over.
    Waiting,

    // Walking in bodyWalk toward its target.
    Walking,

    // Held by a hand above the plaza.
    Held,
};

// The ways the whistle lines the Miis up (squText_10 to _15 of squareNigaoe_00).
enum class PlazaArrangement
{
    Disband,
    Alphabetical,
    Favorites,
    Color,
    Gender,
};

struct PlazaMii
{
    StoredMii mii;

    // On the ground of the plaza: x across, z toward the camera.
    Formats::Vector2 position = {0, 0};

    // The direction the Mii faces, in degrees; 0 looks toward the camera (+z).
    float facingDegrees = 0;
    float speed = 0;

    PlazaActivity activity = PlazaActivity::Waiting;
    Formats::Vector2 target = {0, 0};
    int stepsToWait = 0;

    // The frame of the animation the Mii plays now.
    float animationFrame = 0;

    // While a whistle has called the Mii to its place in a line: it walks there and stays.
    bool staysAtTarget = false;
};

// The camera of the plaza (8001295C, 80014DA4): it looks down along a fixed direction, and its height is one of four zoom levels.
struct PlazaCamera
{
    Formats::Vector3 eye;

    // Where the camera moves to. The eye glides there a little every step.
    Formats::Vector3 destination;

    int zoomLevel = 1;
};

// The Mii Plaza: the Miis of the database walking and standing on the ground, the camera above them and the hand that grabs one
// (docs/audit/MiiChannel.md, "The plaza"). It is plain arithmetic, so the tests run it without a picture.
class MiiPlaza
{
public:
    // The lowest and the highest zoom level: + goes down, - goes up.
    static constexpr int ClosestZoomLevel = 0;
    static constexpr int FarthestZoomLevel = 3;

    // The plaza of the 16:9 picture, which shows more of the ground than the 4:3 one, so the Miis may walk farther to the sides.
    explicit MiiPlaza(std::uint32_t randomSeed);

    // The Miis come onto the plaza at random places, each standing for a while before it walks.
    void SetMiis(const std::vector<StoredMii>& miis);

    // A Mii that was edited keeps its place; a new one comes in at the middle of the plaza.
    void UpdateMii(const StoredMii& mii);
    void RemoveMii(std::size_t slot);

    const std::vector<PlazaMii>& Miis() const;
    const PlazaMii* FindMii(std::size_t slot) const;

    // One step: every Mii that is not held waits, turns or walks, the Miis push each other apart and stay inside the plaza, and the
    // camera glides toward its destination.
    void Update();

    // The whistle: the Miis walk to their places in lines, in the order the arrangement gives them; Disband lets them go again.
    void Arrange(PlazaArrangement arrangement);

    // The order of the slots in the lines of an arrangement.
    std::vector<std::size_t> ArrangedOrder(PlazaArrangement arrangement) const;

    // The hand picks the Mii up and carries it until it lets go.
    void Grab(std::size_t slot);
    void MoveHeldMii(Formats::Vector2 groundPosition);
    void Drop();
    std::optional<std::size_t> HeldMii() const;

    // The camera.
    const PlazaCamera& Camera() const;
    bool ZoomIn();
    bool ZoomOut();

    // The D-pad and the hand with B held move the camera across the plaza, kept inside the limits of the zoom level.
    void ScrollCamera(Formats::Vector2 direction);

    Graphics::Matrix4 ViewMatrix() const;
    Graphics::Matrix4 ProjectionMatrix() const;

    // Where a point of the plaza appears in the picture, in layout units (middle (0, 0), y upward), for a canvas of this size.
    Formats::Vector2 PlaceInPicture(Formats::Vector3 point, float canvasWidth, float canvasHeight) const;

    // The point of the ground that a place of the picture shows.
    Formats::Vector2 GroundPointAt(Formats::Vector2 picturePlace, float canvasWidth, float canvasHeight) const;

    // The Mii nearest the camera whose body covers the place of the picture, if one does.
    std::optional<std::size_t> MiiAt(Formats::Vector2 picturePlace, float canvasWidth, float canvasHeight) const;

    // The limits of the plaza.
    float HalfWidth() const;
    static float NearestZ();
    static float FarthestZ();

private:
    void MoveMii(PlazaMii& plazaMii);
    void StartWaiting(PlazaMii& plazaMii);
    void StartWalking(PlazaMii& plazaMii, Formats::Vector2 target);
    void Walk(PlazaMii& plazaMii);
    void PushApart();
    void KeepInside(PlazaMii& plazaMii);
    Formats::Vector2 RandomPlace();
    int RandomWait();
    void LimitCameraDestination();

    std::mt19937 random;
    std::vector<PlazaMii> miis;
    std::optional<std::size_t> heldSlot;
    PlazaCamera camera;
};

// The speed at which the walk animation plays for a Mii at a walking speed: a longer body takes longer steps (80022BD0).
float WalkAnimationRate(float speed, const Mii::MiiRecord& record);

}
