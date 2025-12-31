// Apps/IDE/Debug/GDB/UCGDBPlugin.cpp
// GDB Debugger Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGDBPlugin.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <algorithm>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// CONFIGURATION
// ============================================================================

GDBPluginConfig GDBPluginConfig::Default() {
    GDBPluginConfig config;
    config.useMI = true;
    config.prettyPrinting = true;
    config.asyncMode = true;
    config.commandTimeout = 5000;
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCGDBPlugin::UCGDBPlugin() : UCGDBPlugin(GDBPluginConfig::Default()) {}

UCGDBPlugin::UCGDBPlugin(const GDBPluginConfig& config) 
    : config_(config) {
    parser_ = std::make_unique<UCGDBMIParser>();
    
    // Find GDB
    if (config_.gdbPath.empty()) {
        gdbPath_ = FindGDBExecutable();
    } else {
        gdbPath_ = config_.gdbPath;
    }
    
    available_ = !gdbPath_.empty();
    if (available_) {
        gdbVersion_ = GetGDBVersion(gdbPath_);
    }
}

UCGDBPlugin::~UCGDBPlugin() {
    Terminate();
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCGDBPlugin::GetPluginName() const {
    return "GDB Debugger Plugin";
}

std::string UCGDBPlugin::GetPluginVersion() const {
    return "1.0.0";
}

DebuggerType UCGDBPlugin::GetDebuggerType() const {
    return DebuggerType::GDB;
}

std::string UCGDBPlugin::GetDebuggerName() const {
    return "GDB";
}

// ============================================================================
// DEBUGGER DETECTION
// ============================================================================

bool UCGDBPlugin::IsAvailable() {
    return available_;
}

std::string UCGDBPlugin::GetDebuggerPath() {
    return gdbPath_;
}

void UCGDBPlugin::SetDebuggerPath(const std::string& path) {
    gdbPath_ = path;
    available_ = !gdbPath_.empty();
    if (available_) {
        gdbVersion_ = GetGDBVersion(gdbPath_);
    }
}

std::string UCGDBPlugin::GetDebuggerVersion() {
    return gdbVersion_;
}

std::vector<std::string> UCGDBPlugin::GetSupportedExtensions() const {
    return { "", ".exe", ".out", ".elf", ".bin" };
}

// ============================================================================
// SESSION CONTROL
// ============================================================================

bool UCGDBPlugin::Launch(const DebugLaunchConfig& config) {
    if (state_ != DebugSessionState::Inactive) {
        return false;
    }
    
    if (!available_) {
        if (onError) onError("GDB not available");
        return false;
    }
    
    SetState(DebugSessionState::Starting);
    
    // Start GDB process
    process_ = std::make_unique<UCGDBProcess>();
    
    std::vector<std::string> gdbArgs = config_.gdbArgs;
    if (config_.asyncMode) {
        gdbArgs.push_back("--async");
    }
    
    if (!process_->Start(gdbPath_, gdbArgs)) {
        SetState(DebugSessionState::ErrorState);
        if (onError) onError("Failed to start GDB");
        return false;
    }
    
    // Setup process callbacks
    process_->onStdout = [this](const std::string& line) {
        // Output handled by output thread
    };
    
    process_->onStderr = [this](const std::string& err) {
        if (onError) onError(err);
    };
    
    process_->onExit = [this](int code) {
        SetState(DebugSessionState::Terminated);
    };
    
    // Wait for initial prompt
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Set executable
    if (!config.executablePath.empty()) {
        std::string cmd = "-file-exec-and-symbols \"" + 
                          UCGDBMIParser::Escape(config.executablePath) + "\"";
        GDBMIRecord result = SendCommand(cmd);
        if (result.IsError()) {
            Terminate();
            if (onError) onError("Failed to load executable: " + result.GetError());
            return false;
        }
    }
    
    // Set working directory
    if (!config.workingDirectory.empty()) {
        SendCommand("-environment-cd \"" + 
                    UCGDBMIParser::Escape(config.workingDirectory) + "\"");
    }
    
    // Set arguments
    if (!config.arguments.empty()) {
        std::string argsStr;
        for (const auto& arg : config.arguments) {
            if (!argsStr.empty()) argsStr += " ";
            argsStr += "\"" + UCGDBMIParser::Escape(arg) + "\"";
        }
        SendCommand("-exec-arguments " + argsStr);
    }
    
    // Enable pretty printing
    if (config_.prettyPrinting) {
        SendCommand("-enable-pretty-printing");
    }
    
    // Set non-stop mode if async
    if (config_.asyncMode) {
        SendCommand("-gdb-set non-stop on");
        SendCommand("-gdb-set target-async on");
    }
    
    // Start the program
    if (config.stopOnEntry) {
        // Set temporary breakpoint at main
        SendCommand("-break-insert -t main");
    }
    
    GDBMIRecord runResult = SendCommand("-exec-run");
    if (runResult.IsError()) {
        if (onError) onError("Failed to start program: " + runResult.GetError());
        return false;
    }
    
    SetState(DebugSessionState::Running);
    sessionActive = true;
    
    // Start output processing thread
    stopOutputThread_ = false;
    outputThread_ = std::thread(&UCGDBPlugin::ProcessOutput, this);
    
    return true;
}

bool UCGDBPlugin::Attach(int pid) {
    if (state_ != DebugSessionState::Inactive) {
        return false;
    }
    
    SetState(DebugSessionState::Starting);
    
    process_ = std::make_unique<UCGDBProcess>();
    if (!process_->Start(gdbPath_, config_.gdbArgs)) {
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    GDBMIRecord result = SendCommand("-target-attach " + std::to_string(pid));
    if (result.IsError()) {
        Terminate();
        if (onError) onError("Failed to attach: " + result.GetError());
        return false;
    }
    
    SetState(DebugSessionState::Paused);
    sessionActive = true;
    
    stopOutputThread_ = false;
    outputThread_ = std::thread(&UCGDBPlugin::ProcessOutput, this);
    
    return true;
}

bool UCGDBPlugin::AttachByName(const std::string& processName) {
    // GDB doesn't support attach by name directly
    // We would need to use platform-specific code to find the PID
    if (onError) onError("Attach by name not supported - use PID");
    return false;
}

bool UCGDBPlugin::ConnectRemote(const std::string& host, int port) {
    if (state_ != DebugSessionState::Inactive) {
        return false;
    }
    
    SetState(DebugSessionState::Starting);
    
    process_ = std::make_unique<UCGDBProcess>();
    if (!process_->Start(gdbPath_, config_.gdbArgs)) {
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::string target = host + ":" + std::to_string(port);
    GDBMIRecord result = SendCommand("-target-select remote " + target);
    if (result.IsError()) {
        Terminate();
        if (onError) onError("Failed to connect: " + result.GetError());
        return false;
    }
    
    SetState(DebugSessionState::Paused);
    sessionActive = true;
    
    stopOutputThread_ = false;
    outputThread_ = std::thread(&UCGDBPlugin::ProcessOutput, this);
    
    return true;
}

bool UCGDBPlugin::LoadCoreDump(const std::string& corePath, const std::string& executablePath) {
    if (state_ != DebugSessionState::Inactive) {
        return false;
    }
    
    SetState(DebugSessionState::Starting);
    
    process_ = std::make_unique<UCGDBProcess>();
    if (!process_->Start(gdbPath_, config_.gdbArgs)) {
        SetState(DebugSessionState::ErrorState);
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Load executable
    GDBMIRecord result = SendCommand("-file-exec-and-symbols \"" + 
                                     UCGDBMIParser::Escape(executablePath) + "\"");
    if (result.IsError()) {
        Terminate();
        return false;
    }
    
    // Load core
    result = SendCommand("-target-select core \"" + 
                         UCGDBMIParser::Escape(corePath) + "\"");
    if (result.IsError()) {
        Terminate();
        if (onError) onError("Failed to load core: " + result.GetError());
        return false;
    }
    
    SetState(DebugSessionState::Paused);
    sessionActive = true;
    
    return true;
}

bool UCGDBPlugin::Detach() {
    if (!IsSessionActive()) return true;
    
    GDBMIRecord result = SendCommand("-target-detach");
    
    Cleanup();
    SetState(DebugSessionState::Inactive);
    
    return result.IsSuccess();
}

bool UCGDBPlugin::Terminate() {
    if (state_ == DebugSessionState::Inactive) {
        return true;
    }
    
    SetState(DebugSessionState::Stopping);
    
    // Send kill command
    if (process_ && process_->IsRunning()) {
        SendCommandNoWait("-exec-abort");
        SendCommandNoWait("-gdb-exit");
    }
    
    Cleanup();
    SetState(DebugSessionState::Terminated);
    
    return true;
}

DebugSessionState UCGDBPlugin::GetSessionState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

bool UCGDBPlugin::IsSessionActive() const {
    return sessionActive;
}

// ============================================================================
// EXECUTION CONTROL
// ============================================================================

bool UCGDBPlugin::Continue() {
    if (state_ != DebugSessionState::Paused) return false;
    
    GDBMIRecord result = SendCommand("-exec-continue");
    if (result.IsSuccess() || result.resultClass == GDBMIResultClass::Running) {
        SetState(DebugSessionState::Running);
        if (onTargetContinue) onTargetContinue();
        return true;
    }
    return false;
}

bool UCGDBPlugin::Pause() {
    if (state_ != DebugSessionState::Running) return false;
    
    GDBMIRecord result = SendCommand("-exec-interrupt");
    return result.IsSuccess();
}

bool UCGDBPlugin::StepOver() {
    if (state_ != DebugSessionState::Paused) return false;
    
    SetState(DebugSessionState::Stepping);
    GDBMIRecord result = SendCommand("-exec-next");
    return result.IsSuccess() || result.resultClass == GDBMIResultClass::Running;
}

bool UCGDBPlugin::StepInto() {
    if (state_ != DebugSessionState::Paused) return false;
    
    SetState(DebugSessionState::Stepping);
    GDBMIRecord result = SendCommand("-exec-step");
    return result.IsSuccess() || result.resultClass == GDBMIResultClass::Running;
}

bool UCGDBPlugin::StepOut() {
    if (state_ != DebugSessionState::Paused) return false;
    
    SetState(DebugSessionState::Stepping);
    GDBMIRecord result = SendCommand("-exec-finish");
    return result.IsSuccess() || result.resultClass == GDBMIResultClass::Running;
}

bool UCGDBPlugin::StepTo(const std::string& file, int line) {
    return RunToCursor(file, line);
}

bool UCGDBPlugin::RunToCursor(const std::string& file, int line) {
    if (state_ != DebugSessionState::Paused) return false;
    
    // Set temporary breakpoint
    std::string location = "\"" + UCGDBMIParser::Escape(file) + ":" + 
                           std::to_string(line) + "\"";
    GDBMIRecord bpResult = SendCommand("-break-insert -t " + location);
    if (bpResult.IsError()) return false;
    
    // Continue
    return Continue();
}

bool UCGDBPlugin::SetNextStatement(const std::string& file, int line) {
    if (state_ != DebugSessionState::Paused) return false;
    
    std::string location = file + ":" + std::to_string(line);
    GDBMIRecord result = SendCommand("-exec-jump \"" + 
                                     UCGDBMIParser::Escape(location) + "\"");
    return result.IsSuccess();
}

bool UCGDBPlugin::StepInstruction() {
    if (state_ != DebugSessionState::Paused) return false;
    
    SetState(DebugSessionState::Stepping);
    GDBMIRecord result = SendCommand("-exec-step-instruction");
    return result.IsSuccess() || result.resultClass == GDBMIResultClass::Running;
}

bool UCGDBPlugin::StepIntoInstruction() {
    return StepInstruction();
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void UCGDBPlugin::SetState(DebugSessionState newState) {
    DebugSessionState oldState;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
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

void UCGDBPlugin::Cleanup() {
    stopOutputThread_ = true;
    
    if (outputThread_.joinable()) {
        outputThread_.join();
    }
    
    if (process_) {
        process_->Stop();
        process_.reset();
    }
    
    // Clear state
    breakpoints_.clear();
    watchpoints_.clear();
    variableObjects_.clear();
    pendingCommands_.clear();
    
    sessionActive = false;
}

int UCGDBPlugin::GetNextToken() {
    return nextToken_++;
}

} // namespace IDE
} // namespace UltraCanvas
