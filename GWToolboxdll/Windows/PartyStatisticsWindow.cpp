#include "stdafx.h"

#include <algorithm>

#include <GWCA/Constants/Constants.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Packets/StoC.h>

#include <GuiUtils.h>
#include <Modules/Resources.h>
#include <Windows/PartyStatisticsWindow.h>

/*************************/
/* Static Helper Methods */
/*************************/

namespace {
    constexpr wchar_t NONE_PLAYER_NAME[] = L"Hero/Henchman Slot";
    constexpr uint32_t NONE_SKILL = static_cast<uint32_t>(GW::Constants::SkillID::No_Skill);
    constexpr wchar_t UNKNOWN_SKILL_NAME[] = L"Unknown Skill";
    constexpr wchar_t UNKNOWN_PLAYER_NAME[] = L"Unknown Player";
    constexpr uint8_t MAX_NUM_SKILLS = 8;
}

std::wstring& PartyStatisticsWindow::GetSkillName(const uint32_t skill_id) {
    const auto found_it = skill_names.find(skill_id);

    if (found_it == skill_names.end()) {
        const GW::Skill& skill_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);

        skill_names[skill_id] = new GuiUtils::EncString(skill_data.name);
    }

    return skill_names[skill_id]->wstring();
}

std::wstring& PartyStatisticsWindow::GetPlayerName(uint32_t agent_id) {
    auto& agent_names = Instance().agent_names;
    const auto found_it = agent_names.find(agent_id);

    if (found_it == agent_names.end()) {
        agent_names[agent_id] = new GuiUtils::EncString(GW::Agents::GetAgentEncName(agent_id));
    }

    return agent_names[agent_id]->wstring();
}

const GW::Skillbar* PartyStatisticsWindow::GetPlayerSkillbar(const uint32_t player_id) {
    const GW::SkillbarArray skillbar_array = GW::SkillbarMgr::GetSkillbarArray();

    if (!skillbar_array.valid()) return nullptr;

    for (const GW::Skillbar& skillbar : skillbar_array) {
        if (skillbar.agent_id == player_id) return &skillbar;
    }

    return nullptr;
}

std::wstring PartyStatisticsWindow::GetSkillString(
    const std::wstring& agent_name, const std::wstring& skill_name, const uint32_t& skill_count) {
    return agent_name + std::wstring{L" Used Skill "} + skill_name + std::wstring{L" "} + std::to_wstring(skill_count) +
           std::wstring{L" times."};
}

/**********************/
/* Overridden Methods */
/**********************/

void PartyStatisticsWindow::Initialize() {
    send_timer = TIMER_INIT();

    ToolboxWindow::Initialize();

    GW::Chat::CreateCommand(L"skillstats", CmdSkillStatistics);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&MapLoaded_Entry, &MapLoadedCallback);

    /* Skill on self or party player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &GenericValueSelf_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->agent_id;
            const uint32_t target_id = 0U;
            const uint32_t value = packet->value;
            const bool no_target = true;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    /* Skill on enemy player */
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(&GenericValueTarget_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::GenericValueTarget* packet) -> void {
            UNREFERENCED_PARAMETER(status);

            const uint32_t value_id = packet->Value_id;
            const uint32_t caster_id = packet->caster;
            const uint32_t target_id = packet->target;
            const uint32_t value = packet->value;
            const bool no_target = false;
            SkillCallback(value_id, caster_id, target_id, value, no_target);
        });

    UnsetPlayerStatistics();
    SetPlayerStatistics();
}

void PartyStatisticsWindow::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);

    const bool party_size_changed = PartySizeChanged();

    if ((party_size_changed || (party_size == 1U)) &&
        (GW::Constants::InstanceType::Outpost == GW::Map::GetInstanceType())) {
        SetPlayerStatistics();
    }

    if (party_indicies.empty()) {
        SetPartyIndicies();
    }
    if (party_names.empty()) {
        SetPartyNames();
    }
    if (party_stats.empty()) {
        SetPartySkills();
    }

    const float time_diff_threshold{600.0F};
    if (!chat_queue.empty() && (TIMER_DIFF(send_timer) > time_diff_threshold)) {
        send_timer = TIMER_INIT();
        if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) return;
        if (!GW::Agents::GetPlayer()) return;

        GW::Chat::SendChat('#', chat_queue.front().c_str());
        chat_queue.pop();
    }
}

void PartyStatisticsWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;

    if (!GW::Map::GetIsMapLoaded()) return;
    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 250), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
        size_t party_idx{0};
        for (const auto& player_stats : party_stats) {
            DrawPartyMember(player_stats, party_idx);
            ++party_idx;
        }
    }
    ImGui::End();
}

void PartyStatisticsWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    show_abs_values = ini->GetBoolValue(Name(), VAR_NAME(show_abs_values), show_abs_values);
    show_perc_values = ini->GetBoolValue(Name(), VAR_NAME(show_perc_values), show_perc_values);
    print_by_click = ini->GetBoolValue(Name(), VAR_NAME(print_by_click), print_by_click);
}

void PartyStatisticsWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    ini->SetBoolValue(Name(), VAR_NAME(show_abs_values), show_abs_values);
    ini->SetBoolValue(Name(), VAR_NAME(show_perc_values), show_perc_values);
    ini->SetBoolValue(Name(), VAR_NAME(print_by_click), print_by_click);
}

void PartyStatisticsWindow::DrawSettingInternal() {
    ImGui::Checkbox("Show the absolute skill count", &show_abs_values);
    ImGui::SameLine();
    ImGui::Checkbox("Show the percentage skill count", &show_perc_values);
    ImGui::SameLine();
    ImGui::Checkbox("Print skill statistics by Ctrl+LeftClick", &print_by_click);
}

void PartyStatisticsWindow::Terminate() {
    ToolboxWindow::Terminate();
    UnsetPlayerStatistics();
}

/***********************/
/* Draw Helper Methods */
/***********************/

void PartyStatisticsWindow::DrawPartyMember(const PlayerSkills& player_stats, const size_t party_idx) {

    constexpr size_t buffer_length = 255;
    char header_label[buffer_length] = {'\0'};

    std::wstring* agent_name = party_names[player_stats.agent_id];
    snprintf(header_label, buffer_length, "%ls###%u", agent_name ? agent_name->c_str() : UNKNOWN_PLAYER_NAME, party_idx);

    if (ImGui::CollapsingHeader(header_label)) {
        uint32_t total_num_skills{0};
        std::for_each(player_stats.skills.begin(), player_stats.skills.end(),
            [&total_num_skills](const auto& p) { total_num_skills += p.count; });
        if (0U == total_num_skills) total_num_skills = 1U;

        if (print_by_click) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.0F);
            char button_name[buffer_length] = {'\0'};
            snprintf(button_name, buffer_length, "###WriteStatistics%d", party_idx);
            const float width = ImGui::GetWindowWidth();
            const float height = ImGui::GetTextLineHeightWithSpacing() * MAX_NUM_SKILLS;
            if (ImGui::Button(button_name, ImVec2(width, height)) && ImGui::IsKeyDown(VK_CONTROL)) {
                WritePlayerStatisticsAllSkills(party_idx);
            }
            ImGui::PopStyleVar();
            ImGui::SetCursorPos(ImVec2{ImGui::GetCursorPos().x, ImGui::GetCursorPos().y - height});
        }

        char table_name[buffer_length] = {'\0'};
        snprintf(table_name, buffer_length, "###Table%d", party_idx);

        const float width = ImGui::GetWindowWidth();

        if (show_perc_values && show_abs_values) {
            ImGui::Columns(3, table_name, false);
            ImGui::SetColumnWidth(0, width * 0.6F);
            ImGui::SetColumnWidth(1, width * 0.2F);
            ImGui::SetColumnWidth(2, width * 0.2F);
        } else if (show_perc_values || show_abs_values) {
            ImGui::Columns(2, table_name, false);
            ImGui::SetColumnWidth(0, width * 0.7F);
            ImGui::SetColumnWidth(1, width * 0.3F);
        }

        for (const auto [skill_id, skill_count, skill_name] : player_stats.skills) {
            const float percentage = (static_cast<float>(skill_count) / static_cast<float>(total_num_skills)) * 100.0F;

            ImGui::Text("%ls", skill_name->c_str());

            if (show_perc_values && show_abs_values) {
                ImGui::NextColumn();
                ImGui::Text("%u", skill_count);
                ImGui::NextColumn();
                ImGui::Text("%.2f%%", percentage);
                ImGui::NextColumn();
                ImGui::Separator();
            } else if (show_perc_values && !show_abs_values) {
                ImGui::NextColumn();
                ImGui::Text("%.2f%%", percentage);
                ImGui::NextColumn();
                ImGui::Separator();
            } else if (!show_perc_values && show_abs_values) {
                ImGui::NextColumn();
                ImGui::Text("%u", skill_count);
                ImGui::NextColumn();
                ImGui::Separator();
            }
        }

        if (show_perc_values || show_abs_values) {
            ImGui::EndColumns();
        }
    }
}

/********************/
/* Callback Methods */
/********************/

void PartyStatisticsWindow::MapLoadedCallback(GW::HookStatus*, GW::Packet::StoC::MapLoaded*) {
    switch (GW::Map::GetInstanceType()) {
        case GW::Constants::InstanceType::Explorable: {
            if (!Instance().in_explorable) {
                Instance().in_explorable = true;
                Instance().UnsetPlayerStatistics();
            }
            break;
        }
        case GW::Constants::InstanceType::Outpost: {
            Instance().in_explorable = false;
            break;
        }
        case GW::Constants::InstanceType::Loading:
        default: break;
    }
}

void PartyStatisticsWindow::SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t target_id,
    const uint32_t value, const bool no_target) {
    uint32_t agent_id = caster_id;
    const uint32_t activated_skill_id = value;

    switch (value_id) {
        case GW::Packet::StoC::GenericValueID::instant_skill_activated:
        case GW::Packet::StoC::GenericValueID::skill_activated:
        case GW::Packet::StoC::GenericValueID::skill_finished:
        case GW::Packet::StoC::GenericValueID::attack_skill_activated:
        case GW::Packet::StoC::GenericValueID::attack_skill_finished: {
            if (!no_target) {
                agent_id = target_id;
            }
            break;
        }
        default: {
            return;
        }
    }

    if (NONE_SKILL == value) return;

    if (party_indicies.empty() || party_stats.empty()) return;

    if (party_ids.find(agent_id) == party_ids.end()) return;

    const size_t party_idx = party_indicies[agent_id];
    Skills& player_skills = party_stats[party_idx].skills;

    const auto skill_it = std::find_if(player_skills.begin(), player_skills.end(),
        [&activated_skill_id](const auto& skill) { return skill.id == activated_skill_id; });
    const bool skill_found = skill_it != player_skills.end();

    /* Other player skill casted for the first time */
    if (!skill_found) {
        size_t skill_idx{0};
        for (auto& [id, count, name] : player_skills) {
            if (NONE_SKILL != id) {
                ++skill_idx;

                continue;
            }

            id = activated_skill_id;
            count = 1;
            name = &GetSkillName(activated_skill_id);

            return;
        }
    }

    for (auto& [id, count, _] : player_skills) {
        if (activated_skill_id != id) continue;

        ++count;
        return;
    }
}

bool PartyStatisticsWindow::PartySizeChanged() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return false;

    static auto last_party_size = static_cast<size_t>(-1);
    if (static_cast<size_t>(-1) == last_party_size) {
        last_party_size = GW::PartyMgr::GetPartySize();
        party_size = last_party_size;
    }

    const uint32_t current_party_size = GW::PartyMgr::GetPartySize();
    const bool party_size_change = current_party_size != last_party_size;
    if (party_size_change) {
        last_party_size = current_party_size;
        party_size = last_party_size;
    }

    return party_size_change;
}

/********************/
/* Set Data Methods */
/********************/

void PartyStatisticsWindow::UnsetPlayerStatistics() {
    party_ids.clear();
    party_indicies.clear();
    party_names.clear();
    party_stats.clear();

    for (auto& [_, encoder] : skill_names) {
        if (encoder) delete encoder;
    }
    skill_names.clear();

    for (auto& [_, encoder] : agent_names) {
        if (encoder) delete encoder;
    }
    agent_names.clear();
}

void PartyStatisticsWindow::SetPlayerStatistics() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;
    if (!GW::Map::GetIsMapLoaded()) return;

    party_size = GW::PartyMgr::GetPartySize();

    SetPartyIndicies();
    SetPartyNames();
    SetPartySkills();
}

void PartyStatisticsWindow::SetPartyIndicies() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;
    if (!GW::Map::GetIsMapLoaded()) return;

    GW::PartyInfo* info = GW::PartyMgr::GetPartyInfo();
    if (info == nullptr) return;

    GW::PlayerArray players = GW::Agents::GetPlayerArray();
    if (!players.valid()) return;

    party_ids.clear();
    party_indicies.clear();

    size_t index = 0;
    for (GW::PlayerPartyMember& player : info->players) {
        uint32_t id = players[player.login_number].agent_id;

        if (id == GW::Agents::GetPlayerId()) {
            self_party_idx = index;
        }

        party_indicies[id] = index++;
        party_ids.insert(id);

        for (GW::HeroPartyMember& hero : info->heroes) {
            if (hero.owner_player_id == player.login_number) {
                party_indicies[hero.agent_id] = index++;

                party_ids.insert(hero.agent_id);
            }
        }
    }
    for (GW::HenchmanPartyMember& hench : info->henchmen) {
        party_indicies[hench.agent_id] = index++;

        party_ids.insert(hench.agent_id);
    }
}

void PartyStatisticsWindow::SetPartyNames() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;
    if (!GW::Map::GetIsMapLoaded()) return;

    const GW::AgentArray& agents = GW::Agents::GetAgentArray();
    if (!agents.valid()) return;

    party_names.clear();

    for (const auto id : party_ids) {
        party_names[id] = &GetPlayerName(id);
    }
}

void PartyStatisticsWindow::SetPartySkills() {
    if (!GW::PartyMgr::GetIsPartyLoaded()) return;
    if (!GW::Map::GetIsMapLoaded()) return;

    party_stats.clear();
    party_stats = PartySkills{party_size, PlayerSkills{}};

    for (const auto [agent_id, party_idx] : party_indicies) {
        party_stats[party_idx] = {agent_id, Skills{}};
    }

    size_t party_idx{0};
    for (PlayerSkills& player_stats : party_stats) {
        PlayerSkills& player_skills = party_stats[party_idx];

        const uint32_t id = player_stats.agent_id;

        const GW::Skillbar* const skillbar = GetPlayerSkillbar(id);
        Skills skills(MAX_NUM_SKILLS);

        /* Skillbar for other players and henchmen is unknown in outpost init with No_Skill */
        if (nullptr == skillbar) {
            player_skills.skills = skills;
            ++party_idx;
            continue;
        }

        size_t skill_idx = 0;
        for (const GW::SkillbarSkill& skill : skillbar->skills) {
            skills[skill_idx] = {skill.skill_id, 0U, &GetSkillName(skill.skill_id)};
            ++skill_idx;
        }

        player_skills.skills = skills;
        ++party_idx;
    }
}

/************************/
/* Chat Command Methods */
/************************/

void PartyStatisticsWindow::CmdSkillStatistics(const wchar_t* message, int argc, LPWSTR* argv) {
    UNREFERENCED_PARAMETER(message);

    /* command: /skillstats */
    /* will write the stats of the self player */
    if (argc < 2) {
        PartyStatisticsWindow::Instance().WritePlayerStatistics();
        return;
    }

    const auto arg1 = GuiUtils::ToLower(argv[1]);

    if (argc == 2) {
        /* command: /skillstats reset */
        if (arg1 == L"reset") {
            PartyStatisticsWindow::Instance().UnsetPlayerStatistics();
        }
        /* command: /skllstats playerNum */
        else {
            auto player_number = uint32_t{0};
            GuiUtils::ParseUInt(argv[1], &player_number);
            --player_number; // List will start at index zero

            PartyStatisticsWindow::Instance().WritePlayerStatistics(player_number);
        }

        return;
    }

    /* command: /skillstats playerNum skillNum */
    if (argc >= 3) {
        const auto player_number_arg = argv[1];
        auto player_number = uint32_t{0};

        GuiUtils::ParseUInt(player_number_arg, &player_number);
        --player_number; // List will start at index zero

        auto skill_number = uint32_t{0};
        GuiUtils::ParseUInt(argv[2], &skill_number);
        --skill_number; // List will start at index zero

        PartyStatisticsWindow::Instance().WritePlayerStatistics(player_number, skill_number);
    }
}

void PartyStatisticsWindow::WritePlayerStatistics(const uint32_t player_idx, const uint32_t skill_idx) {
    if (GW::Constants::InstanceType::Loading == GW::Map::GetInstanceType()) return;

    /* all skills for self player */
    if (static_cast<size_t>(-1) == player_idx) {
        WritePlayerStatisticsAllSkills(self_party_idx);
        /* single skill for some player */
    } else if (static_cast<uint32_t>(-1) != skill_idx) {
        WritePlayerStatisticsSingleSkill(player_idx, skill_idx);
        /* all skills for some player */
    } else {
        WritePlayerStatisticsAllSkills(player_idx);
    }
}

void PartyStatisticsWindow::WritePlayerStatisticsAllSkills(const uint32_t player_idx) {
    if (player_idx >= party_stats.size()) return;

    const PlayerSkills& player_stats = party_stats[player_idx];
    const Skills& player_skills = player_stats.skills;
    std::wstring* agent_name = party_names[player_stats.agent_id];

    for (const auto& [_, skill_count, skill_name] : player_skills) {
        if (0U == skill_count) continue;

        const std::wstring player_stats_str = GetSkillString(*agent_name, *skill_name, skill_count);

        chat_queue.push(std::wstring(player_stats_str.begin(), player_stats_str.end()));
    }
}

void PartyStatisticsWindow::WritePlayerStatisticsSingleSkill(const uint32_t player_idx, const uint32_t skill_idx) {
    if (skill_idx > (MAX_NUM_SKILLS - 1)) return;
    if (player_idx >= party_stats.size()) return;

    const PlayerSkills& player_stats = party_stats[player_idx];
    const Skills& player_skills = player_stats.skills;
    const std::wstring* agent_name = party_names[player_stats.agent_id];

    const uint32_t skill_count = player_skills[skill_idx].count;
    const std::wstring* skill_name = player_skills[skill_idx].name;

    const std::wstring player_stats_str = GetSkillString(*agent_name, *skill_name, skill_count);

    chat_queue.push(std::wstring(player_stats_str.begin(), player_stats_str.end()));
}
