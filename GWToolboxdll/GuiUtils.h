#pragma once

#include <ImGuiAddons.h>

namespace GuiUtils {
    enum class FontSize {
        text,
        header2,
        header1,
        widget_label,
        widget_small,
        widget_large
    };
    void LoadFonts();
    std::string WikiUrl(std::wstring term);
    void OpenWiki(std::wstring term);
    void SearchWiki(std::wstring term);
    bool FontsLoaded();
    ImFont* GetFont(FontSize size);

    float GetPartyHealthbarHeight();
    float GetGWScaleMultiplier();

    // Reposition a rect within its container to make sure it isn't overflowing it.
    ImVec4& ClampRect(ImVec4& rect, ImVec4& viewport);

    std::string ToLower(std::string s);
    std::wstring ToLower(std::wstring s);
    std::string UrlEncode(std::string str);
    std::wstring RemovePunctuation(std::wstring s);
    std::string RemovePunctuation(std::string s);

    std::string WStringToString(const std::wstring& s);
    std::wstring StringToWString(const std::string& s);

    std::wstring SanitizePlayerName(std::wstring in);

    bool ParseInt(const char *str, int *val, int base = 0);
    bool ParseInt(const wchar_t *str, int *val, int base = 0);

    bool ParseUInt(const char *str, unsigned int *val, int base = 0);
    bool ParseUInt(const wchar_t *str, unsigned int *val, int base = 0);

    bool ParseFloat(const char *str, float *val);
    bool ParseFloat(const wchar_t *str, float *val);

    time_t filetime_to_timet(const FILETIME& ft);
    size_t TimeToString(uint32_t utc_timestamp, std::string& out);
    size_t TimeToString(time_t utc_timestamp, std::string& out);
    size_t TimeToString(FILETIME utc_timestamp, std::string& out);

    // Takes a wstring and translates into a string of hex values, separated by spaces
    bool ArrayToIni(const std::wstring& in, std::string* out);
    bool ArrayToIni(const uint32_t* in, size_t len, std::string* out);
    size_t IniToArray(const std::string& in, std::wstring& out);
    size_t IniToArray(const std::string& in, uint32_t* out, size_t out_len);
    size_t IniToArray(const std::string& in, std::vector<uint32_t>& out);
    // Takes a string of hex values separated by spaces, and returns a wstring respresentation
    std::wstring IniToWString(std::string in);

    char *StrCopy(char *dest, const char *src, size_t dest_size);

    size_t wcstostr(char* dest, const wchar_t* src, size_t n);
    std::wstring ToWstr(std::string& str);

    class EncString {
    protected:
        std::wstring encoded_ws;
        std::wstring decoded_ws;
        std::string decoded_s;
        bool decoding = false;
        bool sanitised = false;
        virtual void sanitise();
    public:
        inline bool IsDecoding() { return decoding && decoded_ws.empty(); };
        // Recycle this EncString by passing a new encoded string id to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        void reset(const uint32_t _enc_string_id = 0, bool sanitise = true);
        // Recycle this EncString by passing a new string to decode.
        // Set sanitise to true to automatically remove guild tags etc from the string
        void reset(const wchar_t* _enc_string = nullptr, bool sanitise = true);
        std::wstring& wstring();
        std::string& string();
        const std::wstring& encoded() const {
            return encoded_ws;
        };
        EncString(const wchar_t* _enc_string = nullptr, bool sanitise = true) {
            reset(_enc_string, sanitise);
        }
        EncString(const uint32_t _enc_string, bool sanitise = true) {
            reset(_enc_string, sanitise);
        }
        // Disable object copying; decoded_ws is passed to GW by reference and would be bad to do this. Pass by pointer instead.
        EncString(const EncString& temp_obj) = delete;
        EncString& operator=(const EncString& temp_obj) = delete;
        ~EncString() {
            ASSERT(!IsDecoding());
        }
    };
};
