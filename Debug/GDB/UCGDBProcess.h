// Apps/IDE/Debug/GDB/UCGDBProcess.h
// GDB Process Management for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Cross-platform GDB process wrapper
 * 
 * Manages the GDB subprocess, handling stdin/stdout/stderr communication.
 */
class UCGDBProcess {
public:
    UCGDBProcess();
    ~UCGDBProcess();
    
    // Process lifecycle
    bool Start(const std::string& gdbPath, const std::vector<std::string>& args = {});
    void Stop();
    bool IsRunning() const;
    int GetExitCode() const;
    
    // Communication
    bool SendCommand(const std::string& command);
    bool SendLine(const std::string& line);
    std::string ReadLine(int timeoutMs = -1);
    std::string ReadAll();
    bool HasOutput() const;
    
    // Callbacks
    std::function<void(const std::string&)> onStdout;
    std::function<void(const std::string&)> onStderr;
    std::function<void(int)> onExit;
    
    // Process info
    int GetPid() const { return pid_; }
    
private:
    void ReadThreadFunc();
    void ErrorThreadFunc();
    
#ifdef _WIN32
    // Windows handles
    void* processHandle_ = nullptr;
    void* threadHandle_ = nullptr;
    void* stdinWrite_ = nullptr;
    void* stdoutRead_ = nullptr;
    void* stderrRead_ = nullptr;
#else
    // POSIX file descriptors
    int stdinFd_ = -1;
    int stdoutFd_ = -1;
    int stderrFd_ = -1;
#endif
    
    int pid_ = -1;
    std::atomic<bool> running_{false};
    std::atomic<int> exitCode_{-1};
    
    // Output buffering
    std::queue<std::string> outputQueue_;
    std::mutex outputMutex_;
    std::condition_variable outputCv_;
    
    // Threads
    std::thread readThread_;
    std::thread errorThread_;
    std::atomic<bool> stopThreads_{false};
};

/**
 * @brief Find GDB executable on the system
 */
std::string FindGDBExecutable();

/**
 * @brief Get GDB version string
 */
std::string GetGDBVersion(const std::string& gdbPath);

/**
 * @brief Check if GDB supports MI interface
 */
bool CheckGDBMISupport(const std::string& gdbPath);

} // namespace IDE
} // namespace UltraCanvas
