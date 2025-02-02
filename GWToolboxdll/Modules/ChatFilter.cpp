#include "stdafx.h"

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Map.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <Defines.h>
#include <ImGuiAddons.h>
#include <Logger.h>

#include <Modules/Resources.h>
#include <Modules/ChatFilter.h>

//#define PRINT_CHAT_PACKETS

#define LOAD_BOOL(var) var = ini->GetBoolValue(Name(), #var, var);
#define SAVE_BOOL(var) ini->SetBoolValue(Name(), #var, var);

static bool IsInChallengeMission() {
    GW::AreaInfo* a = GW::Map::GetCurrentMapInfo();
    return a && a->type == GW::RegionType_Challenge;
}

static void printchar(wchar_t c) {
    if (c >= L' ' && c <= L'~') {
        printf("%lc", c);
    } else {
        printf("0x%X ", c);
    }
}

static wchar_t* GetMessageCore() {
    GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
    return buff ? buff->begin() : nullptr;
}
// Due to the way ChatFilter works, if a previous message was blocked then the buffer would still contain the last message.
// Clear down the message buffer if the packet has been blocked.
static void ClearMessageBufferIfBlocked(GW::HookStatus* status, GW::Packet::StoC::PacketBase*) {
    if (!status->blocked)
        return;
    GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
    if (!buff || !buff->valid()) {
        Log::Log("Failed to clear message buffer!\n");
        return;
    }
    buff->clear();
}
void ChatFilter::Initialize() {
    ToolboxModule::Initialize();

    // server messages
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&BlockIfApplicable_Entry,
    [this](GW::HookStatus *status, GW::Packet::StoC::MessageServer *pak) -> void {
        BlockIfApplicable(status, GetMessageCore(), pak->channel);
    });
    

    // global messages
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageGlobal>(&BlockIfApplicable_Entry,
    [this](GW::HookStatus *status, GW::Packet::StoC::MessageGlobal* pak) -> void {
        BlockIfApplicable(status, GetMessageCore(), pak->channel);
    });
    
    // local messages
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&BlockIfApplicable_Entry,
    [this](GW::HookStatus *status, GW::Packet::StoC::MessageLocal *pak) -> void {
        BlockIfApplicable(status, GetMessageCore(), pak->channel);
    });

    GW::StoC::RegisterPostPacketCallback< GW::Packet::StoC::MessageServer>(&ClearIfApplicable_Entry, ClearMessageBufferIfBlocked);
    GW::StoC::RegisterPostPacketCallback< GW::Packet::StoC::MessageGlobal>(&ClearIfApplicable_Entry, ClearMessageBufferIfBlocked);
    GW::StoC::RegisterPostPacketCallback< GW::Packet::StoC::MessageLocal>(&ClearIfApplicable_Entry, ClearMessageBufferIfBlocked);

    GW::Chat::RegisterLocalMessageCallback(&BlockIfApplicable_Entry,
    [this](GW::HookStatus *status, int channel, wchar_t *message) -> void {
        BlockIfApplicable(status, message, static_cast<uint32_t>(channel));
    });
}
void ChatFilter::BlockIfApplicable(GW::HookStatus* status, const wchar_t* message, uint32_t channel) {
    if (message && ShouldIgnore(message, channel)) {
        /*Log::Log("ChatFilter Blocked:\n");
        for (size_t i = 0; message[i]; i++)
            printchar(message[i]);
        printf("\n");*/
        status->blocked = true;
    }
}

void ChatFilter::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    LOAD_BOOL(self_drop_rare);
    LOAD_BOOL(self_drop_common);
    LOAD_BOOL(ally_drop_rare);
    LOAD_BOOL(ally_drop_common);
    LOAD_BOOL(ally_pickup_rare);
    LOAD_BOOL(ally_pickup_common);
    LOAD_BOOL(player_pickup_common);
    LOAD_BOOL(player_pickup_rare);
    LOAD_BOOL(salvage_messages);

    LOAD_BOOL(skill_points);
    LOAD_BOOL(pvp_messages);
    LOAD_BOOL(skill_points);
    LOAD_BOOL(pvp_messages);
    LOAD_BOOL(guild_announcement);
    LOAD_BOOL(pvp_messages);
    LOAD_BOOL(skill_points);
    LOAD_BOOL(pvp_messages);
    LOAD_BOOL(hoh_messages);
    LOAD_BOOL(favor);
    LOAD_BOOL(ninerings);
    LOAD_BOOL(noonehearsyou);
    LOAD_BOOL(lunars);
    LOAD_BOOL(messagebycontent);
    LOAD_BOOL(you_have_been_playing_for);
    LOAD_BOOL(player_has_achieved_title);
    LOAD_BOOL(invalid_target);
    LOAD_BOOL(opening_chest_messages);
    LOAD_BOOL(inventory_is_full);
    LOAD_BOOL(not_enough_energy);
    LOAD_BOOL(item_cannot_be_used);
    LOAD_BOOL(item_already_identified);
    LOAD_BOOL(faction_gain);
    LOAD_BOOL(challenge_mission_messages);

    LOAD_BOOL(filter_channel_local);
    LOAD_BOOL(filter_channel_guild);
    LOAD_BOOL(filter_channel_team);
    LOAD_BOOL(filter_channel_trade);
    LOAD_BOOL(filter_channel_alliance);
    LOAD_BOOL(filter_channel_emotes);

    LOAD_BOOL(block_messages_from_inactive_channels);
    
    strcpy_s(bycontent_word_buf, "");
    strcpy_s(bycontent_regex_buf, "");

    {
        std::ifstream file;
        file.open(Resources::GetPath(L"FilterByContent.txt"));
        if (file.is_open()) {
            file.get(bycontent_word_buf, FILTER_BUF_SIZE, '\0');
            file.close();
            ParseBuffer(bycontent_word_buf, bycontent_words);
        }
    }
    {
        std::ifstream file;
        file.open(Resources::GetPath(L"FilterByContent_regex.txt"));
        if (file.is_open()) {
            file.get(bycontent_regex_buf, FILTER_BUF_SIZE, '\0');
            file.close();
            ParseBuffer(bycontent_regex_buf, bycontent_regex);
        }
    }

#ifdef EXTENDED_IGNORE_LIST
    std::ifstream byauthor_file;
    byauthor_file.open(Resources::GetPath(L"FilterByAuthor.txt"));
    if (byauthor_file.is_open()) {
        byauthor_file.get(byauthor_buf, FILTER_BUF_SIZE, '\0');
        byauthor_file.close();
        ByAuthor_ParseBuf();
    }
#endif
}

void ChatFilter::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    SAVE_BOOL(self_drop_rare);
    SAVE_BOOL(self_drop_common);
    SAVE_BOOL(ally_drop_rare);
    SAVE_BOOL(ally_drop_common);
    SAVE_BOOL(ally_pickup_rare);
    SAVE_BOOL(ally_pickup_common);
    SAVE_BOOL(player_pickup_rare);
    SAVE_BOOL(player_pickup_common);
    SAVE_BOOL(salvage_messages);

    SAVE_BOOL(skill_points);
    SAVE_BOOL(pvp_messages);
    SAVE_BOOL(skill_points);
    SAVE_BOOL(pvp_messages);
    SAVE_BOOL(guild_announcement);
    SAVE_BOOL(pvp_messages);
    SAVE_BOOL(skill_points);
    SAVE_BOOL(pvp_messages);
    SAVE_BOOL(hoh_messages);
    SAVE_BOOL(favor);
    SAVE_BOOL(ninerings);
    SAVE_BOOL(noonehearsyou);
    SAVE_BOOL(lunars);
    SAVE_BOOL(messagebycontent);
    SAVE_BOOL(you_have_been_playing_for);
    SAVE_BOOL(player_has_achieved_title);
    SAVE_BOOL(invalid_target);
    SAVE_BOOL(opening_chest_messages);
    SAVE_BOOL(inventory_is_full);
    SAVE_BOOL(not_enough_energy);
    SAVE_BOOL(item_cannot_be_used);
    SAVE_BOOL(item_already_identified);
    SAVE_BOOL(faction_gain);
    SAVE_BOOL(challenge_mission_messages);

    SAVE_BOOL(filter_channel_local);
    SAVE_BOOL(filter_channel_guild);
    SAVE_BOOL(filter_channel_team);
    SAVE_BOOL(filter_channel_trade);
    SAVE_BOOL(filter_channel_alliance);
    SAVE_BOOL(filter_channel_emotes);

    SAVE_BOOL(block_messages_from_inactive_channels);

    if (timer_parse_filters) {
        timer_parse_filters = 0;
        ParseBuffer(bycontent_word_buf, bycontent_words);
        bycontent_filedirty = true;
    }

    if (timer_parse_regexes) {
        timer_parse_regexes = 0;
        ParseBuffer(bycontent_regex_buf, bycontent_regex);
        bycontent_filedirty = true;
    }

    if (bycontent_filedirty) {
        std::ofstream file1;
        file1.open(Resources::GetPath(L"FilterByContent.txt"));
        if (file1.is_open()) {
            file1.write(bycontent_word_buf, strlen(bycontent_word_buf));
            file1.close();
        }
        std::ofstream file2;
        file2.open(Resources::GetPath(L"FilterByContent_regex.txt"));
        if (file2.is_open()) {
            file2.write(bycontent_regex_buf, strlen(bycontent_regex_buf));
            file2.close();
        }
        bycontent_filedirty = false;
    }

#ifdef EXTENDED_IGNORE_LIST
    if (byauthor_filedirty) {
        std::ofstream byauthor_file;
        byauthor_file.open(Resources::GetPath(L"FilterByAuthor.txt"));
        if (byauthor_file.is_open()) {
            byauthor_file.write(byauthor_buf, strlen(byauthor_buf));
            byauthor_file.close();
            byauthor_filedirty = false;
        }
    }
#endif
}


const wchar_t* ChatFilter::Get1stSegment(const wchar_t *message) const {
    for (size_t i = 0; message[i] != 0; ++i) {
        if (message[i] == 0x10A) return message + i + 1;
    }
    return nullptr;
};

const wchar_t* ChatFilter::Get2ndSegment(const wchar_t *message) const {
    for (size_t i = 0; message[i] != 0; ++i) {
        if (message[i] == 0x10B) return message + i + 1;
    }
    return nullptr;
}

DWORD ChatFilter::GetNumericSegment(const wchar_t *message) const {
    for (size_t i = 0; message[i] != 0; ++i) {
        if ((0x100 < message[i] && message[i] < 0x107) || (0x10D < message[i] && message[i] < 0x110))
            return (message[i + 1] - 0x100u);
    }
    return 0;
}

bool ChatFilter::ShouldIgnoreByAgentThatDropped(const wchar_t* agent_segment) const {
    if (agent_segment == nullptr) return false;     // something went wrong, don't ignore
    if (agent_segment[0] == 0xBA9 && agent_segment[1] == 0x107) return false;
    return true;
}

bool ChatFilter::IsRare(const wchar_t* item_segment) const {
    if (item_segment == nullptr) return false;      // something went wrong, don't ignore
    if (item_segment[0] == 0xA40) return true;      // don't ignore gold items
    if (FullMatch(item_segment, { 0x108, 0x10A, 0x22D9, 0xE7B8, 0xE9DD, 0x2322 })) 
        return true;    // don't ignore ectos
    if (FullMatch(item_segment, { 0x108, 0x10A, 0x22EA, 0xFDA9, 0xDE53, 0x2D16 } ))
        return true; // don't ignore obby shards
    if (FullMatch(item_segment, { 0x108, 0x10A, 0x8101, 0x730E }))
        return true; // don't ignore lockpicks
    return false;
}

bool ChatFilter::FullMatch(const wchar_t* s, const std::initializer_list<wchar_t>& msg) const {
    int i = 0;
    for (wchar_t b : msg) {
        if (s[i++] != b) return false;
    }
    return true;
}

bool ChatFilter::ShouldIgnore(const wchar_t* message, uint32_t channel) {
    if (ShouldBlockByChannel(channel))
        return true;
    if (ShouldIgnore(message))
        return true;
    if (!ShouldFilterByChannel(channel))
        return false;
    return ShouldIgnoreByContent(message);
}
bool ChatFilter::ShouldIgnore(const wchar_t *message) {
    if (!message)
        return false;

    switch (message[0]) {
        // ==== Messages not ignored ====
    case 0x108: return false; // player message
    case 0x2AFC: return false; // <agent name> hands you <quantity> <item name>
    case 0x0314: return guild_announcement; // Guild Announcement by X: X
    case 0x4C32: return item_cannot_be_used; // Item can only be used in towns or outposts.
    case 0x76D: return false; // whisper received.
    case 0x76E: return false; // whisper sended.
    case 0x777: return false; // I'm level x and x% of the way earning my next skill point  (author is not part of the message)
    case 0x778: return false; // I'm following x            (author is not part of the message)
    case 0x77B: return false; // I'm talking to x           (author is not part of the message)
    case 0x77C: return false; // I'm wielding x         (author is not part of the message)
    case 0x77D: return false; // I'm wielding x and y       (author is not part of the message)
    case 0x781: return false; // I'm targeting x            (author is not part of the message)
    case 0x783: return false; // I'm targeting myself!  (author is not part of the message)
    case 0x791: return false; // emote agree
    case 0x792: return false; // emote attention
    case 0x793: return false; // emote beckon
    case 0x794: return false; // emote beg
    case 0x795: return false; // emote boo
        // all other emotes, in alphabetical order
    case 0x7BE: return false; // emote yawn
    case 0x7BF: return false; // emote yes
    case 0x7C8: return false; // Quest Reward Accepted: <quest name>
    case 0x7C9: return false; // Quest Updated: <quest name>
    case 0x7CB: return false; // You gain (message[5] - 100) experience
    case 0x7CC:
        if (FullMatch(&message[1], { 0x962D, 0xFEB5, 0x1D08, 0x10A, 0xAC2, 0x101, 0x164, 0x1 })) return lunars; // you receive 100 gold
        break;
    case 0x7CD: return false; // You receive <quantity> <item name>
    case 0x7E0: return ally_pickup_common; // party shares gold
    case 0x7ED: return false; // opening the chest reveals x, which your party reserves for y
    case 0x7DF: return ally_pickup_common; // party shares gold ?
    case 0x7F0: { // monster/player x drops item y (no assignment)
                  // first segment describes the agent who dropped, second segment describes the item dropped
        if (!ShouldIgnoreByAgentThatDropped(Get1stSegment(message))) return false;
        bool rare = IsRare(Get2ndSegment(message));
        if (rare) return self_drop_rare;
        else return self_drop_common;
    }
    case 0x7F1: { // monster x drops item y, your party assigns to player z
                  // 0x7F1 0x9A9D 0xE943 0xB33 0x10A <monster> 0x1 0x10B <rarity> 0x10A <item> 0x1 0x1 0x10F <assignee: playernumber + 0x100>
                  // <monster> is wchar_t id of several wchars
                  // <rarity> is 0x108 for common, 0xA40 gold, 0xA42 purple, 0xA43 green
        GW::AgentLiving* me = GW::Agents::GetCharacter();
        bool forplayer = (me && me->player_number == GetNumericSegment(message));
        bool rare = IsRare(Get2ndSegment(message));
        if (forplayer && rare) return self_drop_rare;
        if (forplayer && !rare) return self_drop_common;
        if (!forplayer && rare) return ally_drop_rare;
        if (!forplayer && !rare) return ally_drop_common;
        return false;
    }
    case 0x7F2: return false; // you drop item x
    case 0x7F6: // player x picks up item y (note: item can be unassigned gold)
        return IsRare(Get1stSegment(message)) ? ally_pickup_rare : ally_pickup_common;
    case 0x7FC: // you pick up item y (note: item can be unassigned gold)
        return IsRare(Get1stSegment(message)) ? player_pickup_rare : player_pickup_common;
    case 0x807: return false; // player joined the game
    case 0x816: return skill_points; // you gain a skill point
    case 0x817: return skill_points; // player x gained a skill point
    case 0x846: return false; // 'Screenshot saved as <path>'.
    case 0x87B: return noonehearsyou; // 'no one hears you.' (outpost)
    case 0x87C: return noonehearsyou; // 'no one hears you... ' (explorable)
    case 0x87D: return away; // 'Player <name> might not reply...' (Away)
    case 0x87F: return false; // 'Failed to send whisper to player <name>...' (Do not disturb)
    case 0x880: return false; // 'Player name <name> is invalid.'. (Anyone actually saw it ig ?)
    case 0x881: return false; // 'Player <name> is not online.' (Offline)
    case 0x88E: return invalid_target; // Invalid attack target.
    case 0x89B: return item_cannot_be_used; // Item cannot be used in towns or outposts.
    case 0x89C: return opening_chest_messages; // Chest is being used.
    case 0x89D: return opening_chest_messages; // The chest is empty.
    case 0x89E: return opening_chest_messages; // The chest is locked. You must have the correct key or a lockpick.
    case 0x8A0: return opening_chest_messages; // Already used that chest
    case 0x8A5: return invalid_target; // Target is immune to bleeding (no flesh.)
    case 0x8A6: return invalid_target; // Target is immune to disease (no flesh.)
    case 0x8A7: return invalid_target; // Target is immune to poison (no flesh.)
    case 0x8A8: return not_enough_energy; // Not enough adrenaline
    case 0x8A9: return not_enough_energy; // Not enough energy.
    case 0x8AA: return inventory_is_full; // Inventory is full.
    case 0x8AB: return invalid_target; // Your view of the target is obstructed.
    case 0x8C1: return invalid_target; // That skill requires a different weapon type.
    case 0x8C2: return invalid_target; // Invalid spell target.
    case 0x8C3: return invalid_target; // Target is out of range.
    case 0x52C3: // 0x52C3 0xDE9C 0xCD2F 0x78E4 0x101 0x100 - Hold-out bonus: +(message[5] - 0x100) points
        return FullMatch(&message[1], { 0xDE9C, 0xCD2F, 0x78E4, 0x101 }) && challenge_mission_messages;
    case 0x6C9C: // 0x6C9C 0x866F 0xB8D2 0x5A20 0x101 0x100 - You gain (message[5] - 0x100) Kurzick faction
        if (!FullMatch(&message[1], { 0x866F, 0xB8D2, 0x5A20, 0x101 })) break;
        return faction_gain || challenge_mission_messages && IsInChallengeMission();
    case 0x6D4D: // 0x6D4D 0xDD4E 0xB502 0x71CE 0x101 0x4E8 - You gain (message[5] - 0x100) Luxon faction
        if (!FullMatch(&message[1], { 0xDD4E, 0xB502, 0x71CE, 0x101 })) break;
        return faction_gain || challenge_mission_messages && IsInChallengeMission();
    case 0x7BF4: return you_have_been_playing_for; // You have been playing for x time.
    case 0x7BF5: return you_have_been_playing_for; // You have been playinf for x time. Please take a break.
    case 0x8101:
        switch (message[1]) {
            // nine rings
        case 0x1867: // stay where you are, nine rings is about to begin
        case 0x1868: // teilah takes 10 festival tickets
        case 0x1869: // big winner! 55 tickets
        case 0x186A: // you win 40 tickets
        case 0x186B: // you win 25 festival tickets
        case 0x186C: // you win 15 festival tickets
        case 0x186D: // did not win 9rings
            // rings of fortune
        case 0x1526: // The rings of fortune did not favor you this time. Stay in the area to try again. 
        case 0x1529: // Pan takes 2 festival tickets
        case 0x152A: // stay right were you are! rings of fortune is about to begin!
        case 0x152B: // you win 12 festival tickets
        case 0x152C: // You win 3 festival tickets
            return ninerings;
        case 0x39CD: // you have a special item available: <special item reward>
            return ninerings;
        case 0x3E3: // Spell failed. Spirits are not affected by this spell.
            return invalid_target;
        case 0x679C: return false;  // You cannot use a <profession> tome because you are not a <profession> (Elite == message[5] == 0x6725)
        case 0x72EB: return opening_chest_messages;   // The chest is locked. You must use a lockpick to open it.
        case 0x7B91:    // x minutes of favor of the gods remaining. Note: full message is 0x8101 0x7B91 0xC686 0xE490 0x6922 0x101 0x100+value
        case 0x7B92:    // x more achievements must be performed to earn the favor of the gods. // 0x8101 0x7B92 0x8B0A 0x8DB5 0x5135 0x101 0x100+value
            return favor;
        case 0x7C3E:    // This item cannot be used here.
            return item_cannot_be_used;
        }
        if (FullMatch(&message[1], { 0x6649, 0xA2F9, 0xBBFA, 0x3C27 })) return lunars; // you will celebrate a festive new year (rocket or popper)
        if (FullMatch(&message[1], { 0x664B, 0xDBAB, 0x9F4C, 0x6742 })) return lunars; // something special is in your future! (lucky aura)
        if (FullMatch(&message[1], { 0x6648, 0xB765, 0xBC0D, 0x1F73 })) return lunars; // you will have a prosperous new year! (gain 100 gold)
        if (FullMatch(&message[1], { 0x664C, 0xD634, 0x91F8, 0x76EF })) return lunars; // your new year will be a blessed one (lunar blessing)
        if (FullMatch(&message[1], { 0x664A, 0xEFB8, 0xDE25, 0x363 })) return lunars; // You will find bad luck in this new year... or bad luck will find you
        break;

    case 0x8102:
        switch (message[1]) {
        // 0xEFE is a player message
        case 0x1443: return player_has_achieved_title; // Player has achieved the title...
        case 0x4650: return pvp_messages; // skill has been updated for pvp
        case 0x4651: return pvp_messages; // a hero skill has been updated for pvp
        case 0x223F: return false; // "x minutes of favor of the gods remaining" as a result of /favor command
        case 0x223B: return hoh_messages; // a party won hall of heroes  
        case 0x23E2: return player_has_achieved_title; // Player has achieved... The gods have blessed the world with their favor.
        case 0x23E3: return favor; // The gods have blessed the world
        case 0x23E4: return favor; // 0xF8AA 0x95CD 0x2766 // the world no longer has the favor of the gods
        case 0x23E5: return player_has_achieved_title; // Player has achieved... The gods have extended their blessings
        case 0x23E6: return player_has_achieved_title; // Player has achieved... N more achievements will earn favor of the gods
        case 0x29F1: return item_cannot_be_used; // Cannot use this item when no party members are dead.
        case 0x3772: return false; // I'm under the effect of x
        case 0x3DCA: return item_cannot_be_used; // This item can only be used in a guild hall
        case 0x4684: return item_cannot_be_used; // There is already an ally from a summoning stone present in this instance.
        case 0x4685: return item_cannot_be_used; // You have already used a summoning stone within the last 10 minutes.
        }
        break;
    case 0x8103:
        switch (message[1]) {
        case 0x9CD: return item_cannot_be_used; // You must wait before using another tonic.
        }
    case 0xAD2: return item_already_identified; // That item is already identified
    case 0xAD7: return salvage_messages; // You salvaged <number> <item name(s)> from the <item name>
    case 0xADD: return item_cannot_be_used; // That item has no uses remaining
    //default:
    //  for (size_t i = 0; pak->message[i] != 0; ++i) printf(" 0x%X", pak->message[i]);
    //  printf("\n");
    //  return false;
    }

    return false;
}

bool ChatFilter::ShouldIgnoreByContent(const wchar_t *message, size_t size) {
    UNREFERENCED_PARAMETER(size);

    if (!messagebycontent) return false;
    if (!message) return false;
    if (!(message[0] == 0x108 && message[1] == 0x107)
        && !(message[0] == 0x8102 && message[1] == 0xEFE && message[2] == 0x107)) return false;
    const wchar_t* start = nullptr;
    const wchar_t* end = nullptr;
    size_t i = 0;
    while (start == nullptr && message[i]) {
        if (message[i] == 0x107)
            start = &message[i + 1];
        i++;
    }
    if (start == nullptr) return false; // no string segment in this packet
    while (end == nullptr && message[i]) {
        if (message[i] == 0x1)
            end = &message[i];
        i++;
    }
    if (end == nullptr) end = &message[i];

    // std::string temp(start, end);
    char buffer[1024];
    utf8::string temp = Unicode16ToUtf8(buffer, 1024, start, end);
    if (!temp.count)
        return false;

    utf8::string text = Utf8Normalize(temp.bytes);
    if (!text.count) {
        return false;
    }

    for (const auto& s : bycontent_words) {
        if (strstr(text.bytes, s.c_str())) {
            free(text);
            return true;
        }
    }
    for (const std::regex& r : bycontent_regex) {
        if (std::regex_match(text.bytes, r)) {
            return true;
        }
    }
    free(text);
    return false;
}
bool ChatFilter::ShouldFilterByChannel(uint32_t channel) {
    switch (channel) {
    case static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_ALL):        return filter_channel_local;
    case static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_GUILD):      return filter_channel_guild;
    case static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_GROUP):      return filter_channel_team;
    case static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_TRADE):      return filter_channel_trade;
    case static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_ALLIANCE):   return filter_channel_alliance;
    case static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_EMOTE):      return filter_channel_emotes;
    }
    return false;
}
bool ChatFilter::ShouldBlockByChannel(uint32_t channel) {
    if (Instance().block_messages_from_inactive_channels) {
        // Don't log chat messages if the channel is turned off - avoids hitting the chat log limit
        GW::UI::CheckboxPreference prefCheck = GW::UI::CheckboxPreference_Count;
        switch (static_cast<GW::Chat::Channel>(channel)) {
        case GW::Chat::Channel::CHANNEL_ALL:
            prefCheck = GW::UI::CheckboxPreference_ChannelLocal;
            break;
        case GW::Chat::Channel::CHANNEL_GROUP:
        case GW::Chat::Channel::CHANNEL_ALLIES:
            prefCheck = GW::UI::CheckboxPreference_ChannelGroup;
            break;
        case GW::Chat::Channel::CHANNEL_EMOTE:
            prefCheck = GW::UI::CheckboxPreference_ChannelEmotes;
            break;
        case GW::Chat::Channel::CHANNEL_GUILD:
            prefCheck = GW::UI::CheckboxPreference_ChannelGuild;
            break;
        case GW::Chat::Channel::CHANNEL_ALLIANCE:
            prefCheck = GW::UI::CheckboxPreference_ChannelAlliance;
            break;
        case GW::Chat::Channel::CHANNEL_TRADE:
            prefCheck = GW::UI::CheckboxPreference_ChannelTrade;
            break;
        }
        if(prefCheck != GW::UI::CheckboxPreference_Count
            && GW::UI::GetCheckboxPreference(prefCheck) == 1) {
            return true;
        }
    }
    return false;
}
bool ChatFilter::ShouldIgnoreBySender(const wchar_t *sender, size_t size) {
    UNREFERENCED_PARAMETER(sender);
    UNREFERENCED_PARAMETER(size);
#ifdef EXTENDED_IGNORE_LIST
    if (sender == nullptr) return false;
    char s[32];
    for (size_t i = 0; i < size; i++) {
        if (sender[i] & ~0xff) return false; // We currently don't support non-ascii names
        s[i] = tolower(sender[i]);
        if (sender[i] == 0)
            break;
    }
    if (byauthor_words.find(s) != byauthor_words.end())
        return true;
#endif
    return false;
}

void ChatFilter::DrawSettingInternal() {
    ImGui::Text("Block the following messages:");
    ImGui::Separator();
    ImGui::Text("Drops");
    ImGui::SameLine();
    ImGui::TextDisabled("('Rare' stands for Gold item, Ecto, Obby shard or Lockpick)");
    ImGui::StartSpacedElements(270.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("A rare item drops for you", &self_drop_rare);
    ImGui::NextSpacedElement(); ImGui::Checkbox("A common item drops for you", &self_drop_common);
    ImGui::NextSpacedElement(); ImGui::Checkbox("A rare item drops for an ally", &ally_drop_rare);
    ImGui::NextSpacedElement(); ImGui::Checkbox("A common item drops for an ally", &ally_drop_common);
    ImGui::NextSpacedElement(); ImGui::Checkbox("An ally picks up a rare item", &ally_pickup_rare);
    ImGui::NextSpacedElement(); ImGui::Checkbox("An ally picks up a common item", &ally_pickup_common);
    ImGui::NextSpacedElement(); ImGui::Checkbox("You pick up a rare item", &player_pickup_rare);
    ImGui::NextSpacedElement(); ImGui::Checkbox("You pick up a common item", &player_pickup_common);

    ImGui::Separator();
    ImGui::Text("Announcements");
    ImGui::StartSpacedElements(270.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Guild Announcement", &guild_announcement);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Hall of Heroes winners", &hoh_messages);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Favor of the Gods announcements", &favor);
    ImGui::NextSpacedElement(); ImGui::Checkbox("'You have been playing for...'", &you_have_been_playing_for);
    ImGui::NextSpacedElement(); ImGui::Checkbox("'Player x has achieved title...'", &player_has_achieved_title);
    ImGui::NextSpacedElement(); ImGui::Checkbox("'You gain x faction'", &faction_gain);
    ImGui::Separator();
    ImGui::Text("Warnings");
    ImGui::StartSpacedElements(270.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Unable to use item", &item_cannot_be_used);
    ImGui::ShowHelp("'Item can only/cannot be used in towns or outposts.'\n\
'This item cannot be used here.'\n\
'Cannot use this item when no party members are dead.'\n\
'There is already an ally from a summoning stone present in this instance.'\n\
'You have already used a summoning stone within the last 10 minutes.'\n\
'That item has no uses remaining.'\n\
'You must wait before using another tonic.'\n\
'This item can only be used in a guild hall'");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Invalid target", &invalid_target);
    ImGui::ShowHelp("'Invalid spell/attack target.'\n\
'Spell failed. Spirits are not affected by this spell.'\n\
'Your view of the target is obstructed.'\n\
'That skill requires a different weapon type.'\n\
'Target is out of range.'\n\
'Target is immune to bleeding/disease/poison (no flesh.)'");
    ImGui::NextSpacedElement(); ImGui::Checkbox("'Inventory is full'", &inventory_is_full);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Opening chests", &opening_chest_messages);
    ImGui::ShowHelp("'Chest is being used'\n\
'The chest is locked. You must use a lockpick to open it.'\n\
'The chest is locked. You must have the correct key or a lockpick.'\n\
'The chest is empty.'");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Item already identified", &item_already_identified);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Not enough Adrenaline/Energy", &not_enough_energy);

    ImGui::Separator();
    ImGui::Text("Others");
    ImGui::StartSpacedElements(270.f);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Earning skill points", &skill_points);
    ImGui::NextSpacedElement(); ImGui::Checkbox("PvP messages", &pvp_messages);
    ImGui::ShowHelp("Such as 'A skill was updated for pvp!'");
    ImGui::NextSpacedElement(); ImGui::Checkbox("9 Rings messages", &ninerings);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Lunar fortunes messages", &lunars);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Challenge mission messages", &challenge_mission_messages);
    ImGui::ShowHelp("Such as 'Hold-out bonus: +2 points'");
    ImGui::NextSpacedElement(); ImGui::Checkbox("'No one hears you...'", &noonehearsyou);
    ImGui::NextSpacedElement(); ImGui::Checkbox("'Player x might not reply...", &away);
    ImGui::ShowHelp("...because his / her status is set to away'");
    ImGui::NextSpacedElement(); ImGui::Checkbox("Salvaging messages", &salvage_messages);

    ImGui::Separator();
    ImGui::Checkbox("Block messages from inactive chat channels", &block_messages_from_inactive_channels);
    ImGui::ShowHelp("Chat history in Guild Wars isn't unlimited.\n\nEnable this to prevent the game from logging messages in channels that you have turned off.");
    if (block_messages_from_inactive_channels) {
        ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), "Messages from channels you have turned off in chat are not being logged in-game");
    }
    ImGui::Checkbox("Hide any messages containing:", &messagebycontent);
    ImGui::Indent();
    ImGui::TextDisabled("(Each in a separate line. Not case sensitive)");
    ImGui::Checkbox("Local", &filter_channel_local);
    ImGui::SameLine(0.0f, -1.0f);
    ImGui::Checkbox("Guild", &filter_channel_guild);
    ImGui::SameLine(0.0f, -1.0f);
    ImGui::Checkbox("Team", &filter_channel_team);
    ImGui::SameLine(0.0f, -1.0f);
    ImGui::Checkbox("Trade", &filter_channel_trade);
    ImGui::SameLine(0.0f, -1.0f);
    ImGui::Checkbox("Alliance", &filter_channel_alliance);
    ImGui::SameLine(0.0f, -1.0f);
    ImGui::Checkbox("Emotes", &filter_channel_emotes);
    
    if (ImGui::InputTextMultiline("##bycontentfilter", bycontent_word_buf, 
        FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0f))) {
        timer_parse_filters = GetTickCount() + NOISE_REDUCTION_DELAY_MS;
    }
    ImGui::Text("And messages matching regular expressions:");
    ImGui::ShowHelp("Regular expressions allow you to specify wildcards and express more.\nThe syntax is described at www.cplusplus.com/reference/regex/ECMAScript\nNote that the whole message needs to be matched, so for example you might want .* at the end.");
    if (ImGui::InputTextMultiline("##bycontentfilter_regex", bycontent_regex_buf,
        FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0))) {
        timer_parse_regexes = GetTickCount() + NOISE_REDUCTION_DELAY_MS;
    }
    ImGui::Unindent();

#ifdef EXTENDED_IGNORE_LIST
    ImGui::Separator();
    ImGui::Checkbox("Hide any messages from: ", &messagebyauthor);
    ImGui::Indent();
    ImGui::TextDisabled("(Each in a separate line)");
    ImGui::TextDisabled("(Not implemented)");
    if (ImGui::InputTextMultiline("##byauthorfilter", byauthor_buf, FILTER_BUF_SIZE, ImVec2(-1.0f, 0.0f))) {
        ByAuthor_ParseBuf();
        byauthor_filedirty = true;
    }
    ImGui::Unindent();
#endif // EXTENDED_IGNORE_LIST
}

void ChatFilter::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);
    uint32_t timestamp = GetTickCount();
    if (timer_parse_filters && timer_parse_filters < timestamp) {
        timer_parse_filters = 0;
        ParseBuffer(bycontent_word_buf, bycontent_words);
        bycontent_filedirty = true;
    }

    if (timer_parse_regexes && timer_parse_regexes < timestamp) {
        timer_parse_regexes = 0;
        ParseBuffer(bycontent_regex_buf, bycontent_regex);
        bycontent_filedirty = true;
    }
}

void ChatFilter::ParseBuffer(const char *text, std::vector<std::string> &words) const {
    words.clear();
    utf8::string normalized_text = Utf8Normalize(text);
    std::istringstream stream(normalized_text.bytes);
    std::string word;
    while (std::getline(stream, word)) {
        if (!word.empty()) {
            words.push_back(word);
        }
    }
    free(normalized_text);
}

void ChatFilter::ParseBuffer(const char *text, std::vector<std::wstring> &words) const {
    words.clear();
    wchar_t buffer[1024];
    Utf8ToUnicode(text, buffer, 1024);
    std::wstringstream stream(buffer);
    std::wstring word;
    while (std::getline(stream, word)) {
        if (!word.empty()) {
            words.push_back(word);
        }
    }
}

void ChatFilter::ParseBuffer(const char *text, std::vector<std::regex> &regex) const {
    regex.clear();
    std::istringstream stream(text);
    std::string word;
    while (std::getline(stream, word)) {
        if (!word.empty()) {
            // std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            try {
                regex.push_back(std::regex(word));
            } catch (const std::regex_error&) {
                Log::Warning("Cannot parse regular expression '%s'", word.c_str());
            }
        }
    }
}
