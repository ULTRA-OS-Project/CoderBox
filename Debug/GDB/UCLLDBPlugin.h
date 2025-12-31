// Apps/IDE/Debug/LLDB/UCLLDBPlugin.h
// LLDB Debugger Plugin for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Core/IUCDebuggerPlugin.h"
#include <thread>
#include <mutex>
#include <atomic>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief LLDB plugin configuration
 */
struct LLDBPluginConfig {
    std::string lldbPath;                   // Custom LLDB path
    std::vector<std::string> lldbArgs;      // Additional LLDB arguments
    int commandTimeout = 5000;              // Command timeout (ms)
    
    static LLDBPluginConfig Default();
};

/**
 * @brief LLDB debugger plugin implementation
 * 
 * Implements IUCDebuggerPlugin for LLVM Debugger (LLDB).
 * Uses LLDB's command-line interface for debugging operations.
 */
class UCLLDBPlugin : public IUCDebuggerPlugin {
public:
    UCLLDBPlugin();
    explicit UCLLDBPlugin(const LLDBPluginConfig& config);
    ~UCLLDBPlugin() override;
    
    // Plugin identification
    std::string GetPluginName() const override;
    std::string GetPluginVersion() const override;
    DebuggerType GetDebuggerType() const override;
    std::string GetDebuggerName() const override;
    
    // Debugger detection
    bool IsAvailable() override;
    std::string GetDebuggerPath() override;
    void SetDebuggerPath(const std::string& path) override;
    std::string GetDebuggerVersion() override;
    std::vector<std::string> GetSupportedExtensions() const override;
    
    // Session control
    bool Launch(const DebugLaunchConfig& config) override;
    bool Attach(int pid) override;
    bool AttachByName(const std::string& processName) override;
    bool ConnectRemote(const std::string& host, int port) override;
    bool LoadCoreDump(const std::string& corePath, const std::string& executablePath) override;
    bool Detach() override;
    bool Terminate() override;
    DebugSessionState GetSessionState() const override;
    bool IsSessionActive() const override;
    
    // Execution control
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
    
    // Breakpoints
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
    
    // Watchpoints
    Watchpoint AddWatchpoint(const std::string& expression,
                             WatchpointAccess access = WatchpointAccess::Write) override;
    bool RemoveWatchpoint(int watchpointId) override;
    std::vector<Watchpoint> GetWatchpoints() const override;
    
    // Threads
    std::vector<ThreadInfo> GetThreads() override;
    ThreadInfo GetCurrentThread() override;
    bool SwitchThread(int threadId) override;
    
    // Stack
    std::vector<StackFrame> GetCallStack(int maxFrames = 100) override;
    std::vector<StackFrame> GetCallStack(int threadId, int maxFrames = 100) override;
    bool SwitchFrame(int frameLevel) override;
    StackFrame GetCurrentFrame() override;
    
    // Variables
    std::vector<Variable> GetLocals() override;
    std::vector<Variable> GetArguments() override;
    std::vector<Variable> GetGlobals() override;
    EvaluationResult Evaluate(const std::string& expression) override;
    EvaluationResult Evaluate(const std::string& expression, EvaluationContext context) override;
    bool SetVariable(const std::string& name, const std::string& value) override;
    std::vector<Variable> ExpandVariable(int variablesReference) override;
    
    // Registers
    std::vector<Register> GetRegisters() override;
    std::vector<Register> GetRegisters(RegisterCategory category) override;
    bool SetRegister(const std::string& name, const std::string& value) override;
    
    // Memory
    MemoryRegion ReadMemory(uint64_t address, size_t size) override;
    bool WriteMemory(uint64_t address, const std::vector<uint8_t>& data) override;
    
    // Disassembly
    std::vector<DisassemblyLine> GetDisassembly(uint64_t address, int instructionCount) override;
    std::vector<DisassemblyLine> GetFunctionDisassembly(const std::string& functionName) override;
    
    // Modules
    std::vector<ModuleInfo> GetModules() override;
    
    // Completions
    std::vector<DebugCompletionItem> GetCompletions(const std::string& text, int column) override;
    
private:
    std::string SendCommand(const std::string& command);
    void ProcessOutput(const std::string& output);
    void SetState(DebugSessionState newState);
    void Cleanup();
    
    LLDBPluginConfig config_;
    std::string lldbPath_;
    std::string lldbVersion_;
    bool available_ = false;
    
    DebugSessionState state_ = DebugSessionState::Inactive;
    int currentThreadId_ = 1;
    int currentFrameLevel_ = 0;
    
    std::map<int, Breakpoint> breakpoints_;
    std::map<int, Watchpoint> watchpoints_;
    int nextBreakpointId_ = 1;
    int nextWatchpointId_ = 1;
    
    // Process handles (platform specific)
#ifdef _WIN32
    void* processHandle_ = nullptr;
#else
    int pid_ = -1;
    int stdinFd_ = -1;
    int stdoutFd_ = -1;
#endif
    
    std::thread outputThread_;
    std::atomic<bool> stopOutputThread_{false};
    mutable std::mutex mutex_;
};

// Helper functions
std::string FindLLDBExecutable();
std::string GetLLDBVersion(const std::string& lldbPath);

} // namespace IDE
} // namespace UltraCanvas
