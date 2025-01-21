/*
  Created on 29.07.18.
*/

#include <optional>
#include "mathlib/vector.h"
#include "cdll_int.h"
#include "common.hpp"

#pragma once
class IClientEntity;

namespace hacks::shared::anti_anti_aim
{

struct brutedata
{
    int brutenum{ 0 };
    int hits_in_a_row{ 0 };
    Vector original_angle{};
    Vector new_angle{};
    float last_successful_angle{ 0.0f };
};

// Full PlayerResolverData definition
struct PlayerResolverData {
    bool resolved;
    float resolved_yaw;
    bool is_fake;
    int resolve_type;  // 0 = none, 1 = legit aa, 2 = blatant aa
    float original_yaw;
    float last_delta;
    int missed_shots;
    float last_simtime;
    float avg_delta;
    int delta_samples;
    bool has_hit;
    float last_eye_yaw;
    float last_real_yaw;
    float last_velocity_yaw;
    int aa_type; // 0 = none, 1 = static, 2 = jitter, 3 = spin, 4 = random
    int shots_hit;
    int shots_fired;
    float hit_accuracy;
    PlayerResolverData() : resolved(false), resolved_yaw(0.0f), is_fake(false), resolve_type(0), 
                          original_yaw(0.0f), last_delta(0.0f), missed_shots(0), last_simtime(0.0f),
                          avg_delta(0.0f), delta_samples(0), has_hit(false), last_eye_yaw(0.0f),
                          last_real_yaw(0.0f), last_velocity_yaw(0.0f), aa_type(0), shots_hit(0),
                          shots_fired(0), hit_accuracy(0.0f) {}
};

extern boost::unordered_flat_map<unsigned, brutedata> resolver_map;
extern std::array<CachedEntity *, 32> sniperdot_array;
extern std::array<PlayerResolverData, MAX_PLAYERS> player_resolver_data;

void increaseBruteNum(int idx);
void frameStageNotify(ClientFrameStage_t stage);
bool IsAntiAiming(CachedEntity* entity);
void OnHit(CachedEntity* target);
void OnMiss(CachedEntity* target);
void UpdateResolver(int ent_idx, CachedEntity* player);
// void resolveEnt(int IDX, IClientEntity *entity = nullptr);
} // namespace hacks::shared::anti_anti_aim
