// Apps/IDE/Debug/GDB/UCGDBProcess.cpp
// GDB Process Management Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGDBProcess.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <array>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#endif

namespace UltraCanvas {
namespace IDE {

UCGDBProcess::UCGDBProcess() = default;

UCGDBProcess::~UCGDBProcess() {
    Stop();
}

bool UCGDBProcess::Start(const std::string& gdbPath, const std::vector<std::string>& args) {
    if (running_) {
        return false;
    }
    
#ifdef _WIN32
    // Windows implementation
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE stdinRead, stdinWrite;
    HANDLE stdoutRead, stdoutWrite;
    HANDLE stderrRead, stderrWrite;
    
    if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0) ||
        !CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
        !CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
        return false;
    }
    
    // Don't inherit our end of the pipes
    SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);
    
    // Build command line
    std::string cmdLine = "\"" + gdbPath + "\" --interpreter=mi";
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stderrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    
    if (!CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()),
                        NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(stdinRead);
        CloseHandle(stdinWrite);
        CloseHandle(stdoutRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrRead);
        CloseHandle(stderrWrite);
        return false;
    }
    
    // Close child's ends
    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    CloseHandle(stderrWrite);
    
    processHandle_ = pi.hProcess;
    threadHandle_ = pi.hThread;
    stdinWrite_ = stdinWrite;
    stdoutRead_ = stdoutRead;
    stderrRead_ = stderrRead;
    pid_ = pi.dwProcessId;
    
#else
    // POSIX implementation
    int stdinPipe[2], stdoutPipe[2], stderrPipe[2];
    
    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0) {
        return false;
    }
    
    pid_ = fork();
    
    if (pid_ < 0) {
        // Fork failed
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stderrPipe[0]); close(stderrPipe[1]);
        return false;
    }
    
    if (pid_ == 0) {
        // Child process
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);
        
        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(gdbPath.c_str()));
        argv.push_back(const_cast<char*>("--interpreter=mi"));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        execvp(gdbPath.c_str(), argv.data());
        
        // If we get here, exec failed
        _exit(127);
    }
    
    // Parent process
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    
    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    stderrFd_ = stderrPipe[0];
    
    // Set stdout to non-blocking
    int flags = fcntl(stdoutFd_, F_GETFL, 0);
    fcntl(stdoutFd_, F_SETFL, flags | O_NONBLOCK);
    
    flags = fcntl(stderrFd_, F_GETFL, 0);
    fcntl(stderrFd_, F_SETFL, flags | O_NONBLOCK);
#endif
    
    running_ = true;
    stopThreads_ = false;
    
    // Start output reading threads
    readThread_ = std::thread(&UCGDBProcess::ReadThreadFunc, this);
    errorThread_ = std::thread(&UCGDBProcess::ErrorThreadFunc, this);
    
    return true;
}

void UCGDBProcess::Stop() {
    if (!running_) {
        return;
    }
    
    stopThreads_ = true;
    
#ifdef _WIN32
    if (processHandle_) {
        TerminateProcess(processHandle_, 0);
        WaitForSingleObject(processHandle_, 1000);
        CloseHandle(processHandle_);
        CloseHandle(threadHandle_);
        processHandle_ = nullptr;
        threadHandle_ = nullptr;
    }
    
    if (stdinWrite_) { CloseHandle(stdinWrite_); stdinWrite_ = nullptr; }
    if (stdoutRead_) { CloseHandle(stdoutRead_); stdoutRead_ = nullptr; }
    if (stderrRead_) { CloseHandle(stderrRead_); stderrRead_ = nullptr; }
#else
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        
        // Wait briefly for graceful termination
        int status;
        int waitResult = waitpid(pid_, &status, WNOHANG);
        if (waitResult == 0) {
            // Still running, wait a bit more
            usleep(100000); // 100ms
            waitResult = waitpid(pid_, &status, WNOHANG);
            if (waitResult == 0) {
                // Force kill
                kill(pid_, SIGKILL);
                waitpid(pid_, &status, 0);
            }
        }
        
        if (WIFEXITED(status)) {
            exitCode_ = WEXITSTATUS(status);
        }
        pid_ = -1;
    }
    
    if (stdinFd_ >= 0) { close(stdinFd_); stdinFd_ = -1; }
    if (stdoutFd_ >= 0) { close(stdoutFd_); stdoutFd_ = -1; }
    if (stderrFd_ >= 0) { close(stderrFd_); stderrFd_ = -1; }
#endif
    
    // Wait for threads
    if (readThread_.joinable()) {
        outputCv_.notify_all();
        readThread_.join();
    }
    if (errorThread_.joinable()) {
        errorThread_.join();
    }
    
    running_ = false;
    
    if (onExit) {
        onExit(exitCode_);
    }
}

bool UCGDBProcess::IsRunning() const {
    if (!running_) return false;
    
#ifdef _WIN32
    if (processHandle_) {
        DWORD exitCode;
        if (GetExitCodeProcess(processHandle_, &exitCode)) {
            return exitCode == STILL_ACTIVE;
        }
    }
    return false;
#else
    if (pid_ > 0) {
        int status;
        int result = waitpid(pid_, &status, WNOHANG);
        return result == 0;
    }
    return false;
#endif
}

int UCGDBProcess::GetExitCode() const {
    return exitCode_;
}

bool UCGDBProcess::SendCommand(const std::string& command) {
    return SendLine(command);
}

bool UCGDBProcess::SendLine(const std::string& line) {
    if (!running_) return false;
    
    std::string toSend = line + "\n";
    
#ifdef _WIN32
    DWORD written;
    return WriteFile(stdinWrite_, toSend.c_str(), 
                     static_cast<DWORD>(toSend.size()), &written, NULL) &&
           written == toSend.size();
#else
    ssize_t written = write(stdinFd_, toSend.c_str(), toSend.size());
    return written == static_cast<ssize_t>(toSend.size());
#endif
}

std::string UCGDBProcess::ReadLine(int timeoutMs) {
    std::unique_lock<std::mutex> lock(outputMutex_);
    
    if (outputQueue_.empty()) {
        if (timeoutMs < 0) {
            outputCv_.wait(lock, [this] { 
                return !outputQueue_.empty() || stopThreads_; 
            });
        } else if (timeoutMs > 0) {
            outputCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this] {
                return !outputQueue_.empty() || stopThreads_;
            });
        }
    }
    
    if (outputQueue_.empty()) {
        return "";
    }
    
    std::string line = outputQueue_.front();
    outputQueue_.pop();
    return line;
}

std::string UCGDBProcess::ReadAll() {
    std::lock_guard<std::mutex> lock(outputMutex_);
    
    std::string result;
    while (!outputQueue_.empty()) {
        result += outputQueue_.front() + "\n";
        outputQueue_.pop();
    }
    return result;
}

bool UCGDBProcess::HasOutput() const {
    std::lock_guard<std::mutex> lock(outputMutex_);
    return !outputQueue_.empty();
}

void UCGDBProcess::ReadThreadFunc() {
    std::string lineBuffer;
    std::array<char, 4096> buffer;
    
    while (!stopThreads_ && running_) {
#ifdef _WIN32
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(stdoutRead_, NULL, 0, NULL, &bytesAvailable, NULL)) {
            break;
        }
        
        if (bytesAvailable == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        DWORD bytesRead;
        DWORD toRead = std::min(bytesAvailable, static_cast<DWORD>(buffer.size() - 1));
        if (!ReadFile(stdoutRead_, buffer.data(), toRead, &bytesRead, NULL) || bytesRead == 0) {
            break;
        }
#else
        struct pollfd pfd;
        pfd.fd = stdoutFd_;
        pfd.events = POLLIN;
        
        int pollResult = poll(&pfd, 1, 100);
        if (pollResult <= 0) {
            continue;
        }
        
        ssize_t bytesRead = read(stdoutFd_, buffer.data(), buffer.size() - 1);
        if (bytesRead <= 0) {
            if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            break;
        }
#endif
        
        buffer[bytesRead] = '\0';
        lineBuffer += buffer.data();
        
        // Split into lines
        size_t pos;
        while ((pos = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, pos);
            lineBuffer = lineBuffer.substr(pos + 1);
            
            // Remove trailing CR
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            
            {
                std::lock_guard<std::mutex> lock(outputMutex_);
                outputQueue_.push(line);
            }
            outputCv_.notify_one();
            
            if (onStdout) {
                onStdout(line);
            }
        }
    }
}

void UCGDBProcess::ErrorThreadFunc() {
    std::array<char, 1024> buffer;
    
    while (!stopThreads_ && running_) {
#ifdef _WIN32
        DWORD bytesAvailable = 0;
        if (!PeekNamedPipe(stderrRead_, NULL, 0, NULL, &bytesAvailable, NULL)) {
            break;
        }
        
        if (bytesAvailable == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        
        DWORD bytesRead;
        if (!ReadFile(stderrRead_, buffer.data(), 
                      std::min(bytesAvailable, static_cast<DWORD>(buffer.size() - 1)),
                      &bytesRead, NULL) || bytesRead == 0) {
            break;
        }
#else
        struct pollfd pfd;
        pfd.fd = stderrFd_;
        pfd.events = POLLIN;
        
        int pollResult = poll(&pfd, 1, 100);
        if (pollResult <= 0) {
            continue;
        }
        
        ssize_t bytesRead = read(stderrFd_, buffer.data(), buffer.size() - 1);
        if (bytesRead <= 0) {
            if (bytesRead < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                continue;
            }
            break;
        }
#endif
        
        buffer[bytesRead] = '\0';
        std::string errOutput(buffer.data());
        
        if (onStderr) {
            onStderr(errOutput);
        }
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string FindGDBExecutable() {
    std::vector<std::string> searchPaths;
    
#ifdef _WIN32
    searchPaths = {
        "gdb.exe",
        "C:\\MinGW\\bin\\gdb.exe",
        "C:\\msys64\\mingw64\\bin\\gdb.exe",
        "C:\\Program Files\\GDB\\bin\\gdb.exe"
    };
#else
    searchPaths = {
        "/usr/bin/gdb",
        "/usr/local/bin/gdb",
        "/opt/local/bin/gdb",
        "gdb"
    };
#endif
    
    for (const auto& path : searchPaths) {
#ifdef _WIN32
        DWORD attrs = GetFileAttributesA(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
#else
        if (access(path.c_str(), X_OK) == 0) {
            return path;
        }
#endif
    }
    
    // Try to find in PATH
#ifndef _WIN32
    FILE* pipe = popen("which gdb 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            // Remove trailing newline
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
            pclose(pipe);
            if (!result.empty()) {
                return result;
            }
        }
        pclose(pipe);
    }
#endif
    
    return "";
}

std::string GetGDBVersion(const std::string& gdbPath) {
    if (gdbPath.empty()) return "";
    
    std::string command = "\"" + gdbPath + "\" --version";
    
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    
    if (!pipe) return "";
    
    char buffer[256];
    std::string result;
    
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        // Remove trailing newline
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    return result;
}

bool CheckGDBMISupport(const std::string& gdbPath) {
    std::string version = GetGDBVersion(gdbPath);
    // GDB has supported MI since version 5.0 (released 2000)
    // If we can get a version string, assume MI is supported
    return !version.empty();
}

} // namespace IDE
} // namespace UltraCanvas
