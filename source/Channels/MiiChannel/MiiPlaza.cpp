#include "Channels/MiiChannel/MiiPlaza.h"

#include "Mii/MiiBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace Channels
{

namespace
{

// The plaza of the 16:9 picture is 2 x 3050 units wide (2 x 2200 in the 4:3 picture, which WeeOS does not show) and reaches from 1850
// units behind the middle to 2050 units in front of it: 6300 x 0.5 - 100 across, 4100 x 0.5 in front and 200 less behind (80026CD8,
// 803BE164 to 803BE16C, 803BE5C0 to 803BE5C8).
constexpr float WideHalfWidth = 3050.0f;
constexpr float FrontLimit = 2050.0f;
constexpr float BackLimit = -1850.0f;

// The Mii Parade (800258d4, 800260b4, 8002548c; CHN-84): at most 100 Miis walk at a time (75 in 4:3). Each comes in behind the last one,
// at least at z -700 (just above the picture), from 1 unit nearer to 9 units farther, at a random place within 720 units of the middle of
// the road (540 in 4:3), and walks to z 700 (below the picture); a place within 90 units of a Mii less than 120 in front of it is tried
// again, up to 20 times. One in a hundred walks up to 100 units to the side on its way. At the start the Miis come in one at a time,
// every 7 steps (4 when they run); once every Mii of the parade has walked, the road stays empty for 240 steps and the parade starts again.
constexpr std::size_t ParadeBodies = 100;
constexpr float ParadeHalfWidth = 720.0f;
constexpr float ParadeStartZ = -700.0f;
constexpr float ParadeEndZ = 700.0f;
constexpr float ParadeNearestGap = -1.0f;
constexpr float ParadeFarthestGap = 9.0f;
constexpr float ParadeCrowdDepth = 120.0f;
constexpr float ParadeCrowdDistance = 90.0f;
constexpr int ParadePlaceTries = 20;
constexpr int ParadeSwerveChances = 100;
constexpr float ParadeSwerveDistance = 100.0f;
constexpr int StepsBetweenWalkingArrivals = 7;
constexpr int StepsBetweenRunningArrivals = 4;
constexpr int StepsOfTheEmptyRoad = 240;

// The parade walks a little slower than the plaza and runs far slower: its speed grows by 0.095 a step walking (1.9 at most) and 0.375
// running (3.75 at most), in the states 1 and 2 of 8001f7a8 for a Mii of the parade.
constexpr float ParadeSpeedAddedPerStep = 0.095f;
constexpr float ParadeRunSpeedAddedPerStep = 0.375f;

// Two Miis closer than this push each other apart (the 120 x 120 of 803BE5DC).
constexpr float PersonalDistance = 120.0f;

// Walking (8001F7A8, state 1 of a plaza Mii): every step the speed falls to 95 percent and grows by 0.1, so it settles at 2 units a
// step; a Mii closer than 5 units to its target has arrived.
constexpr float SpeedKeptPerStep = 0.95f;
constexpr float SpeedAddedPerStep = 0.1f;
constexpr float ArrivalDistance = 5.0f;

// Running (state 2): the speed falls to 90 percent and grows by 1.5 every step (15 units a step at most); 10 units from its target
// a running Mii has arrived.
constexpr float RunSpeedKeptPerStep = 0.9f;
constexpr float RunSpeedAddedPerStep = 1.5f;
constexpr float RunArrivalDistance = 10.0f;

// The walk animation runs 0.6 / length of the body frames per unit of distance (80022BD0); the run animation the same.
constexpr float WalkFramesPerUnit = 0.6f;

// The lengths of the animations of Character.szs, in frames (AnimationFramesOf; MiiChannelTest compares them with the file).
constexpr float WalkAnimationFrames = 40.0f;
constexpr float RunAnimationFrames = 40.0f;
constexpr float WaitAnimationFrames = 120.0f;
constexpr float HeldAnimationFrames = 12.0f;
constexpr float TalkAnimationFrames = 90.0f;
constexpr float AnswerAnimationFrames = 180.0f;
constexpr float GreetingAnimationFrames = 96.0f;
constexpr float NapAnimationFrames = 360.0f;
constexpr float WakeUpAnimationFrames = 136.0f;
constexpr float SneezeAnimationFrames = 121.0f;
constexpr float AfterSneezeAnimationFrames = 51.0f;
constexpr float SlipAnimationFrames = 56.0f;
constexpr float GetUpAnimationFrames = 69.0f;
constexpr float GetDownAnimationFrames = 34.0f;
constexpr float LandingAnimationFrames = 96.0f;
constexpr float LandingFromHighAnimationFrames = 135.0f;
constexpr float GetUpFromLanding01Frames = 116.0f;
constexpr float GetUpFromLanding02Frames = 116.0f;
constexpr float GetUpFromLanding04Frames = 105.0f;
constexpr float GetUpFromLanding05Frames = 131.0f;

// The frames at which a falling Mii hits the ground (the animation events of 0x80022584, the floats 803BE420 to 803BE468): a smoke
// comes a frame before each hit, the third one larger; the sounds come with the hits. bodySlap_03 falls from higher and hits later. The
// program names a fourth hit at 120 (159), after the end of the animation, so it never comes. A Mii that gets down raises a smoke at
// frame 5.
struct GroundHits
{
    std::array<float, 3> smokeFrames;
    std::array<float, 3> hitFrames;
};

constexpr GroundHits LandingHits = {{29, 58, 79}, {30, 59, 80}};
constexpr GroundHits LandingFromHighHits = {{69, 97, 118}, {70, 98, 119}};
constexpr float GetDownSmokeFrame = 5;

// The camera is high from this zoom level on, and a Mii falls in from higher (0x8001F4D4).
constexpr int HighZoomLevel = 2;

// The three ways to get up after a landing; the second one is bodySlap_02 for a woman and bodySlap_04 for a man (0x8001F7A8 state 7).
constexpr int WaysUp = 3;

// The idle moments (8002B840): every step a Mii that stands or walks draws a number below 30000. Below 7 it talks to a Mii that
// stands or walks within 240 units in front of it (it looks for one within 600, 90 degrees to each side, among a third of the Miis);
// 23 starts a nap of 6 to 17 seconds when it stands, 24 a sneeze.
constexpr int IdleChances = 30000;
constexpr int TalkChances = 7;
constexpr int TalkSearchShare = 3;
constexpr int NapChance = 23;
constexpr int SneezeChance = 24;
constexpr float TalkSearchDistance = 600.0f;
constexpr float TalkDistance = 240.0f;
constexpr float TalkSearchDegrees = 90.0f;
constexpr int ShortestNapSeconds = 6;
constexpr int NapSecondsRange = 12;
constexpr int StepsPerSecond = 60;

// The moments of the effects (EffectManager.cpp): a talker lets out its first balloon at once and the answerer counts from 150; then a
// balloon comes after 100 steps with a chance of one in 100 a step, and surely after 200 (8001F7A8 states 4 and 5). A running Mii
// raises dust after (number of Miis + 50) / 2 steps with a chance of one in as many a step. The slip and the sneeze show at the frames
// that the original's animation events name (80022584).
constexpr int AnswerTalkBalloonStart = 150;
constexpr int TalkBalloonShortestWait = 100;
constexpr int TalkBalloonChances = 100;
constexpr int TalkBalloonLongestWait = 200;
constexpr int RunningDustBaseSteps = 50;
constexpr float SlipDustFrame = 27;
constexpr float SneezeSplashFrame = 13;

// A talk goes on 140 to 160 steps before the two greet (800213F0 with 140 and 160).
constexpr int ShortestTalkSteps = 140;
constexpr int LongestTalkSteps = 160;

// A running Mii slips with a chance of one in 7200 every step while it is more than 200 units from its place (8002AAA8); it slides on
// with its speed falling to 94 percent every step (8001F7A8, state 10).
constexpr int SlipChances = 7200;
constexpr float SlipFreeDistance = 200.0f;
constexpr float SlideSpeedKeptPerStep = 0.94f;

// The talking mouth opens and closes every 8 steps (Assumed, CHN-82).
constexpr int MouthSteps = 8;

// Assumed (CHN-63): how long a Mii stands and how fast it turns; the rules that choose them were not traced.
constexpr int ShortestWaitSteps = 120;
constexpr int LongestWaitSteps = 600;
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

// The camera's limits (8001295C): at the height y it may move 3150 in 16:9 (2300 in 4:3) x (1 - y / 12411.05) to the sides, and 2050 x
// (that same fraction) in front of and behind 1.02 x 0.6 y.
constexpr float WideCameraSideLimit = 3150.0f;
constexpr float CameraDepthLimit = 2050.0f;
constexpr float CameraCenterFactor = 1.02f;

// The view (80014BE4): 14 degrees high, from 10 to 100000 units.
constexpr float VerticalViewDegrees = 14.0f;
constexpr float NearDistance = 10.0f;
constexpr float FarDistance = 100000.0f;
constexpr float WideAspect = 16.0f / 9.0f;

// The D-pad moves the camera 15 units a step (8001295C). The eye glides a tenth of the way to its destination every step (Assumed, CHN-62).
constexpr float ScrollPerStep = 15.0f;
constexpr float GlidePerStep = 0.1f;

// Where a hand finds a Mii: the body from the feet to the top of the head, about 160 units, and 45 units to each side.
constexpr float PickHeight = 160.0f;
constexpr float PickHalfWidth = 45.0f;

constexpr float DegreesPerRadian = 180.0f / std::numbers::pi_v<float>;
constexpr float HalfTurnDegrees = 180.0f;
constexpr float FullTurnDegrees = 360.0f;

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
    float difference = std::fmod(toDegrees - fromDegrees, FullTurnDegrees);
    if (difference > HalfTurnDegrees)
        difference -= FullTurnDegrees;
    if (difference < -HalfTurnDegrees)
        difference += FullTurnDegrees;
    return difference;
}

// One chance in this many.
bool OneChanceIn(int chances, std::mt19937& generator)
{
    return std::uniform_int_distribution<int>(0, chances - 1)(generator) == 0;
}

// The point on the ground that the eye looks at, along the view direction.
Formats::Vector3 GroundPointUnder(Formats::Vector3 eye)
{
    Formats::Vector3 direction = Normalized(ViewDirection);
    float distance = eye.y / -direction.y;
    return {eye.x + direction.x * distance, 0.0f, eye.z + direction.z * distance};
}

// The camera's eye at a height, looking at the same point of the ground as from where it is.
Formats::Vector3 EyeAtHeight(Formats::Vector3 eye, float height)
{
    Formats::Vector3 lookedAt = GroundPointUnder(eye);
    Formats::Vector3 direction = Normalized(ViewDirection);
    float distance = height / -direction.y;
    return {lookedAt.x - direction.x * distance, height, lookedAt.z - direction.z * distance};
}

// A place of the whistle's lines: each line centered across the plaza, the lines one behind the other.
Formats::Vector2 LinePlace(std::size_t place, std::size_t miiCount)
{
    int line = static_cast<int>(place) / MiisPerLine;
    int column = static_cast<int>(place) % MiisPerLine;
    int miisInLine = std::min<int>(MiisPerLine, static_cast<int>(miiCount) - line * MiisPerLine);
    float middleColumn = static_cast<float>(miisInLine - 1) / 2;
    float x = (static_cast<float>(column) - middleColumn) * LineSpacing;
    float z = FirstLineZ - static_cast<float>(line) * LineSpacing;
    return {x, z};
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

MiiPlaza::MiiPlaza(std::uint32_t randomSeed, PlazaKind plazaKind)
    : kind(plazaKind)
    , random(randomSeed)
    , effectRandom(randomSeed)
{
    camera.eye = StartingEye;
    camera.destination = StartingEye;
}

void MiiPlaza::SetMiis(const std::vector<StoredMii>& storedMiis)
{
    miis.clear();
    heldSlot.reset();
    if (kind == PlazaKind::Parade)
    {
        StartParade(storedMiis);
        return;
    }

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
    slipsInLastStep = 0;
    if (kind == PlazaKind::Parade)
        KeepTheParadeComing();

    for (PlazaMii& plazaMii : miis)
        ++plazaMii.stepsInActivity;
    for (PlazaMii& plazaMii : miis)
        MoveMii(plazaMii);
    std::erase_if(miis, [](const PlazaMii& plazaMii) { return plazaMii.leavesTheRoad; });

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
        for (PlazaMii& plazaMii : miis)
        {
            if (plazaMii.mii.slot != order[place] || plazaMii.activity == PlazaActivity::Held)
                continue;

            plazaMii.staysAtTarget = true;
            StartRunning(plazaMii, LinePlace(place, order.size()));
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
            StartActivity(plazaMii, PlazaActivity::GettingDown);
    }
    heldSlot.reset();
}

void MiiPlaza::DropIn(std::size_t slot)
{
    for (PlazaMii& plazaMii : miis)
    {
        if (plazaMii.mii.slot != slot)
            continue;

        bool isTheCameraHigh = camera.zoomLevel >= HighZoomLevel;
        StartActivity(plazaMii, isTheCameraHigh ? PlazaActivity::LandingFromHigh : PlazaActivity::Landing);
        plazaMii.position = {0, 0};
        plazaMii.facingDegrees = 0;
        plazaMii.speed = 0;
        plazaMii.staysAtTarget = false;
    }
}

std::optional<std::size_t> MiiPlaza::HeldMii() const
{
    return heldSlot;
}

void MiiPlaza::SetParadeRunning(bool isRunning)
{
    isParadeRunning = isRunning;
    for (PlazaMii& plazaMii : miis)
    {
        if (isRunning && plazaMii.activity == PlazaActivity::Walking)
            plazaMii.activity = PlazaActivity::Running;
        else if (!isRunning && plazaMii.activity == PlazaActivity::Running)
            plazaMii.activity = PlazaActivity::Walking;
    }
}

bool MiiPlaza::IsParadeRunning() const
{
    return isParadeRunning;
}

int MiiPlaza::SlipsInLastStep() const
{
    return slipsInLastStep;
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
    camera.destination = EyeAtHeight(camera.destination, ZoomHeights[camera.zoomLevel]);
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

    camera.destination = EyeAtHeight(camera.destination, ZoomHeights[camera.zoomLevel]);
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
    return Graphics::MakePerspective(VerticalViewDegrees, WideAspect, NearDistance, FarDistance);
}

Formats::Vector2 MiiPlaza::PlaceInPicture(Formats::Vector3 point, float canvasWidth, float canvasHeight) const
{
    Graphics::Matrix4 viewAndProjection = ProjectionMatrix() * ViewMatrix();
    const auto& rows = viewAndProjection.rows;
    float x = rows[0][0] * point.x + rows[0][1] * point.y + rows[0][2] * point.z + rows[0][3];
    float y = rows[1][0] * point.x + rows[1][1] * point.y + rows[1][2] * point.z + rows[1][3];
    float depth = rows[3][0] * point.x + rows[3][1] * point.y + rows[3][2] * point.z + rows[3][3];
    return {x / depth * canvasWidth / 2, y / depth * canvasHeight / 2};
}

Formats::Vector2 MiiPlaza::GroundPointAt(Formats::Vector2 picturePlace, float canvasWidth, float canvasHeight) const
{
    Formats::Vector3 forward = Normalized(ViewDirection);
    Formats::Vector3 right = Normalized(Cross(forward, {0, 1, 0}));
    Formats::Vector3 up = Cross(right, forward);

    float halfViewTangent = std::tan(VerticalViewDegrees / 2 / DegreesPerRadian);
    float across = picturePlace.x / (canvasWidth / 2) * halfViewTangent * WideAspect;
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
        float leanedTopY = PickHeight * MiiLeanCosine;
        float leanedTopZ = plazaMii.position.y - PickHeight * MiiLeanCosine;
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
    if (kind == PlazaKind::Parade)
        return ParadeHalfWidth;

    return WideHalfWidth;
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
        plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + 1, HeldAnimationFrames);
        return;

    case PlazaActivity::Waiting:
        Wait(plazaMii);
        return;

    case PlazaActivity::Walking:
        Walk(plazaMii);
        if (plazaMii.activity == PlazaActivity::Walking)
            ChooseIdleMoment(plazaMii);
        return;

    case PlazaActivity::Running:
        Run(plazaMii);
        CountRunningDust(plazaMii);
        return;

    case PlazaActivity::Talking:
    case PlazaActivity::Answering:
        CountTalkBalloon(plazaMii);
        PlayForSteps(plazaMii, PlazaActivity::Greeting);
        return;

    case PlazaActivity::Greeting:
        PlayOnce(plazaMii, PlazaActivity::Waiting);
        return;

    case PlazaActivity::Napping:
        PlayForSteps(plazaMii, PlazaActivity::WakingUp);
        return;

    case PlazaActivity::WakingUp:
        PlayOnce(plazaMii, PlazaActivity::Waiting);
        return;

    case PlazaActivity::Sneezing:
        PlayOnce(plazaMii, PlazaActivity::AfterSneeze);
        return;

    case PlazaActivity::AfterSneeze:
        PlayOnceWithMoment(plazaMii, PlazaActivity::Waiting, SneezeSplashFrame, PlazaMoment::Sneezes);
        return;

    case PlazaActivity::Slipping:
        Slide(plazaMii);
        PlayOnceWithMoment(plazaMii, PlazaActivity::GettingUp, SlipDustFrame, PlazaMoment::Slips);
        return;

    case PlazaActivity::GettingUp:
        PlayOnce(plazaMii, PlazaActivity::Waiting);
        return;

    case PlazaActivity::GettingDown:
        PlayOnceWithMoment(plazaMii, PlazaActivity::Waiting, GetDownSmokeFrame, PlazaMoment::RaisesSlapSmoke);
        return;

    case PlazaActivity::Landing:
    case PlazaActivity::LandingFromHigh:
        Land(plazaMii);
        return;

    case PlazaActivity::GettingUpFromLanding01:
    case PlazaActivity::GettingUpFromLanding02:
    case PlazaActivity::GettingUpFromLanding04:
    case PlazaActivity::GettingUpFromLanding05:
        PlayOnce(plazaMii, PlazaActivity::Waiting);
        return;
    }
}

// A standing Mii walks on when its wait is over (on the parade toward the camera); a Mii that the whistle called stays.
void MiiPlaza::Wait(PlazaMii& plazaMii)
{
    plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + 1, WaitAnimationFrames);
    --plazaMii.stepsToWait;
    bool isWaitOver = plazaMii.stepsToWait <= 0;
    if (kind == PlazaKind::Parade && isWaitOver)
        WalkAlongTheParade(plazaMii, plazaMii.position.x);
    else if (!plazaMii.staysAtTarget && isWaitOver)
        StartWalking(plazaMii, RandomPlace());
    else
        ChooseIdleMoment(plazaMii);
}

// The falling Mii's smokes and hits come at their frames; at the end it gets up.
void MiiPlaza::Land(PlazaMii& plazaMii)
{
    const GroundHits& hits = plazaMii.activity == PlazaActivity::Landing ? LandingHits : LandingFromHighHits;
    float frameBefore = plazaMii.animationFrame;
    plazaMii.animationFrame += 1;

    NoteFrameMoment(plazaMii, frameBefore, hits.smokeFrames[0], PlazaMoment::RaisesSlapSmoke);
    NoteFrameMoment(plazaMii, frameBefore, hits.smokeFrames[1], PlazaMoment::RaisesSlapSmoke);
    NoteFrameMoment(plazaMii, frameBefore, hits.smokeFrames[2], PlazaMoment::RaisesLastSlapSmoke);
    NoteFrameMoment(plazaMii, frameBefore, hits.hitFrames[0], PlazaMoment::HitsTheGround);
    for (std::size_t bounce = 1; bounce < hits.hitFrames.size(); ++bounce)
        NoteFrameMoment(plazaMii, frameBefore, hits.hitFrames[bounce], PlazaMoment::BouncesOnTheGround);

    if (plazaMii.animationFrame >= AnimationFramesOf(plazaMii.activity))
        GetUpFromLanding(plazaMii);
}

// The first Mii gets up in any of the three ways, every later one in one of the two others than the Mii before (0x8001F7A8 state 7).
void MiiPlaza::GetUpFromLanding(PlazaMii& plazaMii)
{
    int way = 0;
    if (!lastWayUp)
    {
        way = std::uniform_int_distribution<int>(0, WaysUp - 1)(random);
    }
    else
    {
        int waysAfterTheLast = 1 + std::uniform_int_distribution<int>(0, WaysUp - 2)(random);
        way = (*lastWayUp + waysAfterTheLast) % WaysUp;
    }
    lastWayUp = way;

    constexpr int SecondWay = 1;
    constexpr int ThirdWay = 2;
    bool isWoman = plazaMii.mii.record.gender == Mii::Gender::Female;
    PlazaActivity wayUp = PlazaActivity::GettingUpFromLanding01;
    if (way == SecondWay)
        wayUp = isWoman ? PlazaActivity::GettingUpFromLanding02 : PlazaActivity::GettingUpFromLanding04;
    if (way == ThirdWay)
        wayUp = PlazaActivity::GettingUpFromLanding05;
    StartActivity(plazaMii, wayUp);
}

void MiiPlaza::StartWaiting(PlazaMii& plazaMii)
{
    StartActivity(plazaMii, PlazaActivity::Waiting);
    plazaMii.speed = 0;
    plazaMii.stepsToWait = RandomWait();
}

void MiiPlaza::StartWalking(PlazaMii& plazaMii, Formats::Vector2 target)
{
    StartActivity(plazaMii, PlazaActivity::Walking);
    plazaMii.target = target;
}

void MiiPlaza::StartRunning(PlazaMii& plazaMii, Formats::Vector2 target)
{
    StartActivity(plazaMii, PlazaActivity::Running);
    plazaMii.target = target;
    plazaMii.stepsRunning = 0;
}

void MiiPlaza::StartActivity(PlazaMii& plazaMii, PlazaActivity activity)
{
    plazaMii.talkPartner.reset();
    plazaMii.activity = activity;
    plazaMii.animationFrame = 0;
    plazaMii.stepsInActivity = 0;
}

void MiiPlaza::Walk(PlazaMii& plazaMii)
{
    if (plazaMii.talkPartner)
    {
        WalkToTheTalkPartner(plazaMii);
        if (plazaMii.activity != PlazaActivity::Walking)
            return;
    }

    float remaining = Distance(plazaMii.position, plazaMii.target);
    if (remaining < ArrivalDistance && kind == PlazaKind::Parade)
    {
        ReturnToTheFarEnd(plazaMii);
        return;
    }
    if (remaining < ArrivalDistance)
    {
        StartWaiting(plazaMii);
        if (plazaMii.staysAtTarget)
            plazaMii.facingDegrees = 0;
        return;
    }

    float speedAdded = kind == PlazaKind::Parade ? ParadeSpeedAddedPerStep : SpeedAddedPerStep;
    TurnAndStep(plazaMii, SpeedKeptPerStep, speedAdded, remaining);
    float frames = WalkAnimationRate(plazaMii.speed, plazaMii.mii.record);
    plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + frames, WalkAnimationFrames);
}

void MiiPlaza::Run(PlazaMii& plazaMii)
{
    float remaining = Distance(plazaMii.position, plazaMii.target);
    if (remaining < RunArrivalDistance && kind == PlazaKind::Parade)
    {
        ReturnToTheFarEnd(plazaMii);
        return;
    }
    if (remaining < RunArrivalDistance)
    {
        StartWaiting(plazaMii);
        plazaMii.facingDegrees = 0;
        return;
    }

    bool slips = remaining > SlipFreeDistance && OneChanceIn(SlipChances, random);
    if (slips)
    {
        StartActivity(plazaMii, PlazaActivity::Slipping);
        ++slipsInLastStep;
        return;
    }

    float speedAdded = kind == PlazaKind::Parade ? ParadeRunSpeedAddedPerStep : RunSpeedAddedPerStep;
    TurnAndStep(plazaMii, RunSpeedKeptPerStep, speedAdded, remaining);
    float frames = WalkAnimationRate(plazaMii.speed, plazaMii.mii.record);
    plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + frames, RunAnimationFrames);
}

// The Mii turns a little toward its target and moves on at a speed that settles where what it keeps and what it adds balance.
void MiiPlaza::TurnAndStep(PlazaMii& plazaMii, float speedKept, float speedAdded, float remaining)
{
    float wantedFacing = std::atan2(plazaMii.target.x - plazaMii.position.x, plazaMii.target.y - plazaMii.position.y) * DegreesPerRadian;
    float turn = std::clamp(TurnBetween(plazaMii.facingDegrees, wantedFacing), -TurnDegreesPerStep, TurnDegreesPerStep);
    plazaMii.facingDegrees += turn;

    plazaMii.speed = plazaMii.speed * speedKept + speedAdded;
    float step = std::min(plazaMii.speed, remaining);
    float facingRadians = plazaMii.facingDegrees / DegreesPerRadian;
    plazaMii.position.x += std::sin(facingRadians) * step;
    plazaMii.position.y += std::cos(facingRadians) * step;
}

// A Mii that slipped slides on in the direction it ran, slower every step.
void MiiPlaza::Slide(PlazaMii& plazaMii)
{
    plazaMii.speed *= SlideSpeedKeptPerStep;
    float facingRadians = plazaMii.facingDegrees / DegreesPerRadian;
    plazaMii.position.x += std::sin(facingRadians) * plazaMii.speed;
    plazaMii.position.y += std::cos(facingRadians) * plazaMii.speed;
}

// The animation plays to its end, then the next activity begins.
void MiiPlaza::PlayOnce(PlazaMii& plazaMii, PlazaActivity nextActivity)
{
    plazaMii.animationFrame += 1;
    if (plazaMii.animationFrame < AnimationFramesOf(plazaMii.activity))
        return;

    if (nextActivity == PlazaActivity::Waiting)
        FinishActivity(plazaMii);
    else
        StartActivity(plazaMii, nextActivity);
}

// The animation plays to its end, and a moment at one of its frames comes in the step that passes the frame.
void MiiPlaza::PlayOnceWithMoment(PlazaMii& plazaMii, PlazaActivity nextActivity, float momentFrame, PlazaMoment moment)
{
    float frameBefore = plazaMii.animationFrame;
    PlayOnce(plazaMii, nextActivity);
    NoteFrameMoment(plazaMii, frameBefore, momentFrame, moment);
}

// The animation loops until the steps of the activity are over, then the next activity begins.
void MiiPlaza::PlayForSteps(PlazaMii& plazaMii, PlazaActivity nextActivity)
{
    plazaMii.animationFrame = std::fmod(plazaMii.animationFrame + 1, AnimationFramesOf(plazaMii.activity));
    if (plazaMii.stepsInActivity >= plazaMii.stepsOfActivity)
        StartActivity(plazaMii, nextActivity);
}

// After an idle moment a Mii stands; a Mii that the whistle called runs on to its place.
void MiiPlaza::FinishActivity(PlazaMii& plazaMii)
{
    bool isOnItsWay = plazaMii.staysAtTarget && Distance(plazaMii.position, plazaMii.target) >= RunArrivalDistance;
    if (isOnItsWay)
    {
        StartRunning(plazaMii, plazaMii.target);
        return;
    }
    StartWaiting(plazaMii);
}

void MiiPlaza::ChooseIdleMoment(PlazaMii& plazaMii)
{
    if (plazaMii.talkPartner)
        return;

    int chance = std::uniform_int_distribution<int>(0, IdleChances - 1)(random);
    if (chance < TalkChances && !plazaMii.staysAtTarget)
    {
        StartTalk(plazaMii);
        return;
    }

    if (chance == NapChance && plazaMii.activity == PlazaActivity::Waiting)
    {
        StartActivity(plazaMii, PlazaActivity::Napping);
        int napSeconds = ShortestNapSeconds + std::uniform_int_distribution<int>(0, NapSecondsRange - 1)(random);
        plazaMii.stepsOfActivity = napSeconds * StepsPerSecond;
        moments.push_back({PlazaMoment::FallsAsleep, plazaMii.mii.slot});
        return;
    }

    if (chance == SneezeChance)
        StartActivity(plazaMii, PlazaActivity::Sneezing);
}

// The talker looks for a Mii in front of it among a third of the Miis; when the one it finds is near, the two turn to each other
// and talk, else the talker walks to it first (8002B840 marks it with the partner, 0x728 = 3). Miis that the whistle lined up keep
// their places (Assumed, CHN-81).
void MiiPlaza::StartTalk(PlazaMii& talker)
{
    if (miis.size() < 2)
        return;

    int tries = static_cast<int>(miis.size()) / TalkSearchShare + 1;
    std::uniform_int_distribution<std::size_t> anyMii(0, miis.size() - 1);
    for (int tryNumber = 0; tryNumber < tries; ++tryNumber)
    {
        PlazaMii& listener = miis[anyMii(random)];
        bool isFree = listener.activity == PlazaActivity::Waiting || listener.activity == PlazaActivity::Walking;
        if (&listener == &talker || !isFree || listener.staysAtTarget)
            continue;

        float distance = Distance(talker.position, listener.position);
        float direction = std::atan2(listener.position.x - talker.position.x, listener.position.y - talker.position.y) * DegreesPerRadian;
        bool isInFront = std::abs(TurnBetween(talker.facingDegrees, direction)) < TalkSearchDegrees;
        if (distance >= TalkSearchDistance || !isInFront)
            continue;

        if (distance >= TalkDistance)
        {
            StartWalking(talker, listener.position);
            talker.talkPartner = listener.mii.slot;
            return;
        }

        BeginTalk(talker, listener);
        return;
    }
}

// The talker keeps walking toward its partner while the partner is free; near it, they talk (Assumed, CHN-81: the partner that gets
// busy on the way is given up and the talker walks on to where it saw it).
void MiiPlaza::WalkToTheTalkPartner(PlazaMii& talker)
{
    PlazaMii* partner = FindMiiInSlot(*talker.talkPartner);
    bool isFree = partner != nullptr && (partner->activity == PlazaActivity::Waiting || partner->activity == PlazaActivity::Walking);
    if (!isFree || partner->staysAtTarget)
    {
        talker.talkPartner.reset();
        return;
    }

    talker.target = partner->position;
    if (Distance(talker.position, partner->position) < TalkDistance)
        BeginTalk(talker, *partner);
}

PlazaMii* MiiPlaza::FindMiiInSlot(std::size_t slot)
{
    for (PlazaMii& plazaMii : miis)
    {
        if (plazaMii.mii.slot == slot)
            return &plazaMii;
    }
    return nullptr;
}

void MiiPlaza::BeginTalk(PlazaMii& talker, PlazaMii& listener)
{
    float direction = std::atan2(listener.position.x - talker.position.x, listener.position.y - talker.position.y) * DegreesPerRadian;
    int talkSteps = std::uniform_int_distribution<int>(ShortestTalkSteps, LongestTalkSteps)(random);
    StartActivity(talker, PlazaActivity::Talking);
    talker.facingDegrees = direction;
    talker.stepsOfActivity = talkSteps;
    talker.speed = 0;

    StartActivity(listener, PlazaActivity::Answering);
    listener.facingDegrees = direction + HalfTurnDegrees;
    listener.stepsOfActivity = talkSteps;
    listener.speed = 0;

    talker.stepsSinceTalkBalloon = 0;
    listener.stepsSinceTalkBalloon = AnswerTalkBalloonStart;
    moments.push_back({PlazaMoment::Talks, talker.mii.slot});
}

void MiiPlaza::CountTalkBalloon(PlazaMii& plazaMii)
{
    ++plazaMii.stepsSinceTalkBalloon;
    int waited = plazaMii.stepsSinceTalkBalloon;
    bool comesByChance = waited > TalkBalloonShortestWait && OneChanceIn(TalkBalloonChances, effectRandom);
    if (!comesByChance && waited <= TalkBalloonLongestWait)
        return;

    plazaMii.stepsSinceTalkBalloon = 0;
    moments.push_back({PlazaMoment::Talks, plazaMii.mii.slot});
}

void MiiPlaza::CountRunningDust(PlazaMii& plazaMii)
{
    if (plazaMii.activity != PlazaActivity::Running)
        return;

    ++plazaMii.stepsRunning;
    int chances = (static_cast<int>(miis.size()) + RunningDustBaseSteps) / 2;
    bool raisesDust = plazaMii.stepsRunning > chances && OneChanceIn(chances, effectRandom);
    if (raisesDust)
        moments.push_back({PlazaMoment::RaisesDust, plazaMii.mii.slot});
}

// A moment at a frame of an animation comes in the step in which the animation passes that frame.
void MiiPlaza::NoteFrameMoment(const PlazaMii& plazaMii, float frameBefore, float momentFrame, PlazaMoment moment)
{
    bool passesTheFrame = frameBefore < momentFrame && plazaMii.animationFrame >= momentFrame;
    if (passesTheFrame)
        moments.push_back({moment, plazaMii.mii.slot});
}

std::vector<PlazaMomentOfMii> MiiPlaza::TakeMoments()
{
    std::vector<PlazaMomentOfMii> taken = std::move(moments);
    moments.clear();
    return taken;
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

            WakeWhenBumped(one);
            WakeWhenBumped(other);

            float push = (PersonalDistance - distance) / 2;
            one.position.x -= away.x / distance * push;
            one.position.y -= away.y / distance * push;
            other.position.x += away.x / distance * push;
            other.position.y += away.y / distance * push;
        }
    }
}

// A Mii that is bumped in the first part of an idle moment goes on to its second part (80027A24).
void MiiPlaza::WakeWhenBumped(PlazaMii& plazaMii)
{
    if (plazaMii.activity == PlazaActivity::Napping)
        StartActivity(plazaMii, PlazaActivity::WakingUp);
    else if (plazaMii.activity == PlazaActivity::Sneezing)
        StartActivity(plazaMii, PlazaActivity::AfterSneeze);
    else if (plazaMii.activity == PlazaActivity::Slipping)
        StartActivity(plazaMii, PlazaActivity::GettingUp);
}

void MiiPlaza::KeepInside(PlazaMii& plazaMii)
{
    if (plazaMii.activity == PlazaActivity::Held)
        return;

    plazaMii.position.x = std::clamp(plazaMii.position.x, -HalfWidth(), HalfWidth());
    plazaMii.position.y = std::clamp(plazaMii.position.y, BackLimit, FrontLimit);
}

// A Mii of the parade walks (or runs, in fast forward) toward the camera, to the near end of the road.
void MiiPlaza::WalkAlongTheParade(PlazaMii& plazaMii, float targetX)
{
    Formats::Vector2 nearEnd = {targetX, ParadeEndZ};
    if (isParadeRunning)
        StartRunning(plazaMii, nearEnd);
    else
        StartWalking(plazaMii, nearEnd);
}

// The Miis of the parade walk in the order of the list, shuffled within each hundred (8002e43c), and come onto the road one at a time.
void MiiPlaza::StartParade(const std::vector<StoredMii>& paradeMiis)
{
    paradeOrder = paradeMiis;
    for (std::size_t blockStart = 0; blockStart < paradeOrder.size(); blockStart += ParadeBodies)
        ShuffleParadeBlock(blockStart);

    nextInParade = 0;
    miisStillComingIn = static_cast<int>(std::min(paradeOrder.size(), ParadeBodies));
    stepsSinceLastArrival = 0;
    stepsOfEmptyRoad = 0;
}

// A full hundred swaps two of its Miis a hundred times, the last smaller block as many times as it has Miis.
void MiiPlaza::ShuffleParadeBlock(std::size_t blockStart)
{
    std::size_t blockSize = std::min(ParadeBodies, paradeOrder.size() - blockStart);
    std::uniform_int_distribution<std::size_t> placeInBlock(0, blockSize - 1);
    for (std::size_t swap = 0; swap < blockSize; ++swap)
    {
        std::size_t first = blockStart + placeInBlock(random);
        std::size_t second = blockStart + placeInBlock(random);
        std::swap(paradeOrder[first], paradeOrder[second]);
    }
}

void MiiPlaza::KeepTheParadeComing()
{
    if (miisStillComingIn > 0)
    {
        LetTheNextMiiComeIn();
        return;
    }

    bool isTheParadeOver = miis.empty() && !paradeOrder.empty() && nextInParade >= paradeOrder.size();
    if (!isTheParadeOver)
        return;

    ++stepsOfEmptyRoad;
    if (stepsOfEmptyRoad >= StepsOfTheEmptyRoad)
        StartParade(paradeOrder);
}

void MiiPlaza::LetTheNextMiiComeIn()
{
    ++stepsSinceLastArrival;
    int stepsBetweenArrivals = isParadeRunning ? StepsBetweenRunningArrivals : StepsBetweenWalkingArrivals;
    if (stepsSinceLastArrival < stepsBetweenArrivals)
        return;

    stepsSinceLastArrival = 0;
    --miisStillComingIn;
    PlazaMii plazaMii;
    if (TakeTheNextParadeMii(plazaMii))
        miis.push_back(plazaMii);
}

// At the near end a Mii of the parade leaves the picture and the next Mii of the parade comes in at the back; when every Mii has walked,
// it leaves the road.
void MiiPlaza::ReturnToTheFarEnd(PlazaMii& plazaMii)
{
    if (!TakeTheNextParadeMii(plazaMii))
        plazaMii.leavesTheRoad = true;
}

bool MiiPlaza::TakeTheNextParadeMii(PlazaMii& plazaMii)
{
    if (nextInParade >= paradeOrder.size())
        return false;

    plazaMii.mii = paradeOrder[nextInParade];
    ++nextInParade;
    PlaceAtTheBackOfTheParade(plazaMii);
    return true;
}

void MiiPlaza::PlaceAtTheBackOfTheParade(PlazaMii& plazaMii)
{
    float lastZ = ParadeStartZ;
    for (const PlazaMii& other : miis)
    {
        if (&other != &plazaMii)
            lastZ = std::min(lastZ, other.position.y);
    }

    std::uniform_real_distribution<float> across(-ParadeHalfWidth, ParadeHalfWidth);
    std::uniform_real_distribution<float> gap(ParadeNearestGap, ParadeFarthestGap);
    Formats::Vector2 place = {across(random), lastZ - gap(random)};
    for (int tries = 1; tries < ParadePlaceTries; ++tries)
    {
        if (!IsCrowded(place, plazaMii))
            break;
        place = {across(random), lastZ - gap(random)};
    }

    plazaMii.position = place;
    plazaMii.facingDegrees = 0;

    float targetX = place.x;
    bool swerves = std::uniform_int_distribution<int>(0, ParadeSwerveChances - 1)(random) == 0;
    if (swerves)
        targetX += std::uniform_real_distribution<float>(-ParadeSwerveDistance, ParadeSwerveDistance)(random);
    WalkAlongTheParade(plazaMii, targetX);
}

// A place is crowded when it lies within reach of a Mii that walks less than a little way in front of it.
bool MiiPlaza::IsCrowded(Formats::Vector2 place, const PlazaMii& newcomer) const
{
    for (const PlazaMii& other : miis)
    {
        bool isClose = other.position.y < place.y + ParadeCrowdDepth && Distance(other.position, place) < ParadeCrowdDistance;
        if (&other != &newcomer && isClose)
            return true;
    }
    return false;
}

Formats::Vector2 MiiPlaza::RandomPlace()
{
    std::uniform_real_distribution<float> across(-HalfWidth(), HalfWidth());
    std::uniform_real_distribution<float> depth(BackLimit, FrontLimit);
    return {across(random), depth(random)};
}

int MiiPlaza::RandomWait()
{
    // A Mii of the parade that stands walks on at once (800258d4).
    if (kind == PlazaKind::Parade)
        return 0;

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
    float sideLimit = WideCameraSideLimit * openness;
    float middleZ = CameraCenterFactor * height * (ViewDirection.z / ViewDirection.y);
    float depthLimit = CameraDepthLimit * openness;

    camera.destination.x = std::clamp(camera.destination.x, -sideLimit, sideLimit);
    camera.destination.z = std::clamp(camera.destination.z, middleZ - depthLimit, middleZ + depthLimit);
}

std::string_view AnimationOf(PlazaActivity activity)
{
    switch (activity)
    {
    case PlazaActivity::Waiting:
        return "bodyWait";
    case PlazaActivity::Walking:
        return "bodyWalk";
    case PlazaActivity::Running:
        return "bodyRun";
    case PlazaActivity::Held:
        return "bodyWallow";
    case PlazaActivity::Talking:
        return "bodyTalk";
    case PlazaActivity::Answering:
        return "bodyTalk_01";
    case PlazaActivity::Greeting:
        return "bodyGreeting";
    case PlazaActivity::Napping:
        return "bodyCatnap_00";
    case PlazaActivity::WakingUp:
        return "bodyCatnap_01";
    case PlazaActivity::Sneezing:
        return "bodysnart_00";
    case PlazaActivity::AfterSneeze:
        return "bodysnart_01";
    case PlazaActivity::Slipping:
        return "bodySlip_00";
    case PlazaActivity::GettingUp:
        return "bodySlip_01";
    case PlazaActivity::GettingDown:
        return "bodyGetDown";
    case PlazaActivity::Landing:
        return "bodySlap_00";
    case PlazaActivity::LandingFromHigh:
        return "bodySlap_03";
    case PlazaActivity::GettingUpFromLanding01:
        return "bodySlap_01";
    case PlazaActivity::GettingUpFromLanding02:
        return "bodySlap_02";
    case PlazaActivity::GettingUpFromLanding04:
        return "bodySlap_04";
    case PlazaActivity::GettingUpFromLanding05:
        return "bodySlap_05";
    }
    return "bodyWait";
}

Mii::MiiExpression ExpressionOf(const PlazaMii& plazaMii)
{
    switch (plazaMii.activity)
    {
    case PlazaActivity::Napping:
    case PlazaActivity::Sneezing:
        return Mii::MiiExpression::Blink;

    case PlazaActivity::Talking:
    case PlazaActivity::Answering:
    {
        bool isMouthOpen = plazaMii.stepsInActivity / MouthSteps % 2 == 1;
        if (isMouthOpen)
            return Mii::MiiExpression::OpenMouth;
        return Mii::MiiExpression::Normal;
    }

    default:
        return Mii::MiiExpression::Normal;
    }
}

float AnimationFramesOf(PlazaActivity activity)
{
    switch (activity)
    {
    case PlazaActivity::Waiting:
        return WaitAnimationFrames;
    case PlazaActivity::Walking:
        return WalkAnimationFrames;
    case PlazaActivity::Running:
        return RunAnimationFrames;
    case PlazaActivity::Held:
        return HeldAnimationFrames;
    case PlazaActivity::Talking:
        return TalkAnimationFrames;
    case PlazaActivity::Answering:
        return AnswerAnimationFrames;
    case PlazaActivity::Greeting:
        return GreetingAnimationFrames;
    case PlazaActivity::Napping:
        return NapAnimationFrames;
    case PlazaActivity::WakingUp:
        return WakeUpAnimationFrames;
    case PlazaActivity::Sneezing:
        return SneezeAnimationFrames;
    case PlazaActivity::AfterSneeze:
        return AfterSneezeAnimationFrames;
    case PlazaActivity::Slipping:
        return SlipAnimationFrames;
    case PlazaActivity::GettingUp:
        return GetUpAnimationFrames;
    case PlazaActivity::GettingDown:
        return GetDownAnimationFrames;
    case PlazaActivity::Landing:
        return LandingAnimationFrames;
    case PlazaActivity::LandingFromHigh:
        return LandingFromHighAnimationFrames;
    case PlazaActivity::GettingUpFromLanding01:
        return GetUpFromLanding01Frames;
    case PlazaActivity::GettingUpFromLanding02:
        return GetUpFromLanding02Frames;
    case PlazaActivity::GettingUpFromLanding04:
        return GetUpFromLanding04Frames;
    case PlazaActivity::GettingUpFromLanding05:
        return GetUpFromLanding05Frames;
    }
    return WaitAnimationFrames;
}

}
