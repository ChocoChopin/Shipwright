#include "StalfosTemporal.h"
#include "PlayerContactBridge.h"
#include "PlayerContactCore.hpp"
#include "PlayerTemporal.h"
#include <map>
#include <cmath>
#include <vector>
#include <algorithm>
#include <memory>
extern "C" {
#include "global.h"
#include "overlays/actors/ovl_En_Kanban/z_en_kanban.h"
uint64_t GetPerfCounter(void);
uint64_t GetFrequency(void);
int CollisionCheck_SetATvsAC(PlayState*, Collider*, ColliderInfo*, Vec3f*, Collider*, ColliderInfo*, Vec3f*, Vec3f*);
void CollisionCheck_HitEffects(PlayState*, Collider*, ColliderInfo*, Collider*, ColliderInfo*, Vec3f*);
void CollisionCheck_AC(PlayState*, CollisionCheckContext*, Collider*);
void CollisionCheck_SetHitEffects(PlayState*, CollisionCheckContext*);
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

// At most two sword quads per Player step, six steps per world transaction.
// Target geometry is still the registered world geometry when these are consumed:
// no world-actor update occurs between capture and the next collision pass.
struct WorldSweep {
    Identity owner{};
    uint64_t epoch = 0, opportunity = 0;
    unsigned source = 0;
    ColliderQuad quad{};
    float signDistance = 1.0e38f;
    uint64_t signReservation = 0;
};
std::array<WorldSweep, 12> worldSweeps{};
size_t worldSweepCount = 0;
struct WorldHit {
    Identity owner{};
    uint64_t epoch = 0, opportunity = 0, target = 0;
    Collider* collider = nullptr; // identity only, never dereferenced after destruction
};
std::vector<WorldHit> worldHits;
struct WorldDelivery { uint64_t target = 0; ColliderQuad quad{}; };
// AC consumers can retain their hit pointers while culled. Keep captured source
// data stable until the target dies or receives another hit, not in frame storage.
std::map<Collider*, WorldDelivery> worldDeliveries;
uint64_t worldSweepsQueued = 0, worldContacts = 0, worldPairs = 0;
struct BlockingBody {
    Actor* actor = nullptr;
    uint64_t generation = 0;
    ColliderCylinder cylinder{};
    ColliderJntSph sphere{};
    std::vector<ColliderJntSphElement> elements;
    bool spherical = false;
};
std::array<BlockingBody, COLLISION_CHECK_OC_MAX> blockingBodies;
size_t blockingBodyCount = 0;
uint64_t bodyConstraints = 0;
PlayerContact::BodyMotion bodyMotion{};

bool Registered(Collider* const* list, int count, Collider* collider) {
    for (int i = 0; i < count; ++i) if (list[i] == collider) return true;
    return false;
}
template<typename Count> void Remove(Collider** list, Count& count, Collider* collider) {
    for (int i = 0; i < count; ++i) if (list[i] == collider) {
        for (int j = i + 1; j < count; ++j) list[j - 1] = list[j];
        list[--count] = nullptr;
        --i;
    }
}
void Register(PlayState* play, Collider* collider, bool oc, bool ac, bool at) {
    auto& c = play->colChkCtx;
    if (!oc) Remove(c.colOC, c.colOCCount, collider);
    else if (!Registered(c.colOC, c.colOCCount, collider)) CollisionCheck_SetOC(play, &c, collider);
    // Preserve AC-before-AT ordering for shields. Already registered geometry
    // updates in place; do not reset flags or append again on every substep.
    if (!ac) Remove(c.colAC, c.colACCount, collider);
    else if (!Registered(c.colAC, c.colACCount, collider)) CollisionCheck_SetAC(play, &c, collider);
    if (!at) Remove(c.colAT, c.colATCount, collider);
    else if (!Registered(c.colAT, c.colATCount, collider)) CollisionCheck_SetAT(play, &c, collider);
}

template<typename Fn> void Elements(Collider* collider, Fn fn) {
    switch (collider->shape) {
        case COLSHAPE_CYLINDER: fn(reinterpret_cast<ColliderCylinder*>(collider)->info); break;
        case COLSHAPE_QUAD: fn(reinterpret_cast<ColliderQuad*>(collider)->info); break;
        case COLSHAPE_JNTSPH: {
            auto* c = reinterpret_cast<ColliderJntSph*>(collider);
            for (int i = 0; i < c->count; ++i) fn(c->elements[i].info);
            break;
        }
        case COLSHAPE_TRIS: {
            auto* c = reinterpret_cast<ColliderTris*>(collider);
            for (int i = 0; i < c->count; ++i) fn(c->elements[i].info);
            break;
        }
    }
}

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

void ConsumeWorldSweeps(PlayState* play, Identity owner) {
    CollisionCheckContext effects{};
    auto* player = GET_PLAYER(play);
    for (size_t i = 0; i < worldSweepCount; ++i) {
        auto& sweep = worldSweeps[i];
        if (!(sweep.owner == owner) || sweep.quad.base.actor != &player->actor) continue;
        worldHits.erase(std::remove_if(worldHits.begin(), worldHits.end(), [&](const WorldHit& hit) {
            return !(hit.owner == owner) || hit.epoch != sweep.epoch;
        }), worldHits.end());
        CollisionCheckContext candidates{};
        for (int j = 0; j < play->colChkCtx.colACCount; ++j) {
            auto* c = play->colChkCtx.colAC[j];
            if (!c || !c->actor || c->actor == &player->actor || !c->actor->update ||
                StalfosTemporal_Managed(c->actor) || c->shape > COLSHAPE_QUAD || !(c->acFlags & AC_ON) || (c->acFlags & AC_HIT) || Sign(c)) continue;
            auto target = live.find(c->actor);
            if (target == live.end()) continue;
            bool duplicate = false;
            for (const auto& hit : worldHits)
                duplicate |= hit.target == target->second && hit.collider == c &&
                    hit.opportunity == sweep.opportunity;
            if (!duplicate) candidates.colAC[candidates.colACCount++] = c;
        }
        // Use the actual engine's four collider-shape dispatchers, damage masks,
        // nearest-hit arithmetic, hard-surface bounce and source feedback.
        worldPairs += candidates.colACCount;
        // A sign and another object can overlap the same sweep. The dedicated
        // sign adapter must not bypass the sword's nearest-contact rule.
        sweep.quad.dim.acDist = sweep.signDistance;
        CollisionCheck_AC(play, &candidates, &sweep.quad.base);
        if (sweep.signReservation && (sweep.quad.info.toucherFlags & TOUCH_NEAREST) &&
            sweep.quad.dim.acDist < sweep.signDistance) {
            for (size_t j = 0; j < reservations.count; ++j)
                if (reservations.events[j].sequence == sweep.signReservation)
                    reservations.Resolve(reservations.events[j], false);
        }
        for (int j = 0; j < candidates.colACCount; ++j) {
            auto* c = candidates.colAC[j];
            if (!(c->acFlags & AC_HIT)) continue;
            const auto generation = live.at(c->actor);
            auto& delivery = worldDeliveries[c];
            delivery = {generation, sweep.quad};
            Elements(c, [&](ColliderInfo& info) {
                if (info.acHit == &sweep.quad.base) {
                    info.acHit = &delivery.quad.base;
                    info.acHitInfo = &delivery.quad.info;
                }
            });
            worldHits.push_back({owner, sweep.epoch, sweep.opportunity, generation, c});
            effects.colAC[effects.colACCount++] = c;
            ++worldContacts;
        }
        auto& source = player->meleeWeaponQuads[sweep.source];
        source.base.atFlags |= sweep.quad.base.atFlags & (AT_HIT | AT_BOUNCED);
        if (sweep.quad.base.atFlags & AT_HIT) {
            source.base.at = sweep.quad.base.at;
            source.info.atHit = sweep.quad.info.atHit;
            source.info.atHitInfo = sweep.quad.info.atHitInfo;
            source.info.toucherFlags |= TOUCH_HIT;
        }
    }
    // Legacy damage accumulation and actor reactions follow at their original
    // world slots. Emit only the newly bridged hit effects, once.
    CollisionCheck_SetHitEffects(play, &effects);
    worldSweepCount = 0;
}
}

extern "C" void PlayerContact_RegisterBody(PlayState* play, Player* player, int oc, int ac, int at) {
    Register(play, &player->cylinder.base, oc, ac, at);
}
extern "C" void PlayerContact_RegisterShield(PlayState* play, Player* player) {
    Register(play, &player->shieldQuad.base, false, true, true);
}

extern "C" void PlayerContact_ResolveBlockingBody(PlayState* play, Player* player) {
    // A stationary world body is a geometric constraint, not a 20-Hz impulse.
    // Query private collider/actor copies: original OC flags and reciprocal
    // responses still belong exclusively to the ordinary world collision pass.
    if (!PlayerTemporal_HighStepQuanta(player) ||
        (player->stateFlags1 & (PLAYER_STATE1_DEAD | PLAYER_STATE1_ON_HORSE))) return;
    bodyMotion.evaluated = true;
    std::memcpy(bodyMotion.before, &player->actor.world.pos, sizeof(Vec3f));
    Actor moving{}, fixed{};
    moving.colChkInfo.mass = player->actor.colChkInfo.mass;
    fixed.colChkInfo.mass = MASS_IMMOVABLE;
    for (size_t i = 0; i < blockingBodyCount; ++i) {
        auto& body = blockingBodies[i];
        auto found = live.find(body.actor);
        if (found == live.end() || found->second != body.generation || !body.actor->update) continue;
        if (!body.spherical && StalfosTemporal_IsActor(body.actor)) {
            // This single high-rate heavy actor has moved since world capture.
            // Light Player cannot push MASS_HEAVY; resolve its current constraint.
            body.cylinder.dim.pos={(s16)body.actor->world.pos.x,(s16)body.actor->world.pos.y,(s16)body.actor->world.pos.z};
        }
        ColliderCylinder cylinder = player->cylinder;
        cylinder.base.actor = &moving;
        cylinder.dim.pos = {(s16)player->actor.world.pos.x, (s16)player->actor.world.pos.y,
                            (s16)player->actor.world.pos.z};
        moving.colChkInfo.displacement = {};
        Collider* target = body.spherical ? &body.sphere.base : &body.cylinder.base;
        target->actor = &fixed;
        CollisionCheckContext context{};
        context.colOC[0] = &cylinder.base; context.colOC[1] = target; context.colOCCount = 2;
        CollisionCheck_OC(play, &context);
        target->actor = body.actor;
        target->oc = nullptr; // Do not retain a pointer to the private stack actor.
        const auto correction = moving.colChkInfo.displacement;
        if (correction.x != 0 || correction.z != 0) {
            player->actor.world.pos.x += correction.x;
            player->actor.world.pos.z += correction.z;
            ++bodyConstraints;
        }
    }
    std::memcpy(bodyMotion.after, &player->actor.world.pos, sizeof(Vec3f));
}

extern "C" int PlayerContact_AdmitsCollider(const void* collider) {
    return Sign(static_cast<const Collider*>(collider)) != nullptr;
}
extern "C" int PlayerContact_SwordWasProduced(unsigned index) {
    return index<2 && produced[index];
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
void Scene() {
    reservations.Invalidate(); live.clear(); proxyCount = deliveryCount = worldSweepCount = 0;
    worldHits.clear(); worldDeliveries.clear(); blockingBodyCount = 0;
}
void Created(Actor* actor) { live[actor] = ++nextGeneration; }
void Destroyed(Actor* actor) {
    auto it = live.find(actor);
    if (it == live.end()) return;
    for (size_t i = 0; i < reservations.count; ++i)
        if (reservations.events[i].targetGeneration == it->second) reservations.Resolve(reservations.events[i], false);
    worldHits.erase(std::remove_if(worldHits.begin(), worldHits.end(), [&](const WorldHit& hit) {
        return hit.target == it->second;
    }), worldHits.end());
    for (auto delivery = worldDeliveries.begin(); delivery != worldDeliveries.end(); ) {
        if (delivery->second.target == it->second) delivery = worldDeliveries.erase(delivery);
        else ++delivery;
    }
    live.erase(it);
}
void Invalidate() { reservations.Invalidate(); proxyCount = worldSweepCount = blockingBodyCount = 0; worldHits.clear(); }
BodyMotion ObserveBodyMotion() { return bodyMotion; }
void BeginStep() { produced[0] = produced[1] = false; bodyMotion = {}; }
void Capture(PlayState* play, const WorldStepContext& world) {
    blockingBodyCount = 0;
    for (int i = 0; i < play->colChkCtx.colOCCount; ++i) {
        auto* c = play->colChkCtx.colOC[i];
        if (!c || !c->actor || c->actor == &GET_PLAYER(play)->actor || !c->actor->update ||
            (c->actor->colChkInfo.mass != MASS_IMMOVABLE && !StalfosTemporal_IsActor(c->actor)) ||
            (c->shape != COLSHAPE_CYLINDER && c->shape != COLSHAPE_JNTSPH)) continue;
        auto found = live.find(c->actor);
        if (found == live.end()) continue;
        if (c->shape == COLSHAPE_JNTSPH) {
            const auto* sphere = reinterpret_cast<const ColliderJntSph*>(c);
            if (sphere->count <= 0 || !sphere->elements) continue;
        }
        auto& body = blockingBodies[blockingBodyCount++];
        body.actor = c->actor; body.generation = found->second;
        body.spherical = c->shape == COLSHAPE_JNTSPH;
        if (body.spherical) {
            body.sphere = *reinterpret_cast<ColliderJntSph*>(c);
            body.elements.assign(body.sphere.elements, body.sphere.elements + body.sphere.count);
            body.sphere.elements = body.elements.data();
        } else body.cylinder = *reinterpret_cast<ColliderCylinder*>(c);
    }
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
    auto* player = GET_PLAYER(play);
    if (step.rate != SimulationRate::Hz20 && !(player->stateFlags1 & PLAYER_STATE1_SHIELDING))
        Register(play, &player->shieldQuad.base, false, false, false);
    if (step.rate == SimulationRate::Hz20 || !attack.attacking || !attack.activeWindow) return;
    const auto started = GetPerfCounter();
    for (unsigned q = 0; q < 2; ++q) {
        if (!produced[q]) continue; // no sweep on warm-up, reset or unchanged endpoints
        const auto& quad = player->meleeWeaponQuads[q];
        if (worldSweepCount == worldSweeps.size()) { PlayerTemporal_ContractFailure(); continue; }
        auto& sweep = worldSweeps[worldSweepCount++];
        sweep = {step.identity, attack.epoch, attack.hitOpportunity, q, quad};
        Collider_ResetQuadAT(play, &sweep.quad.base);
        ++worldSweepsQueued;
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
        sweep.signDistance = nearest;
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
        if (reservations.Reserve(event)) sweep.signReservation = reservations.sequence;
        else if (duplicateCount == reservations.duplicates) PlayerTemporal_ContractFailure();
    }
    const auto elapsed = GetPerfCounter() - started;
    queryTicks += elapsed; maxQueryTicks = std::max(maxQueryTicks, elapsed); ++querySteps;
}
void Consume(PlayState* play, Identity owner, SimTime now) {
    deliveryCount = 0;
    ConsumeWorldSweeps(play, owner);
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
    result["world_sweeps"] = worldSweepsQueued;
    result["world_contacts"] = worldContacts;
    result["world_pairs"] = worldPairs;
    result["world_pending_sweeps"] = worldSweepCount;
    result["body_constraints"] = bodyConstraints;
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

nlohmann::json ValidateWorldContacts() {
    nlohmann::json cases = nlohmann::json::array();
    unsigned failures = 0;
    auto check = [&](const char* name, bool result) {
        cases.push_back({{"name", name}, {"passed", result}}); failures += !result;
    };
    auto play = std::make_unique<PlayState>();
    auto player = std::make_unique<Player>();
    Actor target{};
    auto update = [](Actor*, PlayState*) {};
    player->actor.update = target.update = update;
    player->actor.id = ACTOR_PLAYER; target.id = ACTOR_EN_KUSA;
    play->actorCtx.actorLists[ACTORCAT_PLAYER].head = &player->actor;
    player->actor.colChkInfo.mass = target.colChkInfo.mass = 50;
    Created(&target);
    const Identity owner{UINT64_MAX, 1, 1};
    uint64_t epoch = 0;
    ColliderCylinder cylinder{};
    cylinder.dim = {10, 20, 0, {0, 0, 0}};
    ColliderJntSph sphere{};
    ColliderJntSphElement sphereElement{};
    sphere.count = 1; sphere.elements = &sphereElement;
    sphereElement.dim.worldSphere = {{0, 10, 0}, 8};
    ColliderTris tris{};
    ColliderTrisElement triElement{};
    tris.count = 1; tris.elements = &triElement;
    Vec3f a{0, 0, -10}, b{0, 20, 0}, c{0, 0, 10};
    Math3D_TriNorm(&triElement.dim, &a, &b, &c);
    ColliderQuad quad{};
    Vec3f qa{-10, 0, 0}, qb{10, 0, 0}, qc{-10, 20, 0}, qd{10, 20, 0};
    Collider_SetQuadVertices(&quad, &qa, &qb, &qc, &qd);
    Collider* targets[] = {&sphere.base, &cylinder.base, &tris.base, &quad.base};
    const char* names[] = {"sphere sword hit", "cylinder sword hit", "triangles sword hit", "quad sword hit"};
    auto queue = [&]() {
        auto& sweep = worldSweeps[0];
        sweep = {}; sweep.owner = owner; sweep.epoch = epoch; sweep.opportunity = 1;
        auto& at = sweep.quad;
        at.base.actor = &player->actor; at.base.shape = COLSHAPE_QUAD;
        at.base.atFlags = AT_ON | AT_TYPE_PLAYER; at.base.colType = COLTYPE_NONE;
        at.info.toucherFlags = TOUCH_ON | TOUCH_NEAREST;
        at.info.toucher.dmgFlags = 1; at.info.toucher.damage = 4;
        Vec3f p0{-20, 10, -20}, p1{20, 10, -20}, p2{-20, 10, 20}, p3{20, 10, 20};
        Collider_SetQuadVertices(&at, &p0, &p1, &p2, &p3);
        Collider_ResetQuadAT(play.get(), &at.base);
        worldSweepCount = 1;
    };
    auto reset = [&](Collider* ac) {
        ac->acFlags = AC_ON | AC_TYPE_PLAYER;
        Elements(ac, [](ColliderInfo& info) {
            info = {}; info.bumperFlags = BUMP_ON | BUMP_NO_SWORD_SFX;
            info.bumper.dmgFlags = 1;
        });
    };
    for (unsigned shape = 0; shape < 4; ++shape) {
        auto* ac = targets[shape];
        ac->actor = &target; ac->shape = shape; ac->colType = COLTYPE_NONE;
        reset(ac); ++epoch;
        play->colChkCtx = {}; play->colChkCtx.colAC[0] = ac; play->colChkCtx.colACCount = 1;
        queue(); ConsumeWorldSweeps(play.get(), owner);
        check(names[shape], (ac->acFlags & AC_HIT) && worldSweepCount == 0);
        ColliderInfo* hit = nullptr;
        Elements(ac, [&](ColliderInfo& info) { if (info.bumperFlags & BUMP_HIT) hit = &info; });
        check("captured damage survives sweep storage reuse", hit && hit->acHitInfo && hit->acHitInfo->toucher.damage == 4);
        queue(); // overwrites scratch, not the delivered source
        check("delivered source is not scratch", hit && hit->acHit != &worldSweeps[0].quad.base);
        reset(ac); ConsumeWorldSweeps(play.get(), owner);
        check("same attack is not delivered twice", !(ac->acFlags & AC_HIT));
        ++epoch; queue(); ConsumeWorldSweeps(play.get(), owner);
        check("next attack can hit again", (ac->acFlags & AC_HIT) != 0);
    }
    reset(&cylinder.base); cylinder.base.acFlags |= AC_HARD;
    play->colChkCtx.colAC[0] = &cylinder.base;
    ++epoch; queue(); player->meleeWeaponQuads[0].base.atFlags = 0;
    ConsumeWorldSweeps(play.get(), owner);
    check("hard surface returns bounce to live sword", (player->meleeWeaponQuads[0].base.atFlags & AT_BOUNCED) != 0);
    reset(&cylinder.base); ++epoch; queue(); Destroyed(&target);
    ConsumeWorldSweeps(play.get(), owner);
    check("destroyed target is not delivered", !(cylinder.base.acFlags & AC_HIT));
    Created(&target); queue(); Invalidate(); ConsumeWorldSweeps(play.get(), owner);
    check("invalidated Player scope cancels sweeps", !(cylinder.base.acFlags & AC_HIT));
    reset(&cylinder.base); cylinder.info.bumper.dmgFlags = 2;
    ++epoch; queue(); ConsumeWorldSweeps(play.get(), owner);
    check("damage mask mismatch does not hit", !(cylinder.base.acFlags & AC_HIT));
    reset(&cylinder.base); ++epoch; queue(); worldSweeps[0].signDistance = 0;
    ConsumeWorldSweeps(play.get(), owner);
    check("nearer sign shields farther generic target", !(cylinder.base.acFlags & AC_HIT));
    const auto savedReservations = reservations;
    ContactEvent signEvent; signEvent.owner = owner; signEvent.attackEpoch = ++epoch;
    signEvent.hitOpportunity = 1; signEvent.targetGeneration = UINT64_MAX;
    const bool signReserved = reservations.Reserve(signEvent);
    queue(); worldSweeps[0].signReservation = reservations.sequence;
    ConsumeWorldSweeps(play.get(), owner);
    check("nearer generic target cancels conflicting sign reservation", signReserved &&
        reservations.events[reservations.count - 1].response == ResponseState::Rejected &&
        (cylinder.base.acFlags & AC_HIT));
    reservations = savedReservations;
    Actor nearTarget{}; nearTarget.id = ACTOR_EN_KUSA; nearTarget.update = update;
    Created(&nearTarget);
    ColliderCylinder nearCylinder = cylinder;
    nearCylinder.base.actor = &nearTarget;
    nearCylinder.dim.pos.z = 10; cylinder.dim.pos.z = -10;
    reset(&cylinder.base); reset(&nearCylinder.base);
    play->colChkCtx.colAC[1] = &nearCylinder.base; play->colChkCtx.colACCount = 2;
    ++epoch; queue(); ConsumeWorldSweeps(play.get(), owner);
    check("nearest target replaces farther contact", !(cylinder.base.acFlags & AC_HIT) &&
        (nearCylinder.base.acFlags & AC_HIT));
    Destroyed(&nearTarget); cylinder.dim.pos.z = 0;

    auto& body = player->cylinder;
    body.base.actor = &player->actor; body.base.shape = COLSHAPE_CYLINDER;
    body.base.ocFlags1 = OC1_ON | OC1_TYPE_ALL; body.base.ocFlags2 = OC2_TYPE_1;
    body.info.ocElemFlags = OCELEM_ON; body.dim = {10, 20, 0, {0, 0, 0}};
    cylinder.base.ocFlags1 = OC1_ON | OC1_TYPE_ALL; cylinder.base.ocFlags2 = OC2_TYPE_1;
    cylinder.info.ocElemFlags = OCELEM_ON; cylinder.dim.pos.x = 15;
    play->colChkCtx = {};
    PlayerContact_RegisterBody(play.get(), player.get(), true, false, false);
    PlayerContact_RegisterBody(play.get(), player.get(), true, false, false);
    check("body registered once across Player steps", play->colChkCtx.colOCCount == 1);
    play->colChkCtx.colOC[play->colChkCtx.colOCCount++] = &cylinder.base;
    CollisionCheck_OC(play.get(), &play->colChkCtx);
    check("reciprocal body push uses world mass", player->actor.colChkInfo.displacement.x < 0 &&
        target.colChkInfo.displacement.x > 0 &&
        player->actor.colChkInfo.displacement.x + target.colChkInfo.displacement.x == 0);
    PlayerContact_RegisterBody(play.get(), player.get(), false, false, false);
    check("inactive body unregisters", play->colChkCtx.colOCCount == 1 && play->colChkCtx.colOC[0] == &cylinder.base);
    auto& shield = player->shieldQuad;
    shield.base.actor = &player->actor; shield.base.shape = COLSHAPE_QUAD;
    PlayerContact_RegisterShield(play.get(), player.get());
    PlayerContact_RegisterShield(play.get(), player.get());
    check("shield registers AC and AT once", play->colChkCtx.colACCount == 1 && play->colChkCtx.colATCount == 1 &&
        play->colChkCtx.colAC[0] == &shield.base && play->colChkCtx.colAT[0] == &shield.base);
    play->colChkCtx = {};
    body.base.acFlags = AC_ON | AC_TYPE_ENEMY; body.base.colType = COLTYPE_NONE;
    body.info.bumperFlags = BUMP_ON | BUMP_NO_SWORD_SFX; body.info.bumper.dmgFlags = 1;
    cylinder.base.atFlags = AT_ON | AT_TYPE_ENEMY;
    cylinder.info.toucherFlags = TOUCH_ON; cylinder.info.toucher.dmgFlags = 1; cylinder.info.toucher.damage = 4;
    PlayerContact_RegisterBody(play.get(), player.get(), false, true, false);
    CollisionCheck_SetAT(play.get(), &play->colChkCtx, &cylinder.base);
    CollisionCheck_AT(play.get(), &play->colChkCtx);
    CollisionCheck_Damage(play.get(), &play->colChkCtx);
    check("world attack reaches Player damage collider", (body.base.acFlags & AC_HIT) &&
        player->actor.colChkInfo.damage == 4);
    play->colChkCtx = {}; player->actor.colChkInfo.damage = 0;
    shield.base.acFlags = AC_ON | AC_HARD | AC_TYPE_ENEMY; shield.base.colType = COLTYPE_NONE;
    shield.info.bumperFlags = BUMP_ON | BUMP_NO_SWORD_SFX; shield.info.bumper.dmgFlags = 1;
    Collider_SetQuadVertices(&shield, &qa, &qb, &qc, &qd);
    cylinder.dim.pos.x = 0;
    PlayerContact_RegisterShield(play.get(), player.get());
    CollisionCheck_SetAT(play.get(), &play->colChkCtx, &cylinder.base);
    CollisionCheck_AT(play.get(), &play->colChkCtx);
    CollisionCheck_Damage(play.get(), &play->colChkCtx);
    check("shield blocks world attack without health damage", (shield.base.acFlags & AC_HIT) &&
        (cylinder.base.atFlags & AT_BOUNCED) && player->actor.colChkInfo.damage == 0);
    Destroyed(&target); worldSweepCount = 0; worldHits.clear();
    return {{"checks", cases.size()}, {"failures", failures}, {"cases", cases}};
}
}
