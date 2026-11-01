#include "Channels/MiiChannel/MiiPlaza.h"

#include "Mii/MiiBody.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Channels
{

namespace
{

// The plaza is 2 x 2200 units wide (2 x 3050 in a 16:9 picture) and reaches from 1850 units behind the middle to 2050 units in front
// of it: 4600 (6300) x 0.5 - 100 across, 4100 x 0.5 in front and 200 less behind (80026CD8, 803BE164 to 803BE16C, 803BE5C0 to 803BE5C8).
constexpr float StandardHalfWidth = 2200.0f;
constexpr float WideHalfWidth = 3050.0f;
constexpr float FrontLimit = 2050.0f;
constexpr float BackLimit = -1850.0f;

// Two Miis closer than this push each other apart (the 120 x 120 of 803BE5DC).
constexpr float PersonalDistance = 120.0f;

// Walking (8001F7A8, state 1 of a plaza Mii): every step the speed falls to 95 percent and grows by 0.1, so it settles at 2 units a
// step; a Mii closer than 5 units to its target has arrived.
constexpr float SpeedKeptPerStep = 0.95f;
constexpr float SpeedAddedPerStep = 0.1f;
constexpr float ArrivalDistance = 5.0f;

// The walk animation runs 0.6 / length of the body frames per unit of distance (80022BD0).
constexpr float WalkFramesPerUnit = 0.6f;
constexpr float WalkAnimationFrames = 40.0f;
constexpr float WaitAnimationFrames = 120.0f;

// Assumed (CHN-63): how long a Mii stands, how far it walks and how fast it turns; the rules that choose them were not traced.
constexpr int ShortestWaitSteps = 120;
constexpr int LongestWaitSteps = 600;
constexpr float LongestWalk = 900.0f;
constexpr float TurnDegreesPerStep = 8.0f;

// The whistle's lines (Assumed, CHN-64): ten Miis to a line, 160 units apart, the first line in front.
constexpr int MiisPerLine = 10;
constexpr float LineSpacing = 160.0f;
constexpr float FirstLineZ = 700.0f;

// The camera (80014DA4): it looks along (0, -500, -300); its four zoom levels are the heights 1400, 3000, 5400 and the far view
// (0, 12411.05, 7438.11); the plaza starts at the height 3000 with the eye at (0, 3000, 1700).
constexpr Formats::Vector3 ViewDirection = {0.0f, -500.0f, -300.0f};
constexpr float ZoomHeights[] = {1400.0f, 3000.0f, 5400.0f, 12411.05f};
constexpr Formats::Vector3 FarthestEye = {0.0f, 12411.05f, 7438.11f};
constexpr Formats::Vector3 StartingEye = {0.0f, 3000.0f, 1700.0f};

// The camera's limits (8001295C): at the height y it may move 2300 (3150 in 16:9) x (1 - y / 12411.05) to the sides, and 2050 x (that
// same fraction) in front of and behind 1.02 x 0.6 y.
constexpr float StandardCameraSideLimit = 2300.0f;
constexpr float WideCameraSideLimit = 3150.0f;
constexpr float CameraDepthLimit = 2050.0f;
constexpr float CameraCenterFactor = 1.02f;

// The view (80014BE4): 14 degrees high, from 10 to 100000 units.
constexpr float VerticalViewDegrees = 14.0f;
constexpr float NearDistance = 10.0f;
constexpr float FarDistance = 100000.0f;
constexpr float StandardAspect = 4.0f / 3.0f;
constexpr float WideAspect = 16.0f / 9.0f;

// The D-pad moves the camera 15 units a step (8001295C). The eye glides a tenth of the way to its destination every step (Assumed, CHN-62).
constexpr float ScrollPerStep = 15.0f;
constexpr float GlidePerStep = 0.1f;

// Where a hand finds a Mii: the body from the feet to the top of the head, about 160 units, and 45 units to each side.
constexpr float PickHeight = 160.0f;
constexpr float PickHalfWidth = 45.0f;

// The Miis of the plaza lean back by 45 degrees toward the camera (aslope_root of every animation).
constexpr float LeanCosine = 0.70710678f;

constexpr float DegreesPerRadian = 180.0f / std::numbers::pi_v<float>;

Formats::Vector3 Normalized(Formats::Vector3 vector)
{
    float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    return {vector.x / length, vector.y / length, vector.z / length};
}

Formats::Vector3 Cross(Formats::Vector3 first, Formats::Vector3 second)
{
    return {first.y * second.z - first.z * second.y, first.z * second.x - first.x * second.z, first.x * second.y - first.y * second.x};
}

float Distance(Formats::Vector2 first, Formats::Vector2 second)
{
    return std::hypot(second.x - first.x, second.y - first.y);
}

// The difference of two directions, from -180 to 180 degrees.
float TurnBetween(float fromDegrees, float toDegrees)
{
    float difference = std::fmod(toDegrees - fromDegrees, 360.0f);
    if (difference > 180.0f)
        difference -= 360.0f;
    if (difference < -180.0f)
        difference += 360.0f;
    return difference;
}

// The point on the ground that the eye looks at, along the view direction.
Formats::Vector3 GroundPointUnder(Formats::Vector3 eye)
{
    Formats::Vector3 direction = Normalized(ViewDirection);
    float distance = eye.y / -direction.y;
    return {eye.x + direction.x * distance, 0.0f, eye.z + direction.z * distance};
}

std::u16string LowerCase(std::u16string text)
{
    for (char16_t& character : text)
    {
        if (character >= u'A' && character <= u'Z')
            character = static_cast<char16_t>(character - u'A' + u'a');
    }
    return text;
}

}

float WalkAnimationRate(float speed, const Mii::MiiRecord& record)
{
    return speed * WalkFramesPerUnit / Mii::SizeOf(record).length;
}

MiiPlaza::MiiPlaza(bool isWidePicture, std::uint32_t randomSeed)
    : isWide(isWidePicture)
    , random(randomSeed)
{
    camera.eye = StartingEye;
    camera.destination = StartingEye;
}

void MiiPlaza::SetMiis(const std::vector<StoredMii>& storedMiis)
{
    miis.clear();
    heldSlot.reset();
    for (const StoredMii& stored : storedMiis)
    {
        PlazaMii plazaMii;
        plazaMii.mii = stored;
        plazaMii.position = RandomPlace();
        plazaMii.facingDegrees = 0;
        StartWaiting(plazaMii);
        plazaMii.animationFrame = std::uniform_real_distribution<float>(0, WaitAnimationFrames)(random);
        miis.push_back(plazaMii);
    }
}

void MiiPlaza::UpdateMii(const StoredMii& stored)
{
    for (PlazaMii& plazaMii : miis)
    {
        if (plazaMii.mii.slot == stored.slot)
        {
            plazaMii.mii = stored;
            return;
        }
    }

    PlazaMii plazaMii;
    plazaMii.mii = stored;
    StartWaiting(plazaMii);
    miis.push_back(plazaMii);
}

void MiiPlaza::RemoveMii(std::size_t slot)
{
    std::erase_if(miis, [slot](const PlazaMii& plazaMii) { return plazaMii.mii.slot == slot; });
    if (heldSlot == slot)
        heldSlot.reset();
}

const std::vector<PlazaMii>& MiiPlaza::Miis() const
{
    return miis;
}

const PlazaMii* MiiPlaza::FindMii(std::size_t slot) const
{
    for (const PlazaMii& plazaMii : miis)
    {
        if (plazaMii.mii.slot == slot)
            return &plazaMii;
    }
    return nullptr;
}

void MiiPlaza::Update()
{
    for (PlazaMii& plazaMii : miis)
        MoveMii(plazaMii);

    PushApart();
    for (PlazaMii& plazaMii : miis)
        KeepInside(plazaMii);

    camera.eye.x += (camera.destination.x - camera.eye.x) * GlidePerStep;
    camera.eye.y += (camera.destination.y - camera.eye.y) * GlidePerStep;
    camera.eye.z += (camera.destination.z - camera.eye.z) * GlidePerStep;
}

void MiiPlaza::Arrange(PlazaArrangement arrangement)
{
    if (arrangement == PlazaArrangement::Disband)
    {
        for (PlazaMii& plazaMii : miis)
        {
            plazaMii.staysAtTarget = false;
            if (plazaMii.activity != PlazaActivity::Held)
                StartWalking(plazaMii, RandomPlace());
        }
        return;
    }

    std::vector<std::size_t> order = ArrangedOrder(arrangement);
    for (std::size_t place = 0; place < order.size(); ++place)
    {
        int line = static_cast<int>(place) / MiisPerLine;
        int column = static_cast<int>(place) % MiisPerLine;
        int miisInLine = std::min<int>(MiisPerLine, static_cast<int>(order.size()) - line * MiisPerLine);
        float x = (static_cast<float>(column) - static_cast<float>(miisInLine - 1) / 2) * LineSpacing;
        float z = FirstLineZ - static_cast<float>(line) * LineSpacing;

        for (PlazaMii& plazaMii : miis)
        {
            if (plazaMii.mii.slot != order[place] || plazaMii.activity == PlazaActivity::Held)
                continue;

            plazaMii.staysAtTarget = true;
            StartWalking(plazaMii, {x, z});
        }
    }
}

std::vector<std::size_t> MiiPlaza::ArrangedOrder(PlazaArrangement arrangement) const
{
    std::vector<const PlazaMii*> sorted;
    for (const PlazaMii& plazaMii : miis)
        sorted.push_back(&plazaMii);

    auto bySlot = [](const PlazaMii* first, const PlazaMii* second) { return first->mii.slot < second->mii.slot; };
    std::sort(sorted.begin(), sorted.end(), bySlot);

    auto keyOf = [arrangement](const PlazaMii* plazaMii) -> int {
        const Mii::MiiRecord& record = plazaMii->mii.record;
        switch (arrangement)
        {
        case PlazaArrangement::Favorites:
            return record.isFavorite ? 0 : 1;
        case PlazaArrangement::Color:
            return record.favoriteColor;
        case PlazaArrangement::Gender:
            return record.gender == Mii::Gender::Male ? 0 : 1;
        default:
            return 0;
        }
    };

    if (arrangement == PlazaArrangement::Alphabetical)
    {
        std::stable_sort(sorted.begin(), sorted.end(), [](const PlazaMii* first, const PlazaMii* second) {
            return LowerCase(first->mii.record.name) < LowerCase(second->mii.record.name);
        });
    }
    else
    {
        std::stable_sort(
            sorted.begin(), sorted.end(), [&keyOf](const PlazaMii* first, const PlazaMii* second) { return keyOf(first) < keyOf(second); });
    }

    std::vector<std::size_t> order;
    for (const PlazaMii* plazaMii : sorted)
        order.push_back(plazaMii->mii.slot);
    return order;
}

void MiiPlaza::Grab(std::size_t slot)
{
    for (PlazaMii& plazaMii : miis)
    {
        if (plazaMii.mii.slot != slot)
            continue;

        plazaMii.activity = PlazaActivity::Held;
        plazaMii.speed = 0;
        plazaMii.staysAtTarget = false;
        heldSlot = slot;
        return;
    }
}

void MiiPlaza::MoveHeldMii(Formats::Vector2 groundPosition)
{
    for (PlazaMii& plazaMii : miis)
    {
        if (heldSlot == plazaMii.mii.slot)
            plazaMii.position = groundPosition;
    }
}

void MiiPlaza::Drop()
{
    for (PlazaMii& plazaMii : miis)
    {
        if (heldSlot == plazaMii.mii.slot)
            StartWaiting(plazaMii);
    }
    heldSlot.reset();
}

std::optional<std::size_t> MiiPlaza::HeldMii() const
{
    return heldSlot;
}

const PlazaCamera& MiiPlaza::Camera() const
{
    return camera;
}

// A new height keeps the point of the ground in the middle of the view, then the eye stays inside the limits of that height.
bool MiiPlaza::ZoomIn()
{
    if (camera.zoomLevel <= ClosestZoomLevel)
        return false;

    --camera.zoomLevel;
    Formats::Vector3 lookedAt = GroundPointUnder(camera.destination);
    Formats::Vector3 direction = Normalized(ViewDirection);
    float height = ZoomHeights[camera.zoomLevel];
    float distance = height / -direction.y;
    camera.destination = {lookedAt.x - direction.x * distance, height, lookedAt.z - direction.z * distance};
    LimitCameraDestination();
    return true;
}

bool MiiPlaza::ZoomOut()
{
    if (camera.zoomLevel >= FarthestZoomLevel)
        return false;

    ++camera.zoomLevel;
    if (camera.zoomLevel == FarthestZoomLevel)
    {
        camera.destination = FarthestEye;
        return true;
    }

    Formats::Vector3 lookedAt = GroundPointUnder(camera.destination);
    Formats::Vector3 direction = Normalized(ViewDirection);
    float height = ZoomHeights[camera.zoomLevel];
    float distance = height / -direction.y;
    camera.destination = {lookedAt.x - direction.x * distance, height, lookedAt.z - direction.z * distance};
    LimitCameraDestination();
    return true;
}

void MiiPlaza::ScrollCamera(Formats::Vector2 direction)
{
    camera.destination.x += direction.x * ScrollPerStep;
    camera.destination.z += direction.y * ScrollPerStep;
    LimitCameraDestination();
}

Graphics::Matrix4 MiiPlaza::ViewMatrix() const
{
    Formats::Vector3 target = {camera.eye.x + ViewDirection.x, camera.eye.y + ViewDirection.y, camera.eye.z + ViewDirection.z};
    return Graphics::MakeLookAt(camera.eye, target, {0, 1, 0});
}

Graphics::Matrix4 MiiPlaza::ProjectionMatrix() const
{
    float aspect = isWide ? WideAspect : StandardAspect;
    return Graphics::MakePerspective(VerticalViewDegrees, aspect, NearDistance, FarDistance);
}

Formats::Vector2 MiiPlaza::PlaceInPicture(Formats::Vector3 point, float canvasWidth, float canvasHeight) const
{
    Graphics::Matrix4 viewAndProjection = ProjectionMatrix() * ViewMatrix();
    const auto& rows = viewAndProjection.rows;
    float x = rows[0][0] * point.x + rows[0][1] * point.y + rows[0][2] * point.z + rows[0][3];
    float y = rows[1][0] * point.x + rows[1][1] * point.y + rows[1][2] * point.z + rows[1][3];
    float w = rows[3][0] * point.x + rows[3][1] * point.y + rows[3][2] * point.z + rows[3][3];
    return {x / w * canvasWidth / 2, y / w * canvasHeight / 2};
}

Formats::Vector2 MiiPlaza::GroundPointAt(Formats::Vector2 picturePlace, float canvasWidth, float canvasHeight) const
{
    Formats::Vector3 forward = Normalized(ViewDirection);
    Formats::Vector3 right = Normalized(Cross(forward, {0, 1, 0}));
    Formats::Vector3 up = Cross(right, forward);

    float halfViewTangent = std::tan(VerticalViewDegrees / 2 / DegreesPerRadian);
    float aspect = isWide ? WideAspect : StandardAspect;
    float across = picturePlace.x / (canvasWidth / 2) * halfViewTangent * aspect;
    float upward = picturePlace.y / (canvasHeight / 2) * halfViewTangent;

    Formats::Vector3 ray = {forward.x + right.x * across + up.x * upward, forward.y + right.y * across + up.y * upward,
        forward.z + right.z * across + up.z * upward};
    if (ray.y >= 0)
        return {camera.eye.x, camera.eye.z};

    float distance = camera.eye.y / -ray.y;
    return {camera.eye.x + ray.x * distance, camera.eye.z + ray.z * distance};
}

std::optional<std::size_t> MiiPlaza::MiiAt(Formats::Vector2 picturePlace, float canvasWidth, float canvasHeight) const
{
    std::optional<std::size_t> found;
    float nearestZ = -FarDistance;
    for (const PlazaMii& plazaMii : miis)
    {
        float leanedTopY = PickHeight * LeanCosine;
        float leanedTopZ = plazaMii.position.y - PickHeight * LeanCosine;
        Formats::Vector2 feet = PlaceInPicture({plazaMii.position.x, 0, plazaMii.position.y}, canvasWidth, canvasHeight);
        Formats::Vector2 top = PlaceInPicture({plazaMii.position.x, leanedTopY, leanedTopZ}, canvasWidth, canvasHeight);
        Formats::Vector2 side = PlaceInPicture({plazaMii.position.x + PickHalfWidth, 0, plazaMii.position.y}, canvasWidth, canvasHeight);

        float halfWidth = side.x - feet.x;
        bool isAcross = std::abs(picturePlace.x - feet.x) <= halfWidth;
        bool isUpAndDown = picturePlace.y >= feet.y && picturePlace.y <= top.y;
        if (!isAcross || !isUpAndDown || plazaMii.position.y < nearestZ)
            continue;

        nearestZ = plazaMii.position.y;
        found = plazaMii.mii.slot;
    }
    return found;
}

float MiiPlaza::HalfWidth() const
{
    return isWide ? WideHalfWidth : StandardHalfWidth;
}

float MiiPlaza::NearestZ()
{
    return FrontLimit;
}

float MiiPlaza::FarthestZ()
{
    return BackLimit;
}

void MiiPlaza::MoveMii(PlazaMii& plazaMii)
{
    switch (plazaMii.activity)
    {
    case PlazaActivity::Held:
        return;

    case PlazaActivity::Waiting:
        plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + 1, WaitAnimationFrames);
        if (plazaMii.staysAtTarget)
            return;

        --plazaMii.stepsToWait;
        if (plazaMii.stepsToWait <= 0)
            StartWalking(plazaMii, RandomPlace());
        return;

    case PlazaActivity::Walking:
        Walk(plazaMii);
        return;
    }
}

void MiiPlaza::StartWaiting(PlazaMii& plazaMii)
{
    plazaMii.activity = PlazaActivity::Waiting;
    plazaMii.speed = 0;
    plazaMii.stepsToWait = RandomWait();
    plazaMii.animationFrame = 0;
}

void MiiPlaza::StartWalking(PlazaMii& plazaMii, Formats::Vector2 target)
{
    plazaMii.activity = PlazaActivity::Walking;
    plazaMii.target = target;
    plazaMii.animationFrame = 0;
}

void MiiPlaza::Walk(PlazaMii& plazaMii)
{
    float remaining = Distance(plazaMii.position, plazaMii.target);
    if (remaining < ArrivalDistance)
    {
        StartWaiting(plazaMii);
        if (plazaMii.staysAtTarget)
            plazaMii.facingDegrees = 0;
        return;
    }

    float wantedFacing = std::atan2(plazaMii.target.x - plazaMii.position.x, plazaMii.target.y - plazaMii.position.y) * DegreesPerRadian;
    float turn = std::clamp(TurnBetween(plazaMii.facingDegrees, wantedFacing), -TurnDegreesPerStep, TurnDegreesPerStep);
    plazaMii.facingDegrees += turn;

    plazaMii.speed = plazaMii.speed * SpeedKeptPerStep + SpeedAddedPerStep;
    float step = std::min(plazaMii.speed, remaining);
    float facingRadians = plazaMii.facingDegrees / DegreesPerRadian;
    plazaMii.position.x += std::sin(facingRadians) * step;
    plazaMii.position.y += std::cos(facingRadians) * step;

    float frames = WalkAnimationRate(plazaMii.speed, plazaMii.mii.record);
    plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + frames, WalkAnimationFrames);
}

// Each of two Miis that stand too close moves away from the other by half of what is missing.
void MiiPlaza::PushApart()
{
    for (std::size_t first = 0; first < miis.size(); ++first)
    {
        for (std::size_t second = first + 1; second < miis.size(); ++second)
        {
            PlazaMii& one = miis[first];
            PlazaMii& other = miis[second];
            if (one.activity == PlazaActivity::Held || other.activity == PlazaActivity::Held)
                continue;

            float distance = Distance(one.position, other.position);
            if (distance >= PersonalDistance)
                continue;

            Formats::Vector2 away = {other.position.x - one.position.x, other.position.y - one.position.y};
            if (distance < 1)
            {
                away = {1, 0};
                distance = 1;
            }

            float push = (PersonalDistance - distance) / 2;
            one.position.x -= away.x / distance * push;
            one.position.y -= away.y / distance * push;
            other.position.x += away.x / distance * push;
            other.position.y += away.y / distance * push;
        }
    }
}

void MiiPlaza::KeepInside(PlazaMii& plazaMii)
{
    if (plazaMii.activity == PlazaActivity::Held)
        return;

    plazaMii.position.x = std::clamp(plazaMii.position.x, -HalfWidth(), HalfWidth());
    plazaMii.position.y = std::clamp(plazaMii.position.y, BackLimit, FrontLimit);
}

Formats::Vector2 MiiPlaza::RandomPlace()
{
    std::uniform_real_distribution<float> across(-HalfWidth(), HalfWidth());
    std::uniform_real_distribution<float> depth(BackLimit, FrontLimit);
    return {across(random), depth(random)};
}

int MiiPlaza::RandomWait()
{
    return std::uniform_int_distribution<int>(ShortestWaitSteps, LongestWaitSteps)(random);
}

void MiiPlaza::LimitCameraDestination()
{
    if (camera.zoomLevel == FarthestZoomLevel)
    {
        camera.destination = FarthestEye;
        return;
    }

    float height = camera.destination.y;
    float openness = 1.0f - height / FarthestEye.y;
    float sideLimit = (isWide ? WideCameraSideLimit : StandardCameraSideLimit) * openness;
    float middleZ = CameraCenterFactor * height * (ViewDirection.z / ViewDirection.y);
    float depthLimit = CameraDepthLimit * openness;

    camera.destination.x = std::clamp(camera.destination.x, -sideLimit, sideLimit);
    camera.destination.z = std::clamp(camera.destination.z, middleZ - depthLimit, middleZ + depthLimit);
}

}
