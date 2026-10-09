#include "PlayerTemporalCore.hpp"
#include "PlayerSchedulerCore.hpp"
#include "PlayerMotionCore.hpp"
#include <cstdio>
#include <limits>
#include <type_traits>
using namespace PlayerTemporal;
static unsigned checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)
int main() {
    CHECK(ValidRate(20) && ValidRate(60) && ValidRate(120));
    CHECK(!ValidRate(0) && !ValidRate(30) && !ValidRate(119));
    CHECK(StepQuanta(SimulationRate::Hz20) == 6);
    CHECK(StepQuanta(SimulationRate::Hz60) == 2);
    CHECK(StepQuanta(SimulationRate::Hz120) == 1);
    CHECK(StepQuanta(static_cast<SimulationRate>(30)) == 0);
    CHECK(LegacyArithmetic(SimulationRate::Hz20) && !LegacyArithmetic(SimulationRate::Hz120));
    Capability cap;
    CHECK(cap.Request(120) && cap.EffectiveRate() == SimulationRate::Hz20 && !cap.HighRateAdmitted());
    CHECK(!cap.Request(30) && cap.requestedPlayerRate == SimulationRate::Hz120 && cap.WorldRate() == 20);
    SimTime end{9};
    CHECK(!Add({UINT64_MAX}, {1}, end) && end.quanta == 9);
    CHECK(Add({UINT64_MAX-6}, {6}, end) && end.quanta == UINT64_MAX);
    for (auto rate : {SimulationRate::Hz20, SimulationRate::Hz60, SimulationRate::Hz120}) {
        SimTime t{}; bool okay = true; uint64_t boundaries = 0;
        const uint64_t steps = 600000 / StepQuanta(rate);
        for (uint64_t i = 0; i < steps; ++i) {
            okay &= Add(t, {StepQuanta(rate)}, t);
            if (CommonBoundary(t)) ++boundaries;
        }
        CHECK(okay && t.quanta == 600000 && boundaries == 100000);
    }
    Countdown timer;
    CHECK(!timer.Advance({1})); timer.Reset({6});
    CHECK(!timer.Advance({2}) && timer.remaining.quanta == 4);
    CHECK(!timer.Advance({2})); CHECK(timer.Advance({2})); CHECK(!timer.Advance({2}));
    timer.Reset({0}); CHECK(timer.Advance({0}));
    RateRemainder residue; int64_t part = 0, sum = 0;
    for (unsigned i=0;i<6000;++i) { if (!residue.Advance(1,1,part)) return 2; sum += part; }
    CHECK(sum == 1000 && residue.sixths == 0);
    sum = 0;
    for (unsigned i=0;i<6000;++i) { if (!residue.Advance(-1,1,part)) return 2; sum += part; }
    CHECK(sum == -1000 && residue.sixths == 0);
    CHECK(residue.Advance(1,1,part) && part == 0 && residue.sixths == 1);
    CHECK(residue.Advance(-1,1,part) && part == 0 && residue.sixths == 0);
    CHECK(residue.Advance(5,4,part) && part == 3 && residue.sixths == 2); // math s=2/3
    CHECK(residue.Advance(5,4,part) && part == 3 && residue.sixths == 4);
    CHECK(residue.Advance(5,4,part) && part == 4 && residue.sixths == 0);
    CHECK(!residue.Advance(1,0,part) && !residue.Advance(1,7,part));
    residue.sixths = 5; residue.Reset(); CHECK(residue.sixths == 0);
    CHECK(WrapAngle(65535,2) == 1 && WrapAngle(0,-1) == 65535);
    Identity id{1,2,3}; OpportunityCursor source; source.Reset(id,Domain::Player);
    CHECK(!source.Consume({Domain::Player,id,1,{6}}, {5}));
    CHECK(source.Consume({Domain::Player,id,1,{6}}, {6}));
    CHECK(!source.Consume({Domain::Player,id,1,{6}}, {7}));
    CHECK(!source.Consume({Domain::Player,id,0,{6}}, {7}));
    CHECK(source.Consume({Domain::Player,id,2,{6}}, {7}));
    CHECK(!source.Consume({Domain::World,id,3,{6}}, {7}));
    CHECK(!source.Consume({Domain::Player,{2,2,3},3,{6}}, {7}));
    source.Reset({2,2,3},Domain::World);
    WorldStepContext world{6,{0},{6},1,2};
    CHECK(ConsumeWorld(source,world,{2,2,3},{0}));
    bool duplicate = false; for (unsigned q=1;q<6;++q) duplicate |= ConsumeWorld(source,world,{2,2,3},{q});
    CHECK(!duplicate);
    world = {6,{6},{12},2,2}; CHECK(ConsumeWorld(source,world,{2,2,3},{6}));
    world = {1,{7},{8},3,2}; CHECK(!ConsumeWorld(source,world,{2,2,3},{7}));
    MarkerRange range;
    constexpr int64_t f = 65536;
    CHECK(Crossings(0, f, f, 10*f, range) && range.count == 1 && range.firstLoop == 0);
    CHECK(Crossings(f,2*f,f,10*f,range) && range.count == 0);
    CHECK(Crossings(9*f,11*f,0,10*f,range) && range.count == 1 && range.firstLoop == 1);
    CHECK(Crossings(11*f,9*f,0,10*f,range) && range.count == 1 && range.direction == -1);
    CHECK(Crossings(f,-f,0,10*f,range) && range.count == 1 && range.firstLoop == 0);
    CHECK(Crossings(0,-f,0,10*f,range) && range.count == 0);
    CHECK(Crossings(-f,0,0,10*f,range) && range.count == 1);
    CHECK(Crossings(0,30*f,0,10*f,range) && range.count == 3 && range.firstLoop == 1);
    CHECK(Crossings(30*f,0,0,10*f,range) && range.count == 3 && range.firstLoop == 2);
    CHECK(Crossings(0,0,0,10*f,range) && range.count == 0);
    CHECK(!Crossings(0,f,0,0,range) && !Crossings(0,f,10*f,10*f,range));
    AnimationEvents animation; CHECK(animation.Change() && animation.Advance());
    CHECK(Crossings(0,30*f,0,10*f,range));
    CHECK(animation.Consume(0,1,1,range)); CHECK(!animation.Consume(0,1,1,range));
    CHECK(animation.Consume(1,1,1,range)); CHECK(animation.Advance());
    CHECK(!animation.Consume(0,1,1,range)); CHECK(animation.Consume(0,1,2,range));
    CHECK(animation.Change()); CHECK(!animation.Consume(0,1,2,range));
    CHECK(animation.Advance() && animation.Consume(0,2,1,range));
    Lifecycle life; CHECK(life.Scene() && life.CreatePlayer());
    const auto firstOwner = life.identity;
    CHECK(life.attack.Begin() && life.attack.epoch == 1);
    CHECK(life.attack.Window(true) && life.attack.hitOpportunity == 1);
    for (unsigned i=0;i<6;++i) life.attack.Window(true);
    CHECK(life.attack.hitOpportunity == 1);
    CHECK(life.attack.Window(false) && life.attack.Window(true) && life.attack.hitOpportunity == 2);
    life.attack.End(); CHECK(!life.attack.attacking && life.attack.epoch == 1);
    CHECK(life.attack.Begin() && life.attack.epoch == 2 && life.attack.hitOpportunity == 0);
    CHECK(life.DestroyPlayer() && !life.playerAlive && !life.attack.attacking);
    CHECK(life.CreatePlayer() && life.identity.player == firstOwner.player + 1);
    CHECK(!(life.identity == firstOwner));
    CHECK(life.Scene() && life.identity.scene == firstOwner.scene+1 && !life.playerAlive);
    ContactEvent a{}, b{}; a.owner = b.owner = firstOwner;
    a.attackEpoch=b.attackEpoch=1; a.hitOpportunity=b.hitOpportunity=1;
    a.targetGeneration=b.targetGeneration=7; b.attackerCollider=1; b.sequence=3;
    CHECK(SameHitOpportunity(a,b)); // two quads / repeated geometry do not invent hits
    b.hitOpportunity=2; CHECK(!SameHitOpportunity(a,b));
    b.hitOpportunity=1; b.owner=life.identity; CHECK(!SameHitOpportunity(a,b));
    CHECK(std::is_trivially_copyable<ContactEvent>::value);
    InputTimeline input; input.Reset(firstOwner);
    InputEvent press{firstOwner,0,1,100,{6},0x4000,0x4000,0,0}, release{firstOwner,1,2,100,{6},0,0,0x4000,0}, edge;
    CHECK(input.Queue(press) && input.Queue(release) && input.Queued()==2);
    CHECK(!input.Queue(release)); CHECK(!input.Consume({5},edge));
    CHECK(input.Consume({6},edge) && edge.pressed==0x4000 && edge.timeDenominator==100);
    CHECK(input.Consume({6},edge) && edge.released==0x4000);
    CHECK(!input.Consume({6},edge) && input.consumedEdges==2 && input.consumedSequence==1);
    input.Reset(life.identity); CHECK(!input.Queue(press) && input.Queued()==0);
    input.Reset(firstOwner); bool filled=true;
    for (unsigned i=0;i<256;++i) { press.sequence=i; filled &= input.Queue(press); }
    press.sequence=256; CHECK(filled && !input.Queue(press) && input.Queued()==256);
    input.Reset(firstOwner); press.sequence=0;
    CHECK(input.Queue(press) && input.Queue(release));
    PlayerStepContext playerStep{SimulationRate::Hz120,1,{6},{7},7,firstOwner};
    auto invalidStep = playerStep; invalidStep.identity = life.identity;
    CHECK(!input.ConsumeForPlayer(invalidStep,edge));
    invalidStep = playerStep; invalidStep.stepQuanta = 6;
    CHECK(!input.ConsumeForPlayer(invalidStep,edge));
    CHECK(input.ConsumeForPlayer(playerStep,edge) && edge.pressed == 0x4000 && input.consumingPlayerStep == 7);
    CHECK(input.ConsumeForPlayer(playerStep,edge) && edge.released == 0x4000);
    ++playerStep.playerStepId; playerStep.startTime={7}; playerStep.endTime={8};
    CHECK(!input.ConsumeForPlayer(playerStep,edge) && input.consumedEdges == 2);
    press.sequence=2; CHECK(input.Queue(press));
    playerStep.playerStepId=6; CHECK(!input.ConsumeForPlayer(playerStep,edge));
    input.Reset(life.identity); CHECK(input.Queued()==0 && input.consumingPlayerStep==0);
    CanonicalControl qa; qa.Pause(); CHECK(!qa.Begin());
    CHECK(qa.Step() && !qa.Step()); CHECK(qa.Begin() && !qa.Begin() && !qa.Step());
    CHECK(qa.Commit() && qa.time.quanta==6 && qa.transactionId==1);
    CHECK(!qa.Commit() && !qa.Begin()); qa.Run(); CHECK(qa.Begin() && qa.Commit());
    CHECK(qa.time.quanta==12 && qa.transactionId==2);
    qa.Pause(); CHECK(qa.Step()); qa.Run(); CHECK(!qa.stepPending);
    for (unsigned hz : {20U,60U,120U}) {
        FixedPlayerClock clock;
        CHECK(clock.Reset({1,1,1}) && clock.Request(hz) && !clock.Request(30));
        bool scheduleOkay = true;
        const unsigned parts = hz / 20;
        for (unsigned w=0;w<1000;++w) {
            scheduleOkay &= clock.BeginWorld(true) && !clock.BeginWorld(true) && !clock.Request(20);
            for (unsigned p=0;p<parts;++p) {
                scheduleOkay &= clock.BeginPlayer() && !clock.BeginPlayer();
                scheduleOkay &= clock.BoundaryPlayer() == (p==0);
                scheduleOkay &= clock.Player().startTime.quanta == w*6 + p*(120/hz);
                scheduleOkay &= clock.Player().endTime.quanta == w*6 + (p+1)*(120/hz);
                scheduleOkay &= clock.Player().playerStepId == w*parts+p+1;
                scheduleOkay &= !clock.EndWorld() && clock.CommitPlayer() && !clock.CommitPlayer();
            }
            scheduleOkay &= !clock.BeginPlayer() && clock.EndWorld() && !clock.EndWorld();
        }
        CHECK(scheduleOkay && clock.Now().quanta == 6000 && clock.World().worldStepId == 1000);
    }
    FixedPlayerClock clock;
    CHECK(!clock.Reset({1,1,1},{1}));
    CHECK(clock.Reset({1,1,1}) && clock.Request(120) && clock.BeginWorld(false));
    CHECK(clock.Effective() == SimulationRate::Hz20 && clock.BeginPlayer() && clock.Player().stepQuanta==6);
    CHECK(!clock.Reset({2,2,2}) && !clock.RevokeAdmission());
    CHECK(clock.CommitPlayer() && clock.EndWorld());
    CHECK(clock.BeginWorld(true) && clock.BeginPlayer() && !clock.RevokeAdmission());
    CHECK(clock.CommitPlayer() && clock.Now().quanta==7 && clock.RevokeAdmission());
    CHECK(!clock.BeginPlayer() && clock.EndWorld() && clock.Now().quanta==12);
    CHECK(clock.FallbackLatched() && clock.Effective()==SimulationRate::Hz20);
    CHECK(clock.BeginWorld(true) && clock.BeginPlayer() && clock.Player().stepQuanta==6);
    CHECK(clock.CommitPlayer() && clock.EndWorld() && clock.Reset({2,2,2}));
    CHECK(!clock.FallbackLatched() && clock.Now().quanta==0);
    CHECK(clock.Request(120) && clock.BeginWorld(true) && !clock.EndWorld());
    CHECK(clock.RevokeAdmission() && clock.EndWorld());
    PlayerStepControl stepControl;
    CHECK(stepControl.Pause() && !stepControl.Begin(1));
    CHECK(stepControl.Step() && !stepControl.Step() && stepControl.Begin(1));
    CHECK(!stepControl.Pause() && !stepControl.Run() && !stepControl.Begin(1));
    CHECK(stepControl.Commit(false) && !stepControl.Commit(false) && !stepControl.Begin(1));
    CHECK(stepControl.NextWorld(1) && !stepControl.NextWorld(1) && !stepControl.Begin(2));
    bool nextWorldOkay=true;
    for (unsigned p=1;p<6;++p) nextWorldOkay &= stepControl.Begin(1) && stepControl.Commit(p==5);
    CHECK(nextWorldOkay && !stepControl.Begin(2));
    CHECK(stepControl.Run() && stepControl.Begin(2) && stepControl.Commit(false));
    PeriodicPlayerOpportunity legacyPulse;
    CHECK(!legacyPulse.Consume({0},0));
    CHECK(legacyPulse.Consume({0},1) && !legacyPulse.Consume({0},1));
    CHECK(!legacyPulse.Consume({2},2) && !legacyPulse.Consume({4},3));
    CHECK(legacyPulse.Consume({6},4) && !legacyPulse.Consume({6},5));
    CHECK(legacyPulse.Reset({8},false) && !legacyPulse.Consume({12},7));
    CHECK(legacyPulse.Consume({14},8));
    CHECK(legacyPulse.Reset({17},true) && legacyPulse.Consume({17},10) && !legacyPulse.Consume({17},10));
    CHECK(!legacyPulse.Reset({UINT64_MAX},false) && legacyPulse.Consume({23},11));
    CHECK(!legacyPulse.Consume({UINT64_MAX},12));
    for (auto rate : {SimulationRate::Hz60, SimulationRate::Hz120}) {
        const unsigned q = StepQuanta(rate);
        int16_t smallAngle = 0, wrapAngle = 32760, reverseAngle = -32760;
        RateRemainder smallResidue, wrapResidue, reverseResidue;
        bool reached = false, angleOkay = true;
        for (unsigned elapsed = 0; elapsed < 6; elapsed += q) {
            angleOkay &= AdvancePlayerAngle(smallAngle, 1, 1, q, smallResidue, reached);
        }
        CHECK(angleOkay && reached && smallAngle == 1 && smallResidue.sixths == 0);
        for (unsigned elapsed = 0; elapsed < 18; elapsed += q) {
            angleOkay &= AdvancePlayerAngle(wrapAngle, -32760, 6, q, wrapResidue, reached);
            angleOkay &= AdvancePlayerAngle(reverseAngle, 32760, 6, q, reverseResidue, reached);
        }
        CHECK(angleOkay && wrapAngle == -32760 && reverseAngle == 32760);
        CHECK(wrapResidue.sixths == 0 && reverseResidue.sixths == 0);
        CHECK(!AdvancePlayerAngle(smallAngle, 0, -1, q, smallResidue, reached) && smallAngle == 1);
        PlayerMotion constant{{0,0,0},{2.75f,0,-1.5f},0,-20};
        PlayerMotion gravity{{0,0,0},{0,0,0},-0.5f,-1000};
        bool motionOkay = true;
        for (uint64_t start = 0; start < 600; start += q) {
            PlayerStepContext step{rate,q,{start},{start+q},start/q+1,id};
            motionOkay &= AdvancePlayerMotion(constant,step,{1,0,2});
            motionOkay &= AdvancePlayerMotion(gravity,step,{0,0,0});
        }
        CHECK(motionOkay);
        CHECK(constant.position[0] == 512.5f && constant.position[1] == 0 && constant.position[2] == -25);
        CHECK(constant.velocity[0] == 2.75f && constant.velocity[2] == -1.5f);
        // The constant-force affine continuation preserves the integer legacy
        // endpoints; only bounded floating-point accumulation error is permitted.
        const float expectedY = 1.5f * -0.5f * 100 * 101 * 0.5f;
        CHECK(std::fabs(gravity.position[1] - expectedY) < 0.1f);
        CHECK(std::fabs(gravity.velocity[1] + 50) < 0.001f);
        PlayerStepContext step{rate,q,{0},{q},1,id};
        PlayerMotion terminal{{0,0,0},{15,-20,0},-6,-20};
        CHECK(AdvancePlayerMotion(terminal,step,{0,0,0}));
        CHECK(terminal.velocity[1] == -20 && terminal.velocity[0] == 15);
        CHECK(terminal.position[0] == 15 * q * 0.25f && terminal.position[1] == -20.0f * q * 0.25f);
        const auto before = terminal;
        step.endTime.quanta++;
        CHECK(!AdvancePlayerMotion(terminal,step,{0,0,0}) && terminal.position == before.position);
        PlayerMotion crossing{{0,0,0},{0,-19,0},-12,-20};
        step.endTime.quanta--;
        CHECK((!AdvancePlayerMotion(crossing,step,{0,0,0}) && crossing.position == std::array<float,3>{}));
    }
    PlayerMotion invalid;
    PlayerStepContext canonicalMotion{SimulationRate::Hz20,6,{0},{6},1,id};
    CHECK(!AdvancePlayerMotion(invalid,canonicalMotion,{0,0,0}));
    invalid.gravity = std::numeric_limits<float>::quiet_NaN();
    PlayerStepContext fineMotion{SimulationRate::Hz120,1,{0},{1},1,id};
    CHECK((!AdvancePlayerMotion(invalid,fineMotion,{0,0,0}) && invalid.position == std::array<float,3>{}));
    std::printf("Player temporal: %s; %u checks\n", failures ? "FAIL" : "PASS", checks);
    return failures ? 1 : 0;
}
