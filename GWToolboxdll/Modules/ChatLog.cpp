#include "stdafx.h"

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/CharContext.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/UIMgr.h>

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Utilities/Scanner.h>

#include <GuiUtils.h>

#include <Modules/Resources.h>
#include <Modules/ChatLog.h>
#include <Defines.h>

namespace {
    typedef void(__cdecl* AddToSentLog_pt)(wchar_t* message);
    AddToSentLog_pt AddToSentLog_Func = 0;
    AddToSentLog_pt RetAddToSentLog = 0;

    typedef void(__cdecl* ClearChatLog_pt)();
    ClearChatLog_pt ClearChatLog_Func = 0;
    typedef void(__cdecl* InitChatLog_pt)();
    InitChatLog_pt InitChatLog_Func = 0;


    
}
namespace GW {
    namespace Chat {
        const size_t SENT_LOG_LENGTH = 0x32;
    }
}
//#define PRINT_CHAT_PACKETS
void ChatLog::Reset() {
    while (recv_first) {
        Remove(recv_first);
    }
    while (sent_first) {
        RemoveSent(sent_first);
    }
}
void ChatLog::Add(GW::Chat::ChatMessage* in) {
    Add(in->message, in->channel, in->timestamp);
}
void ChatLog::Add(wchar_t* _message, uint32_t _channel, FILETIME _timestamp) {
    if (injecting || !enabled)
        return;
    if (!(_message && _message[0]))
        return; // Empty message
    TBChatMessage* new_message = new TBChatMessage(_message, _channel, _timestamp);
    TBChatMessage* inject = recv_last;
    if (!recv_first) {
        // No first element; log is empty.
        new_message->prev = new_message->next = recv_first = recv_last = new_message;
        goto trim_log;
    }

    while (recv_first) {
        switch (CompareFileTime(&inject->timestamp, &_timestamp)) {
        case 0: // Equal; check message cmp
            if (wcscmp(_message, inject->msg.c_str()) == 0) {
                // Duplicate message?
                return;
            }
        case 1: // new_message is earlier; keep going back
            if (inject == recv_first) {
                // Earliest message, insert before
                new_message->prev = inject->prev;
                new_message->next = inject;
                if (inject->prev != inject) {
                    inject->prev->next = new_message;
                }
                if (inject->next == inject)
                    inject->next = new_message;
                inject->prev = new_message;
                recv_first = new_message;
                goto trim_log;
            }
            inject = inject->prev;
            break;
        case -1: // new_message is later; add here.
            new_message->next = inject->next;
            new_message->prev = inject;
            if (inject->next != inject) {
                inject->next->prev = new_message;
            }
            inject->next = new_message;
            if (inject->prev == inject)
                inject->prev = new_message;
            if (inject == recv_last)
                recv_last = new_message;
            goto trim_log;
        }
    }
trim_log:
    recv_count++;
    while (recv_count > GW::Chat::CHAT_LOG_LENGTH) {
        Remove(recv_first);
    }
}
void ChatLog::AddSent(wchar_t* _message, uint32_t addr) {
    if(injecting || IsAdded(_message, addr))
        return;
    if (!(_message && _message[0]))
        return; // Empty message
    TBSentMessage* new_message = new TBSentMessage(_message, addr);
    TBSentMessage* inject = sent_last;
    if (!sent_first) {
        // No first element; log is empty.
        new_message->prev = new_message->next = sent_first = sent_last = new_message;
        goto trim_log;
    }
    // Add after the last element
    new_message->next = inject->next;
    new_message->prev = inject;
    if (inject->next != inject) {
        inject->next->prev = new_message;
    }
    inject->next = new_message;
    if (inject == sent_last)
        sent_last = new_message;
    goto trim_log;
trim_log:
    sent_count++;
    while (sent_count > GW::Chat::SENT_LOG_LENGTH) {
        RemoveSent(sent_first);
    }
}
void ChatLog::Remove(TBChatMessage* message) {
    if (message == recv_first)
        recv_first = message->next;
    if (message == recv_last)
        recv_last = message->prev;
    message->next->prev = message->prev;
    message->prev->next = message->next;
    if (recv_first == message)
        recv_first = 0;
    if (recv_last == message)
        recv_last = 0;
    recv_count--;
    delete message;
}
void ChatLog::RemoveSent(TBSentMessage* message) {
    if (message == sent_first)
        sent_first = message->next;
    if (message == sent_last)
        sent_last = message->prev;
    message->next->prev = message->prev;
    message->prev->next = message->next;
    if (sent_first == message)
        sent_first = 0;
    if (sent_last == message)
        sent_last = 0;
    sent_count--;
    delete message;
}
void ChatLog::Fetch() {
    if (!enabled)
        return;
    GW::Chat::ChatBuffer* log = GW::Chat::GetChatLog();
    for (size_t i = 0;log &&  i < GW::Chat::CHAT_LOG_LENGTH; i++) {
        if (log->messages[i])
            Add(log->messages[i]);
    }

    // Sent
    auto out_log = GetSentLog();
    if (out_log && out_log->count && out_log->prev) {
        GWSentMessage* oldest = out_log->prev;
        for (size_t i = out_log->count - 1; i < out_log->count; i--) {
            if (oldest->prev)
                oldest = oldest->prev;
        }
        for (size_t i = 0; i < out_log->count; i++) {
            AddSent(oldest->message);
            oldest = oldest->next;
        }
    }
}
void ChatLog::Save() {
    if (!enabled || account.empty())
        return;
    // Received log FIFO
    CSimpleIni* inifile = new CSimpleIni(false, false, false);
    std::string msg_buf;
    char addr_buf[8];
    TBChatMessage* recv = recv_first;
    size_t i = 0;
    std::string datetime_str;
    while (recv) {
        if (GuiUtils::TimeToString(recv->timestamp, datetime_str)) {
            snprintf(addr_buf, 8, "%03x", i++);
            ASSERT(GuiUtils::ArrayToIni(recv->msg.data(), &msg_buf));
            inifile->SetValue(addr_buf, "message", msg_buf.c_str());
            inifile->SetLongValue(addr_buf, "dwLowDateTime", recv->timestamp.dwLowDateTime);
            inifile->SetLongValue(addr_buf, "dwHighDateTime", recv->timestamp.dwHighDateTime);
            inifile->SetLongValue(addr_buf, "channel", recv->channel);
            inifile->SetValue(addr_buf, "datetime", datetime_str.c_str());
        }
        else {
            Log::Log("Failed to turn timestamp for message %d into string",i);
        }

        if (recv == recv_last)
            break;
        recv = recv->next;
    }
    inifile->SaveFile(LogPath(L"recv").c_str());
    delete inifile;

    // Sent log FIFO
    inifile = new CSimpleIni(false, false, false);
    TBSentMessage* sent = sent_first;
    i = 0;
    while (sent) {
        snprintf(addr_buf, 8, "%03x", i++);
        ASSERT(GuiUtils::ArrayToIni(sent->msg.c_str(), &msg_buf));
        inifile->SetValue(addr_buf, "message", msg_buf.c_str());
        inifile->SetLongValue(addr_buf, "addr", sent->gw_message_address);
        if (sent == sent_last)
            break;
        sent = sent->next;
    }
    inifile->SaveFile(LogPath(L"sent").c_str());
    delete inifile;
}
void ChatLog::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);
    Save();
    ini->SetBoolValue(Name(), VAR_NAME(enabled), enabled);
    
}
void ChatLog::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);
    Save();
    enabled = ini->GetBoolValue(Name(), VAR_NAME(enabled), enabled);
    Init();
}
std::filesystem::path ChatLog::LogPath(const wchar_t* prefix) {
    wchar_t fn[128];
    swprintf(fn, 128, L"%s_%s.ini", prefix, account.c_str());
    Resources::EnsureFolderExists(Resources::GetPath(L"chat logs"));
    return Resources::GetPath(L"chat logs", fn);
}
void ChatLog::Load(const std::wstring& _account) {
    Reset();
    // Recv log FIFO
    account = _account;
    CSimpleIni* inifile = new CSimpleIni(false, false, false);
    inifile->LoadFile(LogPath(L"recv").c_str());
    CSimpleIni::TNamesDepend entries;
    inifile->GetAllSections(entries);
    std::wstring buf;
    FILETIME t;
    uint32_t channel = 0;
    uint32_t addr = 0;
    for (CSimpleIni::Entry& entry : entries) {
        std::string message = inifile->GetValue(entry.pItem, "message", "");
        if (message.empty())
            continue;
        size_t written = GuiUtils::IniToArray(message,buf);
        if (!written)
            continue;
        t.dwLowDateTime = inifile->GetLongValue(entry.pItem, "dwLowDateTime", 0);
        t.dwHighDateTime = inifile->GetLongValue(entry.pItem, "dwHighDateTime", 0);
        channel = inifile->GetLongValue(entry.pItem, "channel", 0);
        Add(buf.data(), channel, t);
    }
    delete inifile;

    // sent log FIFO
    inifile = new CSimpleIni(false, false, false);
    inifile->LoadFile(LogPath(L"sent").c_str());
    entries.clear();
    inifile->GetAllSections(entries);
    for (CSimpleIni::Entry& entry : entries) {
        std::string message = inifile->GetValue(entry.pItem, "message", "");
        if (message.empty())
            continue;
        size_t written = GuiUtils::IniToArray(message, buf);
        if (!written)
            continue;
        addr = inifile->GetLongValue(entry.pItem, "addr", 0);
        AddSent(buf.data(), addr);
    }
    delete inifile;
}
void ChatLog::Inject() {
    if (injecting || !enabled || !ClearChatLog_Func || !InitChatLog_Func)
        goto done_injecting;
    if(!pending_inject)
        goto done_injecting;
    TBChatMessage* recv = recv_first;
    if (recv) {
        ClearChatLog_Func();
        GW::Chat::ChatBuffer* log = GW::Chat::GetChatLog();
        ASSERT(!log);
        InitChatLog_Func();
        log = GW::Chat::GetChatLog();
        ASSERT(log);
        // Fill chat log
        size_t log_pos = log ? log->next : 0;
        injecting = true;

        while (recv) {
            timestamp_override_message = recv;
            GW::Chat::AddToChatLog(timestamp_override_message->msg.data(), timestamp_override_message->channel);
            if (!log) log = GW::Chat::GetChatLog();
            ASSERT(log && !timestamp_override_message && log_pos != log->next);
            log_pos = log->next;
            if (recv == recv_last)
                break;
            recv = recv->next;
        }
    }
    InjectSent();

done_injecting:
    injecting = false;
}
void ChatLog::InjectSent() {
    injecting = true;
    // Sent
    auto out_log = GetSentLog();
    if (!AddToSentLog_Func)
        goto done_injecting;
    if (out_log && out_log->count)
        goto done_injecting; // Has log already; becuase we have no way of knowing the order, we cant inject
    // Fill chat log
    TBSentMessage* sent = sent_first;
    while (sent) {
        AddToSentLog_Func(sent->msg.data());
        if (!out_log)
            out_log = GetSentLog();
        sent->gw_message_address = (uint32_t)out_log->prev->message;
        if (sent == sent_last)
            break;
        sent = sent->next;
    }
done_injecting:
    injecting = false;
}
void ChatLog::SetEnabled(bool _enabled) {
    if (enabled == _enabled)
        return;
    enabled = _enabled;
    if (enabled) {
        Init();
    }
}
bool ChatLog::Init() {
    if (!enabled)
        return false;
    std::wstring this_account = GW::GameContext::instance()->character->player_email;
    if (this_account == account)
        return false;
    // GW Account changed, save this log and start fresh.
    Save();
    Load(this_account);
    Fetch();
    pending_inject = true;
    return true;
}

void ChatLog::Initialize() {
    ToolboxModule::Initialize();
    {
        // Hook for logging outgoing (sent) messages
        uintptr_t address = GW::Scanner::FindAssertion("p:\\code\\gw\\chat\\ctchatedit.cpp", "length", -0x5A);
        if (address) {
            AddToSentLog_Func = (AddToSentLog_pt)address;
            GW::Hook::CreateHook(AddToSentLog_Func, OnAddToSentLog, (void**)&RetAddToSentLog);
            GW::Hook::EnableHooks(AddToSentLog_Func);
            address += 0x17;
            gw_sent_log_ptr = *(uintptr_t*)address;
        }
        printf("[SCAN] AddToSentLog_Func = %p\n", (void*)AddToSentLog_Func);
        printf("[SCAN] gw_sent_log_ptr = %p\n", (void*)gw_sent_log_ptr);
    }
    {
        // For properly clearing out the existing chat log (only need to call, not hook)
        ClearChatLog_Func = (ClearChatLog_pt)GW::Scanner::Find("\x83\xc4\x08\x8d\x77\x0c\xbb\x00\x02\x00\x00", "xxxxxxxxxxx", -0x70);
        printf("[SCAN] ClearChatLog_Func = %p\n", (void*)ClearChatLog_Func);
    }
    {
        // For properly clearing out the existing chat log (only need to call, not hook)
        InitChatLog_Func = (InitChatLog_pt)GW::Scanner::FunctionFromNearCall(GW::Scanner::Find("\x68\x7e\x00\x00\x10\x53", "xxxxxx", -0x5));
        printf("[SCAN] InitChatLog_Func = %p\n", (void*)InitChatLog_Func);
    }

    

    GW::Chat::RegisterChatLogCallback(&PreAddToChatLog_entry, OnPreAddToChatLog, -0x4000);
    GW::Chat::RegisterChatLogCallback(&PostAddToChatLog_entry, OnPostAddToChatLog, 0x4000);
    GW::UI::RegisterUIMessageCallback(&UIMessage_Entry, [&](GW::HookStatus*, uint32_t message_id, void*, void*) {
        switch (message_id) {
        case GW::UI::kMapChange:
            // NB: Friends list messages don't play well when clearing the chat log after the map has loaded.
            // Instead, we trigger this immediately before map load.
            // When the game world is rebuilt during map load, the log works properly again.
            Init();
            Inject();
            Save(); // Save the chat log on every map transition
            break;
        }
        },-0x8000);
    Init();
}
void ChatLog::RegisterSettingsContent() {
    ToolboxModule::RegisterSettingsContent("Chat Settings", ICON_FA_COMMENTS,
        [this](const std::string* section, bool is_showing) {
            UNREFERENCED_PARAMETER(section);
            if (!is_showing) return;
            ImGui::Checkbox("Enable GWToolbox chat log", &Instance().enabled);
            ImGui::ShowHelp("Guild Wars doesn't save your chat history or sent messages if you log out of the game.\nTurn this feature on to let GWToolbox keep better track of your chat history between logins");
        }, 0.8f);
}
void ChatLog::OnAddToSentLog(wchar_t* message) {
    GW::HookBase::EnterHook();
    if (!Instance().enabled || Instance().injecting) {
        RetAddToSentLog(message);
        GW::HookBase::LeaveHook();
        return;
    }
    GWSentLog* log = Instance().GetSentLog();
    if (!log || !log->count || Instance().Init()) {
        Instance().InjectSent();
        log = Instance().GetSentLog();
    }
    GWSentMessage* lastest_message = log ? log->prev : 0;
    RetAddToSentLog(message);
    log = Instance().GetSentLog();
    if (log && log->prev != lastest_message)
        Instance().AddSent(log->prev->message);
    GW::HookBase::LeaveHook();
}
void ChatLog::OnPreAddToChatLog(GW::HookStatus*, wchar_t*, uint32_t, GW::Chat::ChatMessage*) {
    if (!Instance().enabled || Instance().injecting)
        return;
    //GW::Chat::ChatBuffer* log = GW::Chat::GetChatLog();
    //bool inject = ((log && log->next == 0 && !log->messages[log->next]) || !log || Instance().Init());
}
void ChatLog::OnPostAddToChatLog(GW::HookStatus*, wchar_t*, uint32_t, GW::Chat::ChatMessage* logged_message) {
    if (!Instance().enabled)
        return;
    if (logged_message) { // NB: Can be false if between maps
        if (Instance().timestamp_override_message) {
            logged_message->timestamp = Instance().timestamp_override_message->timestamp;
            Instance().timestamp_override_message = 0;
        }
        else {
            Instance().Add(logged_message);
        }
    }
}