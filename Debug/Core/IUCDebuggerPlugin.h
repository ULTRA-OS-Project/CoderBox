// Apps/IDE/Debug/Core/IUCDebuggerPlugin.h
// Abstract Debugger Plugin Interface for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Types/UCDebugEnums.h"
#include "../Types/UCDebugStructs.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// ABSTRACT DEBUGGER PLUGIN INTERFACE
// ============================================================================

/**
 * @brief Abstract base class for all debugger plugins
 * 
 * This interface defines the contract that all debugger backends must implement.
 * Modeled after IUCCompilerPlugin for consistency with ULTRA IDE architecture.
 * 
 * Implementations:
 * - UCGDBPlugin: GNU Debugger (GDB) via GDB/MI protocol
 * - UCLLDBPlugin: LLVM Debugger (LLDB) via LLDB API
 */
class IUCDebuggerPlugin {
public:
    virtual ~IUCDebuggerPlugin() = default;
    
    // =========================================================================
    // PLUGIN IDENTIFICATION
    // =========================================================================
    
    /**
     * @brief Get the plugin name
     * @return Human-readable plugin name (e.g., "GDB Debugger Plugin")
     */
    virtual std::string GetPluginName() const = 0;
    
    /**
     * @brief Get the plugin version
     * @return Version string (e.g., "1.0.0")
     */
    virtual std::string GetPluginVersion() const = 0;
    
    /**
     * @brief Get the debugger type
     * @return DebuggerType enum value
     */
    virtual DebuggerType GetDebuggerType() const = 0;
    
    /**
     * @brief Get the debugger name
     * @return Debugger name (e.g., "GDB", "LLDB")
     */
    virtual std::string GetDebuggerName() const = 0;
    
    // =========================================================================
    // DEBUGGER DETECTION
    // =========================================================================
    
    /**
     * @brief Check if the debugger is available on this system
     * @return true if debugger executable is found and usable
     */
    virtual bool IsAvailable() = 0;
    
    /**
     * @brief Get the path to the debugger executable
     * @return Full path to debugger, or empty string if not found
     */
    virtual std::string GetDebuggerPath() = 0;
    
    /**
     * @brief Set a custom path to the debugger executable
     * @param path Path to debugger executable
     */
    virtual void SetDebuggerPath(const std::string& path) = 0;
    
    /**
     * @brief Get the debugger version string
     * @return Version string from the debugger (e.g., "GNU gdb 13.2")
     */
    virtual std::string GetDebuggerVersion() = 0;
    
    /**
     * @brief Get supported file extensions for debugging
     * @return List of extensions (e.g., {".exe", "", ".out"})
     */
    virtual std::vector<std::string> GetSupportedExtensions() const = 0;
    
    // =========================================================================
    // SESSION CONTROL
    // =========================================================================
    
    /**
     * @brief Launch a new debug session
     * @param config Launch configuration
     * @return true if launch initiated successfully
     */
    virtual bool Launch(const DebugLaunchConfig& config) = 0;
    
    /**
     * @brief Attach to a running process by PID
     * @param pid Process ID to attach to
     * @return true if attach initiated successfully
     */
    virtual bool Attach(int pid) = 0;
    
    /**
     * @brief Attach to a running process by name
     * @param processName Process name to find and attach to
     * @return true if attach initiated successfully
     */
    virtual bool AttachByName(const std::string& processName) = 0;
    
    /**
     * @brief Connect to a remote debugging target
     * @param host Remote host address
     * @param port Remote port number
     * @return true if connection initiated successfully
     */
    virtual bool ConnectRemote(const std::string& host, int port) = 0;
    
    /**
     * @brief Load and analyze a core dump file
     * @param corePath Path to core dump file
     * @param executablePath Path to the executable that generated the core
     * @return true if core dump loaded successfully
     */
    virtual bool LoadCoreDump(const std::string& corePath, 
                              const std::string& executablePath) = 0;
    
    /**
     * @brief Detach from the current debug target without terminating it
     * @return true if detach successful
     */
    virtual bool Detach() = 0;
    
    /**
     * @brief Terminate the debug session and target process
     * @return true if termination successful
     */
    virtual bool Terminate() = 0;
    
    /**
     * @brief Get the current session state
     * @return Current DebugSessionState
     */
    virtual DebugSessionState GetSessionState() const = 0;
    
    /**
     * @brief Check if a debug session is currently active
     * @return true if session is active (not Inactive or Terminated)
     */
    virtual bool IsSessionActive() const = 0;
    
    // =========================================================================
    // EXECUTION CONTROL
    // =========================================================================
    
    /**
     * @brief Continue execution from current position
     * @return true if continue command sent successfully
     */
    virtual bool Continue() = 0;
    
    /**
     * @brief Pause/interrupt the running target
     * @return true if pause command sent successfully
     */
    virtual bool Pause() = 0;
    
    /**
     * @brief Step over the current line (step to next line, skip function calls)
     * @return true if step command sent successfully
     */
    virtual bool StepOver() = 0;
    
    /**
     * @brief Step into the current line (enter function calls)
     * @return true if step command sent successfully
     */
    virtual bool StepInto() = 0;
    
    /**
     * @brief Step out of the current function
     * @return true if step command sent successfully
     */
    virtual bool StepOut() = 0;
    
    /**
     * @brief Step to a specific source location
     * @param file Source file path
     * @param line Line number
     * @return true if step command sent successfully
     */
    virtual bool StepTo(const std::string& file, int line) = 0;
    
    /**
     * @brief Run to cursor position (temporary breakpoint)
     * @param file Source file path
     * @param line Line number
     * @return true if command sent successfully
     */
    virtual bool RunToCursor(const std::string& file, int line) = 0;
    
    /**
     * @brief Set the next statement to execute (jump)
     * @param file Source file path
     * @param line Line number
     * @return true if command sent successfully
     */
    virtual bool SetNextStatement(const std::string& file, int line) = 0;
    
    /**
     * @brief Step over a single instruction (assembly level)
     * @return true if step command sent successfully
     */
    virtual bool StepInstruction() = 0;
    
    /**
     * @brief Step into a single instruction (assembly level)
     * @return true if step command sent successfully
     */
    virtual bool StepIntoInstruction() = 0;
    
    // =========================================================================
    // BREAKPOINTS
    // =========================================================================
    
    /**
     * @brief Add a line breakpoint
     * @param file Source file path
     * @param line Line number
     * @return Breakpoint structure with assigned ID
     */
    virtual Breakpoint AddBreakpoint(const std::string& file, int line) = 0;
    
    /**
     * @brief Add a function breakpoint
     * @param functionName Function name to break on
     * @return Breakpoint structure with assigned ID
     */
    virtual Breakpoint AddFunctionBreakpoint(const std::string& functionName) = 0;
    
    /**
     * @brief Add a conditional breakpoint
     * @param file Source file path
     * @param line Line number
     * @param condition Condition expression
     * @return Breakpoint structure with assigned ID
     */
    virtual Breakpoint AddConditionalBreakpoint(const std::string& file, int line,
                                                 const std::string& condition) = 0;
    
    /**
     * @brief Add a breakpoint at a memory address
     * @param address Memory address
     * @return Breakpoint structure with assigned ID
     */
    virtual Breakpoint AddAddressBreakpoint(uint64_t address) = 0;
    
    /**
     * @brief Add a logpoint (breakpoint that logs message without stopping)
     * @param file Source file path
     * @param line Line number
     * @param logMessage Message to log (can contain expressions in {})
     * @return Breakpoint structure with assigned ID
     */
    virtual Breakpoint AddLogpoint(const std::string& file, int line,
                                   const std::string& logMessage) = 0;
    
    /**
     * @brief Remove a breakpoint
     * @param breakpointId Breakpoint ID to remove
     * @return true if removal successful
     */
    virtual bool RemoveBreakpoint(int breakpointId) = 0;
    
    /**
     * @brief Enable or disable a breakpoint
     * @param breakpointId Breakpoint ID
     * @param enabled true to enable, false to disable
     * @return true if operation successful
     */
    virtual bool SetBreakpointEnabled(int breakpointId, bool enabled) = 0;
    
    /**
     * @brief Set or update breakpoint condition
     * @param breakpointId Breakpoint ID
     * @param condition Condition expression (empty to clear)
     * @return true if operation successful
     */
    virtual bool SetBreakpointCondition(int breakpointId, 
                                        const std::string& condition) = 0;
    
    /**
     * @brief Set breakpoint ignore count
     * @param breakpointId Breakpoint ID
     * @param count Number of hits to ignore
     * @return true if operation successful
     */
    virtual bool SetBreakpointIgnoreCount(int breakpointId, int count) = 0;
    
    /**
     * @brief Get all breakpoints
     * @return Vector of all breakpoints
     */
    virtual std::vector<Breakpoint> GetBreakpoints() const = 0;
    
    /**
     * @brief Get a specific breakpoint
     * @param breakpointId Breakpoint ID
     * @return Breakpoint structure (invalid if not found)
     */
    virtual Breakpoint GetBreakpoint(int breakpointId) const = 0;
    
    // =========================================================================
    // WATCHPOINTS
    // =========================================================================
    
    /**
     * @brief Add a watchpoint (data breakpoint)
     * @param expression Expression to watch
     * @param access Access type (read, write, or both)
     * @return Watchpoint structure with assigned ID
     */
    virtual Watchpoint AddWatchpoint(const std::string& expression,
                                     WatchpointAccess access = WatchpointAccess::Write) = 0;
    
    /**
     * @brief Remove a watchpoint
     * @param watchpointId Watchpoint ID
     * @return true if removal successful
     */
    virtual bool RemoveWatchpoint(int watchpointId) = 0;
    
    /**
     * @brief Get all watchpoints
     * @return Vector of all watchpoints
     */
    virtual std::vector<Watchpoint> GetWatchpoints() const = 0;
    
    // =========================================================================
    // THREADS
    // =========================================================================
    
    /**
     * @brief Get all threads
     * @return Vector of thread information
     */
    virtual std::vector<ThreadInfo> GetThreads() = 0;
    
    /**
     * @brief Get the current (selected) thread
     * @return Current thread info
     */
    virtual ThreadInfo GetCurrentThread() = 0;
    
    /**
     * @brief Switch to a different thread
     * @param threadId Thread ID to switch to
     * @return true if switch successful
     */
    virtual bool SwitchThread(int threadId) = 0;
    
    // =========================================================================
    // STACK
    // =========================================================================
    
    /**
     * @brief Get the call stack for the current thread
     * @param maxFrames Maximum number of frames to retrieve
     * @return Vector of stack frames (index 0 = top of stack)
     */
    virtual std::vector<StackFrame> GetCallStack(int maxFrames = 100) = 0;
    
    /**
     * @brief Get the call stack for a specific thread
     * @param threadId Thread ID
     * @param maxFrames Maximum number of frames
     * @return Vector of stack frames
     */
    virtual std::vector<StackFrame> GetCallStack(int threadId, int maxFrames = 100) = 0;
    
    /**
     * @brief Switch to a different stack frame
     * @param frameLevel Frame level (0 = top)
     * @return true if switch successful
     */
    virtual bool SwitchFrame(int frameLevel) = 0;
    
    /**
     * @brief Get the current stack frame
     * @return Current frame info
     */
    virtual StackFrame GetCurrentFrame() = 0;
    
    // =========================================================================
    // VARIABLES
    // =========================================================================
    
    /**
     * @brief Get local variables for the current frame
     * @return Vector of local variables
     */
    virtual std::vector<Variable> GetLocals() = 0;
    
    /**
     * @brief Get function arguments for the current frame
     * @return Vector of argument variables
     */
    virtual std::vector<Variable> GetArguments() = 0;
    
    /**
     * @brief Get global variables
     * @return Vector of global variables
     */
    virtual std::vector<Variable> GetGlobals() = 0;
    
    /**
     * @brief Evaluate an expression
     * @param expression Expression to evaluate
     * @return Evaluation result
     */
    virtual EvaluationResult Evaluate(const std::string& expression) = 0;
    
    /**
     * @brief Evaluate expression in a specific context
     * @param expression Expression to evaluate
     * @param context Evaluation context
     * @return Evaluation result
     */
    virtual EvaluationResult Evaluate(const std::string& expression,
                                      EvaluationContext context) = 0;
    
    /**
     * @brief Set a variable's value
     * @param name Variable name or expression
     * @param value New value
     * @return true if assignment successful
     */
    virtual bool SetVariable(const std::string& name, const std::string& value) = 0;
    
    /**
     * @brief Expand a variable to get its children
     * @param variablesReference Reference from parent variable
     * @return Vector of child variables
     */
    virtual std::vector<Variable> ExpandVariable(int variablesReference) = 0;
    
    // =========================================================================
    // REGISTERS
    // =========================================================================
    
    /**
     * @brief Get all CPU registers
     * @return Vector of registers
     */
    virtual std::vector<Register> GetRegisters() = 0;
    
    /**
     * @brief Get registers by category
     * @param category Register category
     * @return Vector of registers in that category
     */
    virtual std::vector<Register> GetRegisters(RegisterCategory category) = 0;
    
    /**
     * @brief Set a register value
     * @param name Register name
     * @param value New value
     * @return true if assignment successful
     */
    virtual bool SetRegister(const std::string& name, const std::string& value) = 0;
    
    // =========================================================================
    // MEMORY
    // =========================================================================
    
    /**
     * @brief Read memory from target
     * @param address Start address
     * @param size Number of bytes to read
     * @return Memory region with data
     */
    virtual MemoryRegion ReadMemory(uint64_t address, size_t size) = 0;
    
    /**
     * @brief Write memory to target
     * @param address Start address
     * @param data Data to write
     * @return true if write successful
     */
    virtual bool WriteMemory(uint64_t address, const std::vector<uint8_t>& data) = 0;
    
    // =========================================================================
    // DISASSEMBLY
    // =========================================================================
    
    /**
     * @brief Get disassembly at an address
     * @param address Start address
     * @param instructionCount Number of instructions
     * @return Vector of disassembly lines
     */
    virtual std::vector<DisassemblyLine> GetDisassembly(uint64_t address, 
                                                         int instructionCount) = 0;
    
    /**
     * @brief Get disassembly for a function
     * @param functionName Function name
     * @return Vector of disassembly lines
     */
    virtual std::vector<DisassemblyLine> GetFunctionDisassembly(
        const std::string& functionName) = 0;
    
    // =========================================================================
    // MODULES
    // =========================================================================
    
    /**
     * @brief Get loaded modules/libraries
     * @return Vector of module information
     */
    virtual std::vector<ModuleInfo> GetModules() = 0;
    
    // =========================================================================
    // COMPLETIONS
    // =========================================================================
    
    /**
     * @brief Get code completions for debug console
     * @param text Current input text
     * @param column Cursor position
     * @return Vector of completion items
     */
    virtual std::vector<DebugCompletionItem> GetCompletions(
        const std::string& text, int column) = 0;
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    // Event callbacks (use base verb forms per UltraCanvas conventions)
    std::function<void(const DebugEvent&)> onDebugEvent;
    std::function<void(DebugSessionState, DebugSessionState)> onStateChange;
    std::function<void(const Breakpoint&)> onBreakpointHit;
    std::function<void(const Breakpoint&)> onBreakpointChange;
    std::function<void(StopReason, const SourceLocation&)> onTargetStop;
    std::function<void()> onTargetContinue;
    std::function<void(const std::string&)> onOutput;
    std::function<void(const std::string&)> onError;
    std::function<void(const ExceptionInfo&)> onException;
    std::function<void(const ThreadInfo&)> onThreadCreate;
    std::function<void(int)> onThreadExit;
    std::function<void(const ModuleInfo&)> onModuleLoad;
    std::function<void(const std::string&)> onModuleUnload;
    
protected:
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> sessionActive{false};
};

} // namespace IDE
} // namespace UltraCanvas
