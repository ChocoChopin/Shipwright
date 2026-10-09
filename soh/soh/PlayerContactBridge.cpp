#include "PlayerContactBridge.h"
#include "PlayerContactCore.hpp"
#include "PlayerTemporal.h"
#include <map>
#include <cmath>
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_En_Kanban/z_en_kanban.h"
uint64_t GetPerfCounter(void);
uint64_t GetFrequency(void);
int CollisionCheck_SetATvsAC(PlayState*, Collider*, ColliderInfo*, Vec3f*, Collider*, ColliderInfo*, Vec3f*, Vec3f*);
void CollisionCheck_HitEffects(PlayState*, Collider*, ColliderInfo*, Collider*, ColliderInfo*, Vec3f*);
}

namespace {
using namespace PlayerTemporal;
// Pointers only locate currently live actors. Neither proxies nor events retain
// them. Destruction unregisters before memory reuse; generations never recycle.
std::map<Actor*, uint64_t> live;
uint64_t nextGeneration = 0;
struct Proxy {
    uint64_t scene = 0, world = 0, target = 0;
    Cylinder16 cylinder{};
    Vec3f position{};
    uint32_t damageMask = 0;
    uint16_t parts = 0;
    int16_t cooldown = 0;
    uint8_t acFlags = 0, bumperFlags = 0, material = 0, effect = 0;
};
std::array<Proxy, COLLISION_CHECK_AC_MAX> proxies{};
size_t proxyCount = 0;
ContactReservations reservations;
bool produced[2]{};
struct Delivery { uint64_t target = 0; int animation = 0; ColliderInfo info{}; };
std::array<Delivery, 32> deliveries{};
size_t deliveryCount = 0;
uint64_t queries = 0, captures = 0;
uint64_t queryTicks = 0, maxQueryTicks = 0, querySteps = 0;

EnKanban* Sign(const Collider* collider) {
    if (!collider || !collider->actor || collider->actor->id != ACTOR_EN_KANBAN ||
        !collider->actor->update || collider->shape != COLSHAPE_CYLINDER || !live.count(collider->actor)) return nullptr;
    auto* sign = reinterpret_cast<EnKanban*>(collider->actor);
    if (&sign->collider.base != collider || sign->actor.params == ENKANBAN_PIECE ||
        sign->actor.params == ENKANBAN_FISHING || sign->actionState != 0 ||
        collider->colType != COLTYPE_NONE || (collider->acFlags & ~(AC_HIT | AC_ON | AC_TYPE_PLAYER)) ||
        !(collider->acFlags & AC_ON) || sign->collider.info.bumper.dmgFlags != 0xFFCFFFFFu ||
        sign->collider.info.bumper.effect != 0 || sign->collider.info.elemType != ELEMTYPE_UNK0 ||
        (sign->collider.info.bumperFlags & ~(BUMP_ON | BUMP_HIT | BUMP_DRAW_HITMARK)) ||
        !(sign->collider.info.bumperFlags & BUMP_ON) || sign->collider.dim.radius != 20 ||
        sign->collider.dim.height != 50 || sign->collider.dim.yShift != 5) return nullptr;
    return sign;
}
Actor* Resolve(uint64_t generation) {
    for (const auto& entry : live) if (entry.second == generation) return entry.first;
    return nullptr;
}
bool Intersect(const ColliderQuad& source, const Proxy& proxy, Vec3f& hit, float& distance) {
    if (!(source.base.atFlags & AT_ON) || !(source.base.atFlags & proxy.acFlags & AC_TYPE_ALL) ||
        !(source.info.toucherFlags & TOUCH_ON) || !(source.info.toucher.dmgFlags & proxy.damageMask)) return false;
    // Same triangle order, arithmetic and quantized dcMid as QuadVsCyl. Local
    // scratch replaces the legacy function statics; no hit flags are written.
    auto quad = source.dim;
    auto cylinder = proxy.cylinder;
    TriNorm first, second;
    Math3D_TriNorm(&first, &quad.quad[2], &quad.quad[3], &quad.quad[1]);
    Math3D_TriNorm(&second, &quad.quad[2], &quad.quad[1], &quad.quad[0]);
    Vec3f midpoint;
    Math_Vec3s_ToVec3f(&midpoint, &quad.dcMid);
    for (auto* triangle : {&first, &second}) {
        if (!Math3D_CylTriVsIntersect(&cylinder, triangle, &hit)) continue;
        float candidate = Math3D_Vec3fDistSq(&midpoint, &hit);
        if (!(source.info.toucherFlags & TOUCH_NEAREST) || candidate < distance) {
            distance = candidate; return true;
        }
    }
    return false;
}
}

extern "C" int PlayerContact_AdmitsCollider(const void* collider) {
    return Sign(static_cast<const Collider*>(collider)) != nullptr;
}
extern "C" void PlayerContact_SwordProduced(Player* player, const void* quad) {
    if (!PlayerTemporal_HighStepQuanta(player)) return;
    for (unsigned i = 0; i < 2; ++i) if (quad == &player->meleeWeaponQuads[i]) produced[i] = true;
}
extern "C" int PlayerContact_ManagedPlayerHit(const Player* player) {
    for (const auto& quad : player->meleeWeaponQuads) {
        if (quad.base.atFlags & AT_BOUNCED) return false;
        if (!(quad.base.atFlags & AT_HIT)) continue;
        auto it = live.find(quad.base.at);
        if (it == live.end()) return false;
        bool found = false;
        for (size_t i = 0; i < deliveryCount; ++i) found |= deliveries[i].target == it->second;
        if (!found) return false;
    }
    return true;
}
extern "C" int PlayerContact_SignAnimation(const Actor* sign, int legacyAnimation) {
    auto it = live.find(const_cast<Actor*>(sign));
    if (it != live.end()) for (size_t i = 0; i < deliveryCount; ++i)
        if (deliveries[i].target == it->second) return deliveries[i].animation;
    return legacyAnimation;
}
namespace PlayerContact {
void Scene() { reservations.Invalidate(); live.clear(); proxyCount = deliveryCount = 0; }
void Created(Actor* actor) { if (actor->id == ACTOR_EN_KANBAN) live[actor] = ++nextGeneration; }
void Destroyed(Actor* actor) {
    auto it = live.find(actor);
    if (it == live.end()) return;
    for (size_t i = 0; i < reservations.count; ++i)
        if (reservations.events[i].targetGeneration == it->second) reservations.Resolve(reservations.events[i], false);
    live.erase(it);
}
void Invalidate() { reservations.Invalidate(); proxyCount = 0; }
void BeginStep() { produced[0] = produced[1] = false; }
void Capture(PlayState* play, const WorldStepContext& world) {
    proxyCount = 0; ++captures;
    for (int i = 0; i < play->colChkCtx.colACCount; ++i) {
        auto* sign = Sign(play->colChkCtx.colAC[i]);
        if (!sign) continue;
        if (proxyCount == proxies.size()) { PlayerTemporal_ContractFailure(); break; }
        const auto& c = sign->collider;
        proxies[proxyCount++] = {world.sceneEpoch, world.worldStepId, live.at(&sign->actor), c.dim,
            sign->actor.world.pos, c.info.bumper.dmgFlags, sign->partFlags, sign->invincibilityTimer,
            c.base.acFlags, c.info.bumperFlags, c.base.colType, c.info.bumper.effect};
    }
}
void Detect(PlayState* play, const PlayerStepContext& step, const AttackState& attack) {
    if (step.rate == SimulationRate::Hz20 || !attack.attacking || !attack.activeWindow) return;
    const auto started = GetPerfCounter();
    auto* player = GET_PLAYER(play);
    for (unsigned q = 0; q < 2; ++q) {
        if (!produced[q]) continue; // no sweep on warm-up, reset or unchanged endpoints
        const auto& quad = player->meleeWeaponQuads[q];
        float nearest = 1.0e38f;
        const Proxy* selected = nullptr;
        Vec3f selectedHit{};
        for (size_t i = 0; i < proxyCount; ++i) {
            const auto& proxy = proxies[i];
            if (proxy.scene != step.identity.scene || proxy.cooldown || !(proxy.parts & 0x3ff)) continue;
            Vec3f hit; ++queries;
            if (Intersect(quad, proxy, hit, nearest)) { selected = &proxy; selectedHit = hit; }
        }
        if (!selected) continue;
        ContactEvent event;
        event.owner = step.identity; event.attackEpoch = attack.epoch; event.hitOpportunity = attack.hitOpportunity;
        event.playerStepId = step.playerStepId; event.time = step.endTime;
        event.worldProxyGeneration = selected->world; event.targetGeneration = selected->target;
        event.attackerCollider = q; event.targetCollider = 0; event.targetElementGroup = 0;
        event.damageFlags = quad.info.toucher.dmgFlags; event.attackAnimation = player->meleeWeaponAnimation;
        event.targetState = selected->parts;
        event.hitPosition[0] = selectedHit.x; event.hitPosition[1] = selectedHit.y; event.hitPosition[2] = selectedHit.z;
        event.material = selected->material; event.effect = selected->effect;
        event.damage = quad.info.toucher.damage; event.toucherEffect = quad.info.toucher.effect;
        event.toucherFlags = quad.info.toucherFlags & ~(TOUCH_HIT | TOUCH_DREW_HITMARK);
        const auto duplicateCount = reservations.duplicates;
        if (!reservations.Reserve(event) && duplicateCount == reservations.duplicates) PlayerTemporal_ContractFailure();
    }
    const auto elapsed = GetPerfCounter() - started;
    queryTicks += elapsed; maxQueryTicks = std::max(maxQueryTicks, elapsed); ++querySteps;
}
void Consume(PlayState* play, Identity owner, SimTime now) {
    deliveryCount = 0;
    for (size_t i = 0; i < reservations.count; ++i) {
        auto& event = reservations.events[i];
        if (event.response != ResponseState::Reserved || event.time.quanta > now.quanta) continue;
        auto* actor = Resolve(event.targetGeneration);
        auto* sign = actor ? Sign(&reinterpret_cast<EnKanban*>(actor)->collider.base) : nullptr;
        bool valid = event.owner == owner && sign && sign->invincibilityTimer <= 1 &&
            (sign->partFlags & 0x3ff) && sign->partFlags == event.targetState &&
            sign->collider.base.colType == event.material && !(sign->collider.base.acFlags & AC_HIT) &&
            event.attackerCollider < 2 && event.attackAnimation < PLAYER_MWA_SPIN_ATTACK_1H &&
            (event.damageFlags & sign->collider.info.bumper.dmgFlags);
        if (!valid) { reservations.Resolve(event, false); continue; }
        auto& at = GET_PLAYER(play)->meleeWeaponQuads[event.attackerCollider];
        auto& ac = sign->collider;
        auto& delivery = deliveries[deliveryCount++];
        delivery = {event.targetGeneration, static_cast<int>(event.attackAnimation), at.info};
        delivery.info.toucher.dmgFlags = event.damageFlags;
        delivery.info.toucher.damage = event.damage; delivery.info.toucher.effect = event.toucherEffect;
        delivery.info.toucherFlags = event.toucherFlags;
        Vec3f hit{event.hitPosition[0], event.hitPosition[1], event.hitPosition[2]};
        Vec3f atPos = GET_PLAYER(play)->actor.world.pos, acPos;
        Math_Vec3s_ToVec3f(&acPos, &ac.dim.pos);
        // Complete response occurs once at the world boundary. The stable
        // delivery info lives through this world actor traversal, never a step.
        CollisionCheck_SetATvsAC(play, &at.base, &delivery.info, &atPos, &ac.base, &ac.info, &acPos, &hit);
        at.info.atHit = &ac.base; at.info.atHitInfo = &ac.info; at.info.toucherFlags |= TOUCH_HIT;
        if (ac.info.bumperFlags & BUMP_DRAW_HITMARK) {
            CollisionCheck_HitEffects(play, &at.base, &delivery.info, &ac.base, &ac.info, &hit);
            ac.info.bumperFlags &= ~BUMP_DRAW_HITMARK;
        }
        reservations.Resolve(event, true);
    }
}
nlohmann::json Inspect() {
    nlohmann::json result = {{"detected", reservations.detected}, {"duplicates", reservations.duplicates},
        {"reserved", reservations.reserved}, {"committed", reservations.committed}, {"rejected", reservations.rejected},
        {"pending", reservations.Pending()}, {"queries", queries}, {"proxy_commits", captures}};
    result["query_steps"] = querySteps;
    result["query_total_us"] = double(queryTicks) * 1000000.0 / GetFrequency();
    result["query_max_us"] = double(maxQueryTicks) * 1000000.0 / GetFrequency();
    result["events"] = nlohmann::json::array();
    for (size_t i = 0; i < reservations.count; ++i) {
        const auto& e = reservations.events[i];
        result["events"].push_back({{"sequence",e.sequence},{"scene",e.owner.scene},{"player",e.owner.player},
            {"scope",e.owner.scope},{"attack",e.attackEpoch},{"opportunity",e.hitOpportunity},
            {"step",e.playerStepId},{"time_q",e.time.quanta},{"proxy_world",e.worldProxyGeneration},
            {"target",e.targetGeneration},{"quad",e.attackerCollider},{"animation",e.attackAnimation},
            {"damage_flags",e.damageFlags},{"state",static_cast<int>(e.response)}});
    }
    return result;
}
}
