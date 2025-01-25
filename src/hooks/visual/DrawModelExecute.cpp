/*
    This file is part of Cathook.

    Cathook is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cathook is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cathook. If not, see <https://www.gnu.org/licenses/>.
*/

// Codeowners: aUniqueUser

#include <PlayerTools.hpp>
#include "common.hpp"
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
#include "Backtrack.hpp"
#include "EffectGlow.hpp"
#include "Aimbot.hpp"

/* World visual rvars */
static settings::Boolean no_arms{ "remove.arms", "false" };
static settings::Boolean enable{ "chams.enable", "false" };
static settings::Boolean render_original{ "chams.original", "false" };

/* Cham target rvars */
static settings::Boolean teammates{ "chams.show.teammates", "false" };
static settings::Boolean players{ "chams.show.players", "true" };
static settings::Boolean buildings{ "chams.show.buildings", "true" };
static settings::Boolean chamsself{ "chams.self", "true" };

/* Weapon chams settings */
static settings::Boolean weapons_enemy{ "chams.weapons.enemy", "false" };
static settings::Boolean weapons_teammate{ "chams.weapons.teammate", "false" };
static settings::Boolean weapons_original{ "chams.weapons.original", "true" };

/* Alpha settings */
static settings::Float player_alpha{ "chams.player.alpha", "1" };
static settings::Float building_alpha{ "chams.building.alpha", "1" };

/* Seperate cham settings when ignorez */
static settings::Boolean novis{ "chams.novis", "true" };
static settings::Boolean legit{ "chams.legit", "false" };

/* Team colors */
static settings::Rgba visible_team_red{ "chams.red", "ff0000ff" };
static settings::Rgba visible_team_blu{ "chams.blu", "0000ffff" };
static settings::Rgba novis_team_red{ "chams.novis.red", "ff8800ff" };
static settings::Rgba novis_team_blu{ "chams.novis.blu", "bc00ffff" };

/* Material settings */
static settings::Int chams_type{ "chams.type", "0" };  // 0 = Normal, 1 = Flat, 2 = Shaded, 3 = Glossy, 4 = Glow, 5 = Wireframe
static settings::Boolean phong_enable{ "chams.phong", "true" };
static settings::Int phong_boost{ "chams.phongboost", "2" };
static settings::Float envmap_tint{ "chams.envmaptint", "0" };
static settings::Float material_force_rimlight{ "chams.rimlight", "0" };

class Materials
{
public:
    CMaterialReference mat_regular;
    CMaterialReference mat_ignorez;
    bool materials_created;
    int last_chams_type;

    Materials() : materials_created(false), last_chams_type(-1) {}

    void Shutdown()
    {
        if (mat_regular.IsValid())
            mat_regular.Shutdown();
        if (mat_ignorez.IsValid())
            mat_ignorez.Shutdown();
        materials_created = false;
        last_chams_type = -1;
    }

    bool CreateMaterials()
    {
        if (!g_IMaterialSystem)
            return false;

        // Check if materials are already created and valid
        if (materials_created && last_chams_type == *chams_type)
        {
            if (mat_regular.IsValid() && mat_ignorez.IsValid())
            {
                if (mat_regular.m_pMaterial && mat_ignorez.m_pMaterial)
                    return true;
            }
        }

        // Clean up old materials first
        Shutdown();

        // Create base material
        const char* material_type = (*chams_type == 5) ? "UnlitGeneric" : "VertexLitGeneric";
        
        // Create regular material
        KeyValues* kv = new KeyValues(material_type);
        if (!kv)
            return false;

        // Base material setup
        kv->SetString("$basetexture", "vgui/white");
        kv->SetInt("$model", 1);
        
        if (*chams_type != 5)
        {
            kv->SetInt("$nocull", 1);
            kv->SetInt("$halflambert", 1);
        }

        // Material type specific settings
        switch (*chams_type)
        {
        case 1: // Flat
            kv->SetInt("$phong", 0);
            kv->SetInt("$basemapalphaphongmask", 0);
            kv->SetInt("$normalmapalphaenvmapmask", 0);
            break;
            
        case 2: // Shaded
            kv->SetInt("$phong", 1);
            kv->SetFloat("$phongexponent", 15);
            kv->SetFloat("$phongboost", *phong_boost);
            kv->SetString("$envmap", "");
            kv->SetString("$phongfresnelranges", "[.5 .5 1]");
            break;
            
        case 3: // Glossy
            kv->SetInt("$phong", 1);
            kv->SetFloat("$phongexponent", 30);
            kv->SetFloat("$phongboost", *phong_boost);
            kv->SetString("$envmap", "env_cubemap");
            kv->SetFloat("$envmaptint", *envmap_tint);
            kv->SetString("$phongfresnelranges", "[.5 .5 1]");
            break;
            
        case 4: // Glow
            kv->SetInt("$additive", 1);
            kv->SetString("$envmap", "models/effects/cube_white");
            kv->SetInt("$selfillum", 1);
            kv->SetString("$selfillumtint", "[1 1 1]");
            kv->SetFloat("$selfillumboost", 2.0f);
            break;
            
        case 5: // Wireframe
            kv->SetInt("$wireframe", 1);
            break;
            
        default: // Normal
            if (*phong_enable)
            {
                kv->SetInt("$phong", 1);
                kv->SetFloat("$phongexponent", 15);
                kv->SetFloat("$phongboost", *phong_boost);
                kv->SetString("$phongfresnelranges", "[.5 .5 1]");
            }
            break;
        }

        // Create regular material
        IMaterial* reg = g_IMaterialSystem->CreateMaterial("__cathook_chams", kv);
        kv->deleteThis();
        if (!reg || reg->IsErrorMaterial())
            return false;

        // Create ignorez material
        KeyValues* kv_ignorez = new KeyValues(material_type);
        if (!kv_ignorez)
        {
            reg->DecrementReferenceCount();
            return false;
        }

        // Copy base material setup
        kv_ignorez->SetString("$basetexture", "vgui/white");
        kv_ignorez->SetInt("$model", 1);
        kv_ignorez->SetInt("$ignorez", 1);
        
        if (*chams_type != 5)
        {
            kv_ignorez->SetInt("$nocull", 1);
            kv_ignorez->SetInt("$halflambert", 1);
        }

        // Copy material type specific settings
        switch (*chams_type)
        {
        case 1: // Flat
            kv_ignorez->SetInt("$phong", 0);
            kv_ignorez->SetInt("$basemapalphaphongmask", 0);
            kv_ignorez->SetInt("$normalmapalphaenvmapmask", 0);
            break;
            
        case 2: // Shaded
            kv_ignorez->SetInt("$phong", 1);
            kv_ignorez->SetFloat("$phongexponent", 15);
            kv_ignorez->SetFloat("$phongboost", *phong_boost);
            kv_ignorez->SetString("$envmap", "");
            kv_ignorez->SetString("$phongfresnelranges", "[.5 .5 1]");
            break;
            
        case 3: // Glossy
            kv_ignorez->SetInt("$phong", 1);
            kv_ignorez->SetFloat("$phongexponent", 30);
            kv_ignorez->SetFloat("$phongboost", *phong_boost);
            kv_ignorez->SetString("$envmap", "env_cubemap");
            kv_ignorez->SetFloat("$envmaptint", *envmap_tint);
            kv_ignorez->SetString("$phongfresnelranges", "[.5 .5 1]");
            break;
            
        case 4: // Glow
            kv_ignorez->SetInt("$additive", 1);
            kv_ignorez->SetString("$envmap", "models/effects/cube_white");
            kv_ignorez->SetInt("$selfillum", 1);
            kv_ignorez->SetString("$selfillumtint", "[1 1 1]");
            kv_ignorez->SetFloat("$selfillumboost", 2.0f);
            break;
            
        case 5: // Wireframe
            kv_ignorez->SetInt("$wireframe", 1);
            break;
            
        default: // Normal
            if (*phong_enable)
            {
                kv_ignorez->SetInt("$phong", 1);
                kv_ignorez->SetFloat("$phongexponent", 15);
                kv_ignorez->SetFloat("$phongboost", *phong_boost);
                kv_ignorez->SetString("$phongfresnelranges", "[.5 .5 1]");
            }
            break;
        }

        IMaterial* ignorez = g_IMaterialSystem->CreateMaterial("__cathook_chams_ignorez", kv_ignorez);
        kv_ignorez->deleteThis();
        
        if (!ignorez || ignorez->IsErrorMaterial())
        {
            reg->DecrementReferenceCount();
            return false;
        }

        mat_regular.Init(reg);
        mat_ignorez.Init(ignorez);
        materials_created = true;
        last_chams_type = *chams_type;
        return true;
    }
};

class ChamColors
{
public:
    rgba_t rgba;
    ChamColors(rgba_t col = colors::empty)
    {
        rgba = col;
    }
};

namespace hooked_methods
{
static bool init_mat = false;
static Materials mats;

template <typename T> void rvarCallback(settings::VariableBase<T> &, T)
{
    init_mat = false;
}

static InitRoutine init_dme(
    []()
    {
        EC::Register(
            EC::LevelShutdown,
            []()
            {
                mats.Shutdown();
                init_mat = false;
            },
            "dme_lvl_shutdown");

        EC::Register(
            EC::Shutdown,
            []()
            {
                mats.Shutdown();
                init_mat = false;
            },
            "dme_shutdown");

        phong_enable.installChangeCallback(rvarCallback<bool>);
        phong_boost.installChangeCallback(rvarCallback<int>);
        chams_type.installChangeCallback(rvarCallback<int>);
        envmap_tint.installChangeCallback(rvarCallback<float>);
        material_force_rimlight.installChangeCallback(rvarCallback<float>);
    });

bool ShouldRenderChams(IClientEntity *entity)
{
    if (!entity || !enable || CE_BAD(LOCAL_E))
        return false;
        
    int idx = entity->entindex();
    if (idx < 0)
        return false;
        
    CachedEntity *ent = ENTITY(idx);
    if (!ent || CE_BAD(ent))
        return false;
        
    if (ent && LOCAL_E && ent->m_IDX == LOCAL_E->m_IDX)
        return *chamsself;

    switch (ent->m_Type())
    {
    case ENTITY_BUILDING:
        if (!buildings)
            return false;
        if (!ent->m_bEnemy() && !teammates)
            return false;
        if (ent->m_iHealth() == 0 || !ent->m_iHealth())
            return false;
        return true;
    case ENTITY_PLAYER:
        if (!players)
            return false;
        if (!teammates && !ent->m_bEnemy())
            return false;
        if (CE_BYTE(ent, netvar.iLifeState))
            return false;
        return true;
    default:
        break;
    }
    return false;
}

// Purpose => Get ChamColors struct from internal entity
static ChamColors GetChamColors(IClientEntity *entity, bool ignorez)
{
    if (!entity)
        return ChamColors(colors::white);
        
    CachedEntity *ent = ENTITY(entity->entindex());
    if (CE_BAD(ent))
        return ChamColors(colors::white);

    int team = ent->m_iTeam();
    if (team != TEAM_RED && team != TEAM_BLU)
        return ChamColors(colors::white);

    // For weapons, get team from the owner
    const char* model_name = g_IModelInfo->GetModelName(ent->InternalEntity()->GetModel());
    if (model_name && (strstr(model_name, "weapons/w_") || strstr(model_name, "weapons/c_")))
    {
        int owner_idx = HandleToIDX(NET_INT(ent, netvar.hOwner));
        if (owner_idx > 0 && owner_idx <= g_IEngine->GetMaxClients())
        {
            CachedEntity *owner_ent = ENTITY(owner_idx);
            if (!CE_BAD(owner_ent))
                team = owner_ent->m_iTeam();
        }
    }

    // Return appropriate color based on team and visibility
    if (team == TEAM_BLU)
        return ChamColors(ignorez ? *novis_team_blu : *visible_team_blu);
    else if (team == TEAM_RED)
        return ChamColors(ignorez ? *novis_team_red : *visible_team_red);

    return ChamColors(colors::white);
}

void ApplyChams(ChamColors colors, bool ignorez, IClientEntity *entity, IVModelRender *this_, const DrawModelState_t &state, const ModelRenderInfo_t &info, matrix3x4_t *bone)
{
    static bool in_chams = false;
    if (in_chams || !g_IVRenderView || !g_IVModelRender || !g_IMaterialSystem || !entity || !this_)
    {
        if (g_IVModelRender)
            g_IVModelRender->ForcedMaterialOverride(nullptr);
        if (!in_chams)
            original::DrawModelExecute(this_, state, info, bone);
        return;
    }
    
    in_chams = true;

    // Create or update materials if needed
    if (!mats.CreateMaterials())
    {
        if (g_IVModelRender)
            g_IVModelRender->ForcedMaterialOverride(nullptr);
        original::DrawModelExecute(this_, state, info, bone);
        in_chams = false;
        return;
    }

    // Get material pointers
    IMaterial* reg = mats.mat_regular.m_pMaterial;
    IMaterial* ignorez_mat = mats.mat_ignorez.m_pMaterial;

    // Safety check for valid materials
    if (!reg || !ignorez_mat)
    {
        if (g_IVModelRender)
            g_IVModelRender->ForcedMaterialOverride(nullptr);
        original::DrawModelExecute(this_, state, info, bone);
        in_chams = false;
        return;
    }

    // Draw original model if enabled
    if (render_original && !ignorez)
    {
        if (g_IVModelRender)
            g_IVModelRender->ForcedMaterialOverride(nullptr);
        original::DrawModelExecute(this_, state, info, bone);
    }

    // Apply colors
    float r = colors.rgba.r / 255.0f;
    float g = colors.rgba.g / 255.0f;
    float b = colors.rgba.b / 255.0f;
    float a = colors.rgba.a;

    reg->ColorModulate(r, g, b);
    reg->AlphaModulate(a);
    ignorez_mat->ColorModulate(r, g, b);
    ignorez_mat->AlphaModulate(a);
    
    // Draw through-wall chams first
    if (ignorez && !legit && novis)
    {
        g_IVModelRender->ForcedMaterialOverride(ignorez_mat);
        original::DrawModelExecute(this_, state, info, bone);
    }

    // Then draw visible chams
    g_IVModelRender->ForcedMaterialOverride(reg);
    original::DrawModelExecute(this_, state, info, bone);

    g_IVModelRender->ForcedMaterialOverride(nullptr);
    in_chams = false;
}

DEFINE_HOOKED_METHOD(DrawModelExecute, void, IVModelRender *this_, const DrawModelState_t &state, const ModelRenderInfo_t &info, matrix3x4_t *bone)
{
    // Early exit conditions with proper cleanup
    if (!this_ || !g_IVModelRender || !g_IMaterialSystem || !g_IVRenderView || !isHackActive() || 
        effect_glow::g_EffectGlow.drawing || (*clean_screenshots && g_IEngine->IsTakingScreenshot()) || 
        disable_visuals || CE_BAD(LOCAL_E) || !enable)
    {
        if (this_ && g_IVModelRender)
            g_IVModelRender->ForcedMaterialOverride(nullptr);
        return original::DrawModelExecute(this_, state, info, bone);
    }

    // Check if we have a valid entity and model
    if (!info.pModel)
    {
        g_IVModelRender->ForcedMaterialOverride(nullptr);
        return original::DrawModelExecute(this_, state, info, bone);
    }

    IClientEntity *entity = g_IEntityList->GetClientEntity(info.entity_index);
    if (!entity)
    {
        g_IVModelRender->ForcedMaterialOverride(nullptr);
        return original::DrawModelExecute(this_, state, info, bone);
    }

    // Get model name
    const char *name = g_IModelInfo->GetModelName(info.pModel);
    if (!name)
        return original::DrawModelExecute(this_, state, info, bone);

    try
    {
        // Handle arms
        if (strstr(name, "arms") || strstr(name, "c_engineer_gunslinger"))
        {
            if (no_arms)
                return;
            return original::DrawModelExecute(this_, state, info, bone);
        }

        // Handle weapons
        if (strstr(name, "weapons/w_") || strstr(name, "weapons/c_"))
        {
            // Always show original model if chams are disabled
            if (!enable)
                return original::DrawModelExecute(this_, state, info, bone);

            CachedEntity *ent = ENTITY(entity->entindex());
            if (CE_BAD(ent))
                return original::DrawModelExecute(this_, state, info, bone);

            // Get weapon owner
            int owner_idx = HandleToIDX(NET_INT(ent, netvar.hOwner));
            if (owner_idx <= 0 || owner_idx > g_IEngine->GetMaxClients())
                return original::DrawModelExecute(this_, state, info, bone);

            CachedEntity *owner = ENTITY(owner_idx);
            if (CE_BAD(owner))
                return original::DrawModelExecute(this_, state, info, bone);

            bool is_enemy = owner->m_bEnemy();
            bool should_render = (is_enemy && *weapons_enemy) || (!is_enemy && *weapons_teammate);

            if (should_render)
            {
                // If legit mode is off, render through walls first
                if (!legit && novis)
                {
                    ChamColors colors = GetChamColors(owner->InternalEntity(), true);
                    colors.rgba.a = *player_alpha;
                    ApplyChams(colors, true, entity, this_, state, info, bone);
                }

                // Then render normal chams
                ChamColors colors = GetChamColors(owner->InternalEntity(), false);
                colors.rgba.a = *player_alpha;
                ApplyChams(colors, false, entity, this_, state, info, bone);
                return;
            }
            
            // Show original model if no chams should be applied
            if (*weapons_original)
                return original::DrawModelExecute(this_, state, info, bone);
            return;
        }

        // Handle player chams
        if (entity->entindex() > 0 && entity->entindex() <= g_IEngine->GetMaxClients())
        {
            if (!players)
                return original::DrawModelExecute(this_, state, info, bone);

            CachedEntity *ent = ENTITY(entity->entindex());
            if (!ent || !ent->m_bAlivePlayer())
                return original::DrawModelExecute(this_, state, info, bone);

            // Handle local player
            if (ent->m_IDX == g_IEngine->GetLocalPlayer())
            {
                if (!chamsself)
                    return original::DrawModelExecute(this_, state, info, bone);
            }
            // Handle other players
            else
            {
                if (!teammates && !ent->m_bEnemy())
                    return original::DrawModelExecute(this_, state, info, bone);
            }

            // If legit mode is off, render through walls first
            if (!legit && novis)
            {
                ChamColors colors = GetChamColors(entity, true);
                colors.rgba.a = *player_alpha;
                ApplyChams(colors, true, entity, this_, state, info, bone);
            }

            // Then render normal chams
            ChamColors colors = GetChamColors(entity, false);
            colors.rgba.a = *player_alpha;
            ApplyChams(colors, false, entity, this_, state, info, bone);
            return;
        }

        // Handle buildings
        CachedEntity *ent = ENTITY(entity->entindex());
        if (buildings && ent && ent->m_Type() == ENTITY_BUILDING)
        {
            // Skip teammate buildings if not enabled
            if (!ent->m_bEnemy() && !teammates)
                return original::DrawModelExecute(this_, state, info, bone);

            if (ent->m_iHealth() == 0 || !ent->m_iHealth())
                return original::DrawModelExecute(this_, state, info, bone);

            // If legit mode is off, render through walls first
            if (!legit && novis)
            {
                ChamColors colors = GetChamColors(entity, true);
                colors.rgba.a = *building_alpha;
                ApplyChams(colors, true, entity, this_, state, info, bone);
            }

            // Then render normal chams
            ChamColors colors = GetChamColors(entity, false);
            colors.rgba.a = *building_alpha;
            ApplyChams(colors, false, entity, this_, state, info, bone);
            return;
        }
    }
    catch (...)
    {
        // Reset material override in case of exception
        g_IVModelRender->ForcedMaterialOverride(nullptr);
    }

    return original::DrawModelExecute(this_, state, info, bone);
}
} // namespace hooked_methods
