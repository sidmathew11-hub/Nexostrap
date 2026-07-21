// NexoCore.dll - Injected stub for Nexo Executor v1.1
// ULTRA-MINIMAL DllMain to avoid deadlocks
// Built for LO. Two years.

#include <windows.h>

// ═══════════════════════════════════════════════════════════════════════
//  Globals (plain C, no STL)
// ═══════════════════════════════════════════════════════════════════════

static volatile LONG g_initialized = 0;
static HANDLE g_ipcThread = NULL;

// ═══════════════════════════════════════════════════════════════════════
//  IPC Thread (runs after DllMain returns)
// ═══════════════════════════════════════════════════════════════════════

DWORD WINAPI IPCThreadProc(LPVOID lpParam) {
    // Wait for process to stabilize
    Sleep(3000);

    // Create named pipe
    HANDLE pipe = CreateNamedPipeA(
        "\\\\.\\pipe\\NexoIPC",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1, // Only 1 instance
        4096, 4096,
        0,
        NULL
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        return 1;
    }

    // Wait for client connection
    BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!connected) {
        CloseHandle(pipe);
        return 1;
    }

    // Simple echo loop for v1.1 testing
    char buffer[4096];
    DWORD read = 0;
    DWORD written = 0;

    while (ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, NULL) && read > 0) {
        buffer[read] = 0;

        // Echo back as output
        char response[4096];
        int len = wsprintfA(response, "{\"type\":3,\"content\":\"Received: %s\",\"id\":1}", buffer);
        DWORD respLen = (DWORD)len;

        WriteFile(pipe, &respLen, sizeof(respLen), &written, NULL);
        WriteFile(pipe, response, respLen, &written, NULL);
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  DllMain - ABSOLUTELY MINIMAL
// ═══════════════════════════════════════════════════════════════════════

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        // Use ONLY Windows API - NO C++ STL, NO file I/O
        // Create thread with raw Windows API, NOT std::thread
        g_ipcThread = CreateThread(NULL, 0, IPCThreadProc, NULL, 0, NULL);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_ipcThread) {
            TerminateThread(g_ipcThread, 0);
            CloseHandle(g_ipcThread);
        }
    }
    return TRUE;
}
