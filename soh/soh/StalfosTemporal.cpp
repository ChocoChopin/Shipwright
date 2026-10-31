#include "StalfosTemporal.h"
#include "PlayerTemporal.h"
#include "NativeSimulationTest.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <algorithm>
#include <cmath>
extern "C" {
#include "global.h"
#include "regs.h"
#include "overlays/actors/ovl_En_Test/z_en_test.h"
void Actor_Draw(PlayState*, Actor*);
int PlayerContact_SwordWasProduced(unsigned);
void CollisionCheck_AC(PlayState*, CollisionCheckContext*, Collider*);
void CollisionCheck_SetHitEffects(PlayState*, CollisionCheckContext*);
}
namespace {
EnTest* enemy = nullptr;
bool active = false, drawing = false, intermediate = false, animateMain = false;
bool contactOpen=false, movedWorld=false, managed=false;
float duration = 1.0f;
Gfx *opaBinding = nullptr, *xluBinding = nullptr;
StalfosTemporalStats stats{};
uint64_t hitEpoch = UINT64_MAX, hitOpportunity = UINT64_MAX;
bool enemyHit = false;
EnTestActionFunc attackAction = nullptr;
constexpr size_t OpaCapacity = 1024, XluCapacity = 512;

void Paint(PlayState* play) {
    // This selected actor needs CPU pose/contact work even outside the view.
    stats.culledPoses += !(enemy->actor.flags & ACTOR_FLAG_INSIDE_CULLING_VOLUME);
    auto* gfx = play->state.gfxCtx;
    auto* opa = static_cast<Gfx*>(Graph_Alloc(gfx, OpaCapacity * sizeof(Gfx)));
    auto* xlu = static_cast<Gfx*>(Graph_Alloc(gfx, XluCapacity * sizeof(Gfx)));
    auto* savedOpa = gfx->polyOpa.p;
    auto* savedXlu = gfx->polyXlu.p;
    const auto segment = gSegments[6];
    gfx->polyOpa.p = opa; gfx->polyXlu.p = xlu;
    const int recording = FrameInterpolation_SetRecording(false);
    Matrix_Push();
    drawing = true;
    Actor_Draw(play, &enemy->actor);
    drawing = false;
    Matrix_Pop();
    FrameInterpolation_SetRecording(recording);
    gSPEndDisplayList(gfx->polyOpa.p++);
    gSPEndDisplayList(gfx->polyXlu.p++);
    stats.renderCommands += gfx->polyOpa.p - opa + gfx->polyXlu.p - xlu;
    // Capacities cover the fixed61-limb actor plus normal lighting/feet shadow.
    if (gfx->polyOpa.p - opa >= OpaCapacity || gfx->polyXlu.p - xlu >= XluCapacity) ++stats.invalid;
    gfx->polyOpa.p = savedOpa; gfx->polyXlu.p = savedXlu;
    gSegments[6] = segment;
    gSPDisplayList(opaBinding, opa);
    gSPDisplayList(xluBinding, xlu);
    ++stats.poses;
}
bool Listed(Collider* const* list, int count, const Collider* c) {
    for (int i=0;i<count;++i) if(list[i]==c) return true;
    return false;
}
}
extern "C" void StalfosTemporal_Reset() {
    enemy=nullptr; active=drawing=intermediate=animateMain=false; managed=contactOpen=movedWorld=false; duration=1;
    opaBinding=xluBinding=nullptr; stats={}; hitEpoch=hitOpportunity=UINT64_MAX;
    enemyHit=false; attackAction=nullptr;
}
extern "C" void StalfosTemporal_Destroyed(Actor* actor) {
    if (enemy && actor==&enemy->actor) { enemy=nullptr; active=false; opaBinding=xluBinding=nullptr; }
}
extern "C" void StalfosTemporal_BeginFrame() {
    active=false; duration=1; opaBinding=xluBinding=nullptr; animateMain=false; movedWorld=false;
}
extern "C" void StalfosTemporal_BeginUpdate(Actor* actor, PlayState* play) {
    if (enemy && actor==&enemy->actor) managed=false;
    const bool enabled = NativeSimTest_IsEnabled() ? NativeSimTest_ConfigInt("stalfos_hz",0)!=0 :
        CVarGetInteger("gDeveloperTools.NativeStalfosHz",0)!=0;
    const unsigned q=PlayerTemporal_HighStepQuanta(GET_PLAYER(play));
    if (!enabled || !q || actor->params!=STALFOS_TYPE_1 || (enemy && &enemy->actor!=actor)) return;
    enemy=reinterpret_cast<EnTest*>(actor); active=managed=true; duration=q/6.0f;
    ++stats.worlds; ++stats.decisions;
}
extern "C" int StalfosTemporal_IsActor(const Actor* actor) { return active && enemy && actor==&enemy->actor; }
extern "C" int StalfosTemporal_Managed(const Actor* actor) { return managed && enemy && actor==&enemy->actor; }
extern "C" int StalfosTemporal_SkipWorldPair(const void* attack, const void* defense) {
    const auto* at=static_cast<const Collider*>(attack);
    const auto* ac=static_cast<const Collider*>(defense);
    if (contactOpen || !at || !ac || !at->actor || !ac->actor) return false;
    return (StalfosTemporal_Managed(at->actor) && ac->actor->id==ACTOR_PLAYER) ||
           (StalfosTemporal_Managed(ac->actor) && at->actor->id==ACTOR_PLAYER);
}
extern "C" int StalfosTemporal_IsIntermediate() { return intermediate; }
extern "C" int StalfosTemporal_IsContact() { return contactOpen; }
extern "C" void StalfosTemporal_ActionChanged(Actor* actor) {
    if (enemy && actor==&enemy->actor) { attackAction=nullptr; enemyHit=false; }
}
extern "C" float StalfosTemporal_Duration(Actor* actor) { return StalfosTemporal_IsActor(actor) ? duration : 1.0f; }
extern "C" int StalfosTemporal_Animation(Actor* actor, SkelAnime* animation) {
    if (!StalfosTemporal_IsActor(actor)) return SkelAnime_Update(animation);
    if (animation==&enemy->skelAnime) animateMain=true;
    ++stats.animations;
    return SkelAnime_UpdateDuration(animation,duration);
}
extern "C" void StalfosTemporal_Move(Actor* actor) {
    if (!StalfosTemporal_IsActor(actor)) { Actor_MoveXZGravity(actor); return; }
    if (!intermediate) movedWorld=true;
    auto before=actor->world.pos;
    if (intermediate) actor->prevPos=before;
    const float oldY=actor->velocity.y;
    actor->velocity.x=Math_SinS(actor->world.rot.y)*actor->speedXZ;
    actor->velocity.z=Math_CosS(actor->world.rot.y)*actor->speedXZ;
    actor->velocity.y=std::max(actor->minVelocityY,oldY+actor->gravity*duration);
    const float moveY=std::max(actor->minVelocityY,oldY+actor->gravity*(1+duration)*0.5f);
    const float gain=R_UPDATE_RATE*0.5f*duration;
    actor->world.pos.x+=actor->velocity.x*gain+actor->colChkInfo.displacement.x;
    actor->world.pos.y+=moveY*gain+actor->colChkInfo.displacement.y;
    actor->world.pos.z+=actor->velocity.z*gain+actor->colChkInfo.displacement.z;
    actor->colChkInfo.displacement={}; // consume each OC correction once
    const float distance=Math_Vec3f_DistXYZ(&before,&actor->world.pos);
    stats.maxStep=std::max(stats.maxStep,distance); stats.movingSteps+=distance>0.001f;
    stats.horizontalSteps+=std::hypot(actor->world.pos.x-before.x,actor->world.pos.z-before.z)>0.001f;
    ++stats.steps;
}
extern "C" int StalfosTemporal_Draw(PlayState* play, Actor* actor) {
    if (drawing || !StalfosTemporal_IsActor(actor)) return false;
    opaBinding=play->state.gfxCtx->polyOpa.p++;
    xluBinding=play->state.gfxCtx->polyXlu.p++;
    Paint(play);
    return true;
}
extern "C" void StalfosTemporal_Advance(PlayState* play) {
    if (!active || !movedWorld || !enemy || !enemy->actor.update || !enemy->actor.draw || !opaBinding || !xluBinding) return;
    const auto action=enemy->actionFunc;
    const auto timer=enemy->timer;
    NativeSimTest_PlayerSample("stalfos.step.begin",play);
    intermediate=true;
    StalfosTemporal_Move(&enemy->actor);
    // No action callback, RNG, tactical reaction, timer or world effect runs here.
    // The actor helper owns continuous animation, floor support and collider pose.
    const float previousFrame=enemy->skelAnime.curFrame;
    if (animateMain) { SkelAnime_UpdateDuration(&enemy->skelAnime,duration); ++stats.animations; }
    EnTest_AdvanceTemporal(&enemy->actor,play,previousFrame);
    Paint(play);
    intermediate=false;
    if (action!=enemy->actionFunc || timer!=enemy->timer) ++stats.invalid;
    NativeSimTest_PlayerSample("stalfos.step.end",play);
}
extern "C" void StalfosTemporal_Contacts(PlayState* play, uint64_t epoch, uint64_t opportunity) {
    if (!active || !enemy || !enemy->actor.update || !opaBinding) return;
    contactOpen=true;
    auto* p=GET_PLAYER(play);
    if (attackAction!=enemy->actionFunc) { attackAction=enemy->actionFunc; enemyHit=false; Collider_ResetQuadAT(play,&enemy->swordCollider.base); }
    // Query current live geometry at each Player boundary. Damage/reactions are
    // consumed by their existing owners; each attack delivers at most once.
    if (enemy->meleeWeaponState>=1 && !(enemy->swordCollider.base.atFlags&AT_BOUNCED)) {
        ++stats.attackSamples;
        if (!enemyHit) {
            CollisionCheckContext c{};
            if (Listed(play->colChkCtx.colAC,play->colChkCtx.colACCount,&p->shieldQuad.base))
                c.colAC[c.colACCount++]=&p->shieldQuad.base;
            if (Listed(play->colChkCtx.colAC,play->colChkCtx.colACCount,&p->cylinder.base))
                c.colAC[c.colACCount++]=&p->cylinder.base;
            CollisionCheck_AC(play,&c,&enemy->swordCollider.base);
            if (enemy->swordCollider.base.atFlags&(AT_HIT|AT_BOUNCED)) {
                enemyHit=true; ++stats.contacts;
                stats.blocks+=(enemy->swordCollider.base.atFlags&AT_BOUNCED)!=0;
                CollisionCheck_SetHitEffects(play,&c);
            }
        }
    }
    if (epoch!=hitEpoch || opportunity!=hitOpportunity) {
        for (unsigned index=0; index<2; ++index) {
            auto& quad=p->meleeWeaponQuads[index];
            if (!PlayerContact_SwordWasProduced(index)) continue;
            // High-rate sword samples deliberately bypass the world AT list.
            // Produced is the authoritative changed, active sweep admission;
            // requiring world registration here would suppress every counter.
            if (!p->meleeWeaponState) continue;
            CollisionCheckContext c{};
            if (enemy->unk_7DE && Listed(play->colChkCtx.colAC,play->colChkCtx.colACCount,&enemy->shieldCollider.base))
                c.colAC[c.colACCount++]=&enemy->shieldCollider.base;
            if (!(enemy->bodyCollider.base.acFlags&AC_HIT) &&
                Listed(play->colChkCtx.colAC,play->colChkCtx.colACCount,&enemy->bodyCollider.base))
                c.colAC[c.colACCount++]=&enemy->bodyCollider.base;
            CollisionCheck_AC(play,&c,&quad.base);
            if ((enemy->bodyCollider.base.acFlags&AC_HIT) || (enemy->shieldCollider.base.acFlags&AC_HIT)) {
                hitEpoch=epoch; hitOpportunity=opportunity; ++stats.playerContacts;
                CollisionCheck_SetHitEffects(play,&c); break;
            }
        }
    }
    contactOpen=false;
}
extern "C" StalfosTemporalStats StalfosTemporal_Stats() { return stats; }
