#include "stdafx.h"

#include <GWCA/GameEntities/Player.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Item.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/CharContext.h>
#include <GWCA/Context/PreGameContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PlayerMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Modules/Resources.h>
#include <Windows/RerollWindow.h>
#include <Timer.h>

#include <ImGuiAddons.h>
#include <GuiUtils.h>



namespace {
    GW::CharContext* GetCharContext() {
        auto g = GW::GameContext::instance();
        return g ? g->character : 0;
    }

    GW::PartyInfo* GetPlayerParty() {
        auto g = GW::GameContext::instance();
        if (!g) return 0;
        auto c = g->party;
        if (!c) return 0;
        return c->player_party;
    }
    uint32_t GetPlayerNumber() {
        auto c = GetCharContext();
        return c ? c->player_number : 0;
    }
    const wchar_t* GetPlayerName() {
        auto c = GetCharContext();
        return c ? c->player_name : 0;
    }
    const wchar_t* GetAccountEmail() {
        auto c = GetCharContext();
        wchar_t* email = c ? c->player_email : 0;
        if (email && !email[0])
            email = 0;
        return email;
    }
    GW::Guild* GetCurrentGH()
    {
        GW::AreaInfo* m = GW::Map::GetCurrentMapInfo();
        if (!m || m->type != GW::RegionType::RegionType_GuildHall)
            return nullptr;
        const GW::Array<GW::Guild*>& guilds = GW::GuildMgr::GetGuildArray();
        if (!guilds.valid())
            return nullptr;
        for (size_t i = 0; i < guilds.size(); i++) {
            if (!guilds[i])
                continue;
            return guilds[i];
        }
        return nullptr;
    }
    const wchar_t* GetNextPartyLeader() {
        GW::PartyInfo* player_party = GetPlayerParty();
        if (!player_party || !player_party->players.valid() || player_party->players.size() < 2)
            return 0;
        uint32_t player_number = GetPlayerNumber();
        for (size_t i = 0; i < player_party->players.size(); i++) {
            if (player_party->players[i].login_number == player_number)
                continue;
            auto player = GW::PlayerMgr::GetPlayerByID(player_party->players[i].login_number);
            if (!player)
                continue;
            return player->name;
        }
        return 0;
    }
    typedef void(__cdecl* SetOnlineStatus_pt)(uint32_t status);
    SetOnlineStatus_pt SetOnlineStatus_Func;
    SetOnlineStatus_pt RetSetOnlineStatus;

    bool GetIsMapReady() {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading && GW::Map::GetIsMapLoaded() && GW::Agents::GetPlayer();
    }
    bool GetIsCharSelectReady() {
        GW::PreGameContext* pgc = GW::PreGameContext::instance();
        if (!pgc || !pgc->chars.valid())
            return false;
        uint32_t ui_state = 10;
        GW::UI::SendUIMessage(0x10000170, 0, &ui_state);
        return ui_state == 2;
    }

    GW::Constants::MapID GetScrollableOutpostForEliteArea(GW::Constants::MapID elite_area) {
        GW::Constants::MapID map_id = GW::Constants::MapID::Embark_Beach;
        switch (elite_area) {
        case GW::Constants::MapID::The_Deep:
            map_id = GW::Constants::MapID::Cavalon_outpost;
            break;
        case GW::Constants::MapID::Urgozs_Warren:
            map_id = GW::Constants::MapID::House_zu_Heltzer_outpost;
            break;
        default:
            return GW::Constants::MapID::None;
        }
        if (!GW::Map::GetIsMapUnlocked(map_id))
            map_id = GW::Constants::MapID::Embark_Beach;
        return map_id;
    }

    GW::Item* GetScrollItemForEliteArea(GW::Constants::MapID elite_area) {
        uint32_t scroll_model_id = 0;
        switch (elite_area) {
        case GW::Constants::MapID::The_Deep:
            scroll_model_id = 22279;
            break;
        case GW::Constants::MapID::Urgozs_Warren:
            scroll_model_id = 3256;
            break;
        }
        if (!scroll_model_id) return 0;

        return GW::Items::GetItemByModelId(
            scroll_model_id,
            static_cast<int>(GW::Constants::Bag::Backpack),
            static_cast<int>(GW::Constants::Bag::Storage_14));
    }

    const wchar_t* GetRemainingArgsWstr(const wchar_t* message, int argc_start) {
        const wchar_t* out = message;
        for (int i = 0; i < argc_start && out; i++) {
            out = wcschr(out, ' ');
            if (out) out++;
        }
        return out;
    };
    

}

RerollWindow::~RerollWindow() {
    for (auto it : account_characters) {
        delete it.second;
    }
    account_characters.clear();
    if (guild_hall_uuid) {
        delete guild_hall_uuid;
        guild_hall_uuid = 0;
    }
}
RerollWindow::RerollWindow() {
}
void RerollWindow::Draw(IDirect3DDevice9* pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (reroll_stage == PromptPendingLogout) {
        if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
            reroll_stage = PendingLogout;
        }
        else {
            ImGui::OpenPopup("##reroll_confirm_popup");
            if (ImGui::BeginPopupModal("##reroll_confirm_popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("You're currently in an explorable area.\nAre you sure you want to change character?");
                bool check_key = TIMER_DIFF(reroll_stage_set) > 500; // Avoid enter key being read from last press
                if (ImGui::Button("Yes", ImVec2(120, 0)) || (check_key && ImGui::IsKeyPressed(VK_RETURN))) {
                    reroll_stage = PendingLogout;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No", ImVec2(120, 0)) || (check_key && ImGui::IsKeyPressed(VK_ESCAPE))) {
                    reroll_stage = None;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }
    if (!visible) return;



    ImGui::SetNextWindowCenter(ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags()))
        return ImGui::End();

    if (!available_chars_ptr || !available_chars_ptr->valid()) {
        ImGui::TextDisabled("Go to character select screen to record available characters");
    }
    else {
        ImGui::Text("Click on a character name to switch to that character.");
        ImGui::Checkbox("Travel to same location after rerolling",&travel_to_same_location_after_rerolling);
        ImGui::Checkbox("Re-join your party after rerolling", &rejoin_party_after_rerolling);
        const float btnw = ImGui::GetContentRegionAvail().x / 2.f;
        const ImVec2 btn_dim = { btnw,0.f };
        std::string buf;
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
        for (size_t i = 0; i < available_chars_ptr->size(); i++) {
            auto& character = available_chars_ptr->at(i);
            wchar_t* player_name = character.player_name;
            uint32_t profession = character.primary();
            buf = GuiUtils::WStringToString(player_name);
            if ((i % 2) != 0)
                ImGui::SameLine();
            if (ImGui::IconButton(buf.c_str(), Resources::GetProfessionIcon((GW::Constants::Profession)profession),btn_dim)) {
                bool _same_map = travel_to_same_location_after_rerolling;
                bool _same_party = travel_to_same_location_after_rerolling && rejoin_party_after_rerolling;
                if (rejoin_party_after_rerolling && !_same_party) {
                    GW::PartyInfo* p = GetPlayerParty();
                    if (p && p->players.size() > 1)
                        _same_party = true;
                }
                Reroll(player_name, _same_map || _same_party, _same_party);
            }
        }
        ImGui::PopStyleVar();
    }

    ImGui::End();

}
std::vector<std::wstring>* RerollWindow::GetAvailableChars() {
    const wchar_t* email = GetAccountEmail();
    return email ? account_characters[email] : 0;
}

void RerollWindow::CmdReroll(const wchar_t* message, int argc, LPWSTR*) {
    if (argc < 2) {
        Log::Error("Incorrect syntax: /reroll [profession|character_name]");
        return;
    }
    auto available_characters = Instance().available_chars_ptr;
    if (!available_characters || !available_characters->valid()) {
        Log::Error("Failed to get available characters");
        return;
    }
    std::wstring character_or_profession = GuiUtils::ToLower(GetRemainingArgsWstr(message,1));
    wchar_t* to_find[] = {
        L"",
        L"warrior",
        L"ranger",
        L"monk",
        L"necromancer",
        L"mesmer",
        L"elementalist",
        L"assassin",
        L"ritualist",
        L"paragon",
        L"dervish"
    };
    
    // Search by profession
    for (size_t i = 0; i < _countof(to_find); i++) {
        if (!to_find[0])
            continue;
        if (!wcsstr(to_find[i], character_or_profession.c_str()))
            continue;
        for (size_t j = 0; j < available_characters->size(); j++) {
            if (available_characters->at(j).primary() == i) {
                Instance().Reroll(available_characters->at(j).player_name, Instance().travel_to_same_location_after_rerolling, Instance().rejoin_party_after_rerolling);
                return;
            }
        }
    }


    // Search by character name
    for (size_t i = 0; i < available_characters->size(); i++) {
        if (!wcsstr(GuiUtils::ToLower(available_characters->at(i).player_name).c_str(), character_or_profession.c_str()))
            continue;
        Instance().Reroll(available_characters->at(i).player_name, Instance().travel_to_same_location_after_rerolling, Instance().rejoin_party_after_rerolling);
        return;
    }
    Log::Error("Failed to match profession or character name for command");
    return;
}

void RerollWindow::OnSetStatus(uint32_t status) {
    GW::Hook::EnterHook();
    if (Instance().reroll_stage == WaitForCharacterLoad)
        status = Instance().online_status;
    RetSetOnlineStatus(status);
    GW::Hook::LeaveHook();
}
void RerollWindow::OnUIMessage(GW::HookStatus*, uint32_t msg_id, void*, void*) {
    if (msg_id == 0x10000170)
        Instance().check_available_chars = true;
}

void RerollWindow::Initialize() {
    ToolboxWindow::Initialize();
    // Add an entry to check available characters at login screen
    GW::UI::RegisterUIMessageCallback(&OnGoToCharSelect_Entry, OnUIMessage, 0x4000);
    // Hook to override status on login - allows us to keep FL status across rerolls without messing with UI
    SetOnlineStatus_Func = (SetOnlineStatus_pt)GW::Scanner::FindAssertion("p:\\code\\gw\\friend\\friendapi.cpp", "status < FRIEND_STATUSES", -0x11);
    if (SetOnlineStatus_Func) {
        GW::Hook::CreateHook(SetOnlineStatus_Func, OnSetStatus, (void**)&RetSetOnlineStatus);
        GW::Hook::EnableHooks(SetOnlineStatus_Func);
    }

    uintptr_t address = GW::Scanner::Find("\x8b\x35\x00\x00\x00\x00\x57\x69\xF8\x84\x00\x00\x00", "xx????xxxxxxx", 0x2);
    if (address) {
        available_chars_ptr = *(GW::Array<AvailableCharacterInfo>**)address;
    }
    GW::Chat::CreateCommand(L"reroll", CmdReroll);
    GW::Chat::CreateCommand(L"rr", CmdReroll);
}

void RerollWindow::Update(float) {
    if (check_available_chars && IsCharSelectReady()) {
        auto chars = GW::PreGameContext::instance()->chars;
        const wchar_t* email = GetAccountEmail();
        if (email) {
            auto found = account_characters.find(email);
            if (found != account_characters.end() && found->second) {
                found->second->clear();
            }
            for (auto& p : chars) {
                AddAvailableCharacter(email, p.character_name);
            }
            check_available_chars = false;
        }
    }
    if (reroll_stage != None && TIMER_INIT() > reroll_timeout) {
        RerollFailed(L"Reroll timed out");
        return;
    }
    GW::PreGameContext* pgc = GW::PreGameContext::instance();
    switch (reroll_stage) {
        case PendingLogout: {
            uint32_t logout = 1;
            GW::UI::SendUIMessage(0x1000009b, &logout);
            reroll_stage = WaitingForCharSelect;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 10000;
            return;
        }
        case WaitingForCharSelect: {
            if (!GetIsCharSelectReady())
                return;
            reroll_stage = CheckForCharname;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 5000;
            return;
        }
        case CheckForCharname: {
            if (!GetIsCharSelectReady())
                return;
            for (size_t i = 0; i < pgc->chars.size(); i++) {
                if (wcscmp(pgc->chars[i].character_name, reroll_to_player_name) == 0) {
                    reroll_index_needed = i;
                    reroll_index_current = 0xffffffdd;
                    reroll_stage = NavigateToCharname;
                    reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 3000;
                    return;
                }
            }
            return;
        }
        case NavigateToCharname: {
            if (!GetIsCharSelectReady())
                return;
            if (pgc->index_1 == reroll_index_current)
                return; // Not moved yet
            HWND h = GW::MemoryMgr::GetGWWindowHandle();
            if (pgc->index_1 == reroll_index_needed) {
                // Play
                SendMessage(h, WM_KEYDOWN, 0x50, 0x00190001);
                SendMessage(h, WM_CHAR, 'p', 0x00190001);
                SendMessage(h, WM_KEYUP, 0x50, 0xC0190001);
                reroll_stage = WaitForCharacterLoad;
                reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                return;
            }
            reroll_index_current = pgc->index_1;
            SendMessage(h, WM_KEYDOWN, VK_RIGHT, 0x014D0001);
            SendMessage(h, WM_KEYUP, VK_RIGHT, 0xC14D0001);
            return;
        }
        case WaitForCharacterLoad: {
            if (GetIsCharSelectReady())
                return;
            if (!GetIsMapReady() || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
                return;
            const wchar_t* player_name = GetPlayerName();
            if (!player_name || wcscmp(player_name, reroll_to_player_name) != 0) {
                RerollFailed(L"Wrong character was loaded");
                return;
            }
            if (same_map) {
                if (!IsInMap()) {
                    if (guild_hall_uuid) {
                        // Was previously in a guild hall
                        GW::GuildMgr::TravelGH(*(GW::GHKey*)guild_hall_uuid);
                    }
                    else {
                        if (!GW::Map::GetIsMapUnlocked(map_id)) {
                            RerollFailed(L"Map isn't unlocked");
                            return;
                        }
                        reroll_scroll_from_map_id = (uint32_t)GetScrollableOutpostForEliteArea(map_id);
                        if (reroll_scroll_from_map_id) {
                            if (!GW::Map::GetIsMapUnlocked((GW::Constants::MapID)reroll_scroll_from_map_id)) {
                                RerollFailed(L"No scrollable outpost unlocked");
                                return;
                            }
                            if (!GetScrollItemForEliteArea(map_id)) {
                                RerollFailed(L"No scroll available for elite area");
                                return;
                            }
                            if (GW::Map::GetMapID() != (GW::Constants::MapID)reroll_scroll_from_map_id) {
                                GW::Map::Travel((GW::Constants::MapID)reroll_scroll_from_map_id, 0, region_id, language_id);
                            }
                           
                            reroll_stage = WaitForScrollableOutpost;
                            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                            return;
                        }
                        GW::Map::Travel(map_id, 0, region_id, language_id);
                        reroll_stage = WaitForActiveDistrict;
                        reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                        return;
                    }
                }
                reroll_stage = WaitForMapLoad;
                reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
                return;
            }
            RerollSuccess();
            return;
        }        
        case WaitForScrollableOutpost: {
            if (!GetIsMapReady() || GW::Map::GetMapID() != (GW::Constants::MapID)reroll_scroll_from_map_id)
                return;
            GW::Item* scroll = GetScrollItemForEliteArea(map_id);
            if (!scroll) {
                RerollFailed(L"No scroll available for elite area");
                return;
            }
            GW::Items::UseItem(scroll);
            reroll_stage = WaitForActiveDistrict;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
        } break;

        case WaitForActiveDistrict: {
            if (!GetIsMapReady() || !IsInMap(false))
                return;
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
                return;
            if (!IsInMap()) {
                // Same map, wrong district
                GW::Map::Travel(map_id, district_id, region_id, language_id);
            }
            reroll_stage = WaitForMapLoad;
            reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
            return;
        }
        case WaitForMapLoad: {
            if (!GetIsMapReady() || !IsInMap())
                return;
            if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
                return;
            
            if (same_party && party_leader[0]) {
                GW::PartyInfo* player_party = GetPlayerParty();
                if (player_party && player_party->GetPartySize() > 1) {
                    GW::PartyMgr::LeaveParty();
                    GW::PartyMgr::KickAllHeroes();
                }
                reroll_stage = WaitForEmptyParty;
                reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 3000;
                return;
            }
            RerollSuccess();
            return;
        case WaitForEmptyParty:
            GW::PartyInfo* player_party = GetPlayerParty();
            if (player_party && player_party->GetPartySize() > 1)
                return;
            wchar_t msg_buf[32];
            ASSERT(same_party && party_leader[0]);
            ASSERT(swprintf(msg_buf, _countof(msg_buf), L"invite %s", party_leader) != -1);
            GW::Chat::SendChat('/', msg_buf);
            RerollSuccess();
            return;
        }
    }
}
void RerollWindow::AddAvailableCharacter(const wchar_t* email, const wchar_t* charname) {
    if (!charname || !charname[0])
        return;
    auto found = account_characters.find(email);
    if (found == account_characters.end() || !found->second) {
        account_characters[email] = new std::vector<std::wstring>();
    }
    account_characters[email]->push_back(charname);
}
bool RerollWindow::IsInMap(bool include_district) {
    if (guild_hall_uuid) {
        GW::Guild* current_location = GetCurrentGH();
        return current_location && memcmp(&current_location->key, guild_hall_uuid, sizeof(*guild_hall_uuid)) == 0;
    }
    return GW::Map::GetMapID() == map_id && (!include_district || district_id == 0 || GW::Map::GetDistrict() == district_id) && GW::Map::GetRegion() == region_id && GW::Map::GetLanguage() == language_id;
}
void RerollWindow::RerollSuccess() {
    reroll_stage = None;
    if (reverting_reroll && failed_message) {
        Log::ErrorW(failed_message);
    }
}
bool RerollWindow::IsCharSelectReady() {
    GW::PreGameContext* pgc = GW::PreGameContext::instance();
    if (!pgc || !pgc->chars.valid())
        return false;
    uint32_t ui_state = 10;
    GW::UI::SendUIMessage(0x10000170, 0, &ui_state);
    return ui_state == 2;
}
void RerollWindow::RerollFailed(wchar_t* reason) {
    if (reroll_stage == PromptPendingLogout) {
        reroll_stage = None;
        return;
    }
    reroll_stage = None;
    if (reverting_reroll)
        return; // Can't do anything.
    failed_message = reason;
    reverting_reroll = true;
    wcscpy(reroll_to_player_name, initial_player_name);
    same_map = false;
    same_party = true;
    reroll_timeout = TIMER_INIT() + 1000;
    reroll_stage = PendingLogout;
}
bool RerollWindow::Reroll(wchar_t* character_name, GW::Constants::MapID _map_id) {
    if (!Reroll(character_name, true, false))
        return false;
    map_id = _map_id;
    if (guild_hall_uuid) {
        delete[] guild_hall_uuid;
        guild_hall_uuid = 0;
    }
    district_id = 0;
    return true;
}
bool RerollWindow::Reroll(wchar_t* character_name, bool _same_map, bool _same_party) {
    reroll_stage = None;
    reverting_reroll = false;
    failed_message = 0;
    if (!character_name)
        return false;
    bool found = false;
    if (available_chars_ptr && available_chars_ptr->valid()) {
        for (size_t i = 0; !found && i < available_chars_ptr->size(); i++) {
            found = wcscmp(available_chars_ptr->at(i).player_name, character_name) == 0;
        }
    }
    if (!found)
        return false;
    wcscpy(reroll_to_player_name, character_name);
    const wchar_t* player_name = GetPlayerName();
    if (!player_name || wcscmp(player_name,character_name) == 0)
        return false;
    wcscpy(initial_player_name, player_name);
    const wchar_t* party_leader_name = GetNextPartyLeader();
    if (party_leader_name) {
        wcscpy(party_leader, party_leader_name);
        if (!_same_map && _same_party)
            _same_map = true; // Ensure we go to same map if we want to join the same party
    }
    else {
        party_leader[0] = 0;
    }
    map_id = GW::Map::GetMapID();
    district_id = GW::Map::GetDistrict();
    region_id = GW::Map::GetRegion();
    language_id = GW::Map::GetLanguage();
    online_status = GW::FriendListMgr::GetMyStatus();
    if (guild_hall_uuid) {
        delete[] guild_hall_uuid;
        guild_hall_uuid = 0;
    }
    GW::Guild* current_guild_hall = GetCurrentGH();
    if (current_guild_hall) {
        guild_hall_uuid = new uint32_t[4];
        memcpy(guild_hall_uuid, &current_guild_hall->key, sizeof(current_guild_hall->key));
    }
    same_map = _same_map;
    same_party = _same_party;
    reroll_timeout = (reroll_stage_set = TIMER_INIT()) + 20000;
    reroll_stage = PromptPendingLogout;
    return true;
}
void RerollWindow::LoadSettings(CSimpleIni* ini) {
    ToolboxWindow::LoadSettings(ini);
    CSimpleIni::TNamesDepend keys;
    if (ini->GetAllKeys("RerollWindow_AvailableChars", keys)) {
        for (auto& it : keys) {
            std::wstring charname_ws = GuiUtils::StringToWString(it.pItem);
            std::string email_s = ini->GetValue("RerollWindow_AvailableChars", it.pItem, "");
            if (email_s.empty()) continue;
            std::wstring email_ws = GuiUtils::StringToWString(email_s);
            AddAvailableCharacter(email_ws.c_str(), charname_ws.c_str());
        }
    }
    travel_to_same_location_after_rerolling = ini->GetBoolValue(Name(), VAR_NAME(travel_to_same_location_after_rerolling), travel_to_same_location_after_rerolling);
    rejoin_party_after_rerolling = ini->GetBoolValue(Name(), VAR_NAME(rejoin_party_after_rerolling), rejoin_party_after_rerolling);
}
void RerollWindow::SaveSettings(CSimpleIni* ini) {
    ToolboxWindow::SaveSettings(ini);
    for (auto it : account_characters) {
        std::string email_s = GuiUtils::WStringToString(it.first);
        auto chars = it.second;
        for (auto it2 = chars->begin(); it2 != chars->end(); it2++) {
            std::string charname_s = GuiUtils::WStringToString(*it2);
            if (charname_s.empty()) continue;
            ini->SetValue("RerollWindow_AvailableChars", charname_s.c_str(), email_s.c_str());
        }
    }
    ini->SetBoolValue(Name(), VAR_NAME(travel_to_same_location_after_rerolling), travel_to_same_location_after_rerolling);
    ini->SetBoolValue(Name(), VAR_NAME(rejoin_party_after_rerolling), rejoin_party_after_rerolling);
}
