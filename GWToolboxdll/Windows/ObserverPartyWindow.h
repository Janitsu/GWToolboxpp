#pragma once

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\GameEntities\Skill.h>
#include "GWCA\Managers\UIMgr.h"

#include <GuiUtils.h>
#include <Timer.h>
#include <ToolboxWindow.h>
#include <Color.h>

#define NO_AGENT 0

class ObserverPartyWindow : public ToolboxWindow {
public:
    //

private:
    ObserverPartyWindow() {};
    ~ObserverPartyWindow() {
        //
    };

public:
    static ObserverPartyWindow& Instance() {
        static ObserverPartyWindow instance;
        return instance;
    }

    const char* Name() const override { return "Observer Parties"; }
    const char* Icon() const override { return ICON_FA_EYE; }
    void Draw(IDirect3DDevice9* pDevice) override;
    void Initialize() override;

    void DrawBlankPartyMember(float& offset);
    void DrawPartyMember(float& offset, ObserverModule::ObservableAgent& agent, const ObserverModule::ObservableGuild* guild,
        const bool odd, const bool is_player, const bool is_target);
    void DrawParty(float& offset, const ObserverModule::ObservableParty& party);
    void DrawHeaders(const size_t party_count);

    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;
    void DrawSettingInternal() override;


protected:
    float text_long     = 0;
    float text_medium   = 0;
    float text_short    = 0;
    float text_tiny     = 0;


    bool show_player_number = true;
    bool show_profession = true;
    bool show_player_guild_tag = true;
    bool show_player_guild_rating = false;
    bool show_player_guild_rank = false;
    bool show_kills = true;
    bool show_deaths = true;
    bool show_kdr = true;
    bool show_cancels = true;
    bool show_interrupts = true;
    bool show_knockdowns = true;
    bool show_received_party_attacks = true;
    bool show_dealt_party_attacks = true;
    bool show_received_party_crits = true;
    bool show_dealt_party_crits = true;
    bool show_received_party_skills = true;
    bool show_dealt_party_skills = true;
    bool show_skills_used = true;

private:
    // ini
    CSimpleIni* inifile = nullptr;
};

