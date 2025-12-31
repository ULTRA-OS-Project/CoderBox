// Apps/IDE/Debug/LLDB/UCLLDBPlugin.cpp
// LLDB Debugger Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCLLDBPlugin.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// CONFIGURATION
// ============================================================================

LLDBPluginConfig LLDBPluginConfig::Default() {
    LLDBPluginConfig config;
    config.commandTimeout = 5000;
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCLLDBPlugin::UCLLDBPlugin() : UCLLDBPlugin(LLDBPluginConfig::Default()) {}

UCLLDBPlugin::UCLLDBPlugin(const LLDBPluginConfig& config) : config_(config) {
    if (config_.lldbPath.empty()) {
        lldbPath_ = FindLLDBExecutable();
    } else {
        lldbPath_ = config_.lldbPath;
    }
    
    available_ = !lldbPath_.empty();
    if (available_) {
        lldbVersion_ = GetLLDBVersion(lldbPath_);
    }
}

UCLLDBPlugin::~UCLLDBPlugin() {
    Terminate();
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCLLDBPlugin::GetPluginName() const { return "LLDB Debugger Plugin"; }
std::string UCLLDBPlugin::GetPluginVersion() const { return "1.0.0"; }
DebuggerType UCLLDBPlugin::GetDebuggerType() const { return DebuggerType::LLDB; }
std::string UCLLDBPlugin::GetDebuggerName() const { return "LLDB"; }

// ============================================================================
// DEBUGGER DETECTION
// ============================================================================

bool UCLLDBPlugin::IsAvailable() { return available_; }
std::string UCLLDBPlugin::GetDebuggerPath() { return lldbPath_; }

void UCLLDBPlugin::SetDebuggerPath(const std::string& path) {
    lldbPath_ = path;
    available_ = !lldbPath_.empty();
    if (available_) lldbVersion_ = GetLLDBVersion(lldbPath_);
}

std::string UCLLDBPlugin::GetDebuggerVersion() { return lldbVersion_; }

std::vector<std::string> UCLLDBPlugin::GetSupportedExtensions() const {
    return { "", ".exe", ".out", ".app", ".dylib", ".so" };
}

// ============================================================================
// SESSION CONTROL
// ============================================================================

bool UCLLDBPlugin::Launch(const DebugLaunchConfig& config) {
    if (state_ != DebugSessionState::Inactive) return false;
    if (!available_) {
        if (onError) onError("LLDB not available");
        return false;
    }
    
    SetState(DebugSessionState::Starting);
    
#ifndef _WIN32
    int stdinPipe[2], stdoutPipe[2];
    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0) {
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    pid_ = fork();
    if (pid_ < 0) {
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    if (pid_ == 0) {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        
        execlp(lldbPath_.c_str(), lldbPath_.c_str(), nullptr);
        _exit(127);
    }
    
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    
    fcntl(stdoutFd_, F_SETFL, fcntl(stdoutFd_, F_GETFL) | O_NONBLOCK);
#endif
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Load executable
    if (!config.executablePath.empty()) {
        std::string cmd = "file \"" + config.executablePath + "\"";
        std::string result = SendCommand(cmd);
        if (result.find("error") != std::string::npos) {
            Terminate();
            if (onError) onError("Failed to load executable");
            return false;
        }
    }
    
    // Set working directory
    if (!config.workingDirectory.empty()) {
        SendCommand("settings set target.process.working-directory \"" + 
                    config.workingDirectory + "\"");
    }
    
    // Set arguments
    if (!config.arguments.empty()) {
        std::ostringstream args;
        args << "settings set target.run-args";
        for (const auto& arg : config.arguments) {
            args << " \"" << arg << "\"";
        }
        SendCommand(args.str());
    }
    
    // Set stop on entry
    if (config.stopOnEntry) {
        SendCommand("breakpoint set --name main");
    }
    
    // Run
    std::string runResult = SendCommand("run");
    if (runResult.find("error") != std::string::npos) {
        if (onError) onError("Failed to run program");
        return false;
    }
    
    SetState(DebugSessionState::Running);
    sessionActive = true;
    
    stopOutputThread_ = false;
    outputThread_ = std::thread([this]() {
        while (!stopOutputThread_) {
            std::string output = SendCommand("");
            if (!output.empty()) {
                ProcessOutput(output);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    
    return true;
}

bool UCLLDBPlugin::Attach(int pid) {
    if (state_ != DebugSessionState::Inactive) return false;
    
    SetState(DebugSessionState::Starting);
    
    // Start LLDB process (similar to Launch)
#ifndef _WIN32
    int stdinPipe[2], stdoutPipe[2];
    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0) {
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    pid_ = fork();
    if (pid_ == 0) {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        execlp(lldbPath_.c_str(), lldbPath_.c_str(), nullptr);
        _exit(127);
    }
    
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    fcntl(stdoutFd_, F_SETFL, fcntl(stdoutFd_, F_GETFL) | O_NONBLOCK);
#endif
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::string result = SendCommand("attach -p " + std::to_string(pid));
    if (result.find("error") != std::string::npos) {
        Terminate();
        if (onError) onError("Failed to attach");
        return false;
    }
    
    SetState(DebugSessionState::Paused);
    sessionActive = true;
    return true;
}

bool UCLLDBPlugin::AttachByName(const std::string& processName) {
    if (state_ != DebugSessionState::Inactive) return false;
    
    // Similar setup as Attach
    SetState(DebugSessionState::Starting);
    
#ifndef _WIN32
    int stdinPipe[2], stdoutPipe[2];
    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0) {
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    pid_ = fork();
    if (pid_ == 0) {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        execlp(lldbPath_.c_str(), lldbPath_.c_str(), nullptr);
        _exit(127);
    }
    
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    fcntl(stdoutFd_, F_SETFL, fcntl(stdoutFd_, F_GETFL) | O_NONBLOCK);
#endif
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    std::string result = SendCommand("attach -n \"" + processName + "\"");
    if (result.find("error") != std::string::npos) {
        Terminate();
        return false;
    }
    
    SetState(DebugSessionState::Paused);
    sessionActive = true;
    return true;
}

bool UCLLDBPlugin::ConnectRemote(const std::string& host, int port) {
    std::string url = host + ":" + std::to_string(port);
    std::string result = SendCommand("gdb-remote " + url);
    return result.find("error") == std::string::npos;
}

bool UCLLDBPlugin::LoadCoreDump(const std::string& corePath, const std::string& executablePath) {
    if (state_ != DebugSessionState::Inactive) return false;
    
    SetState(DebugSessionState::Starting);
    
    // Start LLDB and load core
#ifndef _WIN32
    int stdinPipe[2], stdoutPipe[2];
    pipe(stdinPipe);
    pipe(stdoutPipe);
    
    pid_ = fork();
    if (pid_ == 0) {
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        execlp(lldbPath_.c_str(), lldbPath_.c_str(), "-c", corePath.c_str(), 
               executablePath.c_str(), nullptr);
        _exit(127);
    }
    
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
#endif
    
    SetState(DebugSessionState::Paused);
    sessionActive = true;
    return true;
}

bool UCLLDBPlugin::Detach() {
    SendCommand("detach");
    Cleanup();
    SetState(DebugSessionState::Inactive);
    return true;
}

bool UCLLDBPlugin::Terminate() {
    if (state_ == DebugSessionState::Inactive) return true;
    
    SetState(DebugSessionState::Stopping);
    SendCommand("kill");
    SendCommand("quit");
    Cleanup();
    SetState(DebugSessionState::Terminated);
    return true;
}

DebugSessionState UCLLDBPlugin::GetSessionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool UCLLDBPlugin::IsSessionActive() const { return sessionActive; }

// ============================================================================
// EXECUTION CONTROL
// ============================================================================

bool UCLLDBPlugin::Continue() {
    if (state_ != DebugSessionState::Paused) return false;
    std::string result = SendCommand("continue");
    SetState(DebugSessionState::Running);
    return true;
}

bool UCLLDBPlugin::Pause() {
    if (state_ != DebugSessionState::Running) return false;
#ifndef _WIN32
    if (pid_ > 0) kill(pid_, SIGINT);
#endif
    return true;
}

bool UCLLDBPlugin::StepOver() {
    if (state_ != DebugSessionState::Paused) return false;
    SetState(DebugSessionState::Stepping);
    SendCommand("next");
    return true;
}

bool UCLLDBPlugin::StepInto() {
    if (state_ != DebugSessionState::Paused) return false;
    SetState(DebugSessionState::Stepping);
    SendCommand("step");
    return true;
}

bool UCLLDBPlugin::StepOut() {
    if (state_ != DebugSessionState::Paused) return false;
    SetState(DebugSessionState::Stepping);
    SendCommand("finish");
    return true;
}

bool UCLLDBPlugin::StepTo(const std::string& file, int line) {
    return RunToCursor(file, line);
}

bool UCLLDBPlugin::RunToCursor(const std::string& file, int line) {
    std::string location = file + ":" + std::to_string(line);
    SendCommand("breakpoint set --one-shot true --file \"" + file + 
                "\" --line " + std::to_string(line));
    return Continue();
}

bool UCLLDBPlugin::SetNextStatement(const std::string& file, int line) {
    std::string cmd = "thread jump --file \"" + file + "\" --line " + std::to_string(line);
    std::string result = SendCommand(cmd);
    return result.find("error") == std::string::npos;
}

bool UCLLDBPlugin::StepInstruction() {
    if (state_ != DebugSessionState::Paused) return false;
    SetState(DebugSessionState::Stepping);
    SendCommand("si");
    return true;
}

bool UCLLDBPlugin::StepIntoInstruction() { return StepInstruction(); }

// ============================================================================
// BREAKPOINTS
// ============================================================================

Breakpoint UCLLDBPlugin::AddBreakpoint(const std::string& file, int line) {
    Breakpoint bp;
    bp.id = nextBreakpointId_++;
    bp.location.filePath = file;
    bp.location.line = line;
    bp.type = BreakpointType::LineBreakpoint;
    
    std::string cmd = "breakpoint set --file \"" + file + "\" --line " + std::to_string(line);
    std::string result = SendCommand(cmd);
    
    // Parse breakpoint ID from result
    size_t pos = result.find("Breakpoint ");
    if (pos != std::string::npos) {
        bp.debuggerId = std::stoi(result.substr(pos + 11));
        bp.state = BreakpointState::Active;
        bp.verified = true;
    } else {
        bp.state = BreakpointState::Invalid;
    }
    
    breakpoints_[bp.id] = bp;
    return bp;
}

Breakpoint UCLLDBPlugin::AddFunctionBreakpoint(const std::string& functionName) {
    Breakpoint bp;
    bp.id = nextBreakpointId_++;
    bp.functionName = functionName;
    bp.type = BreakpointType::FunctionBreakpoint;
    
    std::string result = SendCommand("breakpoint set --name \"" + functionName + "\"");
    
    size_t pos = result.find("Breakpoint ");
    if (pos != std::string::npos) {
        bp.debuggerId = std::stoi(result.substr(pos + 11));
        bp.state = BreakpointState::Active;
        bp.verified = true;
    }
    
    breakpoints_[bp.id] = bp;
    return bp;
}

Breakpoint UCLLDBPlugin::AddConditionalBreakpoint(const std::string& file, int line,
                                                   const std::string& condition) {
    Breakpoint bp = AddBreakpoint(file, line);
    if (bp.debuggerId >= 0 && !condition.empty()) {
        SendCommand("breakpoint modify -c \"" + condition + "\" " + 
                    std::to_string(bp.debuggerId));
        bp.condition = condition;
        breakpoints_[bp.id] = bp;
    }
    return bp;
}

Breakpoint UCLLDBPlugin::AddAddressBreakpoint(uint64_t address) {
    Breakpoint bp;
    bp.id = nextBreakpointId_++;
    bp.address = address;
    bp.type = BreakpointType::AddressBreakpoint;
    
    std::ostringstream cmd;
    cmd << "breakpoint set --address 0x" << std::hex << address;
    std::string result = SendCommand(cmd.str());
    
    size_t pos = result.find("Breakpoint ");
    if (pos != std::string::npos) {
        bp.debuggerId = std::stoi(result.substr(pos + 11));
        bp.state = BreakpointState::Active;
    }
    
    breakpoints_[bp.id] = bp;
    return bp;
}

Breakpoint UCLLDBPlugin::AddLogpoint(const std::string& file, int line,
                                      const std::string& logMessage) {
    Breakpoint bp;
    bp.id = nextBreakpointId_++;
    bp.location.filePath = file;
    bp.location.line = line;
    bp.type = BreakpointType::Logpoint;
    bp.logMessage = logMessage;
    
    std::string cmd = "breakpoint set --file \"" + file + "\" --line " + 
                      std::to_string(line) + " -C \"script print(\\\"" + 
                      logMessage + "\\\")\" --auto-continue true";
    std::string result = SendCommand(cmd);
    
    size_t pos = result.find("Breakpoint ");
    if (pos != std::string::npos) {
        bp.debuggerId = std::stoi(result.substr(pos + 11));
        bp.state = BreakpointState::Active;
    }
    
    breakpoints_[bp.id] = bp;
    return bp;
}

bool UCLLDBPlugin::RemoveBreakpoint(int breakpointId) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    SendCommand("breakpoint delete " + std::to_string(it->second.debuggerId));
    breakpoints_.erase(it);
    return true;
}

bool UCLLDBPlugin::SetBreakpointEnabled(int breakpointId, bool enabled) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    std::string cmd = enabled ? "breakpoint enable " : "breakpoint disable ";
    SendCommand(cmd + std::to_string(it->second.debuggerId));
    it->second.enabled = enabled;
    return true;
}

bool UCLLDBPlugin::SetBreakpointCondition(int breakpointId, const std::string& condition) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    SendCommand("breakpoint modify -c \"" + condition + "\" " + 
                std::to_string(it->second.debuggerId));
    it->second.condition = condition;
    return true;
}

bool UCLLDBPlugin::SetBreakpointIgnoreCount(int breakpointId, int count) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    SendCommand("breakpoint modify -i " + std::to_string(count) + " " + 
                std::to_string(it->second.debuggerId));
    it->second.ignoreCount = count;
    return true;
}

std::vector<Breakpoint> UCLLDBPlugin::GetBreakpoints() const {
    std::vector<Breakpoint> result;
    for (const auto& pair : breakpoints_) result.push_back(pair.second);
    return result;
}

Breakpoint UCLLDBPlugin::GetBreakpoint(int breakpointId) const {
    auto it = breakpoints_.find(breakpointId);
    return (it != breakpoints_.end()) ? it->second : Breakpoint();
}

// ============================================================================
// WATCHPOINTS
// ============================================================================

Watchpoint UCLLDBPlugin::AddWatchpoint(const std::string& expression, WatchpointAccess access) {
    Watchpoint wp;
    wp.id = nextWatchpointId_++;
    wp.expression = expression;
    wp.access = access;
    
    std::string accessStr = (access == WatchpointAccess::Read) ? "-r" :
                           (access == WatchpointAccess::ReadWrite) ? "-a" : "-w";
    
    std::string cmd = "watchpoint set expression " + accessStr + " -- " + expression;
    std::string result = SendCommand(cmd);
    
    size_t pos = result.find("Watchpoint ");
    if (pos != std::string::npos) {
        wp.debuggerId = std::stoi(result.substr(pos + 11));
    }
    
    watchpoints_[wp.id] = wp;
    return wp;
}

bool UCLLDBPlugin::RemoveWatchpoint(int watchpointId) {
    auto it = watchpoints_.find(watchpointId);
    if (it == watchpoints_.end()) return false;
    
    SendCommand("watchpoint delete " + std::to_string(it->second.debuggerId));
    watchpoints_.erase(it);
    return true;
}

std::vector<Watchpoint> UCLLDBPlugin::GetWatchpoints() const {
    std::vector<Watchpoint> result;
    for (const auto& pair : watchpoints_) result.push_back(pair.second);
    return result;
}

// ============================================================================
// THREADS & STACK
// ============================================================================

std::vector<ThreadInfo> UCLLDBPlugin::GetThreads() {
    std::vector<ThreadInfo> threads;
    std::string result = SendCommand("thread list");
    // Parse thread list output
    return threads;
}

ThreadInfo UCLLDBPlugin::GetCurrentThread() {
    auto threads = GetThreads();
    for (const auto& t : threads) if (t.isCurrent) return t;
    return threads.empty() ? ThreadInfo() : threads[0];
}

bool UCLLDBPlugin::SwitchThread(int threadId) {
    std::string result = SendCommand("thread select " + std::to_string(threadId));
    if (result.find("error") == std::string::npos) {
        currentThreadId_ = threadId;
        return true;
    }
    return false;
}

std::vector<StackFrame> UCLLDBPlugin::GetCallStack(int maxFrames) {
    return GetCallStack(currentThreadId_, maxFrames);
}

std::vector<StackFrame> UCLLDBPlugin::GetCallStack(int threadId, int maxFrames) {
    std::vector<StackFrame> frames;
    std::string result = SendCommand("bt " + std::to_string(maxFrames));
    // Parse backtrace output
    return frames;
}

bool UCLLDBPlugin::SwitchFrame(int frameLevel) {
    std::string result = SendCommand("frame select " + std::to_string(frameLevel));
    if (result.find("error") == std::string::npos) {
        currentFrameLevel_ = frameLevel;
        return true;
    }
    return false;
}

StackFrame UCLLDBPlugin::GetCurrentFrame() {
    auto frames = GetCallStack(1);
    return frames.empty() ? StackFrame() : frames[0];
}

// ============================================================================
// VARIABLES
// ============================================================================

std::vector<Variable> UCLLDBPlugin::GetLocals() {
    std::vector<Variable> vars;
    std::string result = SendCommand("frame variable --no-args");
    // Parse variable output
    return vars;
}

std::vector<Variable> UCLLDBPlugin::GetArguments() {
    std::vector<Variable> vars;
    std::string result = SendCommand("frame variable --no-locals");
    return vars;
}

std::vector<Variable> UCLLDBPlugin::GetGlobals() { return {}; }

EvaluationResult UCLLDBPlugin::Evaluate(const std::string& expression) {
    return Evaluate(expression, EvaluationContext::Watch);
}

EvaluationResult UCLLDBPlugin::Evaluate(const std::string& expression, EvaluationContext context) {
    EvaluationResult result;
    std::string output = SendCommand("expression -- " + expression);
    
    if (output.find("error") != std::string::npos) {
        result.success = false;
        result.errorMessage = output;
    } else {
        result.success = true;
        result.variable.name = expression;
        result.variable.value = output;
    }
    return result;
}

bool UCLLDBPlugin::SetVariable(const std::string& name, const std::string& value) {
    std::string result = SendCommand("expression " + name + " = " + value);
    return result.find("error") == std::string::npos;
}

std::vector<Variable> UCLLDBPlugin::ExpandVariable(int variablesReference) { return {}; }

// ============================================================================
// REGISTERS & MEMORY
// ============================================================================

std::vector<Register> UCLLDBPlugin::GetRegisters() {
    std::vector<Register> registers;
    std::string result = SendCommand("register read --all");
    return registers;
}

std::vector<Register> UCLLDBPlugin::GetRegisters(RegisterCategory category) {
    return GetRegisters();
}

bool UCLLDBPlugin::SetRegister(const std::string& name, const std::string& value) {
    std::string result = SendCommand("register write " + name + " " + value);
    return result.find("error") == std::string::npos;
}

MemoryRegion UCLLDBPlugin::ReadMemory(uint64_t address, size_t size) {
    MemoryRegion region;
    region.address = address;
    region.size = size;
    
    std::ostringstream cmd;
    cmd << "memory read --size 1 --count " << size << " 0x" << std::hex << address;
    std::string result = SendCommand(cmd.str());
    
    return region;
}

bool UCLLDBPlugin::WriteMemory(uint64_t address, const std::vector<uint8_t>& data) {
    for (size_t i = 0; i < data.size(); i++) {
        std::ostringstream cmd;
        cmd << "memory write 0x" << std::hex << (address + i) << " " << static_cast<int>(data[i]);
        SendCommand(cmd.str());
    }
    return true;
}

// ============================================================================
// DISASSEMBLY & MODULES
// ============================================================================

std::vector<DisassemblyLine> UCLLDBPlugin::GetDisassembly(uint64_t address, int count) {
    std::vector<DisassemblyLine> lines;
    std::ostringstream cmd;
    cmd << "disassemble --start-address 0x" << std::hex << address << " --count " << count;
    std::string result = SendCommand(cmd.str());
    return lines;
}

std::vector<DisassemblyLine> UCLLDBPlugin::GetFunctionDisassembly(const std::string& functionName) {
    std::vector<DisassemblyLine> lines;
    std::string result = SendCommand("disassemble --name " + functionName);
    return lines;
}

std::vector<ModuleInfo> UCLLDBPlugin::GetModules() {
    std::vector<ModuleInfo> modules;
    std::string result = SendCommand("image list");
    return modules;
}

std::vector<DebugCompletionItem> UCLLDBPlugin::GetCompletions(const std::string& text, int column) {
    return {};
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::string UCLLDBPlugin::SendCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    
#ifndef _WIN32
    if (stdinFd_ < 0 || stdoutFd_ < 0) return "";
    
    if (!command.empty()) {
        std::string cmd = command + "\n";
        write(stdinFd_, cmd.c_str(), cmd.size());
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string result;
    char buffer[4096];
    ssize_t bytesRead;
    
    while ((bytesRead = read(stdoutFd_, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
    }
    
    return result;
#else
    return "";
#endif
}

void UCLLDBPlugin::ProcessOutput(const std::string& output) {
    if (output.find("Process") != std::string::npos && 
        output.find("stopped") != std::string::npos) {
        SetState(DebugSessionState::Paused);
        
        if (onTargetStop) {
            SourceLocation loc;
            onTargetStop(StopReason::Breakpoint, loc);
        }
    }
    
    if (onOutput) onOutput(output);
}

void UCLLDBPlugin::SetState(DebugSessionState newState) {
    DebugSessionState oldState;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        oldState = state_;
        state_ = newState;
    }
    
    if (oldState != newState && onStateChange) {
        onStateChange(oldState, newState);
    }
    
    sessionActive = (newState == DebugSessionState::Running ||
                     newState == DebugSessionState::Paused ||
                     newState == DebugSessionState::Stepping);
}

void UCLLDBPlugin::Cleanup() {
    stopOutputThread_ = true;
    if (outputThread_.joinable()) outputThread_.join();
    
#ifndef _WIN32
    if (pid_ > 0) {
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, WNOHANG);
        pid_ = -1;
    }
    if (stdinFd_ >= 0) { close(stdinFd_); stdinFd_ = -1; }
    if (stdoutFd_ >= 0) { close(stdoutFd_); stdoutFd_ = -1; }
#endif
    
    breakpoints_.clear();
    watchpoints_.clear();
    sessionActive = false;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string FindLLDBExecutable() {
    std::vector<std::string> paths = {
#ifdef __APPLE__
        "/usr/bin/lldb",
        "/Applications/Xcode.app/Contents/Developer/usr/bin/lldb",
        "/Library/Developer/CommandLineTools/usr/bin/lldb"
#else
        "/usr/bin/lldb",
        "/usr/local/bin/lldb"
#endif
    };
    
    for (const auto& path : paths) {
#ifndef _WIN32
        if (access(path.c_str(), X_OK) == 0) return path;
#endif
    }
    
#ifndef _WIN32
    FILE* pipe = popen("which lldb 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                result.pop_back();
            }
            pclose(pipe);
            if (!result.empty()) return result;
        }
        pclose(pipe);
    }
#endif
    
    return "";
}

std::string GetLLDBVersion(const std::string& lldbPath) {
    if (lldbPath.empty()) return "";
    
#ifndef _WIN32
    FILE* pipe = popen((lldbPath + " --version").c_str(), "r");
    if (!pipe) return "";
    
    char buffer[256];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
    }
    pclose(pipe);
    return result;
#else
    return "";
#endif
}

} // namespace IDE
} // namespace UltraCanvas
