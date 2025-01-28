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

#include <PlayerTools.hpp>
#include "common.hpp"
#include "HookedMethods.hpp"
#include "MiscTemporary.hpp"
#include "Backtrack.hpp"
#include "EffectGlow.hpp"
#include "Aimbot.hpp"

/* Core settings */
static settings::Boolean enable{ "chams.enable", "false" };
static settings::Boolean render_original{ "chams.original", "false" };
static settings::Boolean legit{ "chams.legit", "false" };
static settings::Boolean novis{ "chams.novis", "true" };
static settings::Boolean no_arms{ "remove.arms", "false" };

/* Target settings */
static settings::Boolean players{ "chams.show.players", "true" };
static settings::Boolean teammates{ "chams.show.teammates", "false" };
static settings::Boolean buildings{ "chams.show.buildings", "true" };
static settings::Boolean chamsself{ "chams.self", "true" };
static settings::Boolean weapons_enemy{ "chams.weapons.enemy", "false" };
static settings::Boolean weapons_teammate{ "chams.weapons.teammate", "false" };
static settings::Boolean weapons_original{ "chams.weapons.original", "true" };

/* Material settings */
static settings::Int chams_type{ "chams.type", "0" };
static settings::Boolean phong_enable{ "chams.phong", "true" };
static settings::Int phong_boost{ "chams.phongboost", "2" };
static settings::Float envmap_tint{ "chams.envmaptint", "0" };
static settings::Float material_force_rimlight{ "chams.rimlight", "0" };

/* Alpha settings */
static settings::Float player_alpha{ "chams.player.alpha", "1" };
static settings::Float building_alpha{ "chams.building.alpha", "1" };

/* Team colors */
static settings::Rgba visible_team_red{ "chams.red", "ff0000ff" };
static settings::Rgba visible_team_blu{ "chams.blu", "0000ffff" };
static settings::Rgba novis_team_red{ "chams.novis.red", "ff8800ff" };
static settings::Rgba novis_team_blu{ "chams.novis.blu", "bc00ffff" };

class ChamsMaterial {
public:
    IMaterial* material;
    bool created;
    bool ignore_z;
    
    ChamsMaterial() : material(nullptr), created(false), ignore_z(false) {}
    
    void Shutdown() {
        if (material) {
            material->DecrementReferenceCount();
            material = nullptr;
        }
        created = false;
    }

    bool Create(bool ignorez) {
        if (!g_IMaterialSystem)
            return false;

        ignore_z = ignorez;
        const char* material_type = (*chams_type == 5) ? "UnlitGeneric" : "VertexLitGeneric";
        
        KeyValues* kv = new KeyValues(material_type);
        if (!kv)
            return false;

        // Base material setup
        kv->SetString("$basetexture", "vgui/white");
        kv->SetInt("$model", 1);
        
        if (ignorez)
            kv->SetInt("$ignorez", 1);
            
        if (*chams_type != 5) {
            kv->SetInt("$nocull", 1);
            kv->SetInt("$halflambert", 1);
        }

        // Material type specific settings
        switch (*chams_type) {
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
            if (*phong_enable) {
                kv->SetInt("$phong", 1);
                kv->SetFloat("$phongexponent", 15);
                kv->SetFloat("$phongboost", *phong_boost);
                kv->SetString("$phongfresnelranges", "[.5 .5 1]");
            }
            break;
        }

        char material_name[128];
        snprintf(material_name, sizeof(material_name), "__cathook_chams_%s_%d", ignorez ? "ignorez" : "regular", *chams_type);
        
        material = g_IMaterialSystem->CreateMaterial(material_name, kv);
        kv->deleteThis();
        
        if (!material || material->IsErrorMaterial()) {
            material = nullptr;
            return false;
        }

        created = true;
        return true;
    }
};

namespace hooked_methods {

static ChamsMaterial mat_regular;
static ChamsMaterial mat_ignorez;
static int last_chams_type = -1;

static void CreateMaterials() {
    if (!g_IMaterialSystem)
        return;
        
    if (last_chams_type == *chams_type && mat_regular.created && mat_ignorez.created)
        return;
        
    // Safely clean up existing materials first
    mat_regular.Shutdown();
    mat_ignorez.Shutdown();
    
    // Only create new materials if chams is enabled
    if (!enable)
        return;
        
    bool success = mat_regular.Create(false) && mat_ignorez.Create(true);
    if (!success) {
        mat_regular.Shutdown();
        mat_ignorez.Shutdown();
        return;
    }
    
    last_chams_type = *chams_type;
}

static bool ShouldRenderChams(IClientEntity* entity) {
    if (!entity || !enable)
        return false;
        
    int idx = entity->entindex();
    if (idx < 0)
        return false;
        
    CachedEntity* ent = ENTITY(idx);
    if (!ent || CE_BAD(ent))
        return false;
        
    if (ent && LOCAL_E && ent->m_IDX == LOCAL_E->m_IDX)
        return *chamsself;

    switch (ent->m_Type()) {
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

static rgba_t GetEntityColor(IClientEntity* entity, bool ignorez) {
    if (!entity)
        return colors::white;
        
    try {
        CachedEntity* ent = ENTITY(entity->entindex());
        if (CE_BAD(ent))
            return colors::white;

        int team = ent->m_iTeam();
        if (team != TEAM_RED && team != TEAM_BLU)
            return colors::white;

        // For weapons, get team from the owner
        const char* model_name = g_IModelInfo->GetModelName(ent->InternalEntity()->GetModel());
        if (model_name && (strstr(model_name, "weapons/w_") || strstr(model_name, "weapons/c_"))) {
            int owner_idx = HandleToIDX(NET_INT(ent, netvar.hOwner));
            if (owner_idx > 0 && owner_idx <= g_IEngine->GetMaxClients()) {
                CachedEntity* owner = ENTITY(owner_idx);
                if (!CE_BAD(owner))
                    team = owner->m_iTeam();
            }
        }

        if (team == TEAM_BLU)
            return ignorez ? *novis_team_blu : *visible_team_blu;
        else if (team == TEAM_RED)
            return ignorez ? *novis_team_red : *visible_team_red;
    } catch (...) {
        return colors::white;
    }

    return colors::white;
}

static void ApplyChams(IClientEntity* entity, bool ignorez, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone) {
    if (!g_IVRenderView || !g_IVModelRender || !g_IMaterialSystem || !entity)
        return;
        
    IMaterial* material = ignorez ? mat_ignorez.material : mat_regular.material;
    if (!material || material->IsErrorMaterial())
        return;

    // Save the original color modulation and blend
    float original_color[3] = { 0.0f, 0.0f, 0.0f };
    float original_blend = 0.0f;
    
    try {
        g_IVRenderView->GetColorModulation(original_color);
        original_blend = g_IVRenderView->GetBlend();
    } catch (...) {
        return;
    }

    try {
        rgba_t color = GetEntityColor(entity, ignorez);
        float alpha = entity->GetClientClass()->m_ClassID == RCC_PLAYER ? *player_alpha : *building_alpha;
        
        // Clamp alpha to valid range
        alpha = std::clamp(alpha, 0.0f, 1.0f);
        
        // Set color with safety checks
        float new_color[3] = { 
            std::clamp(color.r / 255.0f, 0.0f, 1.0f),
            std::clamp(color.g / 255.0f, 0.0f, 1.0f),
            std::clamp(color.b / 255.0f, 0.0f, 1.0f)
        };
        
        g_IVRenderView->SetColorModulation(new_color);
        g_IVRenderView->SetBlend(alpha);
        
        // Force material override
        g_IVModelRender->ForcedMaterialOverride(material);
        
        // Draw with original material
        original::DrawModelExecute(g_IVModelRender, state, info, bone);
        
        // Immediately restore material to prevent state leaks
        g_IVModelRender->ForcedMaterialOverride(nullptr);
        g_IVRenderView->SetColorModulation(original_color);
        g_IVRenderView->SetBlend(original_blend);
    } catch (...) {
        // Restore original state in case of any exception
        g_IVModelRender->ForcedMaterialOverride(nullptr);
        g_IVRenderView->SetColorModulation(original_color);
        g_IVRenderView->SetBlend(original_blend);
    }
}

DEFINE_HOOKED_METHOD(DrawModelExecute, void, IVModelRender* this_, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone) {
    if (!this_ || !g_IVModelRender || !g_IMaterialSystem || !g_IVRenderView || !isHackActive() || 
        effect_glow::g_EffectGlow.drawing || (*clean_screenshots && g_IEngine->IsTakingScreenshot()) || 
        disable_visuals || CE_BAD(LOCAL_E) || !enable) {
        return original::DrawModelExecute(this_, state, info, bone);
    }

    if (!info.pModel) {
        return original::DrawModelExecute(this_, state, info, bone);
    }

    IClientEntity* entity = g_IEntityList->GetClientEntity(info.entity_index);
    if (!entity) {
        return original::DrawModelExecute(this_, state, info, bone);
    }

    const char* name = g_IModelInfo->GetModelName(info.pModel);
    if (!name) {
        return original::DrawModelExecute(this_, state, info, bone);
    }

    try {
        CreateMaterials();

        // Handle arms
        if (strstr(name, "arms") || strstr(name, "c_engineer_gunslinger")) {
            if (no_arms)
                return;
            return original::DrawModelExecute(this_, state, info, bone);
        }

        // Handle weapons
        if (strstr(name, "weapons/w_") || strstr(name, "weapons/c_")) {
            if (!enable)
                return original::DrawModelExecute(this_, state, info, bone);

            CachedEntity* ent = ENTITY(entity->entindex());
            if (CE_BAD(ent))
                return original::DrawModelExecute(this_, state, info, bone);

            int owner_idx = HandleToIDX(NET_INT(ent, netvar.hOwner));
            if (owner_idx <= 0 || owner_idx > g_IEngine->GetMaxClients())
                return original::DrawModelExecute(this_, state, info, bone);

            CachedEntity* owner = ENTITY(owner_idx);
            if (CE_BAD(owner))
                return original::DrawModelExecute(this_, state, info, bone);

            bool is_enemy = owner->m_bEnemy();
            bool should_render = (is_enemy && *weapons_enemy) || (!is_enemy && *weapons_teammate);

            if (should_render) {
                if (!legit && novis && mat_ignorez.material && !mat_ignorez.material->IsErrorMaterial())
                    ApplyChams(entity, true, state, info, bone);
                if (mat_regular.material && !mat_regular.material->IsErrorMaterial())
                    ApplyChams(entity, false, state, info, bone);
                return;
            }
            
            if (*weapons_original)
                return original::DrawModelExecute(this_, state, info, bone);
            return;
        }

        if (!ShouldRenderChams(entity))
            return original::DrawModelExecute(this_, state, info, bone);

        if (render_original && !legit)
            original::DrawModelExecute(this_, state, info, bone);

        if (!legit && novis && mat_ignorez.material && !mat_ignorez.material->IsErrorMaterial())
            ApplyChams(entity, true, state, info, bone);
        if (mat_regular.material && !mat_regular.material->IsErrorMaterial())
            ApplyChams(entity, false, state, info, bone);
    } catch (...) {
        // Restore default material state and return
        g_IVModelRender->ForcedMaterialOverride(nullptr);
        return original::DrawModelExecute(this_, state, info, bone);
    }
}

static InitRoutine init([]() {
    EC::Register(EC::LevelShutdown, []() {
        mat_regular.Shutdown();
        mat_ignorez.Shutdown();
        last_chams_type = -1;
    }, "dme_lvl_shutdown");

    EC::Register(EC::Shutdown, []() {
        mat_regular.Shutdown();
        mat_ignorez.Shutdown();
        last_chams_type = -1;
    }, "dme_shutdown");

    phong_enable.installChangeCallback([](settings::VariableBase<bool>&, bool) {
        last_chams_type = -1;
    });
    phong_boost.installChangeCallback([](settings::VariableBase<int>&, int) {
        last_chams_type = -1;
    });
    chams_type.installChangeCallback([](settings::VariableBase<int>&, int) {
        last_chams_type = -1;
    });
    envmap_tint.installChangeCallback([](settings::VariableBase<float>&, float) {
        last_chams_type = -1;
    });
    material_force_rimlight.installChangeCallback([](settings::VariableBase<float>&, float) {
        last_chams_type = -1;
    });
});

} // namespace hooked_methods
