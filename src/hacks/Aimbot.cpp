/*
 * Aimbot.cpp
 *
 *  Created on: Oct 9, 2016
 *      Author: nullifiedcat
 */

#include <hacks/Aimbot.hpp>
#include <hacks/CatBot.hpp>
#include <hacks/AntiAim.hpp>
#include <hacks/ESP.hpp>
#include <hacks/Backtrack.hpp>
#include <PlayerTools.hpp>
#include <settings/Bool.hpp>
#include "common.hpp"
#include <targethelper.hpp>
#include "MiscTemporary.hpp"
#include "hitrate.hpp"
#include "FollowBot.hpp"
#include "Warp.hpp"
#include "AntiCheatBypass.hpp"
#include "NavBot.hpp"
namespace hacks::shared::aimbot
{
static settings::Boolean normal_enable{ "aimbot.enable", "false" };
static settings::Button aimkey{ "aimbot.aimkey.button", "<null>" };
static settings::Int aimkey_mode{ "aimbot.aimkey.mode", "1" };
static settings::Boolean autoshoot{ "aimbot.autoshoot", "1" };
static settings::Boolean autoreload{ "aimbot.autoshoot.activate-heatmaker", "false" };
static settings::Boolean autoshoot_disguised{ "aimbot.autoshoot-disguised", "1" };
static settings::Boolean multipoint{ "aimbot.multipoint", "0" };
static settings::Int vischeck_hitboxes{ "aimbot.vischeck-hitboxes", "0" };
static settings::Int hitbox_mode{ "aimbot.hitbox-mode", "0" };
static settings::Float normal_fov{ "aimbot.fov", "0" };
static settings::Int priority_mode{ "aimbot.priority-mode", "0" };
static settings::Boolean wait_for_charge{ "aimbot.wait-for-charge", "0" };
static settings::Boolean wait_for_headshot{ "aimbot.wait-for-headshot", "0" };

static settings::Boolean silent{ "aimbot.silent", "1" };
static settings::Boolean target_lock{ "aimbot.lock-target", "false" };
#if ENABLE_VISUALS
static settings::Boolean assistance_only{ "aimbot.assistance.only", "0" };
#endif
static settings::Int hitbox{ "aimbot.hitbox", "0" };
static settings::Boolean zoomed_only{ "aimbot.zoomed-only", "1" };
static settings::Boolean only_can_shoot{ "aimbot.can-shoot-only", "1" };

static settings::Boolean extrapolate{ "aimbot.extrapolate", "0" };
static settings::Int normal_slow_aim{ "aimbot.slow", "0" };
static settings::Int miss_chance{ "aimbot.miss-chance", "0" };

static settings::Boolean projectile_aimbot{ "aimbot.projectile.enable", "true" };
static settings::Float proj_gravity{ "aimbot.projectile.gravity", "0" };
static settings::Float proj_speed{ "aimbot.projectile.speed", "0" };
static settings::Float proj_start_vel{ "aimbot.projectile.initial-velocity", "0" };

static settings::Float sticky_autoshoot{ "aimbot.projectile.sticky-autoshoot", "0.5" };

static settings::Boolean aimbot_debug{ "aimbot.debug", "0" };

static settings::Boolean autorev{ "aimbot.autorev.enable", "0" };
static settings::Int autorev_distance{ "aimbot.autorev.distance", "1850" };
static settings::Boolean minigun_tapfire{ "aimbot.auto.tapfire", "false" };
static settings::Boolean auto_zoom{ "aimbot.auto.zoom", "0" };
static settings::Boolean auto_unzoom{ "aimbot.auto.unzoom", "0" };
static settings::Int zoom_distance{ "aimbot.zoom.distance", "1850" }; // that's default zoom distance

static settings::Boolean backtrackAimbot{ "aimbot.backtrack", "0" };
static settings::Boolean backtrackLastTickOnly("aimbot.backtrack.only-last-tick", "true");
static bool force_backtrack_aimbot = false;
static settings::Boolean backtrackVischeckAll{ "aimbot.backtrack.vischeck-all", "0" };

// TODO maybe these should be moved into "Targeting"
static settings::Float max_range{ "aimbot.target.max-range", "4096" };
static settings::Boolean ignore_vaccinator{ "aimbot.target.ignore-vaccinator", "1" };
static settings::Boolean ignore_deadringer{ "aimbot.target.ignore-deadringer", "1" };
static settings::Boolean buildings_sentry{ "aimbot.target.sentry", "1" };
static settings::Boolean buildings_other{ "aimbot.target.other-buildings", "1" };
static settings::Boolean npcs{ "aimbot.target.npcs", "1" };
static settings::Boolean stickybot{ "aimbot.target.stickybomb", "0" };
static settings::Boolean rageonly{ "aimbot.target.ignore-non-rage", "0" };
static settings::Int teammates{ "aimbot.target.teammates", "0" };

/*
 * 0 Always on
 * 1 Disable if being spectated in first person
 * 2 Disable if being spectated
 */
static settings::Int specmode("aimbot.spectator-mode", "0");
static settings::Boolean specenable("aimbot.spectator.enable", "0");
static settings::Float specfov("aimbot.spectator.fov", "0");
static settings::Int specslow("aimbot.spectator.slow", "0");

settings::Boolean engine_projpred{ "aimbot.debug.engine-pp", "1" };
int slow_aim;
float fov;
bool enable;
bool projectile_self_damage = false;
#if ENABLE_VISUALS
static settings::Boolean fov_draw{ "aimbot.fov-circle.enable", "0" };
static settings::Float fovcircle_opacity{ "aimbot.fov-circle.opacity", "0.7" };
#endif

int PreviousX, PreviousY;
int CurrentX, CurrentY;

float last_mouse_check = 0;
float stop_moving_time = 0;

// Used to make rapidfire not knock your enemies out of range
unsigned last_target_ignore_timer = 0;
settings::Boolean ignore_cloak{ "aimbot.target.ignore-cloaked-spies", "1" };
// Projectile info
bool projectile_mode{ false };
float cur_proj_speed{ 0.0f };
float cur_proj_grav{ 0.0f };
float cur_proj_start_vel{ 0.0f };

bool shouldbacktrack_cache = false;
// Func to find value of how far to target ents
inline float EffectiveTargetingRange()
{
    if (GetWeaponMode() == weapon_melee)
        return (float) re::C_TFWeaponBaseMelee::GetSwingRange(RAW_ENT(LOCAL_W));
    else if (LOCAL_W->m_iClassID() == CL_CLASS(CTFFlameThrower))
        return 310.0f; // Pyros only have so much until their flames hit
    else if (LOCAL_W->m_iClassID() == CL_CLASS(CTFWeaponFlameBall))
        return 512.0f; // Dragons Fury is fast but short range

    return (float) max_range;
}
inline bool isHitboxMedium(int hitbox)
{
    switch (hitbox)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        return true;

    default:
        return false;
    }
}
inline bool playerTeamCheck(CachedEntity *entity)

{
    return (int) teammates == 2 || (entity->m_bEnemy() && !teammates) || (!entity->m_bEnemy() && teammates) || (CE_GOOD(LOCAL_W) && LOCAL_W->m_iClassID() == CL_CLASS(CTFCrossbow) && entity->m_iHealth() < entity->m_iMaxHealth());
}
// Am I holding Hitman's Heatmaker ?
inline bool CarryingHeatmaker()
{
    return CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 752;
}
// A function to find the best hitbox for a target
inline int BestHitbox(CachedEntity *target)
{
    switch (*hitbox_mode)
    {
    case 0: // AUTO priority
        {
            int hitbox = autoHitbox(target);
            
            if (g_pLocalPlayer->holding_sniper_rifle && hitbox == hitbox_t::head)
            {
                // Get head hitbox
                auto head = target->hitboxes.GetHitbox(hitbox);
                if (!head)
                    return hitbox;
                    
                // Calculate more precise aim point for head
                Vector center = head->center;
                const float dist = center.DistTo(g_pLocalPlayer->v_Eye);
                
                // Dynamic adjustment based on distance and target velocity
                Vector velocity;
                velocity::EstimateAbsVelocity(RAW_ENT(target), velocity);
                float speed = velocity.Length2D();
                
                // Adjust aim point based on distance and movement
                if (dist > 800.0f)
                {
                    // Aim slightly higher for long range
                    center.z += std::min(1.0f, dist / 2000.0f);
                    
                    // Lead moving targets slightly
                    if (speed > 50.0f)
                    {
                        Vector normalized = velocity;
                        normalized.NormalizeInPlace();
                        center += normalized * std::min(2.0f, speed / 400.0f);
                    }
                }
                
                // Verify visibility
                trace_t trace;
                if (IsEntityVectorVisible(target, center, true, MASK_SHOT_HULL, &trace))
                {
                    target->hitboxes.GetHitbox(hitbox)->center = center;
                }
            }
            return hitbox;
        }
        break;
    case 1: // Closest to crosshair
        return ClosestHitbox(target);
    case 2: // Static
        return *hitbox;
    default:
        break;
    }
    return -1;
}
inline float projectileHitboxSize(int projectile_size)
{
    switch (projectile_size)
    {
    case CL_CLASS(CTFRocketLauncher):
    case CL_CLASS(CTFRocketLauncher_Mortar):
    case CL_CLASS(CTFRocketLauncher_AirStrike):
    case CL_CLASS(CTFRocketLauncher_DirectHit):
    case CL_CLASS(CTFPipebombLauncher):
    case CL_CLASS(CTFGrenadeLauncher):
    case CL_CLASS(CTFCannon):
        return 4.0f;
    case CL_CLASS(CTFFlareGun):
    case CL_CLASS(CTFFlareGun_Revenge):
    case CL_CLASS(CTFDRGPomson):
        return 2.0f;
    case CL_CLASS(CTFSyringeGun):
    case CL_CLASS(CTFCompoundBow):
        return 1.0f;
    default:
        return 3.0f;
    }
}
inline void updateShouldBacktrack()
{
    if (hacks::tf2::backtrack::hasData() || projectile_mode || !(*backtrackAimbot || force_backtrack_aimbot))
        shouldbacktrack_cache = false;
    else
        shouldbacktrack_cache = true;
}

inline bool shouldBacktrack(CachedEntity *ent)
{
    if (!shouldbacktrack_cache)
        return false;
    else if (ent && ent->m_Type() != ENTITY_PLAYER)
        return false;
    else if (!tf2::backtrack::getGoodTicks(ent))
        return false;
    return true;
}
void spectatorUpdate()
{
    switch (*specmode)
    {
    // Always on
    default:
    case 0:
        break;
        // Disable if being spectated in first person
    case 1:
    {
        if (g_pLocalPlayer->spectator_state == g_pLocalPlayer->FIRSTPERSON)
        {
            enable   = *specenable;
            slow_aim = *specslow;
            fov      = *specfov;
        }
        break;
    }
        // Disable if being spectated
    case 2:
    {
        if (g_pLocalPlayer->spectator_state != g_pLocalPlayer->NONE)
        {
            enable   = *specenable;
            slow_aim = *specslow;
            fov      = *specfov;
        }
    };
    }
}

#define GET_MIDDLE(c1, c2) (corners[c1] + corners[c2]) / 2.0f

// Get all the valid aim positions
std::vector<Vector> getValidHitpoints(CachedEntity *ent, int hitbox)
{
    // Recorded vischeckable points
    std::vector<Vector> hitpoints;
    auto hb = ent->hitboxes.GetHitbox(hitbox);

    trace_t trace;

    if (IsEntityVectorVisible(ent, hb->center, true, MASK_SHOT_HULL, &trace, true))
    {
        if (trace.hitbox == hitbox)
            hitpoints.push_back(hb->center);
    }
    // Multipoint
    auto bboxmin = hb->bbox->bbmin;
    auto bboxmax = hb->bbox->bbmax;

    auto transform = ent->hitboxes.GetBones()[hb->bbox->bone];
    QAngle rotation;
    Vector origin;

    MatrixAngles(transform, rotation, origin);

    Vector corners[8];
    GenerateBoxVertices(origin, rotation, bboxmin, bboxmax, corners);

    float shrink_size = 1;

    if (!isHitboxMedium(hitbox)) // hitbox should be chosen based on size.
        shrink_size = 3;
    else
        shrink_size = 6;

    // Shrink positions by moving towards opposing corner
    for (int i = 0; i < 8; ++i)
        corners[i] += (corners[7 - i] - corners[i]) / shrink_size;

    // Generate middle points on line segments
    // Define cleans up code

    const Vector line_positions[12] = { GET_MIDDLE(0, 1), GET_MIDDLE(0, 2), GET_MIDDLE(1, 3), GET_MIDDLE(2, 3), GET_MIDDLE(7, 6), GET_MIDDLE(7, 5), GET_MIDDLE(6, 4), GET_MIDDLE(5, 4), GET_MIDDLE(0, 4), GET_MIDDLE(1, 5), GET_MIDDLE(2, 6), GET_MIDDLE(3, 7) };

    // Create combined vector
    std::vector<Vector> positions;

    positions.reserve(sizeof(Vector) * 20);
    positions.insert(positions.end(), corners, &corners[8]);
    positions.insert(positions.end(), line_positions, &line_positions[12]);

    for (int i = 0; i < 20; ++i)
    {
        trace_t trace;

        if (IsEntityVectorVisible(ent, positions[i], true, MASK_SHOT_HULL, &trace, true))
        {
            if (trace.hitbox == hitbox)
                hitpoints.push_back(positions[i]);
        }
    }
    if (*vischeck_hitboxes)
    {
        if (*vischeck_hitboxes == 1 && playerlist::AccessData(ent).state != playerlist::k_EState::RAGE)
        {
            return hitpoints;
        }
        int i                  = 0;
        const u_int8_t max_box = ent->hitboxes.GetNumHitboxes();
        while (hitpoints.empty() && i < max_box) // Prevents returning empty at all costs. Loops through every hitbox
        {
            if (i == hitbox)
            {
                ++i;
                continue;
            }
            hitpoints = getHitpointsVischeck(ent, i);
            ++i;
        }
    }

    return hitpoints;
}
std::vector<Vector> getHitpointsVischeck(CachedEntity *ent, int hitbox)
{
    std::vector<Vector> hitpoints;
    auto hb      = ent->hitboxes.GetHitbox(hitbox);
    auto bboxmin = hb->bbox->bbmin;
    auto bboxmax = hb->bbox->bbmax;

    auto transform = ent->hitboxes.GetBones()[hb->bbox->bone];
    QAngle rotation;
    Vector origin;

    MatrixAngles(transform, rotation, origin);

    Vector corners[8];
    GenerateBoxVertices(origin, rotation, bboxmin, bboxmax, corners);

    float shrink_size = 1;

    if (!isHitboxMedium(hitbox)) // hitbox should be chosen based on size.
        shrink_size = 3;
    else
        shrink_size = 6;

    // Shrink positions by moving towards opposing corner
    for (int i = 0; i < 8; ++i)
        corners[i] += (corners[7 - i] - corners[i]) / shrink_size;

    // Generate middle points on line segments
    // Define cleans up code

    const Vector line_positions[12] = { GET_MIDDLE(0, 1), GET_MIDDLE(0, 2), GET_MIDDLE(1, 3), GET_MIDDLE(2, 3), GET_MIDDLE(7, 6), GET_MIDDLE(7, 5), GET_MIDDLE(6, 4), GET_MIDDLE(5, 4), GET_MIDDLE(0, 4), GET_MIDDLE(1, 5), GET_MIDDLE(2, 6), GET_MIDDLE(3, 7) };

    // Create combined vector
    std::vector<Vector> positions;

    positions.reserve(sizeof(Vector) * 20);
    positions.insert(positions.end(), corners, &corners[8]);
    positions.insert(positions.end(), line_positions, &line_positions[12]);

    for (int i = 0; i < 20; ++i)
    {
        trace_t trace;

        if (IsEntityVectorVisible(ent, positions[i], true, MASK_SHOT_HULL, &trace, true))
        {
            if (trace.hitbox == hitbox)
                hitpoints.push_back(positions[i]);
        }
    }

    return hitpoints;
}
// Get the best point to aim at for a given hitbox
std::optional<Vector> getBestHitpoint(CachedEntity *ent, int hitbox)
{
    auto positions = getValidHitpoints(ent, hitbox);

    std::optional<Vector> best_pos = std::nullopt;
    float max_score                = FLT_MAX;
    for (auto const &position : positions)
    {
        float score = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, position);
        if (score < max_score)
        {
            best_pos  = position;
            max_score = score;
        }
    }

    return best_pos;
}

// Reduce Backtrack lag by checking if the ticks hitboxes are within a reasonable FOV range
bool validateTickFOV(tf2::backtrack::BacktrackData &tick)
{
    if (fov)
    {
        bool valid_fov = false;
        for (auto &hitbox : tick.hitboxes)
        {
            float score = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, hitbox.center);
            // Check if the FOV is within a 2.0f threshhold
            if (score < fov + 2.0f)
            {
                valid_fov = true;
                break;
            }
        }
        return valid_fov;
    }
    return true;
}

void doAutoZoom(bool target_found)
{
    bool isIdle = target_found ? false : hacks::shared::followbot::isIdle();
    auto nearest = hacks::tf2::NavBot::getNearestPlayerDistance();

    // Keep track of our zoom/rev time
    static Timer stateTimer{};

    // autorev
    if (autorev && g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFMinigun))
    {
        bool should_rev = target_found;
        
        // Check distance and idle conditions
        if (nearest.second < *autorev_distance || (isIdle && !stateTimer.check(3000)))
        {
            should_rev = true;
            if (target_found)
                stateTimer.update();
        }
            
        // Update rev state
        if (should_rev && !(current_user_cmd->buttons & IN_ATTACK))
            current_user_cmd->buttons |= IN_ATTACK2;
        else if (stateTimer.check(3000) && (current_user_cmd->buttons & IN_ATTACK2))
            current_user_cmd->buttons &= ~IN_ATTACK2;
            
        return;
    }

    // Handle sniper zoom
    if (auto_zoom && g_pLocalPlayer->holding_sniper_rifle && (target_found || isIdle || nearest.second < *zoom_distance))
    {
        if (target_found)
            stateTimer.update();
        if (not g_pLocalPlayer->bZoomed)
            current_user_cmd->buttons |= IN_ATTACK2;
    }
    else if (!target_found && nearest.second >= *zoom_distance)
    {
        // Auto-Unzoom
        if (auto_unzoom)
            if (g_pLocalPlayer->holding_sniper_rifle && g_pLocalPlayer->bZoomed && stateTimer.check(3000))
                current_user_cmd->buttons |= IN_ATTACK2;
    }
}

// Current Entity
AimbotTarget_t target_last;
bool aimed_this_tick      = false;
Vector viewangles_this_tick(0.0f);


bool slow_can_shoot = false;
bool projectileAimbotRequired;


struct PredictionSystem 
{
    
    struct MovementSimulation 
    {
        Vector position;
        Vector velocity;
        Vector acceleration;
        float groundTime;
        bool onGround;
        bool ducking;
    };

    
    struct ProjectileSimulation 
    {
        Vector position;
        Vector velocity;
        float time;
        bool hasCollided;
    };

    static MovementSimulation SimulateMovement(CachedEntity* target, float time, bool useEngine = true) 
    {
        MovementSimulation result;
        

        result.position = target->m_vecOrigin();
        Vector tempVel;
        velocity::EstimateAbsVelocity(RAW_ENT(target), result.velocity);
        result.acceleration = Vector(0, 0, 0);
        result.onGround = CE_INT(target, netvar.iFlags) & FL_ONGROUND;
        result.ducking = CE_INT(target, netvar.iFlags) & FL_DUCKING;
        
        if (useEngine && g_IEngine->IsInGame())
        {
            // Use engine prediction when available
            auto player = RAW_ENT(target);
            if (player)
            {
                QAngle engineAngles;
                g_IEngine->GetViewAngles(engineAngles);
                
           
                result.position = player->GetAbsOrigin() + (result.velocity * time);
                velocity::EstimateAbsVelocity(player, tempVel);
                result.velocity = tempVel;
            }
        }

        // Apply gravity
        const float gravity = g_ICvar->FindVar("sv_gravity")->GetFloat();
        if (!result.onGround)
        {
            result.acceleration.z = -gravity;
            result.velocity.z += result.acceleration.z * time;
        }

        // Apply air resistance and friction
        const float friction = g_ICvar->FindVar("sv_friction")->GetFloat();
        const float stopSpeed = 100.0f; // Minimum speed before full stop
        
        if (result.onGround)
        {
            float speed = result.velocity.Length2D();
            if (speed > 0)
            {
                float drop = speed * friction * time;
                float newSpeed = std::max(0.0f, speed - drop);
                if (newSpeed != speed)
                {
                    newSpeed /= speed;
                    result.velocity.x *= newSpeed;
                    result.velocity.y *= newSpeed;
                }
            }
        }
        
        // Air movement
        else 
        {
            float airControl = 0.3f;
            Vector airAccel = result.velocity;
            airAccel.NormalizeInPlace();
            airAccel *= airControl;
            result.velocity += airAccel * time;
        }

        // Apply movement bounds
        float maxSpeed = 320.0f;
        if (result.ducking)
            maxSpeed *= 0.33f;

        // Clamp velocity
        if (result.velocity.Length2D() > maxSpeed)
        {
            float ratio = maxSpeed / result.velocity.Length2D();
            result.velocity.x *= ratio;
            result.velocity.y *= ratio;
        }

        // Update position
        result.position += result.velocity * time;
        
        return result;
    }

    static ProjectileSimulation SimulateProjectile(const Vector& startPos, const Vector& startVel, float gravity, float time)
    {
        ProjectileSimulation result;
        result.position = startPos;
        result.velocity = startVel;
        result.time = 0.0f;
        result.hasCollided = false;

        float timeStep = 0.01f; // 10ms simulation steps
        
        while (result.time < time && !result.hasCollided)
        {
            // Update position
            result.position += result.velocity * timeStep;
            
            // Apply gravity
            result.velocity.z -= gravity * timeStep;
            
            // Check for collisions
            trace_t trace;
            Ray_t ray;
            ray.Init(result.position, result.position + result.velocity * timeStep);
            
            class CTraceFilterSimple : public CTraceFilter
            {
            public:
                virtual bool ShouldHitEntity(IHandleEntity* pEntity, int contentsMask)
                {
                    return pEntity != skip;
                }
                virtual TraceType_t GetTraceType() const
                {
                    return TRACE_EVERYTHING;
                }
                IHandleEntity* skip;
            };

            CTraceFilterSimple filter;
            filter.skip = RAW_ENT(LOCAL_E);
            g_ITrace->TraceRay(ray, MASK_SHOT, &filter, &trace);
            
            if (trace.DidHit())
            {
                result.hasCollided = true;
                result.position = trace.endpos;
            }
            
            result.time += timeStep;
        }
        
        return result;
    }

    static Vector PredictTarget(CachedEntity* target, float projectileSpeed, float projectileGravity, bool allowPerfect = false)
    {
        if (!target)
            return {};

        // Get target info
        Vector targetPos = target->m_vecOrigin();
        Vector targetVel;
        velocity::EstimateAbsVelocity(RAW_ENT(target), targetVel);
        
      
        if (target->m_Type() == ENTITY_PLAYER)
        {
            auto hitbox = target->hitboxes.GetHitbox(autoHitbox(target));
            if (hitbox)
                targetPos = hitbox->center;
        }
        
        
        float dist = targetPos.DistTo(g_pLocalPlayer->v_Eye);
        float initialTime = dist / projectileSpeed;
        
       
        const int MAX_ITERATIONS = 3;
        float predictionTime = initialTime;
        Vector bestPrediction = targetPos;
        bool foundGoodPrediction = false;
        
        for (int i = 0; i < MAX_ITERATIONS && !foundGoodPrediction; i++)
        {
            
            auto movement = SimulateMovement(target, predictionTime);
            
            
            Vector shootDir = (movement.position - g_pLocalPlayer->v_Eye);
            shootDir.NormalizeInPlace();
            Vector projectileVel = shootDir * projectileSpeed;
            
            auto projectile = SimulateProjectile(g_pLocalPlayer->v_Eye, projectileVel, projectileGravity, predictionTime);
            
      
            float newDist = projectile.position.DistTo(movement.position);
            float newTime = newDist / projectileSpeed;
            
           
            if (std::abs(newTime - predictionTime) < 0.05f || allowPerfect)
            {
                bestPrediction = movement.position;
                foundGoodPrediction = true;
            }
            else
            {
                predictionTime = (predictionTime + newTime) * 0.5f;
            }
        }

 
        if (target->m_Type() == ENTITY_PLAYER)
        {

            float latencyCompensation = g_IEngine->GetNetChannelInfo() ? 
                (g_IEngine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING) +
                 g_IEngine->GetNetChannelInfo()->GetLatency(FLOW_INCOMING)) : 0.0f;
            
            bestPrediction += targetVel * latencyCompensation;
            

            if (!allowPerfect)
            {
                float randomSpread = 1.0f;
                bestPrediction.x += RandFloatRange(-randomSpread, randomSpread);
                bestPrediction.y += RandFloatRange(-randomSpread, randomSpread);
                bestPrediction.z += RandFloatRange(-randomSpread, randomSpread);
            }
        }

        return bestPrediction;
    }
};

// The main "loop" of the aimbot.
static void CreateMove()
{
    enable          = *normal_enable;
    slow_aim        = *normal_slow_aim;
    fov             = *normal_fov;
    aimed_this_tick = false;

    bool aimkey_status = UpdateAimkey();

    if (*specmode != 0)
        spectatorUpdate();
    if (!enable)
    {
        target_last.valid = false;
        return;
    }
    else if (!LOCAL_E->m_bAlivePlayer() || !g_pLocalPlayer->entity)
    {
        target_last.valid = false;
        return;
    }
    else if (!aimkey_status || !ShouldAim())
    {
        target_last.valid = false;
        return;
    }
    // Unless we're using slow aim, we do not want to aim while reloading
    if (*only_can_shoot && !slow_aim && !CanShoot())
        return;

    doAutoZoom(false);
    if (hacks::tf2::antianticheat::enabled)
        fov = std::fmin(fov > 0.0f ? fov : FLT_MAX, 10.0f);
    bool should_backtrack    = hacks::tf2::backtrack::backtrackEnabled();
    int get_weapon_mode      = g_pLocalPlayer->weapon_mode;
    projectile_mode          = false;
    projectileAimbotRequired = false;
    bool should_zoom         = *auto_zoom;
    switch (get_weapon_mode)
    {
    case weapon_hitscan:
    {
        if (should_backtrack)
            updateShouldBacktrack();
        target_last = RetrieveBestTarget(aimkey_status);

        if (target_last.valid)
        {
            Aim(target_last);
            if (should_zoom)
                doAutoZoom(true);
            int weapon_case = LOCAL_W->m_iClassID();
            if (!hitscanSpecialCases(target_last, weapon_case))
                DoAutoshoot(target_last);
        }
        break;
    }
    case weapon_melee:
    {
        if (should_backtrack)
            updateShouldBacktrack();
        target_last = RetrieveBestTarget(aimkey_status);
        if (target_last.valid)
        {
            Aim(target_last);
            DoAutoshoot(target_last);
        }
        break;
    }
    case weapon_projectile:
    case weapon_throwable:
    {
        if (projectile_aimbot)
        {
            projectileAimbotRequired = true;
            projectile_mode          = GetProjectileData(g_pLocalPlayer->weapon(), cur_proj_speed, cur_proj_grav, cur_proj_start_vel);
            if (!projectile_mode)
            {
                target_last.valid = false;
                return;
            }
            if (proj_speed)
                cur_proj_speed = *proj_speed;
            if (proj_gravity)
                cur_proj_grav = *proj_gravity;
            if (proj_start_vel)
                cur_proj_start_vel = *proj_start_vel;
            target_last = RetrieveBestTarget(aimkey_status);
            if (target_last.valid)
            {
                int weapon_case = g_pLocalPlayer->weapon()->m_iClassID();
                if (projectileSpecialCases(target_last, weapon_case))
                {
                    Aim(target_last);
                    DoAutoshoot(target_last);
                }
            }
        }
        break;
    }
    }
}

bool projectileSpecialCases(AimbotTarget_t target_entity, int weapon_case)
{

    switch (weapon_case)
    {
    case CL_CLASS(CTFCompoundBow):
    {
        bool release = false;
        if (autoshoot)
            current_user_cmd->buttons |= IN_ATTACK;
        // Grab time when charge began
        float begincharge = CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flChargeBeginTime);
        float charge      = g_GlobalVars->curtime - begincharge;
        if (!begincharge)
            charge = 0.0f;
        int damage        = std::floor(50.0f + 70.0f * fminf(1.0f, charge));
        int charge_damage = std::floor(50.0f + 70.0f * fminf(1.0f, charge)) * 3.0f;
        if (HasCondition<TFCond_Slowed>(LOCAL_E) && (autoshoot || !(current_user_cmd->buttons & IN_ATTACK)) && (!wait_for_charge || (charge >= 1.0f || damage >= target_entity.ent->m_iHealth() || charge_damage >= target_entity.ent->m_iHealth())))
            release = true;
        return release;
        break;
    }
    case CL_CLASS(CTFCannon):
    {
        bool release = false;
        if (autoshoot)
            current_user_cmd->buttons |= IN_ATTACK;
        float detonate_time = CE_FLOAT(LOCAL_W, netvar.flDetonateTime);
        // Currently charging up
        if (detonate_time > g_GlobalVars->curtime)
        {
            if (wait_for_charge)
            {
                // Shoot when a straight shot would result in only 100ms left on fuse upon target hit
                float best_charge = PredictEntity(target_entity).DistTo(g_pLocalPlayer->v_Eye) / cur_proj_speed + 0.1;
                if (detonate_time - g_GlobalVars->curtime <= best_charge)
                    release = true;
            }
            else
                release = true;
        }
        return release;
        break;
    }
    case CL_CLASS(CTFPipebombLauncher):
    {
        float chargebegin = CE_FLOAT(LOCAL_W, netvar.flChargeBeginTime);
        float chargetime  = g_GlobalVars->curtime - chargebegin;

        DoAutoshoot();
        bool currently_charging_pipe = false;

        // Grenade started charging
        if (chargetime < 6.0f && chargetime && chargebegin)
            currently_charging_pipe = true;

        // Grenade was released
        if (!(current_user_cmd->buttons & IN_ATTACK) && currently_charging_pipe)
        {
            currently_charging_pipe = false;
            Aim(target_entity);
            return false;
        }
        break;
    }
    default:
        return true;
    }
    return true;
}
int tapfire_delay = 0;
bool hitscanSpecialCases(AimbotTarget_t target_entity, int weapon_case)
{
    switch (weapon_case)
    {
    case CL_CLASS(CTFMinigun):
    {
        if (!minigun_tapfire)
            DoAutoshoot(target_entity);
        else
        {
            // Used to keep track of what tick we're in right now
            tapfire_delay++;

            // This is the exact delay needed to hit
            if (17 <= tapfire_delay || target_entity.ent->m_flDistance() <= 1250.0f)
            {
                DoAutoshoot(target_entity);
                tapfire_delay = 0;
            }
            return true;
        }
        return true;
        break;
    }
    default:
        return false;
    }
}
// Just hold m1 if we were aiming at something before and are in rapidfire
static void CreateMoveWarp()
{
    if (hacks::tf2::warp::in_rapidfire && aimed_this_tick)
    {
        current_user_cmd->viewangles     = viewangles_this_tick;
        g_pLocalPlayer->bUseSilentAngles = *silent;
        current_user_cmd->buttons |= IN_ATTACK;
    }
    // Warp should call aimbot normally
    else if (!hacks::tf2::warp::in_rapidfire)
        CreateMove();
}

#if ENABLE_VISUALS
bool MouseMoving()
{
    if ((g_GlobalVars->curtime - last_mouse_check) < 0.02)
    {
        SDL_GetMouseState(&PreviousX, &PreviousY);
    }
    else
    {
        SDL_GetMouseState(&CurrentX, &CurrentY);
        last_mouse_check = g_GlobalVars->curtime;
    }

    if (PreviousX != CurrentX || PreviousY != CurrentY)
        stop_moving_time = g_GlobalVars->curtime + 0.5;

    if (g_GlobalVars->curtime <= stop_moving_time)
        return true;
    else
        return false;
}
#endif

// The first check to see if the player should aim in the first place
bool ShouldAim()
{
    // Checks should be in order: cheap -> expensive

    // Check for +use
    if (current_user_cmd->buttons & IN_USE)
        return false;
    // Check if using action slot item
    else if (g_pLocalPlayer->using_action_slot_item)
        return false;
    // Using a forbidden weapon?
    else if (!g_pLocalPlayer->weapon() || g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFKnife) || CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 237 || CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 265)
        return false;

    // Carrying A building?
    else if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bCarryingObject))
        return false;
    // Deadringer out?
    else if (CE_BYTE(g_pLocalPlayer->entity, netvar.m_bFeignDeathReady))
        return false;
    else if (g_pLocalPlayer->holding_sapper)
        return false;
    // Is bonked?
    else if (HasCondition<TFCond_Bonked>(g_pLocalPlayer->entity))
        return false;
    // Is taunting?
    else if (HasCondition<TFCond_Taunting>(g_pLocalPlayer->entity))
        return false;
    // Is cloaked
    else if (IsPlayerInvisible(g_pLocalPlayer->entity))
        return false;
    else if (LOCAL_W->m_iClassID() == CL_CLASS(CTFMinigun) && CE_INT(LOCAL_E, netvar.m_iAmmo + 4) == 0)
        return false;
#if ENABLE_VISUALS
    if (assistance_only && !MouseMoving())
        return false;
#endif

    return true;
}

// Function to find a suitable target
AimbotTarget_t RetrieveBestTarget(bool aimkey_state)
{
    // If we have a previously chosen target, target lock is on, and the aimkey
    // is allowed, then attempt to keep the previous target

    if (target_lock && target_last.valid && aimkey_state)
    {
        AimbotTarget_t target = GetTarget(target_last.ent);
        if (shouldBacktrack(target_last.ent))
        {
            auto good_ticks_tmp = hacks::tf2::backtrack::getGoodTicks(target.ent);
            if (good_ticks_tmp)
            {
                auto good_ticks = *good_ticks_tmp;
                if (backtrackLastTickOnly)
                {
                    good_ticks.clear();
                    good_ticks.push_back(good_ticks_tmp->back());
                }
                for (auto &bt_tick : good_ticks)
                {
                    if (!validateTickFOV(bt_tick))
                        continue;
                    hacks::tf2::backtrack::MoveToTick(bt_tick);
                    target = GetTarget(target.ent); // get target for the tick
                    if (target.valid)
                        return target;
                    // Restore if bad target
                    hacks::tf2::backtrack::RestoreEntity(target.ent->m_IDX);
                }
            }

            // Check if previous target is still good
            else if (!shouldbacktrack_cache && target.valid)
            {
                // If it is then return it again
                return target;
            }
        }
    }
    // No last_target found, reset the timer.
    hacks::shared::aimbot::last_target_ignore_timer = 0;

    float target_highest_score, scr = 0.0f;
    AimbotTarget_t target_best;
    target_highest_score                                        = -256;
    std::optional<hacks::tf2::backtrack::BacktrackData> bt_tick = std::nullopt;
    for (auto const &ent : entity_cache::valid_ents)
    {
        // Check for null and dormant
        // Check whether the current ent is good enough to target
        AimbotTarget_t target;
        static std::optional<hacks::tf2::backtrack::BacktrackData> temp_bt_tick = std::nullopt;
        if (shouldBacktrack(ent))
        {
            auto good_ticks_tmp = tf2::backtrack::getGoodTicks(ent);
            if (good_ticks_tmp)
            {
                auto good_ticks = *good_ticks_tmp;
                if (backtrackLastTickOnly)
                {
                    good_ticks.clear();
                    good_ticks.push_back(good_ticks_tmp->back());
                }
                for (auto &bt_tick : good_ticks)
                {
                    if (!validateTickFOV(bt_tick))
                        continue;
                    hacks::tf2::backtrack::MoveToTick(bt_tick);
                    target = GetTarget(ent);
                    if (target.valid)
                    {
                        temp_bt_tick = bt_tick;
                        break;
                    }
                    hacks::tf2::backtrack::RestoreEntity(ent->m_IDX);
                }
            }
        }
        else
        {
            target = GetTarget(ent);
        }
        if (target.valid) // Melee mode straight up won't swing if the target is too far away. No need to prioritize based on distance. Just use whatever the user chooses.
        {
            switch ((int) priority_mode)
            {
            case 0: // Smart Priority
            {
                scr = GetScoreForEntity(ent);
                break;
            }
            case 1: // Fov Priority
            {
                scr = 360.0f - target.fov;
                break;
            }
            case 2:
            {
                scr = 4096.0f - target.aim_position.DistTo(g_pLocalPlayer->v_Eye);
                break;
            }
            case 3: // Health Priority (Lowest)
            {
                scr = 450.0f - ent->m_iHealth();
                break;
            }
            case 4: // Distance Priority (Furthest Away)
            {
                scr = target.aim_position.DistTo(g_pLocalPlayer->v_Eye);
                break;
            }
            case 5: // Health Priority (Highest)
            {
                scr = ent->m_iHealth() * 4;
                break;
            }
            case 6: // Fast
            {

                return target;
            }
            default:
                break;
            }
            // Crossbow logic
            if (!ent->m_bEnemy() && ent->m_Type() == ENTITY_PLAYER && CE_GOOD(LOCAL_W) && LOCAL_W->m_iClassID() == CL_CLASS(CTFCrossbow))
            {
                scr = ((ent->m_iMaxHealth() - ent->m_iHealth()) / ent->m_iMaxHealth()) * (*priority_mode == 2 ? 16384.0f : 2000.0f);
            }
            // Compare the top score to our current ents score
            if (scr > target_highest_score)
            {
                target_highest_score = scr;
                target_best   = target;
                bt_tick              = temp_bt_tick;
            }
        }

        // Restore tick
        if (shouldBacktrack(ent))
            hacks::tf2::backtrack::RestoreEntity(ent->m_IDX);
    }
    if (target_best.valid && bt_tick)
        hacks::tf2::backtrack::MoveToTick(*bt_tick);
    return target_best;
}

// Get the target information from a entity, valid is set to false replacing the bool return value
AimbotTarget_t GetTarget(CachedEntity *entity)
{
    PROF_SECTION(PT_aimbot_targetstatecheck);
    AimbotTarget_t t;
    t.ent = entity;
    const int current_type = entity->m_Type();
    bool is_player         = false;


    switch (current_type)
    {

    case (ENTITY_PLAYER):
    {
        // Local player check
        if (entity == LOCAL_E)
            return t;
        // Dead
        else if (!entity->m_bAlivePlayer())
            return t;
        // Teammates
        else if (!playerTeamCheck(entity))
            return t;
        else if (!player_tools::shouldTarget(entity))
            return t;
        // Invulnerable players, ex: uber, bonk
        else if (IsPlayerInvulnerable(entity))
            return t;
        // Distance
        is_player             = true;
        float targeting_range = EffectiveTargetingRange();
        if (entity->m_flDistance() - 40 > targeting_range && tickcount > hacks::shared::aimbot::last_target_ignore_timer) // m_flDistance includes the collision box. You have to subtract it (Should be the same for every model)
            return t;

        // Rage only check
        if (rageonly)
        {
            if (playerlist::AccessData(entity).state != playerlist::k_EState::RAGE)
            {
                return t;
            }
        }

        // Wait for charge
        if (wait_for_charge && g_pLocalPlayer->holding_sniper_rifle)
        {
            float cdmg  = CE_FLOAT(LOCAL_W, netvar.flChargedDamage) * 3;
            float maxhs = 450.0f;
            if (CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 230 || HasCondition<TFCond_Jarated>(entity))
            {
                cdmg  = int(CE_FLOAT(LOCAL_W, netvar.flChargedDamage) * 1.35f);
                maxhs = 203.0f;
            }
            bool maxCharge = cdmg >= maxhs;

            // Darwins damage correction, Darwins protects against 15% of
            // damage
            //                if (HasDarwins(entity))
            //                    cdmg = (cdmg * .85) - 1;
            // Vaccinator damage correction, Vac charge protects against 75%
            // of damage
            if (IsPlayerInvisible(entity))
                cdmg = (cdmg * .80) - 1;

            else if (HasCondition<TFCond_UberBulletResist>(entity))
            {
                cdmg = (cdmg * .25) - 1;
                // Passive bullet resist protects against 10% of damage
            }
            else if (HasCondition<TFCond_SmallBulletResist>(entity))
            {
                cdmg = (cdmg * .90) - 1;
            }
            // Invis damage correction, Invis spies get protection from 10%
            // of damage

            // Check if player will die from headshot or if target has more
            // than 450 health and sniper has max chage
            float hsdmg = 150.0f;
            if (CE_INT(LOCAL_W, netvar.iItemDefinitionIndex) == 230)
                hsdmg = int(50.0f * 1.35f);

            int health = entity->m_iHealth();
            if (!(health <= hsdmg || health <= cdmg || !g_pLocalPlayer->bZoomed || (maxCharge && health > maxhs)))
            {
                return t;
            }
        }

        // Some global checks

        // cloaked/deadringed players
        if (ignore_cloak || ignore_deadringer)
        {
            if (IsPlayerInvisible(entity))
            {
                // Item id for deadringer is 59 as of time of creation
                if (HasWeapon(entity, 59))
                {
                    if (ignore_deadringer)
                        return t;
                }
                else
                {
                    if (ignore_cloak && !(HasCondition<TFCond_OnFire>(entity)) && !(HasCondition<TFCond_CloakFlicker>(entity)))
                        return t;
                }
            }
        }
        // Vaccinator
        if (ignore_vaccinator && IsPlayerResistantToCurrentWeapon(entity))
            return t;

        t.hitbox = BestHitbox(entity);
        // ngl i would love to avoid nesting this many ifs by using a goto but i have morals.
        if (*vischeck_hitboxes && !*multipoint && is_player)
        {
            if (!(*vischeck_hitboxes == 1 && playerlist::AccessData(entity).state != playerlist::k_EState::RAGE || (projectileAimbotRequired && 0.01f < cur_proj_grav)))
            {
                int i = 0;
                trace_t first_tracer;

                if (!IsEntityVectorVisible(entity, entity->hitboxes.GetHitbox(t.hitbox)->center, true, MASK_SHOT_HULL, &first_tracer, true))
                {
                    const u_int8_t max_box = entity->hitboxes.GetNumHitboxes();
                    bool found = false;
                    while (i < max_box) // Prevents returning empty at all costs. Loops through every hitbox
                    {
                        if (i == t.hitbox)
                        {
                            ++i;
                            continue;
                        }
                        trace_t test_trace;

                        Vector centered_hitbox = entity->hitboxes.GetHitbox(i)->center;

                        if (IsEntityVectorVisible(entity, centered_hitbox, true, MASK_SHOT_HULL, &test_trace, true))
                        {
                            t.hitbox = i;
                            found = true;
                        }
                        ++i;
                    }
                    if (!found)
                        return t; // It looped through every hitbox and found nothing. It isn't visible.
                }
            }
        }
        t.valid = true;
        break;
    }
        // Check for buildings
    case (ENTITY_BUILDING):
    {
        // Enabled check
        if (!(buildings_other || buildings_sentry))
            return t;
        // Teammates, Even with friendly fire enabled, buildings can NOT be
        // damaged
        else if (!entity->m_bEnemy())
            return t;
        // Distance
        else if (EffectiveTargetingRange())
        {
            if (entity->m_flDistance() - 40 > EffectiveTargetingRange() && tickcount > hacks::shared::aimbot::last_target_ignore_timer)
                return t;
        }

        // Building type
        else if (!(buildings_other && buildings_sentry))
        {
            // Check if target is a sentrygun
            if (entity->m_iClassID() == CL_CLASS(CObjectSentrygun))
            {
                if (!buildings_sentry)
                    return t;
                // Other
            }
            else
            {
                if (!buildings_other)
                    return t;
            }
        }
        t.valid = true;
        break;
    }
    case (ENTITY_NPC):
    {
        // NPCs (Skeletons, Merasmus, etc)

        // NPC targeting is disabled
        if (!npcs)
            return t;

        // Cannot shoot this
        else if (entity->m_iTeam() == LOCAL_E->m_iTeam())
            return t;

        // Distance
        float targeting_range = EffectiveTargetingRange();

        if (entity->m_flDistance() - 40 > targeting_range && tickcount > hacks::shared::aimbot::last_target_ignore_timer)
            return t;

        t.valid = true;
        break;
    }
    default:
        break;
    }
    // Check for stickybombs
    if (entity->m_iClassID() == CL_CLASS(CTFGrenadePipebombProjectile))
    {
        // Enabled
        if (!stickybot)
            return t;

        // Only hitscan weapons can break stickys so check for them.
        else if (!(GetWeaponMode() == weapon_hitscan || GetWeaponMode() == weapon_melee))
            return t;

        // Distance
        float targeting_range = EffectiveTargetingRange();
        if (entity->m_flDistance() > targeting_range)
            return t;

        // Teammates, Even with friendly fire enabled, stickys can NOT be
        // destroied
        if (!entity->m_bEnemy())
            return t;

        // Check if target is a pipe bomb
        if (CE_INT(entity, netvar.iPipeType) != 1)
            return t;

        // Moving Sticky?
        Vector velocity;
        velocity::EstimateAbsVelocity(RAW_ENT(entity), velocity);
        if (!velocity.IsZero())
            return t;

        t.valid = true;
    }

    if (t.valid)
    {
        Vector is_it_good = PredictEntity(t);
        if (!projectileAimbotRequired)
        {
            if (!IsEntityVectorVisible(entity, is_it_good, true, MASK_SHOT_HULL, nullptr, true))
            {
                t.valid = false;
                return t;
            }
        }

        Vector angles = CalculateAimAngles(g_pLocalPlayer->v_Eye, is_it_good, LOCAL_E);

        if (projectileAimbotRequired) // unfortunately you have to check this twice, otherwise you'd have to run GetAimAtAngles far too early
        {
            const Vector &orig   = getShootPos(angles);
            const bool grav_comp = (0.01f < cur_proj_grav);
            if (grav_comp)
            {
                const QAngle &angl = VectorToQAngle(angles);
                Vector end_targ    = is_it_good;
                Vector fwd, right, up;
                AngleVectors3(angl, &fwd, &right, &up);
                // I have no clue why this is 200.0f, No where in the SDK explains this.
                // It appears to work though
                Vector vel = fwd * cur_proj_speed + up * 200.0f;
                vel *= 0.9f;
                fwd.z      = 0.0f;
                fwd.NormalizeInPlace();
                float alongvel = std::sqrt(vel.x * vel.x + vel.y * vel.y);
                fwd *= alongvel;
                const float gravity  = cur_proj_grav * g_ICvar->FindVar("sv_gravity")->GetFloat() * -1.0f;
                const float maxTime  = 2.5f;
                const float timeStep = 0.01f;
                Vector curr_pos      = orig;
                trace_t ptr_trace;
                Vector last_pos                 = orig;
                const IClientEntity *rawest_ent = RAW_ENT(entity);
                for (float tm = 0.0f; tm < maxTime; tm += timeStep, last_pos = curr_pos)
                {
                    curr_pos.x = orig.x + fwd.x * tm;
                    curr_pos.y = orig.y + fwd.y * tm;
                    curr_pos.z = orig.z + vel.z * tm + 0.5f * gravity * tm * tm;
                    if (!didProjectileHit(last_pos, curr_pos, entity, projectileHitboxSize(LOCAL_W->m_iClassID()), true, &ptr_trace) || (IClientEntity *) ptr_trace.m_pEnt == rawest_ent)
                    {
                        break;
                    }
                }
                if (!didProjectileHit(end_targ, ptr_trace.endpos, entity, projectileHitboxSize(LOCAL_W->m_iClassID()), true, &ptr_trace))
                {
                    t.valid = false;
                    return t;
                }
                Vector ent_check = entity->m_vecOrigin();
                if (!didProjectileHit(last_pos, ent_check, entity, projectileHitboxSize(LOCAL_W->m_iClassID()), false))
                {
                    t.valid = false;
                    return t;
                }
            }
            else if (!didProjectileHit(orig, is_it_good, entity, projectileHitboxSize(LOCAL_W->m_iClassID()), grav_comp))
            {
                t.valid = false;
                return t;
            }
        }
        if (fov > 0 && t.fov > fov)
            t.valid = false;
    }
    return t;
}

float secant_x(float in)
{
    return 1.0f / (cos(in));
}
float csc_x(float in)
{
    return 1.0f / (sin(in));
}

bool Aim(AimbotTarget_t target)
{
    if (*miss_chance > 0 && UniformRandomInt(0, 99) < *miss_chance)
        return true;

    // Don't aim if waiting for headshot and not aiming at head
    if (*wait_for_headshot)
    {
        // For Sniper Rifle - check if we can headshot
        if (g_pLocalPlayer->holding_sniper_rifle)
        {
            if (target.hitbox != hitbox_t::head)
                return false;
            if (!g_pLocalPlayer->bZoomed)
                return false;
        }
        // For Ambassador - check if we can headshot
        else if (IsAmbassador(g_pLocalPlayer->weapon()))
        {
            if (target.hitbox != hitbox_t::head)
                return false;
            // Only check basic headshot capability
            if (!AmbassadorCanHeadshot())
                return false;
        }
    }

    Vector angles = CalculateAimAngles(g_pLocalPlayer->v_Eye, target.aim_position, LOCAL_E);
    
    if (slow_aim)
    {
        QAngle view;
        g_IEngine->GetViewAngles(view);
        
        // Calculate angle delta
        Vector delta = angles - Vector(view.x, view.y, view.z);
        fClampAngle(delta);
        
        float factor = std::min(1.0f, 1.0f / (slow_aim * 2.5f));
        angles = Vector(view.x, view.y, view.z) + delta * factor;
    }

#if ENABLE_VISUALS
    if (target.ent->m_Type() == ENTITY_PLAYER)
        hacks::shared::esp::SetEntityColor(target.ent, colors::target);
#endif


    current_user_cmd->viewangles = angles;
    
    if (silent && !slow_aim)
        g_pLocalPlayer->bUseSilentAngles = true;
        
    if (!shouldBacktrack(target.ent) && nolerp && target.ent->m_IDX <= g_IEngine->GetMaxClients())
        current_user_cmd->tick_count = TIME_TO_TICKS(CE_FLOAT(target.ent, netvar.m_flSimulationTime));
        
    aimed_this_tick = true;
    viewangles_this_tick = angles;
    
    return true;
}


Vector CalculateAimAngles(const Vector& eyePosition, const Vector& targetPosition, CachedEntity* localEntity) 
{
    Vector delta = targetPosition - eyePosition;
    
    // Apply micro-adjustments for better head hitbox targeting
    if (g_pLocalPlayer->weapon_mode == weapon_hitscan && 
        g_pLocalPlayer->holding_sniper_rifle)
    {
        float dist = targetPosition.DistTo(eyePosition);
        
        // Dynamic compensation based on distance
        if (dist > 1000.0f) 
        {
            delta.z += 0.5f + (dist - 1000.0f) * 0.0001f; // Progressive vertical adjustment
        }
        
        // Micro-adjustments for moving targets
        if (auto target = CurrentTarget())
        {
            Vector velocity;
            velocity::EstimateAbsVelocity(RAW_ENT(target), velocity);
            float speed = velocity.Length2D();
            
            if (speed > 50.0f)
            {
                // Lead the target slightly based on movement
                Vector normalized = velocity;
                normalized.NormalizeInPlace();
                delta += normalized * (speed * 0.001f);
            }
        }
    }

    float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    
    Vector angles;
    angles.x = (float)(RAD2DEG(atan2(-delta.z, hyp)));
    angles.y = (float)(RAD2DEG(atan2(delta.y, delta.x)));
    angles.z = 0.0f;

    // Enhanced angle normalization
    fClampAngle(angles);
    
    // Apply micro-corrections for better accuracy
    if (g_pLocalPlayer->weapon_mode == weapon_hitscan)
    {
        // Fine adjustment for vertical angle
        angles.x = std::floor(angles.x * 100.0f) / 100.0f;
        
        // Fine adjustment for horizontal angle
        angles.y = std::floor(angles.y * 100.0f) / 100.0f;
    }
    
    return angles;
}

// A function to check whether player can autoshoot
bool begancharge = false;
int begansticky  = 0;
void DoAutoshoot(AimbotTarget_t target)
{
    // Enable check
    if (!autoshoot)
        return;
    else if (IsPlayerDisguised(g_pLocalPlayer->entity) && !autoshoot_disguised)
        return;
    // Handle Huntsman/Loose cannon
    else if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFCompoundBow) || g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFCannon))
    {
        if (!only_can_shoot)
        {
            if (!begancharge)
            {
                current_user_cmd->buttons |= IN_ATTACK;
                begancharge = true;
                return;
            }
        }
        begancharge = false;
        current_user_cmd->buttons &= ~IN_ATTACK;
        hacks::shared::antiaim::SetSafeSpace(5);
        return;
    }
    else
        begancharge = false;
    if (g_pLocalPlayer->weapon()->m_iClassID() == CL_CLASS(CTFPipebombLauncher))
    {
        float chargebegin = CE_FLOAT(LOCAL_W, netvar.flChargeBeginTime);
        float chargetime  = g_GlobalVars->curtime - chargebegin;

        // Release Sticky if > chargetime, 3.85 is the max second chargetime,
        // but we also need to consider the release time supplied by the user
        if ((chargetime >= 3.85f * *sticky_autoshoot) && begansticky > 3)
        {
            current_user_cmd->buttons &= ~IN_ATTACK;
            hacks::shared::antiaim::SetSafeSpace(5);
            begansticky = 0;
        }
        // Else just keep charging
        else
        {
            current_user_cmd->buttons |= IN_ATTACK;
            begansticky++;
        }
        return;
    }
    else
        begansticky = 0;
    bool attack = true;

    // Rifle check
    if (g_pLocalPlayer->clazz == tf_class::tf_sniper)
    {
        if (g_pLocalPlayer->holding_sniper_rifle)
        {
            if (zoomed_only && !CanHeadshot())
                attack = false;
            // Wait for headshot check
            if (*wait_for_headshot && target.valid)
            {
                // Only shoot if we're aiming at the head
                if (target.hitbox != hitbox_t::head)
                    attack = false;
                // And can headshot
                if (!CanHeadshot())
                    attack = false;
            }
        }
    }

    // Ambassador check
    else if (IsAmbassador(g_pLocalPlayer->weapon()))
    {
        // Check if ambassador can headshot
        if (!AmbassadorCanHeadshot() && (wait_for_charge || *wait_for_headshot))
            attack = false;
        // Wait for headshot check - only shoot if aiming at head
        if (*wait_for_headshot && target.valid && target.hitbox != hitbox_t::head)
            attack = false;
    }

    // Autoshoot breaks with Slow aimbot, so use a workaround to detect when it
    // can
    else if (slow_aim && !slow_can_shoot)
        attack = false;

    // Dont autoshoot without anything in clip
    else if (CE_INT(g_pLocalPlayer->weapon(), netvar.m_iClip1) == 0)
        attack = false;

    if (attack)
    {
        // TO DO: Sending both reload and attack will activate the hitmans heatmaker ability
        // Don't activate it only on first kill (or somehow activate it before a shot)
        current_user_cmd->buttons |= IN_ATTACK | (*autoreload && CarryingHeatmaker() ? IN_RELOAD : 0);
        if (target.valid)
        {
            auto hitbox = target.hitbox;
            hitrate::AimbotShot(target.ent->m_IDX, hitbox != head);
        }
        *bSendPackets = true;
    }
    if (LOCAL_W->m_iClassID() == CL_CLASS(CTFLaserPointer))
        current_user_cmd->buttons |= IN_ATTACK2;
}

// Update the projectile prediction in PredictEntity
Vector PredictEntity(AimbotTarget_t& target)
{
    Vector &result = target.aim_position;
    const short int curr_type = target.ent->m_Type();

    switch (curr_type)
    {
    case ENTITY_PLAYER:
    {
        if (projectileAimbotRequired)
        {
            // Get proper projectile speed and gravity
            float proj_speed = cur_proj_speed;
            float proj_grav = cur_proj_grav;

            // Adjust for specific weapons
            if (LOCAL_W->m_iClassID() == CL_CLASS(CTFCompoundBow))
            {
                float chargetime = g_GlobalVars->curtime - CE_FLOAT(LOCAL_W, netvar.flChargeBeginTime);
                if (chargetime > 1.0f) chargetime = 1.0f;
                proj_speed *= (float)((chargetime * 0.5f) + 0.5f);
            }

            result = PredictionSystem::PredictTarget(target.ent, proj_speed, proj_grav);
            
            // Verify prediction is reasonable
            if (!result.IsValid() || result.DistTo(g_pLocalPlayer->v_Eye) > 4096.0f)
            {
                target.valid = false;
                return result;
            }
        }
        else
        {
            if (extrapolate)
                result = SimpleLatencyPrediction(target.ent, target.hitbox);
            else
            {
                if (!*multipoint)
                {
                    result = target.ent->hitboxes.GetHitbox(target.hitbox)->center;
                    break;
                }

                std::optional<Vector> best_pos = getBestHitpoint(target.ent, target.hitbox);
                if (best_pos)
                    result = *best_pos;
            }
        }
        break;
    }
    case ENTITY_BUILDING:
    {
        if (cur_proj_grav != 0)
        {
            result = PredictionSystem::PredictTarget(target.ent, cur_proj_speed, cur_proj_grav);
            if (!result.IsValid())
            {
                target.valid = false;
                return result;
            }
        }
        else
            result = GetBuildingPosition(target.ent);
        break;
    }
    case ENTITY_NPC:
    {
        result = target.ent->hitboxes.GetHitbox(std::max(0, target.ent->hitboxes.GetNumHitboxes() / 2 - 1))->center;
        break;
    }
    default:
    {
        result = target.ent->m_vecOrigin();
        break;
    }
    }

    target.predict_tick = tickcount;
    target.fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, result);
    target.aim_position = result;
    return result;
}
int notVisibleHitbox(CachedEntity *target, int preferred)
{
    if (target->hitboxes.VisibilityCheck(preferred))
        return preferred;
    // Else attempt to find any hitbox at all
    else
        return hitbox_t::spine_1;
}
int autoHitbox(CachedEntity *target)
{
    int preferred = hitbox_t::spine_1;
    int target_health = target->m_iHealth();
    int ci = LOCAL_W->m_iClassID();

    if (CanHeadshot())
    {
        float cdmg = CE_FLOAT(LOCAL_W, netvar.flChargedDamage);
        float bdmg = 50;
        // Vaccinator damage correction, protects against 20% of damage
        if (CarryingHeatmaker())
        {
            bdmg = (bdmg * .85f) - 1;
            cdmg = (cdmg * .85f) - 1;
        }
        
        if (HasCondition<TFCond_UberBulletResist>(target))
        {
            bdmg = (bdmg * .20f) - 1;
            cdmg = (cdmg * .20f) - 1;
        }
        else if (HasCondition<TFCond_SmallBulletResist>(target))
        {
            bdmg = (bdmg * .90f) - 1;
            cdmg = (cdmg * .90f) - 1;
        }
        else if (IsPlayerInvisible(target))
        {
            bdmg = (bdmg * .80f) - 1;
            cdmg = (cdmg * .80f) - 1;
        }

        // Always prefer headshots for snipers unless guaranteed bodyshot kill
        if (g_pLocalPlayer->clazz == tf_class::tf_sniper)
        {
            if (std::floor(cdmg) >= target_health || 
                IsPlayerCritBoosted(g_pLocalPlayer->entity) || 
                (target_health <= std::floor(bdmg) && target_health <= 150))
            {
                preferred = hitbox_t::spine_1;
            }
            else
            {
                preferred = hitbox_t::head;
            }
        }
        
        return preferred;
    }

    // Hunstman
    else if (ci == CL_CLASS(CTFCompoundBow))
    {
        float begincharge = CE_FLOAT(g_pLocalPlayer->weapon(), netvar.flChargeBeginTime);
        float charge      = g_GlobalVars->curtime - begincharge;
        int damage        = std::floor(50.0f + 70.0f * fminf(1.0f, charge));
        if (damage >= target_health)
            return hitbox_t::spine_1;
        else
            return hitbox_t::head;
    }

    // Ambassador
    else if (IsAmbassador(g_pLocalPlayer->weapon()))
    {

        // 18 health is a good number to use as thats the usual minimum
        // damage it can do with a bodyshot, but damage could
        // potentially be higher

        if (target_health <= 18 || IsPlayerCritBoosted(g_pLocalPlayer->entity) || target->m_flDistance() > 1200)
            return hitbox_t::spine_1;
        else if (AmbassadorCanHeadshot())
            return hitbox_t::head;
    }

    // Rockets and stickies should aim at the foot if the target is on the ground
    else if (ci == CL_CLASS(CTFPipebombLauncher) || is_rocket(ci))
    {
        bool ground = CE_INT(target, netvar.iFlags) & (1 << 0);
        if (ground)
            preferred = notVisibleHitbox(target, hitbox_t::foot_L); // Only time it is worth the penalty
    }
    return preferred;
}

// Function to find the closesnt hitbox to the crosshair for a given ent
int ClosestHitbox(CachedEntity *target)
{
    // FIXME this will break multithreading if it will be ever implemented. When
    // implementing it, these should be made non-static
    int closest;
    float closest_fov, fov = 0.0f;

    closest     = -1;
    closest_fov = 256;
    for (int i = 0; i < target->hitboxes.GetNumHitboxes(); ++i)
    {
        fov = GetFov(g_pLocalPlayer->v_OrigViewangles, g_pLocalPlayer->v_Eye, target->hitboxes.GetHitbox(i)->center);
        if (fov < closest_fov || closest == -1)
        {
            closest     = i;
            closest_fov = fov;
        }
    }
    return closest;
}

// A helper function to find a user angle that isnt directly on the target
// angle, effectively slowing the aiming process
void DoSlowAim(Vector &input_angle)
{
    auto viewangles = current_user_cmd->viewangles;
    Vector slow_delta = input_angle - viewangles;

    while (slow_delta.y > 180) slow_delta.y -= 360;
    while (slow_delta.y < -180) slow_delta.y += 360;

    slow_delta /= slow_aim;
    input_angle = viewangles + slow_delta;

    fClampAngle(input_angle);

    slow_can_shoot = (std::abs(slow_delta.y) < 0.1 && std::abs(slow_delta.x) < 0.1);
}

// A function that determins whether aimkey allows aiming
bool UpdateAimkey()
{
    static bool aimkey_flip       = false;
    static bool pressed_last_tick = false;
    bool allow_aimkey             = true;
    static bool last_allow_aimkey = true;

    // Check if aimkey is used
    if (aimkey && aimkey_mode)
    {
        // Grab whether the aimkey is depressed
        bool key_down = aimkey.isKeyDown();
        switch ((int) aimkey_mode)
        {
        case 1: // Only while key is depressed
            if (!key_down)
                allow_aimkey = false;
            break;
        case 2: // Only while key is not depressed, enable
            if (key_down)
                allow_aimkey = false;
            break;
        case 3: // Aimkey acts like a toggle switch
            if (!pressed_last_tick && key_down)
                aimkey_flip = !aimkey_flip;
            if (!aimkey_flip)
                allow_aimkey = false;
            break;
        default:
            break;
        }
        // Huntsman and Loose Cannon need special logic since we aim upon m1 being released
        if (!autoshoot && CE_GOOD(LOCAL_W) && (LOCAL_W->m_iClassID() == CL_CLASS(CTFCompoundBow) || LOCAL_W->m_iClassID() == CL_CLASS(CTFCannon)))
        {
            if (!allow_aimkey && last_allow_aimkey)
            {
                allow_aimkey      = true;
                last_allow_aimkey = false;
            }
            else
                last_allow_aimkey = allow_aimkey;
        }
        else
            last_allow_aimkey = allow_aimkey;
        pressed_last_tick = key_down;
    }
    // Return whether the aimkey allows aiming
    return allow_aimkey;
}

// Used mostly by navbot to not accidentally look at path when aiming
bool isAiming()
{
    return aimed_this_tick;
}

// A function used by gui elements to determine the current target
CachedEntity *CurrentTarget()
{
    return target_last.valid ? target_last.ent : nullptr;
}

// Used for when you join and leave maps to reset aimbot vars
void Reset()
{
    target_last = {};
    projectile_mode = false;
}

#if ENABLE_VISUALS
static void DrawText()
{
    // Dont draw to screen when aimbot is disabled
    if (!enable)
        return;

    // Fov ring to represent when a target will be shot
    if (fov_draw)
    {
        // It cant use fovs greater than 180, so we check for that
        if (float(fov) > 0.0f && float(fov) < 180)
        {
            // Dont show ring while player is dead
            if (CE_GOOD(LOCAL_E) && LOCAL_E->m_bAlivePlayer())
            {
                rgba_t color = colors::gui;
                color.a      = float(fovcircle_opacity);

                int width, height;
                g_IEngine->GetScreenSize(width, height);

                // Math
                float mon_fov  = (float(width) / float(height) / (4.0f / 3.0f));
                float fov_real = RAD2DEG(2 * atanf(mon_fov * tanf(DEG2RAD(draw::fov / 2))));
                float radius   = tan(DEG2RAD(float(fov)) / 2) / tan(DEG2RAD(fov_real) / 2) * (width);

                draw::Circle(width / 2, height / 2, radius, color, 1, 100);
            }
        }
    }
    // Debug stuff
    if (!aimbot_debug)
        return;
    for (auto const &ent : entity_cache::player_cache)
    {

        Vector screen;
        Vector oscreen;
        if (draw::WorldToScreen(target_last.aim_position, screen) && draw::WorldToScreen(ent->m_vecOrigin(), oscreen))
        {
            draw::Rectangle(screen.x - 2, screen.y - 2, 4, 4, colors::white);
            draw::Line(oscreen.x, oscreen.y, screen.x - oscreen.x, screen.y - oscreen.y, colors::EntityF(ent), 0.5f);
        }
    }
}
#endif
void rvarCallback(settings::VariableBase<float> &, float after)
{
    force_backtrack_aimbot = after >= 200.0f;
}
static InitRoutine EC(
    []()
    {
        hacks::tf2::backtrack::latency.installChangeCallback(rvarCallback);
        EC::Register(EC::LevelInit, Reset, "INIT_Aimbot", EC::average);
        EC::Register(EC::LevelShutdown, Reset, "RESET_Aimbot", EC::average);
        EC::Register(EC::CreateMove, CreateMove, "CM_Aimbot", EC::late);
        EC::Register(EC::CreateMoveWarp, CreateMoveWarp, "CMW_Aimbot", EC::late);
#if ENABLE_VISUALS
        EC::Register(EC::Draw, DrawText, "DRAW_Aimbot", EC::average);
#endif
    });

// Improve prediction logic for moving targets
Vector PredictEntityPosition(CachedEntity* target, float projectileSpeed, float projectileGravity) {
    Vector predictedPosition = target->m_vecOrigin();
    Vector velocity;
    velocity::EstimateAbsVelocity(RAW_ENT(target), velocity);
    float time = target->m_flDistance() / projectileSpeed;
    predictedPosition += velocity * time;
    predictedPosition.z += 0.5f * projectileGravity * time * time;
    return predictedPosition;
}

} // namespace hacks::shared::aimbot
