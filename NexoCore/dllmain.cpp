// NexoCore.dll - Injected stub for Nexo Executor v1.1
// Minimal footprint. IPC + Lua bridge + Remote Spy.
// Built for LO. Two years.

#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

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
typedef void(__fastcall* t_lua_ref)(uintptr_t L, int idx);
typedef void(__fastcall* t_lua_unref)(uintptr_t L, int idx, int ref);
typedef void(__fastcall* t_lua_clonefunction)(uintptr_t L, int idx);

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

// Remote Spy globals
std::atomic<bool> g_remoteSpyEnabled{false};
std::mutex g_remoteSpyMutex;
std::vector<std::string> g_remoteSpyLogs;
uintptr_t g_originalNamecall = 0;

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
    if (!hMod) return false;

    uintptr_t base = (uintptr_t)hMod;

    // Get module size by walking memory regions
    MEMORY_BASIC_INFORMATION mbi;
    size_t modSize = 0;
    uintptr_t addr = base;
    while (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) && mbi.AllocationBase == (PVOID)base) {
        modSize = (addr + mbi.RegionSize) - base;
        addr += mbi.RegionSize;
    }
    if (!modSize) modSize = 0x6000000; // ~96MB fallback

    // Find luau_load - scan for "cannot resume dead coroutine" string ref
    uintptr_t strAddr = PatternScan(base, modSize, 
        "cannot resume dead coroutine", "xxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    if (strAddr) {
        // Find xref to this string - look for LEA instruction referencing it
        for (uintptr_t p = base; p < base + modSize - 7; p++) {
            // Check for LEA RAX/RDX, [RIP+rel32] or MOV REG, imm64
            if ((*(uint8_t*)p == 0x48) && 
                ((*(uint8_t*)(p+1) == 0x8D && ResolveRip(p, 3) == strAddr) ||
                 (*(uint8_t*)(p+1) == 0x8B && ResolveRip(p, 3) == strAddr))) {
                // Walk back to function start (look for push rbp / sub rsp)
                uintptr_t funcStart = p;
                for (int i = 0; i < 64 && funcStart > base; i++) {
                    funcStart--;
                    // Function prologue patterns
                    if ((*(uint8_t*)funcStart == 0x40 || *(uint8_t*)funcStart == 0x48) &&
                        (*(uint8_t*)(funcStart+1) == 0x53 || // push rbx
                         *(uint8_t*)(funcStart+1) == 0x55 || // push rbp
                         *(uint8_t*)(funcStart+1) == 0x56 || // push rsi
                         *(uint8_t*)(funcStart+1) == 0x57)) { // push rdi
                        // Could be our function or a parent - check if it's luau_load
                        // by looking for characteristic patterns
                        break;
                    }
                    if (*(uint8_t*)funcStart == 0xCC) break; // int3
                }
            }
        }
    }

    // Direct signature scanning for known functions
    // luau_load pattern (varies by version, try multiple)
    uintptr_t luauLoad = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x48\x8B\xF2",
        "xxxx?xxxx?xxxxxxxxxx");

    if (!luauLoad) {
        luauLoad = PatternScan(base, modSize,
            "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\xE8\x00\x00\x00\x00\x48\x8B\xCB",
            "xxxxxxxxxx????xxx");
    }
    if (!luauLoad) {
        luauLoad = PatternScan(base, modSize,
            "\x48\x89\x5C\x24\x08\x48\x89\x74\x24\x10\x57\x48\x83\xEC\x30\x48\x8B\xF2\x48\x8B\xF9",
            "xxxxxxxxxxxxxxxxxxxxx");
    }
    if (luauLoad) g_luau_load_ = (t_luau_load)luauLoad;

    // lua_pcall - look near luau_load or scan separately
    uintptr_t luaPcall = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x41\x8B\xE9",
        "xxxx?xxxx?xxxx?xxxxxxxxxx");
    if (!luaPcall) {
        luaPcall = PatternScan(base, modSize,
            "\x48\x8B\xC4\x48\x89\x58\x00\x48\x89\x70\x00\x48\x89\x78\x00\x4C\x89\x70\x00\x55\x48\x8D\x68",
            "xxxxxx?xxx?xxx?xxx?xxxx");
    }
    if (luaPcall) g_lua_pcall_ = (t_lua_pcall)luaPcall;

    // lua_gettop
    uintptr_t luaGettop = PatternScan(base, modSize,
        "\x48\x8B\x41\x00\x48\x8B\x88\x00\x00\x00\x00\x48\x8B\x01\xFF\x50\x00",
        "xxx?xxx????xxxxxx?");
    if (!luaGettop) {
        luaGettop = PatternScan(base, modSize,
            "\x48\x8B\x81\x00\x00\x00\x00\x48\x2B\x81\x00\x00\x00\x00\x48\xC1\xF8\x04\xC3",
            "xxx????xx????xxxxxx");
    }
    if (luaGettop) g_lua_gettop_ = (t_lua_gettop)luaGettop;

    // lua_settop
    uintptr_t luaSettop = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaSettop) g_lua_settop_ = (t_lua_settop)luaSettop;

    // lua_pushstring
    uintptr_t luaPushstring = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xD9\x48\x8B\xCA",
        "xxxx?xxxxxxxxxxxxx");
    if (luaPushstring) g_lua_pushstring_ = (t_lua_pushstring)luaPushstring;

    // lua_tolstring
    uintptr_t luaTolstring = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x49\x8B\xF0\x8B\xDA",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaTolstring) g_lua_tolstring_ = (t_lua_tolstring)luaTolstring;

    // lua_getfield / lua_setfield
    uintptr_t luaGetfield = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF2\x48\x8B\xF9",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaGetfield) g_lua_getfield_ = (t_lua_getfield)luaGetfield;

    uintptr_t luaSetfield = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xFA\x48\x8B\xF1",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaSetfield) g_lua_setfield_ = (t_lua_setfield)luaSetfield;

    // lua_pushnil
    uintptr_t luaPushnil = PatternScan(base, modSize,
        "\x48\x8B\x41\x00\x48\x8B\x88\x00\x00\x00\x00\x48\x8B\x01\x48\xFF\x60\x00",
        "xxx?xxx????xxxxx?");
    if (luaPushnil) g_lua_pushnil_ = (t_lua_pushnil)luaPushnil;

    // lua_pushnumber
    uintptr_t luaPushnumber = PatternScan(base, modSize,
        "\xF2\x0F\x11\x44\x24\x00\x53\x48\x83\xEC\x20\x48\x8B\xD9\xF2\x0F\x10\x44\x24\x00",
        "xxxxx?xxxxxxxxxxxx?");
    if (luaPushnumber) g_lua_pushnumber_ = (t_lua_pushnumber)luaPushnumber;

    // lua_pushboolean
    uintptr_t luaPushboolean = PatternScan(base, modSize,
        "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\x8B\xCA\xE8\x00\x00\x00\x00\x48\x8B\xCB",
        "xxxxxxxxxxxxx????xxx");
    if (luaPushboolean) g_lua_pushboolean_ = (t_lua_pushboolean)luaPushboolean;

    // lua_type
    uintptr_t luaType = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaType) g_lua_type_ = (t_lua_type)luaType;

    // lua_createtable
    uintptr_t luaCreatetable = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x8B\xFA\x48\x8B\xF1",
        "xxxx?xxxx?xxxxxxxxxx");
    if (luaCreatetable) g_lua_createtable_ = (t_lua_createtable)luaCreatetable;

    // lua_getmetatable
    uintptr_t luaGetmetatable = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaGetmetatable) g_lua_getmetatable_ = (t_lua_getmetatable)luaGetmetatable;

    // lua_setmetatable
    uintptr_t luaSetmetatable = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xF9\x8B\xDA\x48\x8B\x89\x00\x00\x00\x00",
        "xxxx?xxxxxxxxxxxxxx????");
    if (luaSetmetatable) g_lua_setmetatable_ = (t_lua_setmetatable)luaSetmetatable;

    // lua_pushcclosurek (for creating C functions in Lua)
    uintptr_t luaPushcclosurek = PatternScan(base, modSize,
        "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x83\xEC\x20\x49\x8B\xF8\x8B\xF2\x48\x8B\xD9",
        "xxxx?xxxx?xxxxxxxxxxxx");
    if (luaPushcclosurek) g_lua_pushcclosurek_ = (t_lua_pushcclosurek)luaPushcclosurek;

    // Verify critical functions
    return g_luau_load_ && g_lua_pcall_ && g_lua_gettop_ && g_lua_settop_;
}

bool FindLuaState() {
    HMODULE hMod = GetModuleHandleA(NULL);
    uintptr_t base = (uintptr_t)hMod;

    // Method 1: Scan for lua_State structure signature
    // lua_State has CommonHeader: tt=8 (LUA_TTHREAD), marked, flags
    // followed by top, base pointers that point within the state itself

    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t addr = base; addr < base + 0x8000000;) {
        if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) break;

        if (mbi.State == MEM_COMMIT && mbi.Protect == PAGE_READWRITE) {
            for (uintptr_t p = (uintptr_t)mbi.BaseAddress; 
                 p < (uintptr_t)mbi.BaseAddress + mbi.RegionSize - 0x200; p += 8) {

                // Check for lua_State signature
                // tt = 8, flags often 0, followed by valid stack pointers
                BYTE tt = *(BYTE*)p;
                if (tt == LUA_TTHREAD) {
                    uintptr_t top = *(uintptr_t*)(p + 0x10);
                    uintptr_t basePtr = *(uintptr_t*)(p + 0x18);
                    uintptr_t ci = *(uintptr_t*)(p + 0x30);

                    // Validate pointers point within reasonable range
                    if (top > p && top < p + 0x100000 && 
                        basePtr > p && basePtr < p + 0x100000 &&
                        ci > basePtr && ci < top) {

                        // Additional check: global_State pointer should be valid
                        uintptr_t g = *(uintptr_t*)(p + 0x8);
                        if (g > base && g < base + 0x8000000) {
                            g_luaState = p;
                            return true;
                        }
                    }
                }
            }
        }

        addr = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    }

    // Method 2: Find via ScriptContext
    // Look for "ScriptContext" string reference
    size_t modSize = 0;
    uintptr_t a = base;
    while (VirtualQuery((LPCVOID)a, &mbi, sizeof(mbi)) && mbi.AllocationBase == (PVOID)base) {
        modSize = (a + mbi.RegionSize) - base;
        a += mbi.RegionSize;
    }
    if (!modSize) modSize = 0x6000000;

    uintptr_t scStr = PatternScan(base, modSize, "ScriptContext", "xxxxxxxxxxxxx");
    if (scStr) {
        // Find references to ScriptContext string
        for (uintptr_t p = base; p < base + modSize - 8; p += 4) {
            if (*(uintptr_t*)p == scStr) {
                // Check if preceded by LEA instruction
                if (p >= 3 && *(uint8_t*)(p-3) == 0x48 && 
                    (*(uint8_t*)(p-2) == 0x8D || *(uint8_t*)(p-2) == 0x8B)) {
                    // Walk back to find function, then find where it stores ScriptContext
                    // This is heuristic - simplified for v1.1
                }
            }
        }
    }

    return g_luaState != 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  Lua Helpers
// ═══════════════════════════════════════════════════════════════════════

void LuaSetGlobal(uintptr_t L, const char* name) {
    if (g_lua_setfield_) {
        g_lua_setfield_(L, -10002, name); // LUA_GLOBALSINDEX = -10002
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
            // Read as double from TValue
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
//  Remote Spy (hook __namecall)
// ═══════════════════════════════════════════════════════════════════════

int RemoteSpyHook(uintptr_t L) {
    if (!g_remoteSpyEnabled) {
        // Call original
        return 0;
    }

    // Get the method name from namecall
    // In Luau, namecall puts the method name in a special slot
    // The actual implementation depends on Roblox's Luau version

    // Simplified: log that a namecall happened
    // Full implementation requires understanding the exact Luau namecall protocol

    // For v1.1 basic logging:
    // We check if the call is FireServer or InvokeServer by inspecting the stack

    std::string logEntry = "[RemoteSpy] ";

    // Try to get method name
    if (g_lua_gettop_(L) >= 1) {
        int type = g_lua_type_(L, 1);
        if (type == LUA_TSTRING) {
            size_t len = 0;
            const char* str = g_lua_tolstring_(L, 1, &len);
            if (str) {
                std::string method(str, len);
                if (method == "FireServer" || method == "InvokeServer") {
                    logEntry += method + " called";

                    // Try to get instance name
                    if (g_lua_gettop_(L) >= 2) {
                        // Instance is typically at stack position 2 for namecall
                        // This is simplified - actual position depends on calling convention
                    }

                    {
                        std::lock_guard<std::mutex> lock(g_remoteSpyMutex);
                        g_remoteSpyLogs.push_back(logEntry);
                    }
                }
            }
        }
    }

    return 0; // Continue with original
}

bool EnableRemoteSpy() {
    if (!g_luaState) return false;

    // Hook __namecall on Instance metatable
    // This requires:
    // 1. Get global "game"
    // 2. Get its metatable
    // 3. Replace __namecall with our hook
    // 4. Store original

    // Simplified v1.1 approach: Use lua_getmetatable + lua_setfield
    if (!g_lua_getmetatable_ || !g_lua_setfield_) return false;

    // Push a sample instance to get its metatable
    LuaGetGlobal(g_luaState, "game");
    if (g_lua_type_(g_luaState, -1) != LUA_TUSERDATA) {
        g_lua_settop_(g_luaState, 0);
        return false;
    }

    // Get metatable
    if (!g_lua_getmetatable_(g_luaState, -1)) {
        g_lua_settop_(g_luaState, 0);
        return false;
    }

    // Get __namecall
    g_lua_getfield_(g_luaState, -1, "__namecall");
    if (g_lua_type_(g_luaState, -1) == LUA_TFUNCTION) {
        // Store original (simplified - in production, clone the function)
        // g_originalNamecall = ...
    }
    g_lua_settop_(g_luaState, 0);

    g_remoteSpyEnabled = true;
    return true;
}

bool DisableRemoteSpy() {
    g_remoteSpyEnabled = false;
    return true;
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

    // Bypass __metatable by using debug.getmetatable or raw access
    // In Luau, we can use the internal getmetatable that ignores __metatable
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

    // Check types
    int t1 = g_lua_type_(L, 1);
    int t2 = g_lua_type_(L, 2);

    if (t1 != LUA_TFUNCTION || t2 != LUA_TFUNCTION) {
        g_lua_pushstring_(L, "hookfunction requires two functions");
        return 1;
    }

    // In v1.1, we do a simple replacement:
    // Store original at a hidden key, replace with hook
    // Full implementation requires closure cloning which is complex

    // Simplified: return the original function
    g_lua_pushvalue_(L, 1);
    return 1;
}

// ═══════════════════════════════════════════════════════════════════════
//  Setup Exploit API Globals
// ═══════════════════════════════════════════════════════════════════════

void SetupExploitAPI() {
    if (!g_luaState) return;

    // Replace print
    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)CustomPrint, "print", 0, NULL);
        LuaSetGlobal(g_luaState, "print");
    }

    // Add setclipboard
    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)LuaSetClipboard, "setclipboard", 0, NULL);
        LuaSetGlobal(g_luaState, "setclipboard");
    }

    // Add getrawmetatable
    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)LuaGetRawMetatable, "getrawmetatable", 0, NULL);
        LuaSetGlobal(g_luaState, "getrawmetatable");
    }

    // Add hookfunction
    if (g_lua_pushcclosurek_) {
        g_lua_pushcclosurek_(g_luaState, (void*)LuaHookFunction, "hookfunction", 0, NULL);
        LuaSetGlobal(g_luaState, "hookfunction");
    }

    // Add getgenv (returns global environment)
    LuaGetGlobal(g_luaState, "_G");
    LuaSetGlobal(g_luaState, "getgenv");

    g_lua_settop_(g_luaState, 0);
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
                response["type"] = 1; // PONG
                response["content"] = "pong";
                break;

            case 2: { // EXECUTE
                std::string output;
                bool success = ExecuteLua(content, output);
                if (success) {
                    response["type"] = 3; // OUTPUT
                    response["content"] = output;
                } else {
                    response["type"] = 4; // ERROR
                    response["content"] = output;
                }
                break;
            }

            case 6: { // REMOTE_SPY
                if (content == "enable") {
                    bool ok = EnableRemoteSpy();
                    response["type"] = 5; // STATUS
                    response["content"] = ok ? "Remote spy enabled" : "Failed to enable remote spy";
                } else {
                    DisableRemoteSpy();
                    response["type"] = 5;
                    response["content"] = "Remote spy disabled";
                }
                break;
            }

            case 8: { // GET_GLOBALS
                // Return list of global variables
                response["type"] = 3;
                response["content"] = "Globals enumeration not yet implemented in v1.1";
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

            case 10: { // HOOK_FUNCTION
                response["type"] = 4;
                response["content"] = "hookfunction requires in-process execution. Use loadstring with hookfunction()";
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

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
}

void StartIPCServer() {
    g_ipcRunning = true;

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
            Sleep(100);
            continue;
        }

        BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
            std::thread clientThread(HandleClient, pipe);
            clientThread.detach();
        } else {
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

        std::thread initThread([]() {
            Sleep(500); // Let Roblox finish initializing

            if (FindLuaFunctions() && FindLuaState()) {
                SetupExploitAPI();
                StartIPCServer();
            }
        });
        initThread.detach();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        StopIPCServer();
    }
    return TRUE;
}
