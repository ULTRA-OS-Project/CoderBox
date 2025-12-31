// Apps/IDE/Debug/GDB/UCGDBPlugin.h
// GDB Debugger Plugin for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Core/IUCDebuggerPlugin.h"
#include "UCGDBMIParser.h"
#include "UCGDBProcess.h"
#include <thread>
#include <queue>
#include <condition_variable>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief GDB plugin configuration
 */
struct GDBPluginConfig {
    std::string gdbPath;                    // Custom GDB path
    std::vector<std::string> gdbArgs;       // Additional GDB arguments
    bool useMI = true;                      // Use GDB/MI interface
    bool prettyPrinting = true;             // Enable pretty printing
    bool asyncMode = true;                  // Use async/non-stop mode
    int commandTimeout = 5000;              // Command timeout (ms)
    
    static GDBPluginConfig Default();
};

/**
 * @brief GDB debugger plugin implementation
 * 
 * Implements IUCDebuggerPlugin for GNU Debugger (GDB) using the GDB/MI protocol.
 * Supports all standard debugging operations including breakpoints, stepping,
 * variable inspection, and memory access.
 */
class UCGDBPlugin : public IUCDebuggerPlugin {
public:
    UCGDBPlugin();
    explicit UCGDBPlugin(const GDBPluginConfig& config);
    ~UCGDBPlugin() override;
    
    // =========================================================================
    // PLUGIN IDENTIFICATION
    // =========================================================================
    
    std::string GetPluginName() const override;
    std::string GetPluginVersion() const override;
    DebuggerType GetDebuggerType() const override;
    std::string GetDebuggerName() const override;
    
    // =========================================================================
    // DEBUGGER DETECTION
    // =========================================================================
    
    bool IsAvailable() override;
    std::string GetDebuggerPath() override;
    void SetDebuggerPath(const std::string& path) override;
    std::string GetDebuggerVersion() override;
    std::vector<std::string> GetSupportedExtensions() const override;
    
    // =========================================================================
    // SESSION CONTROL
    // =========================================================================
    
    bool Launch(const DebugLaunchConfig& config) override;
    bool Attach(int pid) override;
    bool AttachByName(const std::string& processName) override;
    bool ConnectRemote(const std::string& host, int port) override;
    bool LoadCoreDump(const std::string& corePath, const std::string& executablePath) override;
    bool Detach() override;
    bool Terminate() override;
    DebugSessionState GetSessionState() const override;
    bool IsSessionActive() const override;
    
    // =========================================================================
    // EXECUTION CONTROL
    // =========================================================================
    
    bool Continue() override;
    bool Pause() override;
    bool StepOver() override;
    bool StepInto() override;
    bool StepOut() override;
    bool StepTo(const std::string& file, int line) override;
    bool RunToCursor(const std::string& file, int line) override;
    bool SetNextStatement(const std::string& file, int line) override;
    bool StepInstruction() override;
    bool StepIntoInstruction() override;
    
    // =========================================================================
    // BREAKPOINTS
    // =========================================================================
    
    Breakpoint AddBreakpoint(const std::string& file, int line) override;
    Breakpoint AddFunctionBreakpoint(const std::string& functionName) override;
    Breakpoint AddConditionalBreakpoint(const std::string& file, int line,
                                         const std::string& condition) override;
    Breakpoint AddAddressBreakpoint(uint64_t address) override;
    Breakpoint AddLogpoint(const std::string& file, int line,
                           const std::string& logMessage) override;
    bool RemoveBreakpoint(int breakpointId) override;
    bool SetBreakpointEnabled(int breakpointId, bool enabled) override;
    bool SetBreakpointCondition(int breakpointId, const std::string& condition) override;
    bool SetBreakpointIgnoreCount(int breakpointId, int count) override;
    std::vector<Breakpoint> GetBreakpoints() const override;
    Breakpoint GetBreakpoint(int breakpointId) const override;
    
    // =========================================================================
    // WATCHPOINTS
    // =========================================================================
    
    Watchpoint AddWatchpoint(const std::string& expression,
                             WatchpointAccess access = WatchpointAccess::Write) override;
    bool RemoveWatchpoint(int watchpointId) override;
    std::vector<Watchpoint> GetWatchpoints() const override;
    
    // =========================================================================
    // THREADS
    // =========================================================================
    
    std::vector<ThreadInfo> GetThreads() override;
    ThreadInfo GetCurrentThread() override;
    bool SwitchThread(int threadId) override;
    
    // =========================================================================
    // STACK
    // =========================================================================
    
    std::vector<StackFrame> GetCallStack(int maxFrames = 100) override;
    std::vector<StackFrame> GetCallStack(int threadId, int maxFrames = 100) override;
    bool SwitchFrame(int frameLevel) override;
    StackFrame GetCurrentFrame() override;
    
    // =========================================================================
    // VARIABLES
    // =========================================================================
    
    std::vector<Variable> GetLocals() override;
    std::vector<Variable> GetArguments() override;
    std::vector<Variable> GetGlobals() override;
    EvaluationResult Evaluate(const std::string& expression) override;
    EvaluationResult Evaluate(const std::string& expression, EvaluationContext context) override;
    bool SetVariable(const std::string& name, const std::string& value) override;
    std::vector<Variable> ExpandVariable(int variablesReference) override;
    
    // =========================================================================
    // REGISTERS
    // =========================================================================
    
    std::vector<Register> GetRegisters() override;
    std::vector<Register> GetRegisters(RegisterCategory category) override;
    bool SetRegister(const std::string& name, const std::string& value) override;
    
    // =========================================================================
    // MEMORY
    // =========================================================================
    
    MemoryRegion ReadMemory(uint64_t address, size_t size) override;
    bool WriteMemory(uint64_t address, const std::vector<uint8_t>& data) override;
    
    // =========================================================================
    // DISASSEMBLY
    // =========================================================================
    
    std::vector<DisassemblyLine> GetDisassembly(uint64_t address, int instructionCount) override;
    std::vector<DisassemblyLine> GetFunctionDisassembly(const std::string& functionName) override;
    
    // =========================================================================
    // MODULES
    // =========================================================================
    
    std::vector<ModuleInfo> GetModules() override;
    
    // =========================================================================
    // COMPLETIONS
    // =========================================================================
    
    std::vector<DebugCompletionItem> GetCompletions(const std::string& text, int column) override;
    
private:
    // Command execution
    GDBMIRecord SendCommand(const std::string& command);
    GDBMIRecord SendCommandAsync(const std::string& command);
    bool SendCommandNoWait(const std::string& command);
    int GetNextToken();
    
    // Response handling
    void ProcessOutput();
    void HandleAsyncRecord(const GDBMIRecord& record);
    void HandleStopEvent(const GDBMIRecord& record);
    void HandleRunningEvent(const GDBMIRecord& record);
    void HandleBreakpointEvent(const GDBMIRecord& record);
    void HandleThreadEvent(const GDBMIRecord& record);
    void HandleLibraryEvent(const GDBMIRecord& record);
    
    // Parsing helpers
    Breakpoint ParseBreakpointResult(const GDBMIRecord& record);
    StackFrame ParseFrameResult(const std::map<std::string, GDBMIValue>& frame);
    Variable ParseVariableResult(const std::map<std::string, GDBMIValue>& var);
    ThreadInfo ParseThreadResult(const std::map<std::string, GDBMIValue>& thread);
    
    // State management
    void SetState(DebugSessionState newState);
    void Cleanup();
    
    // Configuration
    GDBPluginConfig config_;
    std::string gdbPath_;
    std::string gdbVersion_;
    bool available_ = false;
    
    // Process
    std::unique_ptr<UCGDBProcess> process_;
    std::unique_ptr<UCGDBMIParser> parser_;
    
    // State
    DebugSessionState state_ = DebugSessionState::Inactive;
    int currentThreadId_ = 1;
    int currentFrameLevel_ = 0;
    
    // Command handling
    std::atomic<int> nextToken_{1};
    std::map<int, std::promise<GDBMIRecord>> pendingCommands_;
    std::mutex commandMutex_;
    
    // Breakpoints
    std::map<int, Breakpoint> breakpoints_;
    std::map<int, Watchpoint> watchpoints_;
    int nextBreakpointId_ = 1;
    int nextWatchpointId_ = 1;
    
    // Variable objects (for expansion)
    std::map<int, std::string> variableObjects_;
    int nextVariableRef_ = 1;
    
    // Output processing thread
    std::thread outputThread_;
    std::atomic<bool> stopOutputThread_{false};
    
    mutable std::mutex stateMutex_;
};

} // namespace IDE
} // namespace UltraCanvas
