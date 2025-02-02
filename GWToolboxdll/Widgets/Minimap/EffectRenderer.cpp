#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/Packets/StoC.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <GuiUtils.h>
#include <Widgets/Minimap/EffectRenderer.h>

namespace {
    enum SkillEffect {
        Chaos_storm = 131,
        Meteor_Shower = 341,
        Savannah_heat = 346,
        Lava_font = 347,
        Breath_of_fire = 351,
        Maelstrom = 381,
        Barbed_Trap = 772,
        Barbed_Trap_Activate = 773,
        Flame_Trap = 774,
        Flame_Trap_Activate = 775,
        Spike_Trap = 777,
        Spike_Trap_Activate = 778,
        Bed_of_coals = 875,
        Churning_earth = 994
    };
}
EffectRenderer::EffectRenderer() {
    LoadDefaults();
}
void EffectRenderer::LoadDefaults() {
    aoe_effect_settings.clear();
    aoe_effect_settings.emplace(Maelstrom, new EffectSettings("Maelstrom", Maelstrom, GW::Constants::Range::Adjacent, 10000));
    aoe_effect_settings.emplace(Chaos_storm, new EffectSettings("Chaos Storm", Chaos_storm, GW::Constants::Range::Adjacent, 10000));
    aoe_effect_settings.emplace(Savannah_heat, new EffectSettings("Savannah Heat", Savannah_heat, GW::Constants::Range::Adjacent, 5000));
    aoe_effect_settings.emplace(Breath_of_fire, new EffectSettings("Breath of Fire", Breath_of_fire, GW::Constants::Range::Adjacent, 5000));
    aoe_effect_settings.emplace(Maelstrom, new EffectSettings("Lava font", Lava_font, GW::Constants::Range::Adjacent, 5000));
    aoe_effect_settings.emplace(Churning_earth, new EffectSettings("Churning Earth", Churning_earth, GW::Constants::Range::Nearby, 5000));

    aoe_effect_settings.emplace(Barbed_Trap, new EffectSettings("Barbed Trap", Barbed_Trap, GW::Constants::Range::Adjacent, 90000, GW::Packet::StoC::GenericValue::STATIC_HEADER));
    aoe_effect_triggers.emplace(Barbed_Trap_Activate, new EffectTrigger(Barbed_Trap, 2000, GW::Constants::Range::Nearby));
    aoe_effect_settings.emplace(Flame_Trap, new EffectSettings("Flame Trap", Flame_Trap, GW::Constants::Range::Adjacent, 90000, GW::Packet::StoC::GenericValue::STATIC_HEADER));
    aoe_effect_triggers.emplace(Flame_Trap_Activate, new EffectTrigger(Flame_Trap, 2000, GW::Constants::Range::Nearby));
    aoe_effect_settings.emplace(Spike_Trap, new EffectSettings("Spike Trap", Barbed_Trap, GW::Constants::Range::Adjacent, 90000, GW::Packet::StoC::GenericValue::STATIC_HEADER));
    aoe_effect_triggers.emplace(Spike_Trap_Activate, new EffectTrigger(Spike_Trap, 2000, GW::Constants::Range::Nearby));
}
EffectRenderer::~EffectRenderer() {
    for (auto settings : aoe_effect_settings) {
        delete settings.second;
    }
    aoe_effect_settings.clear();
    for (auto triggers : aoe_effect_triggers) {
        delete triggers.second;
    }
    aoe_effect_triggers.clear();
}
void EffectRenderer::Invalidate() {
    VBuffer::Invalidate();
    for (auto p : aoe_effects) {
        delete p;
    }
    aoe_effects.clear();
    for (auto trigger : aoe_effect_triggers) {
        trigger.second->triggers_handled.clear();
    }
}
void EffectRenderer::LoadSettings(CSimpleIni* ini, const char* section) {
    Invalidate();
    LoadDefaults();
    for (auto settings : aoe_effect_settings) {
        char color_buf[64];
        sprintf(color_buf, "color_aoe_effect_%d", settings.first);
        settings.second->color = Colors::Load(ini, section, color_buf, settings.second->color);
    }
}
void EffectRenderer::SaveSettings(CSimpleIni* ini, const char* section) const {
    for (auto settings : aoe_effect_settings) {
        char color_buf[64];
        sprintf(color_buf, "color_aoe_effect_%d", settings.first);
        Colors::Save(ini, section, color_buf, settings.second->color);
    }
}
void EffectRenderer::DrawSettings() {
    bool confirm = false;
    if (ImGui::SmallConfirmButton("Restore Defaults", &confirm)) {
        LoadDefaults();
    }
    for (auto s : aoe_effect_settings) {
        ImGui::PushID(static_cast<int>(s.first));
        Colors::DrawSettingHueWheel("", &s.second->color, 0);
        ImGui::SameLine();
        ImGui::Text(s.second->name.c_str());
        ImGui::PopID();
    }

}
void EffectRenderer::RemoveTriggeredEffect(uint32_t effect_id, GW::Vec2f* pos) {
    auto it1 = aoe_effect_triggers.find(effect_id);
    if (it1 == aoe_effect_triggers.end())
        return;
    auto trigger = it1->second;
    auto settings = aoe_effect_settings.find(trigger->triggered_effect_id)->second;
    std::pair<float, float> posp = { pos->x,pos->y };
    auto trap_handled = trigger->triggers_handled.find(posp);
    if (trap_handled != trigger->triggers_handled.end() && TIMER_DIFF(trap_handled->second) < 5000) {
        return; // Already handled this trap, e.g. Spike Trap triggers 3 times over 2 seconds; we only care about the first.
    }
    trigger->triggers_handled.emplace(posp, TIMER_INIT());
    std::lock_guard<std::recursive_mutex> lock(effects_mutex);
    Effect* closest = nullptr;
    float closestDistance = GW::Constants::SqrRange::Nearby;
    uint32_t closest_idx = 0;
    for (size_t i = 0; i < aoe_effects.size();i++) {
        Effect* effect = aoe_effects[i];
        if (!effect || effect->effect_id != settings->effect_id)
            continue;
        // Need to estimate position; player may have moved on cast slightly.
        float newDistance = GW::GetSquareDistance(*pos,effect->pos);
        if (newDistance > closestDistance)
            continue;
        closest_idx = i;
        closest = effect;
        closestDistance = newDistance;
    }
    if (closest) {
        // Trigger this trap to time out in 2 seconds' time. Increase damage radius from adjacent to nearby.
        closest->start = TIMER_INIT();
        closest->duration = trigger->duration;
        closest->circle.range = trigger->range;
    }
}

void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValue* pak) {
    if (!initialized) return;
    if (pak->Value_id != 21) // Effect on agent
        return;
    auto it = aoe_effect_settings.find(pak->value);
    if (it == aoe_effect_settings.end())
        return;
    auto settings = it->second;
    if (settings->stoc_header && settings->stoc_header != pak->header)
        return;
    GW::AgentLiving* caster = static_cast<GW::AgentLiving * >(GW::Agents::GetAgentByID(pak->agent_id));
    if (!caster || caster->allegiance != 0x3) return;
    aoe_effects.push_back(new Effect(pak->value, caster->pos.x, caster->pos.y, settings->duration, settings->range, &settings->color));
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::GenericValueTarget* pak) {
    if (!initialized) return;
    if (pak->Value_id != 20) // Effect on target
        return;
    auto it = aoe_effect_settings.find(pak->value);
    if (it == aoe_effect_settings.end())
        return;
    auto settings = it->second;
    if (settings->stoc_header && settings->stoc_header != pak->header)
        return;
    if (pak->caster == pak->target) return;
    GW::AgentLiving* caster = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->caster));
    if (!caster || caster->allegiance != 0x3) return;
    GW::Agent* target = GW::Agents::GetAgentByID(pak->target);
    if (!target) return;
    aoe_effects.push_back(new Effect(pak->value, target->pos.x, target->pos.y, settings->duration, settings->range, &settings->color));
}
void EffectRenderer::PacketCallback(GW::Packet::StoC::PlayEffect* pak) {
    if (!initialized) return;
    // TODO: Fire storm and Meteor shower have no caster!
    // Need to record GenericValueTarget with value_id matching these skills, then roughly match the coords after.
    RemoveTriggeredEffect(pak->effect_id, &pak->coords);
    auto it = aoe_effect_settings.find(pak->effect_id);
    if (it == aoe_effect_settings.end())
        return;
    auto settings = it->second;
    if (settings->stoc_header && settings->stoc_header != pak->header)
        return;
    GW::AgentLiving* a = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
    if (!a || a->allegiance != 0x3) return;
    aoe_effects.push_back(new Effect(pak->effect_id, pak->coords.x, pak->coords.y, settings->duration, settings->range, &settings->color));
}
void EffectRenderer::Initialize(IDirect3DDevice9* device) {
    if (initialized)
        return;
    initialized = true;
    type = D3DPT_LINELIST;

    HRESULT hr = device->CreateVertexBuffer(sizeof(D3DVertex) * vertices_max, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    if (FAILED(hr)) {
        printf("Error setting up PingsLinesRenderer vertex buffer: HRESULT: 0x%lX\n", hr);
    }
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&StoC_Hook, [&](GW::HookStatus*, GW::Packet::StoC::GameSrvTransfer*) {
        need_to_clear_effects = true;
        });
}
void EffectRenderer::Render(IDirect3DDevice9* device) {
    Initialize(device);
    DrawAoeEffects(device);
}
void EffectRenderer::DrawAoeEffects(IDirect3DDevice9* device) {
    if (need_to_clear_effects) {
        Invalidate();
        need_to_clear_effects = false;
    }
    if (aoe_effects.empty())
        return;
    D3DXMATRIX translate, scale, world;
    std::lock_guard<std::recursive_mutex> lock(effects_mutex);
    size_t effect_size = aoe_effects.size();
    for (size_t i = 0; i < effect_size; i++) {
        Effect* effect = aoe_effects[i];
        if (!effect)
            continue;
        if (TIMER_DIFF(effect->start) > static_cast<clock_t>(effect->duration)) {
            aoe_effects.erase(aoe_effects.begin() + static_cast<int>(i));
            delete effect;
            i--;
            effect_size--;
            continue;
        }
        D3DXMatrixScaling(&scale, effect->circle.range, effect->circle.range, 1.0f);
        D3DXMatrixTranslation(&translate, effect->pos.x, effect->pos.y, 0.0f);
        world = scale * translate;
        device->SetTransform(D3DTS_WORLD, &world);
        effect->circle.Render(device);
    }
}

void EffectRenderer::EffectCircle::Initialize(IDirect3DDevice9* device) {
    type = D3DPT_LINESTRIP;
    count = 16; // polycount
    unsigned int vertex_count = count + 1;
    D3DVertex* vertices = nullptr;

    if (buffer) buffer->Release();
    device->CreateVertexBuffer(sizeof(D3DVertex) * vertex_count, 0,
        D3DFVF_CUSTOMVERTEX, D3DPOOL_MANAGED, &buffer, NULL);
    buffer->Lock(0, sizeof(D3DVertex) * vertex_count,
        (VOID**)&vertices, D3DLOCK_DISCARD);

    for (size_t i = 0; i < count; ++i) {
        float angle = i * (2 * (float)M_PI / count);
        vertices[i].x = std::cos(angle);
        vertices[i].y = std::sin(angle);
        vertices[i].z = 0.0f;
        vertices[i].color = *color;
    }
    vertices[count] = vertices[0];

    buffer->Unlock();
}
