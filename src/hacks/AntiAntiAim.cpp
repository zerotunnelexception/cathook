/*
 * Created on 29.07.18.
 */

#include "common.hpp"
#include "hacks/AntiAntiAim.hpp"
#include "sdk/dt_recv_redef.h"

namespace hacks::shared::anti_anti_aim
{
static settings::Boolean enable{ "anti-anti-aim.enable", "false" };
static settings::Boolean debug{ "anti-anti-aim.debug.enable", "false" };
static settings::Int resolver_mode{ "anti-anti-aim.resolver.mode", "0" };
static settings::Int min_hits{ "anti-anti-aim.resolver.min-hits", "2" };

boost::unordered_flat_map<unsigned, brutedata> resolver_map;
std::array<CachedEntity *, 32> sniperdot_array;

static inline void modifyAngles()
{
    for (auto const &player : entity_cache::player_cache)
    {

        if (CE_BAD(player) || !player->m_bAlivePlayer() || !player->m_bEnemy() || !player->player_info->friendsID)
            continue;
        auto &data  = resolver_map[player->player_info->friendsID];
        auto &angle = CE_VECTOR(player, netvar.m_angEyeAngles);
        angle.x     = data.new_angle.x;
        angle.y     = data.new_angle.y;
    }
}
static inline void CreateMove()
{
    // Empty the array
    sniperdot_array.fill(0);
    // Find sniper dots
    for (auto &dot_ent : entity_cache::valid_ents)
    {

        // Not a sniper dot
        if (dot_ent->m_iClassID() != CL_CLASS(CSniperDot))
            continue;
        // Get the player it belongs to
        auto ent_idx = HandleToIDX(CE_INT(dot_ent, netvar.m_hOwnerEntity));
        // IDX check
        if (IDX_BAD(ent_idx) || ent_idx > sniperdot_array.size() || ent_idx <= 0)
            continue;
        // Good sniper dot, add to array
        sniperdot_array.at(ent_idx - 1) = dot_ent;
    }
}

void frameStageNotify(ClientFrameStage_t stage)
{
#if !ENABLE_TEXTMODE
    if (!enable || !g_IEngine->IsInGame())
        return;
    if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START)
    {
        modifyAngles();
    }
#endif
}

static std::array<float, 8> yaw_resolves{ 0.0f, 180.0f, 90.0f, -90.0f, 45.0f, -45.0f, 135.0f, -135.0f };

static float resolveAngleYaw(float angle, brutedata &brute)
{
    brute.original_angle.y = angle;
    while (angle > 180)
        angle -= 360;

    while (angle < -180)
        angle += 360;

    // If we've hit the target enough times with this angle, keep using it
    if (brute.hits_in_a_row >= *min_hits)
        return brute.new_angle.y;
        
    // Different modes have different strategies
    switch (*resolver_mode) 
    {
    case 0: // Aggressive - try all angles quickly
    {
        int entry = (brute.brutenum % yaw_resolves.size());
        angle += yaw_resolves[entry];
        break;
    }
    case 1: // Adaptive - base on success rate
    {
        int entry = (int)std::floor((brute.brutenum / 2.0f)) % yaw_resolves.size();
        if (brute.hits_in_a_row > 0)
            entry = brute.last_successful_angle;
        angle += yaw_resolves[entry];
        break;
    }
    case 2: // Conservative - change angles slowly
    {
        int entry = (int)std::floor((brute.brutenum / 4.0f)) % yaw_resolves.size();
        angle += yaw_resolves[entry];
        break;
    }
    }

    while (angle > 180)
        angle -= 360;

    while (angle < -180)
        angle += 360;
        
    brute.new_angle.y = angle;
    return angle;
}

static float resolveAnglePitch(float angle, brutedata &brute, CachedEntity *ent)
{
    brute.original_angle.x = angle;

    // Get CSniperDot associated with entity
    CachedEntity *sniper_dot = nullptr;

    // Get Weapon id
    auto weapon_id = HandleToIDX(CE_INT(ent, netvar.hActiveWeapon));

    // Check IDX for validity
    if (IDX_GOOD(weapon_id))
    {
        auto weapon_ent = ENTITY(weapon_id);
        // Check weapon for validity
        if (CE_GOOD(weapon_ent) && (weapon_ent->m_iClassID() == CL_CLASS(CTFSniperRifle) || weapon_ent->m_iClassID() == CL_CLASS(CTFSniperRifleDecap) || weapon_ent->m_iClassID() == CL_CLASS(CTFSniperRifleClassic)))
        {
            // Get Sniperdot
            sniper_dot = sniperdot_array.at(ent->m_IDX - 1);
            // Check if the dot is still good, if not then set to nullptr
            if (CE_BAD(sniper_dot) || sniper_dot->m_iClassID() != CL_CLASS(CSniperDot))
                sniper_dot = nullptr;
        }
    }
    
    // Enhanced pitch resolver based on mode
    if (sniper_dot == nullptr)
    {
        switch (*resolver_mode)
        {
        case 0: // Aggressive
            if (angle >= 89.0f || angle <= -89.0f)
            {
                if (brute.brutenum % 4 == 0)
                    angle = 89.0f;
                else if (brute.brutenum % 4 == 1)
                    angle = -89.0f;
                else if (brute.brutenum % 4 == 2)
                    angle = 0.0f;
                else
                    angle = angle > 0 ? -89.0f : 89.0f;
            }
            break;
            
        case 1: // Adaptive
            if (brute.hits_in_a_row > 0)
                angle = brute.last_successful_angle;
            else if (angle >= 89.0f || angle <= -89.0f)
            {
                if (brute.brutenum % 2)
                    angle = angle > 0 ? -89.0f : 89.0f;
            }
            break;
            
        case 2: // Conservative
            if (angle >= 89.0f || angle <= -89.0f)
            {
                if (brute.brutenum % 2)
                    angle = 0.0f;
            }
            break;
        }
    }
    else
    {
        // Get End and start point
        auto dot_origin = sniper_dot->m_vecOrigin();
        auto eye_origin = re::C_BasePlayer::GetEyePosition(RAW_ENT(ent));
        // Get Angle from eye to dot
        Vector diff = dot_origin - eye_origin;
        Vector angles;
        VectorAngles(diff, angles);
        // Use the pitch (yaw is not useable because sadly the sniper dot does not represent it with fake yaw)
        angle = angles.x;
    }

    brute.new_angle.x = angle;
    return angle;
}

void increaseBruteNum(int idx)
{
    auto ent = ENTITY(idx);
    if (CE_BAD(ent) || !ent->player_info->friendsID)
        return;
    auto &data = hacks::shared::anti_anti_aim::resolver_map[ent->player_info->friendsID];
    
    // If we've hit enough times in a row, consider this angle resolved
    if (data.hits_in_a_row >= *min_hits)
    {
        data.hits_in_a_row = *min_hits - 1;
        data.last_successful_angle = (int)std::floor((data.brutenum / 2.0f)) % yaw_resolves.size();
        return;
    }
    else if (data.hits_in_a_row >= 2)
        data.hits_in_a_row = 0;
    else
    {
        data.brutenum++;
        if (debug)
            logging::Info("AAA: Brutenum for entity %i increased to %i", idx, data.brutenum);
        data.hits_in_a_row = 0;
        auto &angle        = CE_VECTOR(ent, netvar.m_angEyeAngles);
        angle.x            = resolveAnglePitch(data.original_angle.x, data, ent);
        angle.y            = resolveAngleYaw(data.original_angle.y, data);
        data.new_angle.x   = angle.x;
        data.new_angle.y   = angle.y;
    }
}

static void pitchHook(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
    float flPitch      = pData->m_Value.m_Float;
    float *flPitch_out = (float *) pOut;

    if (!enable)
    {
        *flPitch_out = flPitch;
        return;
    }

    auto client_ent   = (IClientEntity *) (pStruct);
    CachedEntity *ent = ENTITY(client_ent->entindex());
    if (CE_GOOD(ent))
        *flPitch_out = resolveAnglePitch(flPitch, resolver_map[ent->player_info->friendsID], ent);
}

static void yawHook(const CRecvProxyData *pData, void *pStruct, void *pOut)
{
    float flYaw      = pData->m_Value.m_Float;
    float *flYaw_out = (float *) pOut;

    if (!enable)
    {
        *flYaw_out = flYaw;
        return;
    }

    auto client_ent   = (IClientEntity *) (pStruct);
    CachedEntity *ent = ENTITY(client_ent->entindex());
    if (CE_GOOD(ent))
        *flYaw_out = resolveAngleYaw(flYaw, resolver_map[ent->player_info->friendsID]);
}

// *_ptr points to what we need to modify while *_ProxyFn holds the old value
static RecvVarProxyFn *original_ptrX;
static RecvVarProxyFn original_ProxyFnX;
static RecvVarProxyFn *original_ptrY;
static RecvVarProxyFn original_ProxyFnY;

static void hook()
{
    auto pClass = g_IBaseClient->GetAllClasses();
    while (pClass)
    {
        const char *pszName = pClass->m_pRecvTable->m_pNetTableName;
        // "DT_TFPlayer", "tfnonlocaldata"
        if (!strcmp(pszName, "DT_TFPlayer"))
        {
            for (int i = 0; i < pClass->m_pRecvTable->m_nProps; ++i)
            {
                RecvPropRedef *pProp1 = (RecvPropRedef *) &(pClass->m_pRecvTable->m_pProps[i]);
                if (!pProp1)
                    continue;
                const char *pszName2 = pProp1->m_pVarName;
                if (!strcmp(pszName2, "tfnonlocaldata"))
                    for (int j = 0; j < pProp1->m_pDataTable->m_nProps; j++)
                    {
                        RecvPropRedef *pProp2 = (RecvPropRedef *) &(pProp1->m_pDataTable->m_pProps[j]);
                        if (!pProp2)
                            continue;
                        const char *name = pProp2->m_pVarName;

                        // Pitch Fix
                        if (!strcmp(name, "m_angEyeAngles[0]"))
                        {
                            original_ptrX     = &pProp2->m_ProxyFn;
                            original_ProxyFnX = pProp2->m_ProxyFn;
                            pProp2->m_ProxyFn = pitchHook;
                        }

                        // Yaw Fix
                        if (!strcmp(name, "m_angEyeAngles[1]"))
                        {
                            original_ptrY = &pProp2->m_ProxyFn;
                            logging::Info("Yaw Fix Applied");
                            original_ProxyFnY = pProp2->m_ProxyFn;
                            pProp2->m_ProxyFn = yawHook;
                        }
                    }
            }
        }
        pClass = pClass->m_pNext;
    }
}

static void shutdown()
{
    *original_ptrX = original_ProxyFnX;
    *original_ptrY = original_ProxyFnY;
}

static InitRoutine init(
    []()
    {
        hook();
        EC::Register(EC::Shutdown, shutdown, "antiantiaim_shutdown");
        EC::Register(EC::CreateMove, CreateMove, "cm_antiantiaim");
        EC::Register(EC::CreateMoveWarp, CreateMove, "cmw_antiantiaim");
#if ENABLE_TEXTMODE
        EC::Register(EC::CreateMove, modifyAngles, "cm_textmodeantiantiaim");
        EC::Register(EC::CreateMoveWarp, modifyAngles, "cmw_textmodeantiantiaim");
#endif
    });
} // namespace hacks::shared::anti_anti_aim
