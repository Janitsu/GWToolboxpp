#pragma once

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameContainers/Array.h>

#include <GWCA/Constants/Constants.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/ItemMgr.h>

#include <ToolboxWidget.h>
namespace GW {
    namespace Constants {
        enum class Rarity : uint8_t {
            White, Blue, Purple, Gold, Green
        };
    }
}

class InventoryManager : public ToolboxUIElement {
public:
    InventoryManager();
    ~InventoryManager();
    enum class SalvageAllType : uint8_t {
        None,
        White,
        BlueAndLower,
        PurpleAndLower,
        GoldAndLower
    };
    enum class IdentifyAllType : uint8_t {
        None,
        All,
        Blue,
        Purple,
        Gold
    };

    static InventoryManager& Instance() {
        static InventoryManager instance;
        return instance;
    }
    const char* Name() const override { return "Inventory Management"; }
    const char* SettingsName() const override { return "Inventory Settings"; }

    void Draw(IDirect3DDevice9* device) override;
    bool DrawItemContextMenu(bool open = false);

    void IdentifyAll(IdentifyAllType type);
    void SalvageAll(SalvageAllType type);
    bool IsPendingIdentify();
    bool IsPendingSalvage();
    bool HasSettings() { return true; };
    void Initialize() override;
    void Update(float delta) override;
    void DrawSettingInternal() override;
    void LoadSettings(CSimpleIni* ini) override;
    void SaveSettings(CSimpleIni* ini) override;

    bool WndProc(UINT, WPARAM, LPARAM);

    // Find an empty (or partially empty) inventory slot that this item can go into
    std::pair<GW::Bag*, uint32_t> GetAvailableInventorySlot(GW::Item* like_item = nullptr);
    // Find an empty (or partially empty) inventory slot that this item can go into. !entire_stack = Returns slots that are the same item, but won't hold all of them.
    GW::Item* GetAvailableInventoryStack(GW::Item* like_item, bool entire_stack = false);
    // Checks model info and struct info to make sure item is the same.
    static bool IsSameItem(GW::Item* item1, GW::Item* item2);

    static void ItemClickCallback(GW::HookStatus*, uint32_t type, uint32_t slot, GW::Bag* bag);
    static void OnOfferTradeItem(GW::HookStatus* status, uint32_t item_id, uint32_t quantity);
    static void OnMoveItemPacket(GW::HookStatus* status, void* pak);
    static wchar_t* OnAsyncDecodeStr(GW::HookStatus*, GW::UI::DecodingString* to_decode);
    static void __fastcall OnAddItemToWindow(void* ecx, void* edx, uint32_t frame, uint32_t item_id);


    IdentifyAllType identify_all_type = IdentifyAllType::None;
    SalvageAllType salvage_all_type = SalvageAllType::None;

protected:
    void ShowVisibleRadio() override {};

private:
    bool trade_whole_stacks = false;
    bool show_item_context_menu = false;
    bool is_identifying = false;
    bool is_identifying_all = false;
    bool is_salvaging = false;
    bool is_salvaging_all = false;
    bool has_prompted_salvage = false;
    bool show_salvage_all_popup = true;
    bool salvage_listeners_attached = false;
    bool only_use_superior_salvage_kits = false;
    bool salvage_rare_mats = false;
    bool show_transact_quantity_popup = false;
    bool transaction_listeners_attached = false;
    bool hide_unsellable_items = true;

    bool wiki_link_on_context_menu = false;
    bool right_click_context_menu_in_explorable = true;
    bool right_click_context_menu_in_outpost = true;

    std::map<GW::Constants::Bag, bool> bags_to_salvage_from = {
        { GW::Constants::Bag::Backpack,true },
        { GW::Constants::Bag::Belt_Pouch,true },
        { GW::Constants::Bag::Bag_1,true },
        { GW::Constants::Bag::Bag_2,true }
    };

    size_t identified_count = 0;
    size_t salvaged_count = 0;

    GW::Packet::StoC::SalvageSession current_salvage_session;

    void ContinueIdentify();
    void ContinueSalvage();

    GW::HookEntry on_map_change_entry;
    GW::HookEntry salvage_hook_entry;
    GW::HookEntry transaction_hook_entry;
    GW::HookEntry ItemClick_Entry;

    void FetchPotentialItems();
    void AttachSalvageListeners();
    void DetachSalvageListeners();
    static void ClearSalvageSession(GW::HookStatus *status = nullptr, void * packet = nullptr);
    void CancelSalvage();
    void CancelIdentify();
    void CancelAll();
    void ContinueTransaction();
    void CancelTransaction();
    static void ClearTransactionSession(GW::HookStatus* status = nullptr, void* packet = nullptr);
    void AttachTransactionListeners();
    void DetachTransactionListeners();
public:
    struct Item : GW::Item {
        GW::ItemModifier *GetModifier(uint32_t identifier);
        GW::Constants::Rarity GetRarity();
        uint32_t GetUses();
        bool IsIdentificationKit();
        bool IsSalvageKit();
        bool IsTome();
        bool IsLesserKit();
        bool IsExpertSalvageKit();
        bool IsPerfectSalvageKit();
        bool IsWeapon();
        bool IsArmor();
        bool IsSalvagable();
        
        bool IsRareMaterial();
        bool IsWeaponSetItem();
        bool IsOfferedInTrade();
        bool CanOfferToTrade();
        inline bool IsSparkly() {
            return (interaction & 0x2000) == 0;
        }
        inline bool GetIsIdentified() {
            return (interaction & 1) != 0;
        }
        inline bool IsStackable() {
            return (interaction & 0x80000) != 0;
        }
        inline bool IsUsable() {
            return (interaction & 0x1000000) != 0;
        }
        inline bool IsTradable() {
            return (interaction & 0x100) == 0;
        }
        inline bool IsBlue() {
            return single_item_name && single_item_name[0] == 0xA3F;
        }
        inline bool IsPurple() {
            return (interaction & 0x400000) != 0;
        }
        inline bool IsGreen() {
            return (interaction & 0x10) != 0;
        }
        inline bool IsGold() {
            return (interaction & 0x20000) != 0;
        }
    };
public:

    Item* GetNextUnsalvagedItem(Item* salvage_kit = nullptr, Item* start_after_item = nullptr);
    Item* GetNextUnidentifiedItem(Item* start_after_item = nullptr);
    void Identify(Item* item, Item* kit);
    void Salvage(Item* item, Item* kit);

    uint32_t stack_prompt_item_id = 0;
private:
    struct CtoS_TransactItems {
        uint32_t header = GAME_CMSG_BUY_MATERIALS;
        uint32_t type;
        uint32_t gold_give = 0;
        uint32_t item_give_count = 0;
        uint32_t item_give_ids[16];
        uint32_t gold_recv = 0;
        uint32_t item_recv_count = 0;
        uint32_t item_recv_ids[16];
    };
    static_assert(sizeof(CtoS_TransactItems) == 0x98);
    struct CtoS_QuoteItem {
        uint32_t header = GAME_CMSG_REQUEST_QUOTE;
        uint32_t type;
        uint32_t unk1 = 0;
        uint32_t gold_give = 0;
        uint32_t item_give_count = 0;
        uint32_t item_give_ids[16];
        uint32_t gold_recv = 0;
        uint32_t item_recv_count = 0;
        uint32_t item_recv_ids[16];
    };
    static_assert(sizeof(CtoS_QuoteItem) == 0x9C);
    struct PendingTransaction {
        enum State : uint8_t {
            None,
            Prompt,
            Pending,
            Quoting,
            Quoted,
            Transacting
        } state = None;
        uint32_t type;
        uint32_t price;
        uint32_t item_id;
        clock_t state_timestamp = 0;
        uint8_t retries = 0;
        void setState(State _state) {
            state = _state;
            state_timestamp = clock();
        }
        CtoS_QuoteItem quote();
        CtoS_TransactItems transact();
        Item* item();
        bool in_progress() { return state > State::Prompt; }
        bool selling();
    };
    

    struct PendingItem {

        uint32_t item_id = 0;
        uint32_t slot = 0;
        GW::Constants::Bag bag = GW::Constants::Bag::None;
        uint32_t uses = 0;
        uint32_t quantity = 0;
        bool set(Item *item = nullptr);
        GuiUtils::EncString name;
        GuiUtils::EncString single_item_name;
        GuiUtils::EncString desc;

        class PluralEncString : public GuiUtils::EncString {
        protected:
            void sanitise() override {
                if (sanitised)
                    return;
                GuiUtils::EncString::sanitise();
                if (sanitised) {
                    static const std::wregex plural(L"256 ");
                    decoded_ws = std::regex_replace(decoded_ws, plural, L"");
                }
            };
        };
        PluralEncString plural_item_name;

        Item *item();
    };
    struct PotentialItem : PendingItem {
        bool proceed = true;
    };
    std::vector<PotentialItem*> potential_salvage_all_items; // List of items that would be processed if user confirms Salvage All
    void ClearPotentialItems();
    PendingItem pending_identify_item;
    PendingItem pending_identify_kit;
    PendingItem pending_salvage_item;
    PendingItem pending_salvage_kit;
    PendingTransaction pending_transaction;

    int pending_transaction_amount = 0;
    bool pending_cancel_transaction = false;
    bool is_transacting = false;
    bool has_prompted_transaction = false;

    clock_t pending_salvage_at = 0;
    clock_t pending_identify_at = 0;
    PendingItem context_item;
    bool pending_cancel_salvage = false;
};
