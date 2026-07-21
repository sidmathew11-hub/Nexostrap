#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <nlohmann/json.hpp>
#include "memory.hpp"

namespace nexo {

class Offsets {
public:
    Offsets() {
        cacheDir_ = GetCacheDir();
        cacheFile_ = cacheDir_ + "\\offsets.json";
        configFile_ = cacheDir_ + "\\config.json";
        EnsureCacheDir();
    }

    // Fetch latest offsets from Theo's API
    bool FetchLatest() {
        HINTERNET hSession = WinHttpOpen(L"NexoExecutor/1.1", 
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        HINTERNET hConnect = WinHttpConnect(hSession, 
            L"offsets.imtheo.lol", INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return false;
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/offsets.json",
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Set timeout
        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, 
            (LPVOID*)&connectTimeout_, sizeof(DWORD));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT,
            (LPVOID*)&sendTimeout_, sizeof(DWORD));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT,
            (LPVOID*)&receiveTimeout_, sizeof(DWORD));

        BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::string response;
        DWORD downloaded = 0;
        do {
            DWORD available = 0;
            WinHttpQueryDataAvailable(hRequest, &available);
            if (!available) break;

            std::vector<char> buf(available);
            WinHttpReadData(hRequest, buf.data(), available, &downloaded);
            response.append(buf.data(), downloaded);
        } while (downloaded > 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        try {
            nlohmann::json newData = nlohmann::json::parse(response);
            if (!ValidateJson(newData)) return false;

            data_ = newData;
            SaveCache();
            lastUpdate_ = std::chrono::system_clock::now();
            return true;
        } catch (...) {
            return false;
        }
    }

    // Initialize: try cache first, then fetch, then fallback
    bool Initialize() {
        // Try to load cache
        if (LoadCache()) {
            auto path = std::filesystem::path(cacheFile_);
            if (std::filesystem::exists(path)) {
                auto lastWrite = std::filesystem::last_write_time(path);
                auto now = std::filesystem::file_time_type::clock::now();
                auto diff = std::chrono::duration_cast<std::chrono::minutes>(
                    now - lastWrite).count();

                if (diff < 120) { // Cache is < 2 hours old
                    lastUpdate_ = std::chrono::system_clock::now();
                    return true;
                }
            }
        }

        // Fetch fresh
        if (FetchLatest()) return true;

        // Fallback to cache even if old
        return LoadCache();
    }

    // Check if offsets need update (call periodically)
    bool NeedsUpdate() {
        auto now = std::chrono::system_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::minutes>(
            now - lastUpdate_).count();
        return diff >= 30; // Check every 30 minutes
    }

    std::string GetRobloxVersion() const {
        if (!data_.contains("Roblox Version")) return "";
        return data_["Roblox Version"].get<std::string>();
    }

    std::string GetDumperVersion() const {
        if (!data_.contains("Dumper Version")) return "";
        return data_["Dumper Version"].get<std::string>();
    }

    int GetTotalOffsets() const {
        if (!data_.contains("Total Offsets")) return 0;
        return data_["Total Offsets"].get<int>();
    }

    // Get raw offset from Offsets category
    uintptr_t Get(const std::string& category, const std::string& name) const {
        if (!data_.contains("Offsets")) return 0;
        auto& offsets = data_["Offsets"];
        if (!offsets.contains(category)) return 0;
        auto& cat = offsets[category];
        if (!cat.contains(name)) return 0;

        if (cat[name].is_number()) {
            return cat[name].get<uintptr_t>();
        }
        return 0;
    }

    // Get nested offset (e.g., "DataModel.ScriptContext")
    uintptr_t GetNested(const std::string& path) const {
        auto parts = SplitPath(path);
        nlohmann::json current = data_;

        for (const auto& part : parts) {
            if (!current.contains(part)) return 0;
            current = current[part];
        }

        if (current.is_number()) {
            return current.get<uintptr_t>();
        }
        return 0;
    }

    // Get full category as JSON
    nlohmann::json GetCategory(const std::string& category) const {
        if (!data_.contains("Offsets")) return {};
        auto& offsets = data_["Offsets"];
        if (!offsets.contains(category)) return {};
        return offsets[category];
    }

    bool IsValid() const {
        return data_.contains("Roblox Version") && 
               data_.contains("Offsets") &&
               data_["Offsets"].contains("DataModel");
    }

    std::string GetLastUpdateTime() const {
        auto time = std::chrono::system_clock::to_time_t(lastUpdate_);
        char buf[64];
        ctime_s(buf, sizeof(buf), &time);
        std::string result(buf);
        if (!result.empty() && result.back() == '\n') result.pop_back();
        return result;
    }

private:
    nlohmann::json data_;
    std::string cacheDir_;
    std::string cacheFile_;
    std::string configFile_;
    std::chrono::system_clock::time_point lastUpdate_;

    DWORD connectTimeout_ = 10000;
    DWORD sendTimeout_ = 10000;
    DWORD receiveTimeout_ = 30000;

    std::string GetCacheDir() {
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            return std::string(path) + "\\Nexo";
        }
        return "";
    }

    void EnsureCacheDir() {
        if (!cacheDir_.empty()) {
            CreateDirectoryA(cacheDir_.c_str(), NULL);
        }
    }

    bool LoadCache() {
        std::ifstream file(cacheFile_);
        if (!file.is_open()) return false;
        try {
            file >> data_;
            return ValidateJson(data_);
        } catch (...) {
            return false;
        }
    }

    void SaveCache() {
        std::ofstream file(cacheFile_);
        if (file.is_open()) {
            file << data_.dump(2);
        }
    }

    bool ValidateJson(const nlohmann::json& j) const {
        return j.is_object() &&
               j.contains("Roblox Version") &&
               j.contains("Offsets") &&
               j["Offsets"].is_object();
    }

    std::vector<std::string> SplitPath(const std::string& path) const {
        std::vector<std::string> parts;
        std::stringstream ss(path);
        std::string part;
        while (std::getline(ss, part, '.')) {
            parts.push_back(part);
        }
        return parts;
    }
};

} // namespace nexo
