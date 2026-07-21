#include "ui.hpp"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <tchar.h>
#include <windows.h>
#include <chrono>
#include <fstream>
#include <sstream>

// Forward declare message handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace nexo {

// D3D11 globals
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, 
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, 
        &g_pd3dDeviceContext) != S_OK)
        return false;

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
    return true;
}

static void CleanupDeviceD3D() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

UI::UI() {}
UI::~UI() { Shutdown(); }

bool UI::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    if (!CreateDeviceD3D(hwnd)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ApplyTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    NewTab("Script 1");

    initialized_ = true;
    return true;
}

void UI::Shutdown() {
    if (!initialized_) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    initialized_ = false;
}

void UI::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Deep black base
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.055f, 0.055f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.055f, 0.055f, 0.055f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.055f, 0.055f, 0.055f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.055f, 0.055f, 0.055f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(accentColor_[0] * 1.2f, accentColor_[1] * 1.2f, accentColor_[2] * 1.2f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(accentColor_[0] * 0.3f, accentColor_[1] * 0.3f, accentColor_[2] * 0.3f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(accentColor_[0] * 0.5f, accentColor_[1] * 0.5f, accentColor_[2] * 0.5f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(accentColor_[0] * 0.25f, accentColor_[1] * 0.25f, accentColor_[2] * 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(accentColor_[0] * 0.4f, accentColor_[1] * 0.4f, accentColor_[2] * 0.4f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(accentColor_[0] * 0.25f, accentColor_[1] * 0.25f, accentColor_[2] * 0.25f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(accentColor_[0] * 0.4f, accentColor_[1] * 0.4f, accentColor_[2] * 0.4f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.055f, 0.055f, 0.055f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(accentColor_[0] * 0.5f, accentColor_[1] * 0.5f, accentColor_[2] * 0.5f, 0.35f);
    colors[ImGuiCol_NavHighlight] = ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.00f);

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(6, 4);
}

void UI::ProcessMessage(MSG& msg) {
    if (!initialized_) return;
    ImGui_ImplWin32_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam);
}

void UI::Render() {
    if (!initialized_) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("NexoDockSpace", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("NexoDockspace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    RenderTitleBar();
    ImGui::End();

    RenderSidebar();
    RenderMainArea();

    ImGui::Render();
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    const float clearColor[4] = { 0.055f, 0.055f, 0.055f, 1.00f };
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    g_pSwapChain->Present(1, 0);
}

void UI::RenderTitleBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    if (ImGui::BeginMenuBar()) {
        // Logo
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.0f));
        ImGui::Text("N");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("Nexo Executor");
        ImGui::SameLine();
        ImGui::TextDisabled("1.1.0");

        ImGui::SameLine(ImGui::GetWindowWidth() - 140);

        // Status dot
        if (isInjected_) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "\xE2\x97\x8F");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Injected");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "\xE2\x97\x8F");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Not Injected");
        }

        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar();
}

void UI::RenderSidebar() {
    ImGui::SetNextWindowSize(ImVec2(50, 0), ImGuiCond_Always);
    ImGui::Begin("Sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    auto ButtonIcon = [&](const char* label, SidebarItem item, const char* tooltip) {
        bool active = activeSidebar_ == item;

        ImVec4 bgColor = active ?
            ImVec4(accentColor_[0] * 0.25f, accentColor_[1] * 0.25f, accentColor_[2] * 0.25f, 1.0f) :
            ImVec4(0, 0, 0, 0);
        ImVec4 textColor = active ?
            ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.0f) :
            ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(accentColor_[0] * 0.15f, accentColor_[1] * 0.15f, accentColor_[2] * 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(accentColor_[0] * 0.3f, accentColor_[1] * 0.3f, accentColor_[2] * 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        if (ImGui::Button(label, ImVec2(36, 36))) {
            activeSidebar_ = item;
        }

        if (ImGui::IsItemHovered()) ImGui::SetTooltip(tooltip);

        ImGui::PopStyleColor(4);
    };

    // Icons using Unicode box drawing / symbols
    ButtonIcon("\xE2\x8C\x98", SidebarItem::Explorer, "Explorer");  // ⌘
    ButtonIcon("\xE2\x8C\xA8", SidebarItem::Scripts, "Scripts");     // ⌨
    ButtonIcon("\xE2\x8E\x99", SidebarItem::Output, "Output");        // ⎙
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 8));
    ButtonIcon("\xE2\x9A\x99", SidebarItem::Settings, "Settings");    // ⚙

    ImGui::End();
}

void UI::RenderMainArea() {
    switch (activeSidebar_) {
    case SidebarItem::Explorer:
        RenderExplorerPanel();
        break;
    case SidebarItem::Scripts:
        RenderScriptEditor();
        break;
    case SidebarItem::Output:
        RenderOutputConsole();
        break;
    case SidebarItem::Settings:
        RenderSettingsPanel();
        break;
    }
}

void UI::RenderScriptEditor() {
    ImGui::Begin("Scripts", nullptr, ImGuiWindowFlags_NoCollapse);

    // Tab bar
    if (ImGui::BeginTabBar("ScriptTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (int i = 0; i < (int)tabs_.size(); i++) {
            if (!tabs_[i].open) continue;

            bool open = true;
            ImGuiTabItemFlags flags = 0;
            if (tabs_[i].modified) flags |= ImGuiTabItemFlags_UnsavedDocument;

            if (ImGui::BeginTabItem(tabs_[i].name.c_str(), &open, flags)) {
                currentTab_ = i;

                // Editor
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.04f, 0.04f, 1.0f));
                ImGui::InputTextMultiline("##editor", editorBuffer_, sizeof(editorBuffer_),
                    ImVec2(-1, -40),
                    ImGuiInputTextFlags_AllowTabInput);
                ImGui::PopStyleColor();

                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    tabs_[i].content = editorBuffer_;
                    tabs_[i].modified = true;
                }

                ImGui::EndTabItem();
            }

            if (!open) {
                tabs_[i].open = false;
                if (currentTab_ == i) currentTab_ = -1;
            }
        }

        // New tab button
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
            if (onNewTab) onNewTab();
        }

        ImGui::EndTabBar();
    }

    // Bottom action bar
    ImGui::Separator();
    RenderBottomBar();

    ImGui::End();
}

void UI::RenderBottomBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));

    if (ImGui::Button("\xE2\x96\xB6 Execute", ImVec2(90, 28))) {
        if (onExecute) onExecute();
    }
    ImGui::SameLine();

    if (ImGui::Button("\xE2\x9C\x98 Clear", ImVec2(70, 28))) {
        if (onClear) onClear();
    }
    ImGui::SameLine();

    if (ImGui::Button("\xE2\x8F\xB5 Open", ImVec2(60, 28))) {
        if (onOpen) onOpen();
    }
    ImGui::SameLine();

    if (ImGui::Button("\xE2\x8F\xB7 Save", ImVec2(60, 28))) {
        if (onSave) onSave();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 100);

    ImVec4 injectColor = isInjected_ ?
        ImVec4(0.2f, 1.0f, 0.2f, 1.0f) :
        ImVec4(accentColor_[0], accentColor_[1], accentColor_[2], 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, injectColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
        ImVec4(injectColor.x * 1.2f, injectColor.y * 1.2f, injectColor.z * 1.2f, 1.0f));
    if (ImGui::Button("\xE2\x9A\xA1 Inject", ImVec2(85, 28))) {
        if (onInject) onInject();
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();
}

void UI::RenderOutputConsole() {
    ImGui::Begin("Output", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::BeginChild("OutputScroll", ImVec2(0, -35), true);

    for (const auto& entry : outputEntries_) {
        ImVec4 color(
            ((entry.color >> 16) & 0xFF) / 255.0f,
            ((entry.color >> 8) & 0xFF) / 255.0f,
            (entry.color & 0xFF) / 255.0f,
            ((entry.color >> 24) & 0xFF) / 255.0f
        );
        ImGui::TextColored(color, "%s", entry.text.c_str());
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::Separator();

    // Output controls
    if (ImGui::Button("\xE2\x8C\x98 Copy", ImVec2(60, 24))) {
        std::string all;
        for (const auto& e : outputEntries_) all += e.text + "\n";
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, all.length() + 1);
            if (hMem) {
                memcpy(GlobalLock(hMem), all.c_str(), all.length() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            CloseClipboard();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("\xE2\x9C\x98 Clear", ImVec2(60, 24))) {
        ClearOutput();
    }
    ImGui::SameLine();
    if (ImGui::Button("\xE2\x8F\xB7 Save Log", ImVec2(80, 24))) {
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&ofn)) {
            std::ofstream file(filename);
            if (file.is_open()) {
                for (const auto& e : outputEntries_) file << e.text << "\n";
                file.close();
            }
        }
    }

    ImGui::End();
}

void UI::RenderExplorerPanel() {
    ImGui::Begin("Explorer", nullptr, ImGuiWindowFlags_NoCollapse);

    static char searchBuf[256] = {};
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.06f, 0.06f, 1.0f));
    ImGui::InputTextWithHint("##search", "Search Explorer..", searchBuf, sizeof(searchBuf));
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (ImGui::Button("\xE2\x86\xBB")) { /* Refresh */ }

    ImGui::BeginChild("ExplorerTree", ImVec2(0, 0), true);

    if (ImGui::TreeNode("Workspace")) {
        ImGui::TextDisabled("  (instance tree not yet populated)");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("ReplicatedStorage")) {
        ImGui::TextDisabled("  (instance tree not yet populated)");
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Players")) {
        ImGui::TextDisabled("  (instance tree not yet populated)");
        ImGui::TreePop();
    }

    ImGui::EndChild();

    ImGui::End();
}

void UI::RenderSettingsPanel() {
    ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Text("Appearance");
    ImGui::Separator();

    ImGui::Text("Accent Color");
    if (ImGui::ColorEdit3("##accent", accentColor_, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        ApplyTheme();
    }

    ImGui::Dummy(ImVec2(0, 8));
    ImGui::Checkbox("Auto-update offsets on launch", &autoUpdate_);

    ImGui::Dummy(ImVec2(0, 16));
    ImGui::Text("Status");
    ImGui::Separator();
    ImGui::Text("Nexo Executor v1.1.0");
    ImGui::Text("Offset source: offsets.imtheo.lol");
    ImGui::Text("Roblox version: %s", robloxVersion_.c_str());
    ImGui::Text("Offset version: %s", offsetVersion_.c_str());

    ImGui::Dummy(ImVec2(0, 16));
    ImGui::Text("Built for LO. Two years.");

    ImGui::End();
}

void UI::NewTab(const std::string& name) {
    ScriptTab tab;
    tab.name = name;
    tab.open = true;
    tabs_.push_back(tab);
    currentTab_ = (int)tabs_.size() - 1;
    memset(editorBuffer_, 0, sizeof(editorBuffer_));
}

void UI::CloseTab(int index) {
    if (index >= 0 && index < (int)tabs_.size()) {
        tabs_[index].open = false;
    }
}

void UI::SetTabContent(int index, const std::string& content) {
    if (index >= 0 && index < (int)tabs_.size()) {
        tabs_[index].content = content;
        if (index == currentTab_) {
            strncpy_s(editorBuffer_, content.c_str(), sizeof(editorBuffer_) - 1);
        }
    }
}

std::string UI::GetCurrentScript() const {
    if (currentTab_ >= 0 && currentTab_ < (int)tabs_.size()) {
        return editorBuffer_;
    }
    return "";
}

void UI::AddOutput(const std::string& text, uint32_t color) {
    OutputEntry entry;
    entry.text = text;
    entry.color = color;
    entry.timestamp = (float)ImGui::GetTime();
    outputEntries_.push_back(entry);
}

void UI::ClearOutput() {
    outputEntries_.clear();
}

void UI::SetStatus(const std::string& status, uint32_t color) {
    statusText_ = status;
    statusColor_ = color;
}

} // namespace nexo
