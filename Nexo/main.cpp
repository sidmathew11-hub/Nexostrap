#include <windows.h>
#include <string>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>

#include "ui.hpp"
#include "memory.hpp"
#include "offsets.hpp"
#include "injector.hpp"
#include "ipc_client.hpp"

// ImGui Win32 message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace nexo {

std::unique_ptr<UI> g_ui;
std::unique_ptr<Offsets> g_offsets;
std::unique_ptr<IPCClient> g_ipc;
std::unique_ptr<Injector> g_injector;

DWORD g_robloxPid = 0;
HANDLE g_robloxHandle = NULL;
uintptr_t g_robloxBase = 0;
bool g_injected = false;
static int g_tabCounter = 2;

// Forward declarations
bool FindRoblox();
bool Inject();
void ExecuteScript();
void UpdateOffsets();
void CheckAutoUpdate();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool CreateWindowClass(HINSTANCE hInstance, WNDCLASSEXW& wc) {
    wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"NexoExecutor";
    return RegisterClassExW(&wc) != 0;
}

void CheckAutoUpdate() {
    if (!g_offsets) return;

    if (g_offsets->NeedsUpdate()) {
        g_ui->AddOutput("[Nexo] Checking for offset updates...", 0xFFAAAAAA);
        if (g_offsets->FetchLatest()) {
            g_ui->SetOffsetVersion(g_offsets->GetRobloxVersion());
            g_ui->AddOutput("[Nexo] Offsets updated to: " + g_offsets->GetRobloxVersion(), 0xFF00FF00);
        } else {
            g_ui->AddOutput("[Nexo] Offset update check failed, using cached.", 0xFFFFAA00);
        }
    }
}

int MainLoop() {
    WNDCLASSEXW wc;
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    if (!CreateWindowClass(hInstance, wc)) {
        MessageBoxA(nullptr, "Failed to register window class", "Nexo Error", MB_OK);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0, L"NexoExecutor", L"Nexo Executor 1.1.0",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 950, 650,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        MessageBoxA(nullptr, "Failed to create window", "Nexo Error", MB_OK);
        return 1;
    }

    // Initialize UI
    g_ui = std::make_unique<UI>();

    // Set callbacks - capture hwnd by reference
    g_ui->onInject = []() { Inject(); };
    g_ui->onExecute = []() { ExecuteScript(); };
    g_ui->onClear = []() { g_ui->ClearOutput(); };
    g_ui->onOpen = [&hwnd]() {
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = "Lua Scripts\0*.lua\0Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        if (GetOpenFileNameA(&ofn)) {
            std::ifstream file(filename);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
                g_ui->SetTabContent(g_ui->GetCurrentTab(), content);
                file.close();
                g_ui->AddOutput(std::string("[Nexo] Opened: ") + filename, 0xFFAAAAAA);
            }
        }
    };
    g_ui->onSave = [&hwnd]() {
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = "Lua Scripts\0*.lua\0Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn)) {
            std::ofstream file(filename);
            if (file.is_open()) {
                file << g_ui->GetCurrentScript();
                file.close();
                g_ui->AddOutput(std::string("[Nexo] Saved: ") + filename, 0xFFAAAAAA);
            }
        }
    };
    g_ui->onNewTab = []() {
        g_ui->NewTab("Script " + std::to_string(g_tabCounter++));
    };

    if (!g_ui->Initialize(hwnd)) {
        MessageBoxA(nullptr, "Failed to initialize UI", "Nexo Error", MB_OK);
        return 1;
    }

    // Initialize offsets
    g_offsets = std::make_unique<Offsets>();
    g_ui->AddOutput("[Nexo] Initializing offset manager...", 0xFFAAAAAA);

    if (!g_offsets->Initialize()) {
        g_ui->AddOutput("[Nexo] Warning: Failed to load offsets. Injection may fail.", 0xFFFFAA00);
    } else {
        g_ui->SetOffsetVersion(g_offsets->GetRobloxVersion());
        g_ui->AddOutput("[Nexo] Offsets loaded: " + g_offsets->GetRobloxVersion(), 0xFF00FF00);
        g_ui->AddOutput("[Nexo] Total offsets: " + std::to_string(g_offsets->GetTotalOffsets()), 0xFFAAAAAA);
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    auto lastUpdateCheck = std::chrono::steady_clock::now();

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            g_ui->ProcessMessage(msg);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Periodic auto-update check (every 5 minutes)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::minutes>(now - lastUpdateCheck).count() >= 5) {
            CheckAutoUpdate();
            lastUpdateCheck = now;
        }

        // Check IPC messages
        if (g_ipc && g_ipc->HasMessage()) {
            auto response = g_ipc->PopMessage();
            switch (response.type) {
            case NexoIPCType::IPC_OUTPUT:
                g_ui->AddOutput(response.content, 0xFFFFFFFF);
                break;
            case NexoIPCType::IPC_ERROR:
                g_ui->AddOutput("[Error] " + response.content, 0xFFFF5555);
                break;
            case NexoIPCType::IPC_STATUS_MSG:
                g_ui->AddOutput("[Status] " + response.content, 0xFF55AAFF);
                break;
            case NexoIPCType::IPC_REMOTE_SPY_LOG:
                g_ui->AddOutput("[RemoteSpy] " + response.content, 0xFFFFAA00);
                break;
            default:
                break;
            }
        }

        g_ui->Render();
    }

    // Cleanup
    if (g_ipc) g_ipc->Disconnect();
    if (g_robloxHandle) CloseHandle(g_robloxHandle);
    g_ui->Shutdown();
    DestroyWindow(hwnd);
    UnregisterClassW(L"NexoExecutor", hInstance);

    return 0;
}

bool FindRoblox() {
    g_robloxPid = Memory::GetProcessIdByName(L"RobloxPlayerBeta.exe");
    if (!g_robloxPid) {
        g_ui->AddOutput("[Nexo] Roblox not found. Launch Roblox first.", 0xFFFF5555);
        return false;
    }

    g_robloxHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_robloxPid);
    if (!g_robloxHandle) {
        g_ui->AddOutput("[Nexo] Failed to open Roblox process. Run as Administrator?", 0xFFFF5555);
        return false;
    }

    g_robloxBase = Memory::GetModuleBase(g_robloxPid, L"RobloxPlayerBeta.exe");
    if (!g_robloxBase) {
        g_ui->AddOutput("[Nexo] Failed to find Roblox module base.", 0xFFFF5555);
        CloseHandle(g_robloxHandle);
        g_robloxHandle = NULL;
        return false;
    }

    g_ui->SetRobloxVersion("PID: " + std::to_string(g_robloxPid));
    return true;
}

bool Inject() {
    if (g_injected) {
        g_ui->AddOutput("[Nexo] Already injected.", 0xFFFFAA00);
        return true;
    }

    if (!FindRoblox()) return false;

    g_ui->AddOutput("[Nexo] Injecting into Roblox (PID: " + std::to_string(g_robloxPid) + ")...", 0xFFAAAAAA);

    // Build path to NexoCore.dll
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    exeDir = exeDir.substr(0, lastSlash);
    std::string dllPath = exeDir + "\\NexoCore.dll";

    DWORD attr = GetFileAttributesA(dllPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        g_ui->AddOutput("[Nexo] NexoCore.dll not found at: " + dllPath, 0xFFFF5555);
        return false;
    }

    g_injector = std::make_unique<Injector>();
    if (!g_injector->ManualMap(g_robloxHandle, dllPath)) {
        g_ui->AddOutput("[Nexo] Injection failed. Possible causes:", 0xFFFF5555);
        g_ui->AddOutput("  - Not running as Administrator", 0xFFFF5555);
        g_ui->AddOutput("  - Roblox anti-tamper blocked injection", 0xFFFF5555);
        g_ui->AddOutput("  - DLL is incompatible with current Roblox version", 0xFFFF5555);
        return false;
    }

    g_ui->AddOutput("[Nexo] DLL injected. Waiting for IPC...", 0xFFAAAAAA);

    // Connect IPC (with retries)
    g_ipc = std::make_unique<IPCClient>();
    if (!g_ipc->Connect("NexoIPC", 50)) {
        g_ui->AddOutput("[Nexo] IPC connection failed. DLL may not have initialized.", 0xFFFF5555);
        return false;
    }

    g_ui->AddOutput("[Nexo] Connected to Roblox Lua state.", 0xFF00FF00);
    g_ui->SetInjected(true);
    g_injected = true;

    return true;
}

void ExecuteScript() {
    if (!g_injected || !g_ipc || !g_ipc->IsConnected()) {
        g_ui->AddOutput("[Nexo] Not injected. Click Inject first.", 0xFFFF5555);
        return;
    }

    std::string script = g_ui->GetCurrentScript();
    if (script.empty()) {
        g_ui->AddOutput("[Nexo] Script is empty.", 0xFFFFAA00);
        return;
    }

    static uint32_t execId = 1;
    if (!g_ipc->Execute(script, execId++)) {
        g_ui->AddOutput("[Nexo] Failed to send script to injector.", 0xFFFF5555);
    } else {
        g_ui->AddOutput("[Nexo] Script sent. Waiting for output...", 0xFFAAAAAA);
    }
}

} // namespace nexo

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Check administrator privileges
    BOOL isAdmin = FALSE;
    PSID administratorsGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administratorsGroup)) {
        CheckTokenMembership(NULL, administratorsGroup, &isAdmin);
        FreeSid(administratorsGroup);
    }

    if (!isAdmin) {
        MessageBoxA(nullptr,
            "Nexo Executor requires Administrator privileges.\n"
            "Please right-click Nexo.exe and select 'Run as administrator'.",
            "Nexo - Administrator Required", MB_OK | MB_ICONWARNING);
        return 1;
    }

    return nexo::MainLoop();
}
