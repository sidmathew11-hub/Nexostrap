// NexoCore.dll - Injected stub for Nexo Executor v1.1
// Minimal footprint. IPC + Lua bridge.
// Built for LO. Two years.

#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

// ═══════════════════════════════════════════════════════════════════════
//  Debug Logging
// ═══════════════════════════════════════════════════════════════════════

std::mutex g_logMutex;

void Log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ofstream log("C:\\Users\\admin\\AppData\\Roaming\\Nexo\\NexoCore.log", std::ios::app);
    if (log.is_open()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        log << "[" << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "] " << msg << std::endl;
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Lua Type Definitions (Luau / Roblox)
// ═══════════════════════════════════════════════════════════════════════

#define LUA_TNIL        0
#define LUA_TBOOLEAN    1
#define LUA_TNUMBER     2
#define LUA_TSTRING     4
#define LUA_TTABLE      5
#define LUA_TFUNCTION   6
#define LUA_TUSERDATA   7
#define LUA_TTHREAD     8
#define LUA_TBUFFER     9
#define LUA_TVECTOR     10

// ═══════════════════════════════════════════════════════════════════════
//  Function Pointer Types
// ═══════════════════════════════════════════════════════════════════════

typedef int(__fastcall* t_lua_pcall)(uintptr_t L, int nargs, int nresults, int errfunc);
typedef int(__fastcall* t_luau_load)(uintptr_t L, const char* chunkname, const char* data, size_t size, int env);
typedef void(__fastcall* t_lua_pushstring)(uintptr_t L, const char* s);
typedef const char*(__fastcall* t_lua_tolstring)(uintptr_t L, int idx, size_t* len);
typedef void(__fastcall* t_lua_settop)(uintptr_t L, int idx);
typedef int(__fastcall* t_lua_gettop)(uintptr_t L);
typedef void(__fastcall* t_lua_pushvalue)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_getfield)(uintptr_t L, int idx, const char* k);
typedef void(__fastcall* t_lua_setfield)(uintptr_t L, int idx, const char* k);
typedef void(__fastcall* t_lua_pushnil)(uintptr_t L);
typedef void(__fastcall* t_lua_pushnumber)(uintptr_t L, double n);
typedef void(__fastcall* t_lua_pushboolean)(uintptr_t L, int b);
typedef int(__fastcall* t_lua_type)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_createtable)(uintptr_t L, int narr, int nrec);
typedef void(__fastcall* t_lua_settable)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_gettable)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_rawgeti)(uintptr_t L, int idx, int n);
typedef void(__fastcall* t_lua_rawseti)(uintptr_t L, int idx, int n);
typedef void(__fastcall* t_lua_remove)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_insert)(uintptr_t L, int idx);
typedef int(__fastcall* t_lua_next)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_getglobal)(uintptr_t L, const char* name);
typedef void(__fastcall* t_lua_setglobal)(uintptr_t L, const char* name);
typedef void*(__fastcall* t_lua_touserdata)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_pushlightuserdata)(uintptr_t L, void* p);
typedef int(__fastcall* t_lua_getmetatable)(uintptr_t L, int idx);
typedef int(__fastcall* t_lua_setmetatable)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_pushcclosurek)(uintptr_t L, void* fn, const char* debugname, int nup, void* cont);

// ═══════════════════════════════════════════════════════════════════════
//  Globals
// ═══════════════════════════════════════════════════════════════════════

std::atomic<bool> g_ipcRunning{false};
std::thread g_ipcThread;
HANDLE g_pipe = INVALID_HANDLE_VALUE;

uintptr_t g_luaState = 0;
uintptr_t g_scriptContext = 0;
uintptr_t g_dataModel = 0;

// Function pointers
t_lua_pcall g_lua_pcall_ = nullptr;
t_luau_load g_luau_load_ = nullptr;
t_lua_pushstring g_lua_pushstring_ = nullptr;
t_lua_tolstring g_lua_tolstring_ = nullptr;
t_lua_settop g_lua_settop_ = nullptr;
t_lua_gettop g_lua_gettop_ = nullptr;
t_lua_pushvalue g_lua_pushvalue_ = nullptr;
t_lua_getfield g_lua_getfield_ = nullptr;
t_lua_setfield g_lua_setfield_ = nullptr;
t_lua_pushnil g_lua_pushnil_ = nullptr;
t_lua_pushnumber g_lua_pushnumber_ = nullptr;
t_lua_pushboolean g_lua_pushboolean_ = nullptr;
t_lua_type g_lua_type_ = nullptr;
t_lua_createtable g_lua_createtable_ = nullptr;
t_lua_settable g_lua_settable_ = nullptr;
t_lua_gettable g_lua_gettable_ = nullptr;
t_lua_rawgeti g_lua_rawgeti_ = nullptr;
t_lua_rawseti g_lua_rawseti_ = nullptr;
t_lua_remove g_lua_remove_ = nullptr;
t_lua_insert g_lua_insert_ = nullptr;
t_lua_next g_lua_next_ = nullptr;
t_lua_getglobal g_lua_getglobal_ = nullptr;
t_lua_setglobal g_lua_setglobal_ = nullptr;
t_lua_touserdata g_lua_touserdata_ = nullptr;
t_lua_pushlightuserdata g_lua_pushlightuserdata_ = nullptr;
t_lua_getmetatable g_lua_getmetatable_ = nullptr;
t_lua_setmetatable g_lua_setmetatable_ = nullptr;
t_lua_pushcclosurek g_lua_pushcclosurek_ = nullptr;

// Output buffer
std::mutex g_outputMutex;
std::string g_outputBuffer;

// ═══════════════════════════════════════════════════════════════════════
//  Memory Utilities (Local Process)
// ═══════════════════════════════════════════════════════════════════════

uintptr_t PatternScan(uintptr_t start, size_t size, const char* pattern, const char* mask) {
    size_t patternLen = strlen(mask);
    for (size_t i = 0; i <= size - patternLen; i++) {
        bool found = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (mask[j] != '?' && ((char*)start)[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }
        if (found) return start + i;
    }
    return 0;
}

uintptr_t ResolveRip(uintptr_t addr, uint32_t offset) {
    uint32_t rel = *(uint32_t*)(addr + offset);
    return addr + offset + 4 + rel;
}

// ═══════════════════════════════════════════════════════════════════════
//  Lua State Discovery
// ═══════════════════════════════════════════════════════════════════════

bool FindLuaFunctions() {
    HMODULE hMod = GetModuleHandleA(NULL);
    if (!hMod) {
        Log("GetModuleHandleA failed");
        return false;
    }

    uintptr_t base = (uintptr_t)hMod;
    Log("Module base: 0x" + std::to_string(base));

    // Get module size
    MEMORY_BASIC_INFORMATION mbi;
    size_t modSize = 0;
    uintptr_t addr = base;
    while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) && mbi.AllocationBase == (PVOID)base) {
        modSize = (addr + mbi.RegionSize) - base;
        addr += mbi.RegionSize;
    }
    if (!modSize) modSize = 0x6000000;
    Log("Module size: 0x" + std::to_string(modSize));

    // Direct signature scanning for known patterns
    // luau_load
    uintptr_t luauLoad = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x48\x8B\xF2",
        "xxxx?xxxx?xxxxxxxxxx");
    if (!luauLoad) {
        luauLoad = PatternScan(base, modSize,
            "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x00\x00\x00\x00\x48\x8B\xCB",
            "xxxxxxxxxx????xxx");
    }
    if (luauLoad) {
        g_luau_load_ = (t_luau_load)luauLoad;
        Log("Found luau_load at: 0x" + std::to_string(luauLoad));
    } else {
        Log("luau_load NOT FOUND");
    }

    // lua_pcall
    uintptr_t luaPcall = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x41\x8B\xE9",
        "xxxx?xxxx?xxxx?xxxxxxxxxx");
    if (!luaPcall) {
        luaPcall = PatternScan(base, modSize,
            "\x48\x8B\xC4\x48\x89\x58\x00\x48\x89\x70\x00\x48\x89\x78\x00\x4C\x89\x70\x00\x55\x48\x8D\x68",
            "xxxxxx?xxx?xxx?xxx?xxxx");
    }
    if (luaPcall) {
        g_lua_pcall_ = (t_lua_pcall)luaPcall;
        Log("Found lua_pcall at: 0x" + std::to_string(luaPcall));
    } else {
        Log("lua_pcall NOT FOUND");
    }

    // lua_gettop
    uintptr_t luaGettop = PatternScan(base, modSize,
        "\x48\x8B\x41\x00\x48\x8B\x88\x00\x00\x00\x00\x48\x8B\x01\xFF\x50\x00",
        "xxx?xxx????xxxxxx?");
    if (!luaGettop) {
        luaGettop = PatternScan(base, modSize,
            "\x48\x8B\x81\x00\x00\x00\x00\x48\x2B\x81\x00\x00\x00\x00\x48\xC1\xF8\x04\xC3",
            "xxx????xx????xxxxxx");
    }
    if (luaGettop) {
        g_lua_gettop_ = (t_lua_gettop)luaGettop;
        Log("Found lua_gettop at: 0x" + std::to_string(luaGettop));
    }

    // lua_settop
    uintptr_t luaSettop = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaSettop) {
        g_lua_settop_ = (t_lua_settop)luaSettop;
        Log("Found lua_settop at: 0x" + std::to_string(luaSettop));
    }

    // lua_pushstring
    uintptr_t luaPushstring = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9\x48\x8B\xCA",
        "xxxx?xxxxxxxxxxxxx");
    if (luaPushstring) {
        g_lua_pushstring_ = (t_lua_pushstring)luaPushstring;
        Log("Found lua_pushstring at: 0x" + std::to_string(luaPushstring));
    }

    // lua_tolstring
    uintptr_t luaTolstring = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x49\x8B\xF0\x8B\xDA",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaTolstring) {
        g_lua_tolstring_ = (t_lua_tolstring)luaTolstring;
        Log("Found lua_tolstring at: 0x" + std::to_string(luaTolstring));
    }

    // lua_getfield / lua_setfield
    uintptr_t luaGetfield = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF2\x48\x8B\xF9",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaGetfield) {
        g_lua_getfield_ = (t_lua_getfield)luaGetfield;
        Log("Found lua_getfield at: 0x" + std::to_string(luaGetfield));
    }

    uintptr_t luaSetfield = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xF1",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaSetfield) {
        g_lua_setfield_ = (t_lua_setfield)luaSetfield;
        Log("Found lua_setfield at: 0x" + std::to_string(luaSetfield));
    }

    // lua_pushnil
    uintptr_t luaPushnil = PatternScan(base, modSize,
        "\x48\x8B\x41\x00\x48\x8B\x88\x00\x00\x00\x00\x48\x8B\x01\x48\xFF\x60\x00",
        "xxx?xxx????xxxxx?");
    if (luaPushnil) {
        g_lua_pushnil_ = (t_lua_pushnil)luaPushnil;
        Log("Found lua_pushnil at: 0x" + std::to_string(luaPushnil));
    }

    // lua_pushnumber
    uintptr_t luaPushnumber = PatternScan(base, modSize,
        "\xF2\x0F\x11\x44\x24\x00\x53\x48\x83\xEC\x20\x48\x8B\xD9\xF2\x0F\x10\x44\x24\x00",
        "xxxxx?xxxxxxxxxxxx?");
    if (luaPushnumber) {
        g_lua_pushnumber_ = (t_lua_pushnumber)luaPushnumber;
        Log("Found lua_pushnumber at: 0x" + std::to_string(luaPushnumber));
    }

    // lua_pushboolean
    uintptr_t luaPushboolean = PatternScan(base, modSize,
        "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\x8B\xCA\xE8\x00\x00\x00\x00\x48\x8B\xCB",
        "xxxxxxxxxxxxx????xxx");
    if (luaPushboolean) {
        g_lua_pushboolean_ = (t_lua_pushboolean)luaPushboolean;
        Log("Found lua_pushboolean at: 0x" + std::to_string(luaPushboolean));
    }

    // lua_type
    uintptr_t luaType = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaType) {
        g_lua_type_ = (t_lua_type)luaType;
        Log("Found lua_type at: 0x" + std::to_string(luaType));
    }

    // lua_createtable
    uintptr_t luaCreatetable = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x8B\xFA\x48\x8B\xF1",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaCreatetable) {
        g_lua_createtable_ = (t_lua_createtable)luaCreatetable;
        Log("Found lua_createtable at: 0x" + std::to_string(luaCreatetable));
    }

    // lua_getmetatable
    uintptr_t luaGetmetatable = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaGetmetatable) {
        g_lua_getmetatable_ = (t_lua_getmetatable)luaGetmetatable;
        Log("Found lua_getmetatable at: 0x" + std::to_string(luaGetmetatable));
    }

    // lua_setmetatable
    uintptr_t luaSetmetatable = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaSetmetatable) {
        g_lua_setmetatable_ = (t_lua_setmetatable)luaSetmetatable;
        Log("Found lua_setmetatable at: 0x" + std::to_string(luaSetmetatable));
    }

    // lua_pushcclosurek
    uintptr_t luaPushcclosurek = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x49\x8B\xF8\x8B\xF2\x48\x8B\xD9",
        "xxxx?xxxx?xxxxxxxxxxxx");
    if (luaPushcclosurek) {
        g_lua_pushcclosurek_ = (t_lua_pushcclosurek)luaPushcclosurek;
        Log("Found lua_pushcclosurek at: 0x" + std::to_string(luaPushcclosurek));
    }

    bool ok = g_luau_load_ && g_lua_pcall_ && g_lua_gettop_ && g_lua_settop_;
    Log("FindLuaFunctions result: " + std::string(ok ? "SUCCESS" : "FAILED"));
    return ok;
}

bool FindLuaState() {
    HMODULE hMod = GetModuleHandleA(NULL);
    uintptr_t base = (uintptr_t)hMod;
    Log("FindLuaState: base = 0x" + std::to_string(base));

    // Scan memory for lua_State
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t addr = base; addr < base + 0x8000000;) {
        if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;

        if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY))) {
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress; 
                 p < (uintptr_t)mbi.BaseAddress + mbi.RegionSize - 0x200; p += 8) {

                BYTE tt = *(BYTE*)p;
                if (tt == LUA_TTHREAD) {
                    uintptr_t top = *(uintptr_t*)(p + 0x10);
                    uintptr_t basePtr = *(uintptr_t*)(p + 0x18);
                    uintptr_t ci = *(uintptr_t*)(p + 0x30);

                    if (top > p && top < p + 0x100000 && 
                        basePtr > p && basePtr < p + 0x100000 &&
                        ci > basePtr && ci < top) {

                        uintptr_t g = *(uintptr_t*)(p + 0x8);
                        if (g > base && g < base + 0x8000000) {
                            g_luaState = p;
                            Log("Found lua_State at: 0x" + std::to_string(p));
                            return true;
                        }
                    }
                }
            }
        }
        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }

    Log("lua_State NOT FOUND");
    return false;
}

// ═══════════════════════════════════════════════════════════════════════
//  Lua Helpers
// ═══════════════════════════════════════════════════════════════════════

void LuaSetGlobal(uintptr_t L, const char* name) {
    if (g_lua_setfield_) {
        g_lua_setfield_(L, -10002, name);
    }
}

void LuaGetGlobal(uintptr_t L, const char* name) {
    if (g_lua_getfield_) {
        g_lua_getfield_(L, -10002, name);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Custom Print (redirects to output buffer)
// ═══════════════════════════════════════════════════════════════════════

int CustomPrint(uintptr_t L) {
    int nargs = g_lua_gettop_(L);
    std::string output;

    for (int i = 1; i <= nargs; i++) {
        int type = g_lua_type_(L, i);
        switch (type) {
        case LUA_TSTRING: {
            size_t len = 0;
            const char* str = g_lua_tolstring_(L, i, &len);
            if (str) output.append(str, len);
            break;
        }
        case LUA_TNUMBER: {
            double val = *(double*)(*(uintptr_t*)(L + 0x10) + i * 0x10 + 0x8);
            output += std::to_string(val);
            break;
        }
        case LUA_TBOOLEAN: {
            int val = *(int*)(*(uintptr_t*)(L + 0x10) + i * 0x10 + 0x8);
            output += val ? "true" : "false";
            break;
        }
        case LUA_TNIL:
            output += "nil";
            break;
        default:
            output += "[" + std::to_string(type) + "]";
            break;
        }
        if (i < nargs) output += "\t";
    }
    output += "\n";

    {
        std::lock_guard<std::mutex> lock(g_outputMutex);
        g_outputBuffer += output;
    }

    g_lua_settop_(L, 0);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  Set Clipboard
// ═══════════════════════════════════════════════════════════════════════

int LuaSetClipboard(uintptr_t L) {
    int nargs = g_lua_gettop_(L);
    if (nargs < 1) {
        g_lua_pushstring_(L, "setclipboard requires 1 argument");
        return 1;
    }

    size_t len = 0;
    const char* str = g_lua_tolstring_(L, 1, &len);
    if (!str) {
        g_lua_pushstring_(L, "setclipboard requires a string");
        return 1;
    }

    std::string text(str, len);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
        if (hMem) {
            memcpy(GlobalLock(hMem), text.c_str(), text.length() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }

    g_lua_settop_(L, 0);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  GetRawMetatable
// ═══════════════════════════════════════════════════════════════════════

int LuaGetRawMetatable(uintptr_t L) {
    int nargs = g_lua_gettop_(L);
    if (nargs < 1) {
        g_lua_pushnil_(L);
        return 1;
    }

    if (g_lua_getmetatable_(L, 1)) {
        return 1;
    }

    g_lua_pushnil_(L);
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════
//  HookFunction (basic closure replacement)
// ═══════════════════════════════════════════════════════════════════════

int LuaHookFunction(uintptr_t L) {
    int nargs = g_lua_gettop_(L);
    if (nargs < 2) {
        g_lua_pushstring_(L, "hookfunction requires 2 arguments");
        return 1;
    }

    int t1 = g_lua_type_(L, 1);
    int t2 = g_lua_type_(L, 2);

    if (t1 != LUA_TFUNCTION || t2 != LUA_TFUNCTION) {
        g_lua_pushstring_(L, "hookfunction requires two function");
        return 1;
    }

    g_lua_pushvalue_(L, 1);
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════
//  Setup Exploit API Globals
// ═══════════════════════════════════════════════════════════════════════

void SetupExploitAPI() {
    if (!g_luaState) return;
    Log("Setting up exploit API...");

    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)CustomPrint, "print", 0, NULL);
        LuaSetGlobal(g_luaState, "print");
        Log("Registered print");
    }

    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)LuaSetClipboard, "setclipboard", 0, NULL);
        LuaSetGlobal(g_luaState, "setclipboard");
        Log("Registered setclipboard");
    }

    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)LuaGetRawMetatable, "getrawmetatable", 0, NULL);
        LuaSetGlobal(g_luaState, "getrawmetatable");
        Log("Registered getrawmetatable");
    }

    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)LuaHookFunction, "hookfunction", 0, NULL);
        LuaSetGlobal(g_luaState, "hookfunction");
        Log("Registered hookfunction");
    }

    LuaGetGlobal(g_luaState, "_G");
    LuaSetGlobal(g_luaState, "getgenv");
    Log("Registered getgenv");

    g_lua_settop_(g_luaState, 0);
    Log("Exploit API setup complete");
}

// ═══════════════════════════════════════════════════════════════════════
//  Lua Execution
// ═══════════════════════════════════════════════════════════════════════

bool ExecuteLua(const std::string& script, std::string& output) {
    if (!g_luaState || !g_luau_load_ || !g_lua_pcall_) {
        output = "Lua state not initialized";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_outputMutex);
        g_outputBuffer.clear();
    }

    int loadResult = g_luau_load_(g_luaState, "=Nexo", script.c_str(), script.length(), 0);
    if (loadResult != 0) {
        size_t len = 0;
        const char* err = g_lua_tolstring_(g_luaState, -1, &len);
        output = "Compile error: " + std::string(err ? err : "unknown");
        g_lua_settop_(g_luaState, 0);
        return false;
    }

    int pcallResult = g_lua_pcall_(g_luaState, 0, 0, 0);
    if (pcallResult != 0) {
        size_t len = 0;
        const char* err = g_lua_tolstring_(g_luaState, -1, &len);
        output = "Runtime error: " + std::string(err ? err : "unknown");
        g_lua_settop_(g_luaState, 0);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(g_outputMutex);
        output = g_outputBuffer;
    }

    g_lua_settop_(g_luaState, 0);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
//  IPC Server
// ═══════════════════════════════════════════════════════════════════════

void SendResponse(HANDLE pipe, const nlohmann::json& response) {
    std::string jsonStr = response.dump();
    uint32_t len = static_cast<uint32_t>(jsonStr.length());
    DWORD written = 0;
    WriteFile(pipe, &len, sizeof(len), &written, NULL);
    WriteFile(pipe, jsonStr.c_str(), len, &written, NULL);
}

void HandleClient(HANDLE pipe) {
    Log("Client connected to pipe");
    while (g_ipcRunning) {
        uint32_t len = 0;
        DWORD read = 0;

        if (!ReadFile(pipe, &len, sizeof(len), &read, NULL) || read != sizeof(len))
            break;

        if (len == 0 || len > 1024 * 1024) break;

        std::vector<char> buf(len + 1, 0);
        if (!ReadFile(pipe, buf.data(), len, &read, NULL) || read != len)
            break;

        try {
            auto j = nlohmann::json::parse(buf.data());
            int type = j.value("type", 0);
            uint32_t id = j.value("id", 0);
            std::string content = j.value("content", "");

            nlohmann::json response;
            response["id"] = id;

            switch (type) {
            case 0: // PING
                response["type"] = 1;
                response["content"] = "pong";
                break;

            case 2: { // EXECUTE
                std::string output;
                bool success = ExecuteLua(content, output);
                if (success) {
                    response["type"] = 3;
                    response["content"] = output;
                } else {
                    response["type"] = 4;
                    response["content"] = output;
                }
                break;
            }

            case 6: { // REMOTE_SPY
                response["type"] = 5;
                response["content"] = "Remote spy not yet implemented in v1.1";
                break;
            }

            case 9: { // SET_CLIPBOARD
                if (OpenClipboard(NULL)) {
                    EmptyClipboard();
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, content.length() + 1);
                    if (hMem) {
                        memcpy(GlobalLock(hMem), content.c_str(), content.length() + 1);
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_TEXT, hMem);
                    }
                    CloseClipboard();
                }
                response["type"] = 5;
                response["content"] = "Clipboard set";
                break;
            }

            default:
                response["type"] = 4;
                response["content"] = "Unknown command type: " + std::to_string(type);
                break;
            }

            SendResponse(pipe, response);

        } catch (...) {
            nlohmann::json err;
            err["type"] = 4;
            err["content"] = "Invalid JSON";
            SendResponse(pipe, err);
        }
    }

    Log("Client disconnected");
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

void StartIPCServer() {
    g_ipcRunning = true;
    Log("Starting IPC server...");

    while (g_ipcRunning) {
        HANDLE pipe = CreateNamedPipeA(
            "\\\\.\\pipe\\NexoIPC",
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536, 65536,
            0,
            NULL
        );

        if (pipe == INVALID_HANDLE_VALUE) {
            Log("CreateNamedPipe failed: " + std::to_string(GetLastError()));
            Sleep(500);
            continue;
        }

        Log("Named pipe created, waiting for connection...");

        BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            Log("Client connected");
            std::thread clientThread(HandleClient, pipe);
            clientThread.detach();
        } else {
            Log("ConnectNamedPipe failed: " + std::to_string(GetLastError()));
            CloseHandle(pipe);
        }
    }
}

void StopIPCServer() {
    g_ipcRunning = false;
}

// ═══════════════════════════════════════════════════════════════════════
//  DllMain
// ═══════════════════════════════════════════════════════════════════════

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Ensure log directory exists
        CreateDirectoryA("C:\\Users\\admin\\AppData\\Roaming\\Nexo", NULL);

        Log("=== DLL_PROCESS_ATTACH ===");

        // CRITICAL: Do NOT do heavy work in DllMain.
        // Create a thread and let DllMain return immediately.
        std::thread initThread([]() {
            Log("Init thread started");

            // Wait for Roblox to finish its own initialization
            Sleep(2000);

            Log("Finding Lua Functions...");
            if (!FindLuaFunctions()) {
                Log("ERROR: FindLuaFunctions failed");
                return;
            }

            Log("Finding Lua State...");
            if (!FindLuaState()) {
                Log("ERROR: FindLuaState failed");
                return;
            }

            Log("Setting up Exploit API...");
            SetupExploitAPI();

            Log("Starting IPC Server...");
            StartIPCServer();
        });

        initThread.detach();
        Log("Init thread detached, DllMain returning");
    }
    else if (reason == DLL_PROCESS_DETACH) {
        Log("=== DLL_PROCESS_DETACH ===");
        StopIPCServer();
    }
    return TRUE;
}
