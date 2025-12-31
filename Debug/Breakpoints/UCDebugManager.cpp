// Apps/IDE/Debug/Core/UCDebugManager.cpp
// Debug Session Manager Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDebugManager.h"
#include "../GDB/UCGDBPlugin.h"
#include "../LLDB/UCLLDBPlugin.h"
#include <iostream>
#include <algorithm>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// CONFIGURATION
// ============================================================================

DebugManagerConfig DebugManagerConfig::Default() {
    DebugManagerConfig config;
    
#ifdef __APPLE__
    config.preferredDebugger = DebuggerType::LLDB;
#else
    config.preferredDebugger = DebuggerType::GDB;
#endif
    
    config.autoSelectDebugger = true;
    config.saveBreakpointsOnExit = true;
    config.loadBreakpointsOnStart = true;
    config.confirmOnTerminate = true;
    config.stopOnEntry = false;
    config.maxCallStackDepth = 100;
    config.maxVariableChildren = 100;
    config.launchTimeout = 10000;
    config.commandTimeout = 5000;
    
    return config;
}

// ============================================================================
// SINGLETON
// ============================================================================

UCDebugManager& UCDebugManager::Instance() {
    static UCDebugManager instance;
    return instance;
}

UCDebugManager::UCDebugManager() 
    : config_(DebugManagerConfig::Default()) {
}

UCDebugManager::~UCDebugManager() {
    Shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool UCDebugManager::Initialize() {
    return Initialize(DebugManagerConfig::Default());
}

bool UCDebugManager::Initialize(const DebugManagerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) return true;
    
    config_ = config;
    
    // Register built-in plugins
    RegisterBuiltInDebuggerPlugins();
    
    std::cout << "[DebugManager] Initialized with "
              << UCDebuggerPluginRegistry::Instance().GetPluginCount()
              << " debugger plugins" << std::endl;
    
    initialized_ = true;
    return true;
}

void UCDebugManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) return;
    
    // Stop any active session
    if (activeDebugger_ && activeDebugger_->IsSessionActive()) {
        activeDebugger_->Terminate();
    }
    
    activeDebugger_.reset();
    currentProject_.reset();
    
    initialized_ = false;
    
    std::cout << "[DebugManager] Shutdown complete" << std::endl;
}

// ============================================================================
// SESSION CONTROL
// ============================================================================

bool UCDebugManager::StartDebugging(const DebugLaunchConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        std::cerr << "[DebugManager] Not initialized" << std::endl;
        return false;
    }
    
    if (activeDebugger_ && activeDebugger_->IsSessionActive()) {
        std::cerr << "[DebugManager] Debug session already active" << std::endl;
        return false;
    }
    
    // Select debugger
    if (!activeDebugger_) {
        if (config.debuggerType != DebuggerType::UnknownDebugger) {
            activeDebugger_ = UCDebuggerPluginRegistry::Instance().GetPlugin(config.debuggerType);
        }
        if (!activeDebugger_) {
            activeDebugger_ = UCDebuggerPluginRegistry::Instance().GetBestDebugger();
        }
    }
    
    if (!activeDebugger_) {
        std::cerr << "[DebugManager] No debugger available" << std::endl;
        HandleError("No debugger available. Please install GDB or LLDB.");
        return false;
    }
    
    if (!activeDebugger_->IsAvailable()) {
        std::cerr << "[DebugManager] Selected debugger not available" << std::endl;
        HandleError("Selected debugger is not available on this system.");
        return false;
    }
    
    // Setup callbacks
    SetupDebuggerCallbacks();
    
    // Store config
    currentLaunchConfig_ = config;
    
    std::cout << "[DebugManager] Starting debug session with "
              << activeDebugger_->GetDebuggerName() << std::endl;
    
    // Launch
    if (!activeDebugger_->Launch(config)) {
        std::cerr << "[DebugManager] Failed to launch debugger" << std::endl;
        HandleError("Failed to launch debugger.");
        return false;
    }
    
    // Sync breakpoints
    breakpointManager_.SyncToDebugger(activeDebugger_.get());
    
    return true;
}

bool UCDebugManager::StartDebugging(std::shared_ptr<UCIDEProject> project) {
    if (!project) return false;
    
    currentProject_ = project;
    DebugLaunchConfig config = CreateLaunchConfig(project);
    
    return StartDebugging(config);
}

bool UCDebugManager::AttachToProcess(int pid) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !activeDebugger_) {
        activeDebugger_ = UCDebuggerPluginRegistry::Instance().GetBestDebugger();
    }
    
    if (!activeDebugger_ || !activeDebugger_->IsAvailable()) {
        HandleError("No debugger available for attach.");
        return false;
    }
    
    SetupDebuggerCallbacks();
    
    return activeDebugger_->Attach(pid);
}

bool UCDebugManager::AttachToProcess(const std::string& processName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !activeDebugger_) {
        activeDebugger_ = UCDebuggerPluginRegistry::Instance().GetBestDebugger();
    }
    
    if (!activeDebugger_ || !activeDebugger_->IsAvailable()) {
        HandleError("No debugger available for attach.");
        return false;
    }
    
    SetupDebuggerCallbacks();
    
    return activeDebugger_->AttachByName(processName);
}

bool UCDebugManager::LoadCoreDump(const std::string& corePath, const std::string& execPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !activeDebugger_) {
        activeDebugger_ = UCDebuggerPluginRegistry::Instance().GetBestDebugger();
    }
    
    if (!activeDebugger_ || !activeDebugger_->IsAvailable()) {
        HandleError("No debugger available for core dump analysis.");
        return false;
    }
    
    SetupDebuggerCallbacks();
    
    return activeDebugger_->LoadCoreDump(corePath, execPath);
}

bool UCDebugManager::StopDebugging() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!activeDebugger_ || !activeDebugger_->IsSessionActive()) {
        return true;  // Already stopped
    }
    
    std::cout << "[DebugManager] Stopping debug session" << std::endl;
    
    bool result = activeDebugger_->Terminate();
    
    // Clear debugger IDs from breakpoints
    breakpointManager_.ClearDebuggerIds();
    
    return result;
}

bool UCDebugManager::RestartDebugging() {
    if (!StopDebugging()) return false;
    
    // Wait a bit for clean shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return StartDebugging(currentLaunchConfig_);
}

bool UCDebugManager::Detach() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!activeDebugger_) return true;
    
    bool result = activeDebugger_->Detach();
    breakpointManager_.ClearDebuggerIds();
    
    return result;
}

bool UCDebugManager::IsDebugging() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeDebugger_ && activeDebugger_->IsSessionActive();
}

DebugSessionState UCDebugManager::GetSessionState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return DebugSessionState::Inactive;
    return activeDebugger_->GetSessionState();
}

// ============================================================================
// EXECUTION CONTROL
// ============================================================================

bool UCDebugManager::Continue() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->Continue();
}

bool UCDebugManager::Pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->Pause();
}

bool UCDebugManager::StepOver() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->StepOver();
}

bool UCDebugManager::StepInto() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->StepInto();
}

bool UCDebugManager::StepOut() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->StepOut();
}

bool UCDebugManager::StepInstruction() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->StepInstruction();
}

bool UCDebugManager::RunToCursor(const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->RunToCursor(file, line);
}

bool UCDebugManager::SetNextStatement(const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->SetNextStatement(file, line);
}

// ============================================================================
// BREAKPOINTS
// ============================================================================

int UCDebugManager::AddBreakpoint(const std::string& file, int line) {
    int id = breakpointManager_.AddBreakpoint(file, line);
    
    if (IsDebugging()) {
        std::lock_guard<std::mutex> lock(mutex_);
        Breakpoint bp = activeDebugger_->AddBreakpoint(file, line);
        breakpointManager_.UpdateBreakpoint(bp);
    }
    
    return id;
}

int UCDebugManager::AddFunctionBreakpoint(const std::string& functionName) {
    int id = breakpointManager_.AddFunctionBreakpoint(functionName);
    
    if (IsDebugging()) {
        std::lock_guard<std::mutex> lock(mutex_);
        Breakpoint bp = activeDebugger_->AddFunctionBreakpoint(functionName);
        breakpointManager_.UpdateBreakpoint(bp);
    }
    
    return id;
}

int UCDebugManager::AddConditionalBreakpoint(const std::string& file, int line,
                                              const std::string& condition) {
    int id = breakpointManager_.AddConditionalBreakpoint(file, line, condition);
    
    if (IsDebugging()) {
        std::lock_guard<std::mutex> lock(mutex_);
        Breakpoint bp = activeDebugger_->AddConditionalBreakpoint(file, line, condition);
        breakpointManager_.UpdateBreakpoint(bp);
    }
    
    return id;
}

int UCDebugManager::AddLogpoint(const std::string& file, int line,
                                 const std::string& logMessage) {
    int id = breakpointManager_.AddLogpoint(file, line, logMessage);
    
    if (IsDebugging()) {
        std::lock_guard<std::mutex> lock(mutex_);
        Breakpoint bp = activeDebugger_->AddLogpoint(file, line, logMessage);
        breakpointManager_.UpdateBreakpoint(bp);
    }
    
    return id;
}

bool UCDebugManager::RemoveBreakpoint(int id) {
    auto* bp = breakpointManager_.GetBreakpoint(id);
    if (!bp) return false;
    
    if (IsDebugging() && bp->GetDebuggerId() >= 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        activeDebugger_->RemoveBreakpoint(bp->GetDebuggerId());
    }
    
    return breakpointManager_.RemoveBreakpoint(id);
}

bool UCDebugManager::RemoveBreakpoint(const std::string& file, int line) {
    auto* bp = breakpointManager_.GetBreakpointAt(file, line);
    if (!bp) return false;
    
    return RemoveBreakpoint(bp->GetId());
}

int UCDebugManager::ToggleBreakpoint(const std::string& file, int line) {
    if (breakpointManager_.HasBreakpointAt(file, line)) {
        RemoveBreakpoint(file, line);
        return 0;
    } else {
        return AddBreakpoint(file, line);
    }
}

bool UCDebugManager::SetBreakpointEnabled(int id, bool enabled) {
    auto* bp = breakpointManager_.GetBreakpoint(id);
    if (!bp) return false;
    
    if (IsDebugging() && bp->GetDebuggerId() >= 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        activeDebugger_->SetBreakpointEnabled(bp->GetDebuggerId(), enabled);
    }
    
    return breakpointManager_.SetBreakpointEnabled(id, enabled);
}

bool UCDebugManager::SetBreakpointCondition(int id, const std::string& condition) {
    auto* bp = breakpointManager_.GetBreakpoint(id);
    if (!bp) return false;
    
    if (IsDebugging() && bp->GetDebuggerId() >= 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        activeDebugger_->SetBreakpointCondition(bp->GetDebuggerId(), condition);
    }
    
    return breakpointManager_.SetBreakpointCondition(id, condition);
}

std::vector<Breakpoint> UCDebugManager::GetBreakpoints() const {
    return breakpointManager_.GetAllBreakpointData();
}

bool UCDebugManager::HasBreakpointAt(const std::string& file, int line) const {
    return breakpointManager_.HasBreakpointAt(file, line);
}

std::set<int> UCDebugManager::GetBreakpointLines(const std::string& file) const {
    return breakpointManager_.GetBreakpointLines(file);
}

// ============================================================================
// WATCHPOINTS
// ============================================================================

int UCDebugManager::AddWatchpoint(const std::string& expression, WatchpointAccess access) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_ || !activeDebugger_->IsSessionActive()) {
        return -1;
    }
    
    Watchpoint wp = activeDebugger_->AddWatchpoint(expression, access);
    return wp.id;
}

bool UCDebugManager::RemoveWatchpoint(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->RemoveWatchpoint(id);
}

std::vector<Watchpoint> UCDebugManager::GetWatchpoints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetWatchpoints();
}

// ============================================================================
// WATCH EXPRESSIONS
// ============================================================================

int UCDebugManager::AddWatch(const std::string& expression) {
    int id = watchManager_.AddWatch(expression);
    
    if (IsDebugging()) {
        watchManager_.RefreshWatch(id, activeDebugger_.get());
    }
    
    return id;
}

bool UCDebugManager::RemoveWatch(int id) {
    return watchManager_.RemoveWatch(id);
}

void UCDebugManager::RefreshWatches() {
    if (IsDebugging()) {
        watchManager_.RefreshAll(activeDebugger_.get());
    }
}

// ============================================================================
// THREADS & STACK
// ============================================================================

std::vector<ThreadInfo> UCDebugManager::GetThreads() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetThreads();
}

ThreadInfo UCDebugManager::GetCurrentThread() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetCurrentThread();
}

bool UCDebugManager::SwitchThread(int threadId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->SwitchThread(threadId);
}

std::vector<StackFrame> UCDebugManager::GetCallStack(int maxFrames) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetCallStack(maxFrames);
}

StackFrame UCDebugManager::GetCurrentFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetCurrentFrame();
}

bool UCDebugManager::SwitchFrame(int frameLevel) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->SwitchFrame(frameLevel);
}

// ============================================================================
// VARIABLES
// ============================================================================

std::vector<Variable> UCDebugManager::GetLocals() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetLocals();
}

std::vector<Variable> UCDebugManager::GetArguments() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetArguments();
}

std::vector<Variable> UCDebugManager::GetGlobals() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetGlobals();
}

EvaluationResult UCDebugManager::Evaluate(const std::string& expression) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) {
        EvaluationResult result;
        result.success = false;
        result.errorMessage = "No active debug session";
        return result;
    }
    return activeDebugger_->Evaluate(expression);
}

bool UCDebugManager::SetVariable(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->SetVariable(name, value);
}

std::vector<Variable> UCDebugManager::ExpandVariable(int variablesReference) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->ExpandVariable(variablesReference);
}

// ============================================================================
// REGISTERS & MEMORY
// ============================================================================

std::vector<Register> UCDebugManager::GetRegisters() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetRegisters();
}

bool UCDebugManager::SetRegister(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->SetRegister(name, value);
}

MemoryRegion UCDebugManager::ReadMemory(uint64_t address, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->ReadMemory(address, size);
}

bool UCDebugManager::WriteMemory(uint64_t address, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return false;
    return activeDebugger_->WriteMemory(address, data);
}

// ============================================================================
// DISASSEMBLY & MODULES
// ============================================================================

std::vector<DisassemblyLine> UCDebugManager::GetDisassembly(uint64_t address, int count) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetDisassembly(address, count);
}

std::vector<DisassemblyLine> UCDebugManager::GetFunctionDisassembly(const std::string& function) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetFunctionDisassembly(function);
}

std::vector<ModuleInfo> UCDebugManager::GetModules() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!activeDebugger_) return {};
    return activeDebugger_->GetModules();
}

// ============================================================================
// DEBUGGER SELECTION
// ============================================================================

std::shared_ptr<IUCDebuggerPlugin> UCDebugManager::GetBestDebugger() {
    return UCDebuggerPluginRegistry::Instance().GetBestDebugger();
}

std::vector<std::shared_ptr<IUCDebuggerPlugin>> UCDebugManager::GetAvailableDebuggers() {
    return UCDebuggerPluginRegistry::Instance().GetAvailablePlugins();
}

bool UCDebugManager::SelectDebugger(DebuggerType type) {
    auto plugin = UCDebuggerPluginRegistry::Instance().GetPlugin(type);
    if (!plugin || !plugin->IsAvailable()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    activeDebugger_ = plugin;
    return true;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void UCDebugManager::SetupDebuggerCallbacks() {
    if (!activeDebugger_) return;
    
    activeDebugger_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
        HandleStateChange(oldState, newState);
    };
    
    activeDebugger_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
        HandleTargetStop(reason, loc);
    };
    
    activeDebugger_->onBreakpointHit = [this](const Breakpoint& bp) {
        HandleBreakpointHit(bp);
    };
    
    activeDebugger_->onOutput = [this](const std::string& output) {
        HandleOutput(output);
    };
    
    activeDebugger_->onError = [this](const std::string& error) {
        HandleError(error);
    };
}

void UCDebugManager::HandleStateChange(DebugSessionState oldState, DebugSessionState newState) {
    lastState_ = newState;
    
    if (onStateChange) {
        onStateChange(oldState, newState);
    }
    
    DebugEvent event(DebugEventType::SessionStart);
    if (newState == DebugSessionState::Terminated) {
        event.type = DebugEventType::SessionEnd;
    }
    
    if (onDebugEvent) {
        onDebugEvent(event);
    }
}

void UCDebugManager::HandleTargetStop(StopReason reason, const SourceLocation& location) {
    // Refresh watches when stopped
    RefreshWatches();
    
    if (onTargetStop) {
        onTargetStop(reason, location);
    }
    
    DebugEvent event(DebugEventType::TargetStop);
    event.stopReason = reason;
    event.location = location;
    
    if (onDebugEvent) {
        onDebugEvent(event);
    }
}

void UCDebugManager::HandleBreakpointHit(const Breakpoint& bp) {
    // Find local breakpoint and update hit count
    auto* localBp = breakpointManager_.FindByDebuggerId(bp.debuggerId);
    if (localBp) {
        breakpointManager_.RecordBreakpointHit(localBp->GetId());
    }
    
    if (onBreakpointHit) {
        onBreakpointHit(bp);
    }
}

void UCDebugManager::HandleOutput(const std::string& output) {
    if (onOutput) {
        onOutput(output);
    }
}

void UCDebugManager::HandleError(const std::string& error) {
    if (onError) {
        onError(error);
    }
}

DebugLaunchConfig UCDebugManager::CreateLaunchConfig(std::shared_ptr<UCIDEProject> project) {
    DebugLaunchConfig config;
    
    // TODO: Extract from project
    // config.executablePath = project->GetOutputPath();
    // config.workingDirectory = project->GetRootDirectory();
    // config.arguments = project->GetRunArguments();
    
    config.debuggerType = config_.preferredDebugger;
    config.stopOnEntry = config_.stopOnEntry;
    
    return config;
}

// ============================================================================
// PLUGIN REGISTRATION
// ============================================================================

void RegisterBuiltInDebuggerPlugins() {
    auto& registry = UCDebuggerPluginRegistry::Instance();
    
    std::cout << "[DebugManager] Registering built-in debugger plugins..." << std::endl;
    
    // Register GDB plugin
    auto gdbPlugin = std::make_shared<UCGDBPlugin>();
    registry.RegisterPlugin(gdbPlugin);
    std::cout << "  - Registered GDB plugin";
    if (gdbPlugin->IsAvailable()) {
        std::cout << " [Available: " << gdbPlugin->GetDebuggerVersion() << "]";
    } else {
        std::cout << " [Not Available]";
    }
    std::cout << std::endl;
    
    // Register LLDB plugin
    auto lldbPlugin = std::make_shared<UCLLDBPlugin>();
    registry.RegisterPlugin(lldbPlugin);
    std::cout << "  - Registered LLDB plugin";
    if (lldbPlugin->IsAvailable()) {
        std::cout << " [Available: " << lldbPlugin->GetDebuggerVersion() << "]";
    } else {
        std::cout << " [Not Available]";
    }
    std::cout << std::endl;
}

} // namespace IDE
} // namespace UltraCanvas
