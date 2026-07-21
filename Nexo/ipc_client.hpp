#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace nexo {

enum class NexoIPCType : int {
    IPC_PING = 0,
    IPC_PONG = 1,
    IPC_EXECUTE = 2,
    IPC_OUTPUT = 3,
    IPC_ERROR = 4,
    IPC_STATUS_MSG = 5,
    IPC_REMOTE_SPY_LOG = 6,
    IPC_SET_CLIPBOARD = 9,
    IPC_HOOK_FUNCTION = 10
};

struct IPCMessage {
    NexoIPCType type;
    std::string content;
    uint32_t id;
    
    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["type"] = static_cast<int>(type);
        j["content"] = content;
        j["id"] = id;
        return j;
    }
    
    static IPCMessage FromJson(const nlohmann::json& j) {
        IPCMessage msg;
        msg.type = static_cast<NexoIPCType>(j.value("type", 0));
        msg.content = j.value("content", "");
        msg.id = j.value("id", 0);
        return msg;
    }
};

class IPCClient {
public:
    IPCClient() : connected_(false), pipe_(INVALID_HANDLE_VALUE) {}
    
    ~IPCClient() {
        Disconnect();
    }

    bool Connect(const std::string& pipeName = "NexoIPC", int retries = 30) {
        pipeName_ = "\\\\.\\pipe\\" + pipeName;
        
        for (int i = 0; i < retries; i++) {
            pipe_ = CreateFileA(
                pipeName_.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED,
                NULL
            );
            
            if (pipe_ != INVALID_HANDLE_VALUE) {
                DWORD mode = PIPE_READMODE_MESSAGE;
                SetNamedPipeHandleState(pipe_, &mode, NULL, NULL);
                connected_ = true;
                StartReadThread();
                return true;
            }
            
            if (GetLastError() == ERROR_PIPE_BUSY) {
                if (!WaitNamedPipeA(pipeName_.c_str(), 1000)) {
                    Sleep(100);
                }
            } else {
                Sleep(200);
            }
        }
        
        return false;
    }

    void Disconnect() {
        running_ = false;
        if (readThread_.joinable()) readThread_.join();
        if (pipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_);
            pipe_ = INVALID_HANDLE_VALUE;
        }
        connected_ = false;
    }

    bool IsConnected() const {
        return connected_;
    }

    bool Send(const IPCMessage& msg) {
        if (!connected_ || pipe_ == INVALID_HANDLE_VALUE) return false;
        
        auto jsonStr = msg.ToJson().dump();
        uint32_t len = static_cast<uint32_t>(jsonStr.length());
        
        DWORD written = 0;
        if (!WriteFile(pipe_, &len, sizeof(len), &written, NULL) || written != sizeof(len))
            return false;
        
        if (!WriteFile(pipe_, jsonStr.c_str(), len, &written, NULL) || written != len)
            return false;
        
        return true;
    }

    bool Execute(const std::string& script, uint32_t id) {
        IPCMessage msg;
        msg.type = NexoIPCType::IPC_EXECUTE;
        msg.content = script;
        msg.id = id;
        return Send(msg);
    }

    bool HasMessage() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        return !recvQueue_.empty();
    }

    IPCMessage PopMessage() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        IPCMessage msg = recvQueue_.front();
        recvQueue_.pop();
        return msg;
    }

private:
    std::string pipeName_;
    HANDLE pipe_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread readThread_;
    std::queue<IPCMessage> recvQueue_;
    std::mutex queueMutex_;

    void StartReadThread() {
        running_ = true;
        readThread_ = std::thread(&IPCClient::ReadLoop, this);
    }

    void ReadLoop() {
        while (running_) {
            uint32_t len = 0;
            DWORD read = 0;
            
            if (!ReadFile(pipe_, &len, sizeof(len), &read, NULL) || read != sizeof(len)) {
                if (GetLastError() == ERROR_BROKEN_PIPE || GetLastError() == ERROR_PIPE_NOT_CONNECTED) {
                    connected_ = false;
                    break;
                }
                continue;
            }
            
            if (len == 0 || len > 1024 * 1024) continue;
            
            std::vector<char> buf(len + 1, 0);
            if (!ReadFile(pipe_, buf.data(), len, &read, NULL) || read != len) {
                continue;
            }
            
            try {
                auto j = nlohmann::json::parse(buf.data());
                IPCMessage msg = IPCMessage::FromJson(j);
                
                std::lock_guard<std::mutex> lock(queueMutex_);
                recvQueue_.push(msg);
            } catch (...) {
                // Invalid JSON, ignore
            }
        }
    }
};

} // namespace nexo
