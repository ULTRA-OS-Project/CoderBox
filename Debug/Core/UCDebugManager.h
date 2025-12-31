// Apps/IDE/Debug/Core/UCDebugManager.h
// Debug Session Manager for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "IUCDebuggerPlugin.h"
#include "UCDebuggerPluginRegistry.h"
#include "../Breakpoints/UCBreakpointManager.h"
#include "../Breakpoints/UCWatchManager.h"
#include "../Stack/UCStackFrame.h"
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace UltraCanvas {
namespace IDE {

// Forward declaration
class UCIDEProject;

/**
 * @brief Debug manager configuration
 */
struct DebugManagerConfig {
    DebuggerType preferredDebugger = DebuggerType::UnknownDebugger;
    std::string gdbPath;
    std::string lldbPath;
    
    bool autoSelectDebugger = true;
    bool saveBreakpointsOnExit = true;
    bool loadBreakpointsOnStart = true;
    bool confirmOnTerminate = true;
    bool stopOnEntry = false;
    
    int maxCallStackDepth = 100;
    int maxVariableChildren = 100;
    int launchTimeout = 10000;
    int commandTimeout = 5000;
    
    static DebugManagerConfig Default();
};

/**
 * @brief Singleton debug session manager
 * 
 * Orchestrates debug sessions, manages debugger plugins, breakpoints,
 * and provides the main interface for IDE debug operations.
 */
class UCDebugManager {
public:
    static UCDebugManager& Instance();
    
    // Prevent copying
    UCDebugManager(const UCDebugManager&) = delete;
    UCDebugManager& operator=(const UCDebugManager&) = delete;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    bool Initialize();
    bool Initialize(const DebugManagerConfig& config);
    void Shutdown();
    bool IsInitialized() const { return initialized_; }
    
    // =========================================================================
    // SESSION CONTROL
    // =========================================================================
    
    bool StartDebugging(const DebugLaunchConfig& config);
    bool StartDebugging(std::shared_ptr<UCIDEProject> project);
    bool AttachToProcess(int pid);
    bool AttachToProcess(const std::string& processName);
    bool LoadCoreDump(const std::string& corePath, const std::string& execPath);
    
    bool StopDebugging();
    bool RestartDebugging();
    bool Detach();
    
    bool IsDebugging() const;
    DebugSessionState GetSessionState() const;
    
    // =========================================================================
    // EXECUTION CONTROL
    // =========================================================================
    
    bool Continue();
    bool Pause();
    bool StepOver();
    bool StepInto();
    bool StepOut();
    bool StepInstruction();
    bool RunToCursor(const std::string& file, int line);
    bool SetNextStatement(const std::string& file, int line);
    
    // =========================================================================
    // BREAKPOINTS
    // =========================================================================
    
    int AddBreakpoint(const std::string& file, int line);
    int AddFunctionBreakpoint(const std::string& functionName);
    int AddConditionalBreakpoint(const std::string& file, int line, const std::string& condition);
    int AddLogpoint(const std::string& file, int line, const std::string& logMessage);
    
    bool RemoveBreakpoint(int id);
    bool RemoveBreakpoint(const std::string& file, int line);
    int ToggleBreakpoint(const std::string& file, int line);
    
    bool SetBreakpointEnabled(int id, bool enabled);
    bool SetBreakpointCondition(int id, const std::string& condition);
    
    std::vector<Breakpoint> GetBreakpoints() const;
    bool HasBreakpointAt(const std::string& file, int line) const;
    std::set<int> GetBreakpointLines(const std::string& file) const;
    
    UCBreakpointManager& GetBreakpointManager() { return breakpointManager_; }
    const UCBreakpointManager& GetBreakpointManager() const { return breakpointManager_; }
    
    // =========================================================================
    // WATCHPOINTS
    // =========================================================================
    
    int AddWatchpoint(const std::string& expression, WatchpointAccess access = WatchpointAccess::Write);
    bool RemoveWatchpoint(int id);
    std::vector<Watchpoint> GetWatchpoints() const;
    
    // =========================================================================
    // WATCH EXPRESSIONS
    // =========================================================================
    
    int AddWatch(const std::string& expression);
    bool RemoveWatch(int id);
    void RefreshWatches();
    
    UCWatchManager& GetWatchManager() { return watchManager_; }
    const UCWatchManager& GetWatchManager() const { return watchManager_; }
    
    // =========================================================================
    // THREADS & STACK
    // =========================================================================
    
    std::vector<ThreadInfo> GetThreads();
    ThreadInfo GetCurrentThread();
    bool SwitchThread(int threadId);
    
    std::vector<StackFrame> GetCallStack(int maxFrames = 100);
    StackFrame GetCurrentFrame();
    bool SwitchFrame(int frameLevel);
    
    // =========================================================================
    // VARIABLES
    // =========================================================================
    
    std::vector<Variable> GetLocals();
    std::vector<Variable> GetArguments();
    std::vector<Variable> GetGlobals();
    
    EvaluationResult Evaluate(const std::string& expression);
    bool SetVariable(const std::string& name, const std::string& value);
    std::vector<Variable> ExpandVariable(int variablesReference);
    
    // =========================================================================
    // REGISTERS & MEMORY
    // =========================================================================
    
    std::vector<Register> GetRegisters();
    bool SetRegister(const std::string& name, const std::string& value);
    
    MemoryRegion ReadMemory(uint64_t address, size_t size);
    bool WriteMemory(uint64_t address, const std::vector<uint8_t>& data);
    
    // =========================================================================
    // DISASSEMBLY
    // =========================================================================
    
    std::vector<DisassemblyLine> GetDisassembly(uint64_t address, int count);
    std::vector<DisassemblyLine> GetFunctionDisassembly(const std::string& function);
    
    // =========================================================================
    // MODULES
    // =========================================================================
    
    std::vector<ModuleInfo> GetModules();
    
    // =========================================================================
    // DEBUGGER SELECTION
    // =========================================================================
    
    std::shared_ptr<IUCDebuggerPlugin> GetActiveDebugger() const { return activeDebugger_; }
    std::shared_ptr<IUCDebuggerPlugin> GetBestDebugger();
    std::vector<std::shared_ptr<IUCDebuggerPlugin>> GetAvailableDebuggers();
    bool SelectDebugger(DebuggerType type);
    
    // =========================================================================
    // CONFIGURATION
    // =========================================================================
    
    const DebugManagerConfig& GetConfig() const { return config_; }
    void SetConfig(const DebugManagerConfig& config) { config_ = config; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const DebugEvent&)> onDebugEvent;
    std::function<void(DebugSessionState, DebugSessionState)> onStateChange;
    std::function<void(StopReason, const SourceLocation&)> onTargetStop;
    std::function<void()> onTargetContinue;
    std::function<void(const Breakpoint&)> onBreakpointHit;
    std::function<void(const std::string&)> onOutput;
    std::function<void(const std::string&)> onError;
    
private:
    UCDebugManager();
    ~UCDebugManager();
    
    void SetupDebuggerCallbacks();
    void HandleStateChange(DebugSessionState oldState, DebugSessionState newState);
    void HandleTargetStop(StopReason reason, const SourceLocation& location);
    void HandleBreakpointHit(const Breakpoint& bp);
    void HandleOutput(const std::string& output);
    void HandleError(const std::string& error);
    
    DebugLaunchConfig CreateLaunchConfig(std::shared_ptr<UCIDEProject> project);
    
    bool initialized_ = false;
    DebugManagerConfig config_;
    
    std::shared_ptr<IUCDebuggerPlugin> activeDebugger_;
    std::shared_ptr<UCIDEProject> currentProject_;
    DebugLaunchConfig currentLaunchConfig_;
    
    UCBreakpointManager breakpointManager_;
    UCWatchManager watchManager_;
    
    DebugSessionState lastState_ = DebugSessionState::Inactive;
    
    mutable std::mutex mutex_;
};

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

inline UCDebugManager& GetDebugManager() {
    return UCDebugManager::Instance();
}

void RegisterBuiltInDebuggerPlugins();

} // namespace IDE
} // namespace UltraCanvas
