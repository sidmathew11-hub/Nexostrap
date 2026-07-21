#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace nexo {

struct ScriptTab {
    std::string name;
    std::string content;
    bool modified = false;
    bool open = true;
};

struct OutputEntry {
    std::string text;
    uint32_t color; // RGBA
    float timestamp;
};

enum class SidebarItem {
    Explorer,
    Scripts,
    Output,
    Settings
};

class UI {
public:
    UI();
    ~UI();

    bool Initialize(HWND hwnd);
    void Shutdown();
    void Render();
    void ProcessMessage(MSG& msg);

    bool ShouldClose() const { return shouldClose_; }

    // Script management
    void NewTab(const std::string& name = "Untitled");
    void CloseTab(int index);
    void SetTabContent(int index, const std::string& content);
    std::string GetCurrentScript() const;
    int GetCurrentTab() const { return currentTab_; }

    // Output
    void AddOutput(const std::string& text, uint32_t color = 0xFFFFFFFF);
    void ClearOutput();

    // Status
    void SetStatus(const std::string& status, uint32_t color = 0xFFFFFFFF);
    void SetInjected(bool injected) { isInjected_ = injected; }
    void SetRobloxVersion(const std::string& ver) { robloxVersion_ = ver; }
    void SetOffsetVersion(const std::string& ver) { offsetVersion_ = ver; }

    // Callbacks (set by main)
    std::function<void()> onInject;
    std::function<void()> onExecute;
    std::function<void()> onClear;
    std::function<void()> onOpen;
    std::function<void()> onSave;
    std::function<void()> onNewTab;

    // Settings
    float accentColor_[3] = {0.35f, 0.55f, 1.0f}; // Default blue accent
    bool autoUpdate_ = true;

private:
    void RenderSidebar();
    void RenderMainArea();
    void RenderScriptEditor();
    void RenderOutputConsole();
    void RenderBottomBar();
    void RenderTitleBar();
    void RenderSettingsPanel();
    void RenderExplorerPanel();
    void ApplyTheme();

    HWND hwnd_ = NULL;
    bool shouldClose_ = false;
    bool initialized_ = false;

    // UI State
    SidebarItem activeSidebar_ = SidebarItem::Scripts;
    std::vector<ScriptTab> tabs_;
    int currentTab_ = -1;
    std::vector<OutputEntry> outputEntries_;
    std::string statusText_ = "Ready";
    uint32_t statusColor_ = 0xFFAAAAAA;
    bool isInjected_ = false;
    std::string robloxVersion_ = "Unknown";
    std::string offsetVersion_ = "Unknown";

    // Editor state
    char editorBuffer_[1024 * 256] = {}; // 256KB max per tab
    bool editorModified_ = false;
};

} // namespace nexo
