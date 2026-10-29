#pragma once
#include <stdint.h>
struct Actor;
struct Player;
struct PlayState;
#ifdef __cplusplus
extern "C" {
#endif
void PlayerContact_SwordProduced(struct Player* player, const void* quad);
void PlayerContact_RegisterBody(struct PlayState* play, struct Player* player, int oc, int ac, int at);
void PlayerContact_RegisterShield(struct PlayState* play, struct Player* player);
void PlayerContact_ResolveBlockingBody(struct PlayState* play, struct Player* player);
int PlayerContact_AdmitsCollider(const void* collider);
int PlayerContact_ManagedPlayerHit(const struct Player* player);
int PlayerContact_SignAnimation(const struct Actor* sign, int legacyAnimation);
#ifdef __cplusplus
}
#include "PlayerTemporalCore.hpp"
#include <nlohmann/json.hpp>
namespace PlayerContact {
struct BodyMotion { bool evaluated; float before[3], after[3]; };
BodyMotion ObserveBodyMotion();
void Scene();
void Created(Actor* actor);
void Destroyed(Actor* actor);
void Invalidate();
void BeginStep();
void Capture(PlayState* play, const PlayerTemporal::WorldStepContext& world);
void Detect(PlayState* play, const PlayerTemporal::PlayerStepContext& step,
            const PlayerTemporal::AttackState& attack);
void Consume(PlayState* play, PlayerTemporal::Identity owner, PlayerTemporal::SimTime now);
nlohmann::json Inspect();
// Asset-free geometry/lifetime checks invoked only by an explicit engine fixture.
nlohmann::json ValidateWorldContacts();
}
#endif
