#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

namespace nexo {

class Memory {
public:
    static DWORD GetProcessIdByName(const std::wstring& procName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe = { sizeof(pe) };
        DWORD pid = 0;

        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, procName.c_str()) == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }

        CloseHandle(snap);
        return pid;
    }

    static uintptr_t GetModuleBase(DWORD pid, const std::wstring& modName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W me = { sizeof(me) };
        uintptr_t base = 0;

        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, modName.c_str()) == 0) {
                    base = (uintptr_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }

        CloseHandle(snap);
        return base;
    }

    static size_t GetModuleSize(DWORD pid, const std::wstring& modName) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        MODULEENTRY32W me = { sizeof(me) };
        size_t size = 0;

        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, modName.c_str()) == 0) {
                    size = me.modBaseSize;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }

        CloseHandle(snap);
        return size;
    }

    static bool Read(HANDLE hProc, uintptr_t addr, void* buf, size_t size) {
        SIZE_T read = 0;
        return ReadProcessMemory(hProc, (LPCVOID)addr, buf, size, &read) && read == size;
    }

    static bool Write(HANDLE hProc, uintptr_t addr, void* buf, size_t size) {
        SIZE_T written = 0;
        return WriteProcessMemory(hProc, (LPVOID)addr, buf, size, &written) && written == size;
    }

    static bool ReadString(HANDLE hProc, uintptr_t addr, std::string& out, size_t maxLen = 256) {
        std::vector<char> buf(maxLen + 1, 0);
        if (!Read(hProc, addr, buf.data(), maxLen)) return false;
        out = std::string(buf.data());
        return true;
    }

    static bool ReadPtr(HANDLE hProc, uintptr_t addr, uintptr_t& out) {
        return Read(hProc, addr, &out, sizeof(out));
    }

    static bool ReadDword(HANDLE hProc, uintptr_t addr, DWORD& out) {
        return Read(hProc, addr, &out, sizeof(out));
    }

    static bool ReadFloat(HANDLE hProc, uintptr_t addr, float& out) {
        return Read(hProc, addr, &out, sizeof(out));
    }

    // Pattern scan in target process memory
    static uintptr_t ScanPattern(HANDLE hProc, uintptr_t start, size_t scanSize, 
                                  const char* pattern, const char* mask) {
        std::vector<char> buffer(scanSize);
        SIZE_T read = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)start, buffer.data(), scanSize, &read))
            return 0;

        size_t patternLen = strlen(mask);
        for (size_t i = 0; i <= read - patternLen; i++) {
            bool found = true;
            for (size_t j = 0; j < patternLen; j++) {
                if (mask[j] != '?' && buffer[i + j] != pattern[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return start + i;
        }
        return 0;
    }

    // Scan entire module for pattern
    static uintptr_t ScanModule(HANDLE hProc, DWORD pid, const std::wstring& modName,
                                 const char* pattern, const char* mask) {
        uintptr_t base = GetModuleBase(pid, modName);
        if (!base) return 0;
        size_t modSize = GetModuleSize(pid, modName);
        if (!modSize) return 0;
        return ScanPattern(hProc, base, modSize, pattern, mask);
    }

    // Resolve RIP-relative address (x64)
    static uintptr_t ResolveRip(HANDLE hProc, uintptr_t addr, uint32_t offset) {
        uint32_t rel = 0;
        if (!Read(hProc, addr + offset, &rel, sizeof(rel))) return 0;
        return addr + offset + 4 + rel;
    }

    // Scan for string reference in module
    static uintptr_t FindStringRef(HANDLE hProc, DWORD pid, const std::wstring& modName,
                                    const char* str) {
        uintptr_t base = GetModuleBase(pid, modName);
        size_t modSize = GetModuleSize(pid, modName);
        if (!base || !modSize) return 0;

        // First find the string in memory
        size_t strLen = strlen(str);
        std::vector<char> moduleData(modSize);
        SIZE_T read = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)base, moduleData.data(), modSize, &read))
            return 0;

        uintptr_t strAddr = 0;
        for (size_t i = 0; i <= read - strLen; i++) {
            if (memcmp(moduleData.data() + i, str, strLen) == 0) {
                strAddr = base + i;
                break;
            }
        }

        if (!strAddr) return 0;

        // Now find references to this string (simplified: scan for the address value)
        for (size_t i = 0; i <= read - 8; i += 4) {
            uintptr_t val = *(uintptr_t*)(moduleData.data() + i);
            if (val == strAddr) {
                // Check if it's a lea/rip-relative reference
                // Pattern: 48 8D 05/0D/15/1D/25/2D/35/3D (LEA with RIP)
                uint8_t* p = (uint8_t*)(moduleData.data() + i);
                if (i >= 3) {
                    uint8_t b0 = p[-3];
                    uint8_t b1 = p[-2];
                    if (b0 == 0x48 && (b1 == 0x8D || b1 == 0x8B || b1 == 0x89)) {
                        return base + i - 3;
                    }
                }
            }
        }
        return 0;
    }
};

} // namespace nexo
