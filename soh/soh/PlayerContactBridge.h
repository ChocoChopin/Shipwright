#pragma once
#include <stdint.h>
struct Actor;
struct Player;
struct PlayState;
#ifdef __cplusplus
extern "C" {
#endif
void PlayerContact_SwordProduced(struct Player* player, const void* quad);
int PlayerContact_AdmitsCollider(const void* collider);
int PlayerContact_ManagedPlayerHit(const struct Player* player);
int PlayerContact_SignAnimation(const struct Actor* sign, int legacyAnimation);
#ifdef __cplusplus
}
#include "PlayerTemporalCore.hpp"
#include <nlohmann/json.hpp>
namespace PlayerContact {
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
}
#endif
