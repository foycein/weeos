#pragma once

#include "Channels/MiiChannel/MiiChannelData.h"

#include "Mii/MiiExpression.h"

#include "Formats/Vectors.h"
#include "Graphics/Matrix4.h"

#include <cstddef>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

namespace Channels
{

// What a Mii of the plaza is doing (the states of 8001F7A8), each with its animation of the body.
enum class PlazaActivity
{
    // Standing in bodyWait until its wait is over.
    Waiting,

    // Walking in bodyWalk toward its target.
    Walking,

    // Running in bodyRun to its place in a line of the whistle.
    Running,

    // Held by a hand above the plaza, kicking in bodyWallow.
    Held,

    // Two Miis that face each other: one talks (bodyTalk), the other answers (bodyTalk_01); then both greet (bodyGreeting).
    Talking,
    Answering,
    Greeting,

    // The idle moments: a nap (bodyCatnap_00, then bodyCatnap_01 to wake up), a sneeze (bodysnart_00, bodysnart_01) and a slip while
    // running (bodySlip_00 sliding on, bodySlip_01 getting up). A Mii that is bumped during the first part goes on to the second.
    Napping,
    WakingUp,
    Sneezing,
    AfterSneeze,
    Slipping,
    GettingUp,

    // A Mii that the hand lets go gets down onto the ground (bodyGetDown).
    GettingDown,

    // A Mii that comes into the plaza from the editor falls in from the sky and bounces on the ground: bodySlap_00, or bodySlap_03 when
    // the camera is high (zoom level 2 or 3). Then it gets up in one of the ways of bodySlap_01, _02 (a woman), _04 (a man) or _05.
    Landing,
    LandingFromHigh,
    GettingUpFromLanding01,
    GettingUpFromLanding02,
    GettingUpFromLanding04,
    GettingUpFromLanding05,
};

// The ground the Miis walk on: the plaza, where they wander, or the road of the Mii Parade, where they walk from the far end toward the
// camera and start again at the far end.
enum class PlazaKind
{
    Plaza,
    Parade,
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

    // The steps since the activity began, and how many a nap or a talk lasts.
    int stepsInActivity = 0;
    int stepsOfActivity = 0;

    // The frame of the animation the Mii plays now.
    float animationFrame = 0;

    // While a whistle has called the Mii to its place in a line: it walks there and stays.
    bool staysAtTarget = false;

    // A Mii that found a Mii to talk to further away walks to it, and they talk when it is near.
    std::optional<std::size_t> talkPartner;

    // The steps since the last balloon of a talk, and since the Mii began to run (for its dust).
    int stepsSinceTalkBalloon = 0;
    int stepsRunning = 0;

    // A Mii of the parade that walked to the near end when no Mii of the parade was left to come.
    bool leavesTheRoad = false;
};

// A moment of a plaza Mii that an effect shows (EffectManager.cpp of the original).
enum class PlazaMoment
{
    // A talking Mii lets out a balloon: at the start of a talk and now and then while it lasts.
    Talks,

    // A running Mii raises dust now and then.
    RaisesDust,

    // A running Mii slips and hits the ground (frame 27 of bodySlip_00).
    Slips,

    // A Mii sneezes (frame 13 of bodysnart_01).
    Sneezes,

    // A Mii falls asleep; its Zzz rise above it while it naps.
    FallsAsleep,

    // A Mii that falls in or gets down hits the ground: a puff of smoke (Nc_slapsmoke01), and at its last bounce a larger one
    // (Nc_slapsmoke02).
    RaisesSlapSmoke,
    RaisesLastSlapSmoke,

    // A falling Mii hits the ground the first time (SEQ_DOOL_THROW_GND1), and then bounces (SEQ_DOOL_THROW_GND2).
    HitsTheGround,
    BouncesOnTheGround,

    // A walking or running Mii puts its left or its right foot down (MiiFootsteps.h).
    StepsWithTheLeftFoot,
    StepsWithTheRightFoot,
};

struct PlazaMomentOfMii
{
    PlazaMoment moment;
    std::size_t slot;
};

// The camera of the plaza (8001295C, 80014DA4): it looks down along a fixed direction, and its height is one of four zoom levels. The
// parade has a camera of its own (the third of 800143FC, set up by 80014BE4 with mode 2) that stands where the plaza's starts and never
// moves: the parade neither zooms nor scrolls.
struct PlazaCamera
{
    Formats::Vector3 eye;

    // Where the camera moves to. The eye glides there a little every step.
    Formats::Vector3 destination;

    int zoomLevel = 1;
};

// The Mii Plaza: the Miis of the database walking and standing on the ground, the camera above them and the hand that grabs one.
// It is plain arithmetic, so the tests run it without a picture.
class MiiPlaza
{
public:
    // The lowest and the highest zoom level: + goes down, - goes up.
    static constexpr int ClosestZoomLevel = 0;
    static constexpr int FarthestZoomLevel = 3;

    // The Miis of the plaza lean back by 45 degrees toward the camera (aslope_root of every animation).
    static constexpr float MiiLeanCosine = 0.70710678f;

    // A Mii's middle in the picture: 80 units above its feet, leaning back with the Mii.
    static constexpr float MiiMiddleHeight = 80.0f * MiiLeanCosine;

    // The plaza of the 16:9 picture, which shows more of the ground than the 4:3 one, so the Miis may walk farther to the sides.
    explicit MiiPlaza(std::uint32_t randomSeed, PlazaKind plazaKind = PlazaKind::Plaza);

    // The parade's fast forward: its Miis run instead of walking.
    void SetParadeRunning(bool isRunning);
    bool IsParadeRunning() const;

    // The Miis come onto the plaza at random places, each standing for a while before it walks. The Miis of the parade come onto the road
    // one at a time, at most a hundred at once, and the others follow as those walk out of the picture.
    void SetMiis(const std::vector<StoredMii>& storedMiis);

    // A Mii that was edited keeps its place; a new one comes in at the middle of the plaza.
    void UpdateMii(const StoredMii& mii);
    void RemoveMii(std::size_t slot);

    // A Mii leaves the parade and the list of the parade, whose later places move one down; the parade goes on. The body of an erased Mii
    // takes the next Mii of the parade at the back (800257EC); the body of a Mii sent to the plaza stays away until the parade starts
    // again (8005B3F8).
    void EraseFromParade(std::size_t place);
    void SendFromParade(std::size_t place);

    const std::vector<PlazaMii>& Miis() const;
    const PlazaMii* FindMii(std::size_t slot) const;

    // One step: every Mii that is not held waits, turns or walks, the Miis push each other apart and stay inside the plaza, and the
    // camera glides toward its destination.
    void Update();

    // The whistle: the Miis walk to their places in lines, in the order the arrangement gives them; Disband lets them go again.
    void Arrange(PlazaArrangement arrangement);

    // The order of the slots in the lines of an arrangement.
    std::vector<std::size_t> ArrangedOrder(PlazaArrangement arrangement) const;

    // The number of Miis that slipped in the last step, for the sound of the fall.
    int SlipsInLastStep() const;

    // The moments of the Miis since the last call, for their effects.
    std::vector<PlazaMomentOfMii> TakeMoments();

    // The hand picks the Mii up and carries it until it lets go; then it gets down.
    void Grab(std::size_t slot);
    void MoveHeldMii(Formats::Vector2 groundPosition);
    void Drop();

    // A Mii that the editor saved falls into the middle of the plaza (0x8002AC54, 0x8001F4D4).
    void DropIn(std::size_t slot);
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

    // The point of the ground in the middle of the picture, straight ahead of the camera.
    Formats::Vector2 GroundPointAhead() const;

    // The Mii nearest the camera whose body covers the place of the picture, if one does.
    std::optional<std::size_t> MiiAt(Formats::Vector2 picturePlace, float canvasWidth, float canvasHeight) const;

    // The limits of the plaza.
    float HalfWidth() const;
    static float NearestZ();
    static float FarthestZ();

private:
    void MoveMii(PlazaMii& plazaMii);
    void Wait(PlazaMii& plazaMii);
    void StartWaiting(PlazaMii& plazaMii);
    void StartWalking(PlazaMii& plazaMii, Formats::Vector2 target);
    void StartRunning(PlazaMii& plazaMii, Formats::Vector2 target);
    void StartActivity(PlazaMii& plazaMii, PlazaActivity activity);
    void Walk(PlazaMii& plazaMii);
    void Run(PlazaMii& plazaMii);
    void Slide(PlazaMii& plazaMii);
    void PlayOnce(PlazaMii& plazaMii, PlazaActivity nextActivity);
    void PlayOnceWithMoment(PlazaMii& plazaMii, PlazaActivity nextActivity, float momentFrame, PlazaMoment moment);
    void PlayForSteps(PlazaMii& plazaMii, PlazaActivity nextActivity);
    void FinishActivity(PlazaMii& plazaMii);
    void ChooseIdleMoment(PlazaMii& plazaMii);
    void StartTalk(PlazaMii& talker);
    void BeginTalk(PlazaMii& talker, PlazaMii& listener);
    void WalkToTheTalkPartner(PlazaMii& talker);
    PlazaMii* FindMiiInSlot(std::size_t slot);
    void CountTalkBalloon(PlazaMii& plazaMii);
    void CountRunningDust(PlazaMii& plazaMii);
    void NoteFrameMoment(const PlazaMii& plazaMii, float frameBefore, float momentFrame, PlazaMoment moment);
    void NoteFootsteps(const PlazaMii& plazaMii, float frameBefore);
    void TurnAndStep(PlazaMii& plazaMii, float speedKept, float speedAdded, float remaining);
    void PushApart();
    void WakeWhenBumped(PlazaMii& plazaMii);
    void KeepInside(PlazaMii& plazaMii);
    Formats::Vector2 RandomPlace();
    void WalkAlongTheParade(PlazaMii& plazaMii, float targetX);
    void StartParade(const std::vector<StoredMii>& paradeMiis);
    void ShuffleParadeBlock(std::size_t blockStart);
    void KeepTheParadeComing();
    void LetTheNextMiiComeIn();
    void ReturnToTheFarEnd(PlazaMii& plazaMii);
    void LeaveTheParadeOrder(std::size_t place);
    void MovePlacesDownAfter(std::size_t place);
    bool TakeTheNextParadeMii(PlazaMii& plazaMii);
    void PlaceAtTheBackOfTheParade(PlazaMii& plazaMii);
    bool IsCrowded(Formats::Vector2 place, const PlazaMii& newcomer) const;
    void Land(PlazaMii& plazaMii);
    void GetUpFromLanding(PlazaMii& plazaMii);
    int RandomWait();
    void LimitCameraDestination();

    PlazaKind kind;
    bool isParadeRunning = false;

    // The Miis of the parade in the order they walk, the next of them to come, how many more come in one at a time at the start, and
    // the steps since the last came in and since the road is empty.
    std::vector<StoredMii> paradeOrder;
    std::size_t nextInParade = 0;
    int miisStillComingIn = 0;
    int stepsSinceLastArrival = 0;
    int stepsOfEmptyRoad = 0;
    std::mt19937 random;

    // The chances of the effects' moments have their own numbers, so that the effects do not change what the Miis do.
    std::mt19937 effectRandom;
    std::vector<PlazaMii> miis;
    std::optional<std::size_t> heldSlot;
    PlazaCamera camera;
    int slipsInLastStep = 0;
    std::vector<PlazaMomentOfMii> moments;

    // The way the last Mii got up after its landing; the next one gets up another way.
    std::optional<int> lastWayUp;
};

// The speed at which the walk animation plays for a Mii at a walking speed: a longer body takes longer steps (80022BD0).
float WalkAnimationRate(float speed, const Mii::MiiRecord& record);

// The animation of the body that a Mii plays in an activity, and its length in frames.
std::string_view AnimationOf(PlazaActivity activity);
float AnimationFramesOf(PlazaActivity activity);

// The face a Mii makes in its activity: eyes shut while it naps and sneezes, the mouth opening and closing while it talks.
Mii::MiiExpression ExpressionOf(const PlazaMii& plazaMii);

}
