/*
 * antiaim.cpp
 *
 *  Created on: Jun 5, 2017
 *      Author: nullifiedcat
 */

#include <hacks/AntiCheat.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include "angles.hpp"

namespace ac::antiaim
{
static settings::Boolean enable{ "find-cheaters.antiaim.enable", "true" };
static settings::Boolean detect_legit{ "find-cheaters.antiaim.detect-legit", "true" };
static settings::Int min_detections{ "find-cheaters.antiaim.min-detections", "3" };

unsigned long last_accusation[MAX_PLAYERS]{ 0 };

struct DetectionData {
    float last_simtime;
    float original_yaw;
    float last_delta;
    float avg_delta;
    int delta_samples;
    float last_velocity_yaw;
    int detections;
    int aa_type; // 0 = none, 1 = legit, 2 = jitter, 3 = spin, 4 = random
    bool warned;
    DetectionData() : last_simtime(0.0f), original_yaw(0.0f), last_delta(0.0f), 
                      avg_delta(0.0f), delta_samples(0), last_velocity_yaw(0.0f),
                      detections(0), aa_type(0), warned(false) {}
};

static std::array<DetectionData, MAX_PLAYERS> detection_data;

void ResetEverything()
{
    memset(last_accusation, 0, sizeof(unsigned long) * MAX_PLAYERS);
    detection_data.fill(DetectionData());
}

void ResetPlayer(int idx)
{
    last_accusation[idx - 1] = 0;
    detection_data[idx - 1] = DetectionData();
}

int amount[MAX_PLAYERS] = {};

bool DetectAntiAim(CachedEntity* player, DetectionData& data) 
{
    if (!player || player->m_Type() != ENTITY_PLAYER)
        return false;

    float simtime = CE_FLOAT(player, netvar.m_flSimulationTime);
    
    // Skip if player hasn't moved/updated
    if (simtime == data.last_simtime)
        return false;
        
    data.last_simtime = simtime;
    
    // Get current angles and velocity
    Vector angles = CE_VECTOR(player, netvar.m_angEyeAngles);
    float curr_yaw = angles.y;
    
    Vector velocity;
    velocity::EstimateAbsVelocity(RAW_ENT(player), velocity);
    QAngle vel_angles;
    VectorAngles(velocity, vel_angles);
    float velocity_yaw = vel_angles.y;
    
    // Normalize angles
    while (curr_yaw > 180) curr_yaw -= 360;
    while (curr_yaw < -180) curr_yaw += 360;

    // Calculate delta from previous
    float delta = std::abs(curr_yaw - data.original_yaw);
    if (delta > 180) delta = 360 - delta;
    
    // Calculate velocity delta
    float vel_delta = std::abs(curr_yaw - velocity_yaw);
    if (vel_delta > 180) vel_delta = 360 - vel_delta;
    
    // Update average delta with decay
    if (data.delta_samples < 10) {
        data.avg_delta = (data.avg_delta * data.delta_samples + delta) / (data.delta_samples + 1);
        data.delta_samples++;
    } else {
        data.avg_delta = data.avg_delta * 0.9f + delta * 0.1f;
    }
    
    data.original_yaw = curr_yaw;
    data.last_delta = delta;
    data.last_velocity_yaw = velocity_yaw;

    bool is_aa = false;
    
    // Check for legit anti-aim first
    if (detect_legit && velocity.Length2D() > 0.1f) {
        // Check if player is strafing suspiciously
        if (vel_delta > 130.0f && vel_delta < 150.0f) {
            is_aa = true;
            data.aa_type = 1; // Legit AA
        }
    }
    
    // Check for blatant anti-aim
    if (!is_aa) {
        // Large consistent angle changes
        if (data.avg_delta > 90.0f && data.delta_samples >= 5) {
            is_aa = true;
            data.aa_type = 2; // Jitter
        }
            
        // Detect spin
        static float last_angles[MAX_PLAYERS];
        static int spin_count[MAX_PLAYERS];
        
        if (std::abs(curr_yaw - last_angles[player->m_IDX]) > 30.0f) {
            spin_count[player->m_IDX]++;
            if (spin_count[player->m_IDX] >= 3) {
                is_aa = true;
                data.aa_type = 3; // Spin
            }
        } else {
            spin_count[player->m_IDX] = 0;
        }
        
        last_angles[player->m_IDX] = curr_yaw;
        
        // Detect random
        if (data.avg_delta > 45.0f && data.delta_samples >= 5 && !is_aa) {
            float randomness = std::abs(delta - data.last_delta);
            if (randomness > 30.0f) {
                is_aa = true;
                data.aa_type = 4; // Random
            }
        }
    }

    if (is_aa && data.delta_samples >= *min_detections)
        data.detections++;
        
    return is_aa;
}

void Update(CachedEntity *player)
{
    if (!enable)
        return;
        
    if (tickcount - last_accusation[player->m_IDX - 1] < 60 * 60)
        return;
        
    auto &data = detection_data[player->m_IDX - 1];
    
    if (DetectAntiAim(player, data)) {
        if (data.detections >= *min_detections && !data.warned) {
            player_info_t info;
            GetPlayerInfo(player->m_IDX, &info);
            
            std::string aa_type;
            switch (data.aa_type) {
                case 1: aa_type = "Legit"; break;
                case 2: aa_type = "Jitter"; break;
                case 3: aa_type = "Spin"; break;
                case 4: aa_type = "Random"; break;
                default: aa_type = "Unknown"; break;
            }
            
            std::string reason = format("Type: ", aa_type, " Delta: ", data.avg_delta);
            
            if (data.aa_type > 1) { // Blatant AA
                hacks::shared::anticheat::SetRage(info);
            }
            
            hacks::shared::anticheat::Accuse(player->m_IDX, "AntiAim", reason);
            last_accusation[player->m_IDX - 1] = tickcount;
            data.warned = true;
        }
    }
}
} // namespace ac::antiaim
