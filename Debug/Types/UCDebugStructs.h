// Apps/IDE/Debug/Types/UCDebugStructs.h
// Debug Data Structures for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCDebugEnums.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>
#include <functional>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// SOURCE LOCATION
// ============================================================================

/**
 * @brief Source code location (file + line + column)
 */
struct SourceLocation {
    std::string filePath;           // Absolute or relative file path
    std::string fileName;           // Just the filename
    int line = 0;                   // 1-based line number
    int column = 0;                 // 1-based column number
    int endLine = 0;                // End line (for ranges)
    int endColumn = 0;              // End column (for ranges)
    
    SourceLocation() = default;
    
    SourceLocation(const std::string& path, int ln, int col = 0)
        : filePath(path), line(ln), column(col) {
        // Extract filename
        size_t pos = path.find_last_of("/\\");
        fileName = (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }
    
    bool IsValid() const {
        return !filePath.empty() && line > 0;
    }
    
    std::string ToString() const {
        std::string result = filePath + ":" + std::to_string(line);
        if (column > 0) {
            result += ":" + std::to_string(column);
        }
        return result;
    }
    
    std::string ToShortString() const {
        std::string result = fileName + ":" + std::to_string(line);
        if (column > 0) {
            result += ":" + std::to_string(column);
        }
        return result;
    }
    
    bool operator==(const SourceLocation& other) const {
        return filePath == other.filePath && line == other.line;
    }
    
    bool operator!=(const SourceLocation& other) const {
        return !(*this == other);
    }
    
    bool operator<(const SourceLocation& other) const {
        if (filePath != other.filePath) return filePath < other.filePath;
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

// ============================================================================
// BREAKPOINT
// ============================================================================

/**
 * @brief Breakpoint information
 */
struct Breakpoint {
    int id = -1;                            // Local breakpoint ID
    int debuggerId = -1;                    // ID assigned by debugger backend
    BreakpointType type = BreakpointType::LineBreakpoint;
    BreakpointState state = BreakpointState::Pending;
    
    // Location information
    SourceLocation location;
    std::string functionName;               // For function breakpoints
    uint64_t address = 0;                   // For address breakpoints
    std::string moduleName;                 // Module/library name
    
    // Conditions
    std::string condition;                  // Conditional expression
    int ignoreCount = 0;                    // Skip first N hits
    int hitCount = 0;                       // Times breakpoint was hit
    
    // Logpoint
    std::string logMessage;                 // Message to log (for logpoints)
    
    // State
    bool enabled = true;
    bool verified = false;                  // Debugger confirmed location valid
    bool pending = true;                    // Not yet set in debugger
    std::string errorMessage;               // Error if state is Invalid
    
    // Timestamps
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point lastHitAt;
    
    Breakpoint() {
        createdAt = std::chrono::system_clock::now();
    }
    
    bool IsValid() const {
        return id >= 0 && state != BreakpointState::Invalid;
    }
    
    bool IsActive() const {
        return state == BreakpointState::Active && enabled;
    }
    
    bool IsPending() const {
        return state == BreakpointState::Pending || pending;
    }
    
    std::string GetDisplayText() const {
        std::string text;
        switch (type) {
            case BreakpointType::LineBreakpoint:
                text = location.ToShortString();
                break;
            case BreakpointType::FunctionBreakpoint:
                text = functionName + "()";
                break;
            case BreakpointType::AddressBreakpoint:
                text = "0x" + std::to_string(address);
                break;
            case BreakpointType::Logpoint:
                text = location.ToShortString() + " [Log]";
                break;
            default:
                text = location.ToShortString();
        }
        if (!condition.empty()) {
            text += " if " + condition;
        }
        return text;
    }
};

// ============================================================================
// WATCHPOINT
// ============================================================================

/**
 * @brief Watchpoint (data breakpoint) information
 */
struct Watchpoint {
    int id = -1;
    int debuggerId = -1;
    
    std::string expression;                 // Watch expression
    WatchpointAccess access = WatchpointAccess::Write;
    uint64_t address = 0;                   // Memory address being watched
    size_t size = 0;                        // Size of watched memory
    std::string type;                       // Type of watched variable
    
    bool enabled = true;
    int hitCount = 0;
    std::string oldValue;                   // Previous value (on hit)
    std::string newValue;                   // New value (on hit)
    
    std::string errorMessage;
    
    bool IsValid() const {
        return id >= 0 && !expression.empty();
    }
    
    std::string GetDisplayText() const {
        std::string text = expression;
        text += " (" + WatchpointAccessToString(access) + ")";
        return text;
    }
};

// ============================================================================
// STACK FRAME
// ============================================================================

/**
 * @brief Stack frame information
 */
struct StackFrame {
    int level = 0;                          // Frame level (0 = current/top)
    int threadId = 0;                       // Thread this frame belongs to
    
    std::string functionName;               // Function name
    std::string fullName;                   // Fully qualified name
    SourceLocation location;                // Source location
    uint64_t address = 0;                   // Instruction pointer
    uint64_t frameBase = 0;                 // Frame base pointer
    
    std::string moduleName;                 // Module/library name
    std::string args;                       // Function arguments (summary)
    
    bool hasDebugInfo = false;              // Source-level debug info available
    bool isInlined = false;                 // Inlined function frame
    bool isArtificial = false;              // Compiler-generated frame
    
    bool IsValid() const {
        return level >= 0;
    }
    
    bool HasSource() const {
        return hasDebugInfo && location.IsValid();
    }
    
    std::string GetDisplayText() const {
        std::string text = functionName;
        if (!args.empty()) {
            text += "(" + args + ")";
        }
        if (location.IsValid()) {
            text += " at " + location.ToShortString();
        }
        return text;
    }
    
    std::string GetShortDisplayText() const {
        return functionName.empty() ? "??" : functionName;
    }
};

// ============================================================================
// THREAD INFO
// ============================================================================

/**
 * @brief Thread information
 */
struct ThreadInfo {
    int id = 0;                             // Thread ID (debugger assigned)
    int systemId = 0;                       // OS thread ID
    std::string name;                       // Thread name (if available)
    
    DebugSessionState state = DebugSessionState::Running;
    StopReason stopReason = StopReason::UnknownStop;
    SourceLocation currentLocation;
    std::string currentFunction;
    
    int frameCount = 0;                     // Number of stack frames
    bool isCurrent = false;                 // Currently selected thread
    bool isMain = false;                    // Main thread
    
    uint64_t stackPointer = 0;
    uint64_t instructionPointer = 0;
    
    bool IsValid() const {
        return id > 0;
    }
    
    bool IsStopped() const {
        return state == DebugSessionState::Paused;
    }
    
    std::string GetDisplayText() const {
        std::string text = "Thread " + std::to_string(id);
        if (!name.empty()) {
            text += " \"" + name + "\"";
        }
        if (!currentFunction.empty()) {
            text += " in " + currentFunction;
        }
        return text;
    }
};

// ============================================================================
// VARIABLE
// ============================================================================

/**
 * @brief Variable/expression value representation
 */
struct Variable {
    std::string name;                       // Variable name
    std::string value;                      // Display value
    std::string type;                       // Type name
    VariableCategory category = VariableCategory::UnknownCategory;
    
    std::string evaluateName;               // Expression to re-evaluate
    std::string memoryReference;            // Memory address (if applicable)
    
    int numChildren = 0;                    // Number of child variables
    bool hasChildren = false;               // Has expandable children
    bool isExpanded = false;                // Currently expanded in UI
    
    int variablesReference = 0;             // Reference for fetching children
    int namedVariables = 0;                 // Number of named children
    int indexedVariables = 0;               // Number of indexed children
    
    std::string parentName;                 // Parent variable name
    int frameLevel = 0;                     // Stack frame level
    int threadId = 0;                       // Thread ID
    
    bool isReadOnly = false;                // Cannot be modified
    bool isPointer = false;                 // Is a pointer type
    bool isReference = false;               // Is a reference type
    bool isDereferenced = false;            // Has been dereferenced
    
    std::string errorMessage;               // Error if evaluation failed
    
    std::vector<Variable> children;         // Child variables (if fetched)
    
    bool IsValid() const {
        return !name.empty();
    }
    
    bool HasError() const {
        return !errorMessage.empty();
    }
    
    bool CanModify() const {
        return !isReadOnly && !HasError();
    }
    
    std::string GetDisplayValue() const {
        if (HasError()) {
            return "<error: " + errorMessage + ">";
        }
        return value;
    }
    
    std::string GetDisplayType() const {
        return type.empty() ? "?" : type;
    }
};

// ============================================================================
// REGISTER
// ============================================================================

/**
 * @brief CPU register information
 */
struct Register {
    std::string name;                       // Register name (e.g., "rax", "eip")
    std::string value;                      // Current value
    std::string previousValue;              // Previous value (for highlighting)
    RegisterCategory category = RegisterCategory::General;
    
    int number = -1;                        // Register number
    int size = 0;                           // Size in bytes
    
    bool hasChanged = false;                // Value changed since last stop
    bool isReadOnly = false;                // Cannot be modified
    
    bool IsValid() const {
        return !name.empty();
    }
    
    std::string GetDisplayText() const {
        return name + " = " + value;
    }
};

// ============================================================================
// MEMORY REGION
// ============================================================================

/**
 * @brief Memory region/block information
 */
struct MemoryRegion {
    uint64_t address = 0;                   // Start address
    size_t size = 0;                        // Size in bytes
    std::vector<uint8_t> data;              // Raw data
    std::string formatted;                  // Formatted display
    
    bool isReadable = true;
    bool isWritable = true;
    bool isExecutable = false;
    
    std::string errorMessage;               // Error if read failed
    
    bool IsValid() const {
        return size > 0 && !data.empty();
    }
    
    bool HasError() const {
        return !errorMessage.empty();
    }
};

// ============================================================================
// DISASSEMBLY LINE
// ============================================================================

/**
 * @brief Single disassembly instruction
 */
struct DisassemblyLine {
    uint64_t address = 0;                   // Instruction address
    std::string instruction;                // Disassembled instruction
    std::string opcodes;                    // Raw opcodes (hex)
    std::string comment;                    // Comment or symbol reference
    
    SourceLocation sourceLocation;          // Corresponding source (if available)
    std::string functionName;               // Function this belongs to
    int functionOffset = 0;                 // Offset from function start
    
    bool isCurrentInstruction = false;      // Current IP
    bool hasBreakpoint = false;             // Breakpoint set here
    
    std::string GetDisplayText() const {
        return instruction;
    }
    
    std::string GetAddressText() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%016lx", (unsigned long)address);
        return buf;
    }
};

// ============================================================================
// MODULE INFO
// ============================================================================

/**
 * @brief Loaded module/library information
 */
struct ModuleInfo {
    std::string name;                       // Module name
    std::string path;                       // Full path
    uint64_t baseAddress = 0;               // Load address
    uint64_t size = 0;                      // Size in memory
    
    bool hasDebugInfo = false;              // Debug symbols available
    std::string debugInfoPath;              // Path to debug info
    
    bool isUserCode = false;                // User code vs system library
    bool isMainModule = false;              // Main executable
    
    std::string GetDisplayText() const {
        return name;
    }
};

// ============================================================================
// EXCEPTION INFO
// ============================================================================

/**
 * @brief Exception/signal information
 */
struct ExceptionInfo {
    std::string type;                       // Exception type name
    std::string message;                    // Exception message
    uint64_t code = 0;                      // Exception code
    uint64_t address = 0;                   // Address where exception occurred
    
    std::string signalName;                 // Signal name (SIGSEGV, etc.)
    int signalNumber = 0;                   // Signal number
    
    bool isHardware = false;                // Hardware exception
    bool isCatchable = true;                // Can be caught
    
    SourceLocation location;                // Source location
    std::string functionName;               // Function where it occurred
    
    std::string GetDisplayText() const {
        if (!signalName.empty()) {
            return signalName + ": " + message;
        }
        return type + ": " + message;
    }
};

// ============================================================================
// DEBUG LAUNCH CONFIG
// ============================================================================

/**
 * @brief Configuration for launching a debug session
 */
struct DebugLaunchConfig {
    // Target
    std::string executablePath;             // Path to executable
    std::string workingDirectory;           // Working directory
    std::vector<std::string> arguments;     // Command line arguments
    std::map<std::string, std::string> environment; // Environment variables
    
    // Debugger
    DebuggerType debuggerType = DebuggerType::UnknownDebugger;
    std::string debuggerPath;               // Path to debugger executable
    std::vector<std::string> debuggerArgs;  // Additional debugger arguments
    
    // Attach mode
    bool attachMode = false;                // Attach to running process
    int attachPid = 0;                      // PID to attach to
    std::string attachProcessName;          // Process name to attach to
    
    // Remote debugging
    bool remoteMode = false;                // Remote debugging
    std::string remoteHost;                 // Remote host
    int remotePort = 0;                     // Remote port
    
    // Core dump
    bool coreDumpMode = false;              // Analyze core dump
    std::string coreDumpPath;               // Path to core dump
    
    // Options
    bool stopOnEntry = false;               // Stop at program entry
    bool externalConsole = false;           // Use external console
    bool justMyCode = false;                // Skip system code
    
    // Source paths
    std::vector<std::string> sourcePaths;   // Additional source paths
    std::map<std::string, std::string> pathMappings; // Source path mappings
    
    bool IsValid() const {
        if (attachMode) {
            return attachPid > 0 || !attachProcessName.empty();
        }
        if (coreDumpMode) {
            return !coreDumpPath.empty() && !executablePath.empty();
        }
        if (remoteMode) {
            return !remoteHost.empty() && remotePort > 0;
        }
        return !executablePath.empty();
    }
    
    std::string GetDisplayName() const {
        if (attachMode) {
            if (attachPid > 0) {
                return "Attach to PID " + std::to_string(attachPid);
            }
            return "Attach to " + attachProcessName;
        }
        if (coreDumpMode) {
            return "Core: " + coreDumpPath;
        }
        if (remoteMode) {
            return "Remote: " + remoteHost + ":" + std::to_string(remotePort);
        }
        // Extract filename from path
        size_t pos = executablePath.find_last_of("/\\");
        return (pos != std::string::npos) ? 
               executablePath.substr(pos + 1) : executablePath;
    }
};

// ============================================================================
// DEBUG EVENT
// ============================================================================

/**
 * @brief Debug event for callbacks
 */
struct DebugEvent {
    DebugEventType type;
    
    // Context
    int threadId = 0;
    int breakpointId = 0;
    SourceLocation location;
    StopReason stopReason = StopReason::UnknownStop;
    
    // Data
    std::string message;
    std::string details;
    int exitCode = 0;
    
    // Associated data (based on event type)
    Breakpoint breakpoint;
    Watchpoint watchpoint;
    ExceptionInfo exception;
    ThreadInfo thread;
    ModuleInfo module;
    
    std::chrono::system_clock::time_point timestamp;
    
    DebugEvent() {
        timestamp = std::chrono::system_clock::now();
    }
    
    DebugEvent(DebugEventType t) : type(t) {
        timestamp = std::chrono::system_clock::now();
    }
    
    DebugEvent(DebugEventType t, const std::string& msg) 
        : type(t), message(msg) {
        timestamp = std::chrono::system_clock::now();
    }
};

// ============================================================================
// EVALUATION RESULT
// ============================================================================

/**
 * @brief Result of expression evaluation
 */
struct EvaluationResult {
    bool success = false;
    Variable variable;                      // Result as variable
    std::string errorMessage;               // Error if failed
    
    // For assignment expressions
    bool isAssignment = false;
    std::string assignedValue;
    
    bool IsValid() const {
        return success && variable.IsValid();
    }
};

// ============================================================================
// COMPLETION ITEM
// ============================================================================

/**
 * @brief Code completion item for debug console
 */
struct DebugCompletionItem {
    std::string label;                      // Display text
    std::string insertText;                 // Text to insert
    std::string detail;                     // Type or detail info
    std::string documentation;              // Documentation
    
    enum class Kind {
        Variable,
        Function,
        Field,
        Keyword,
        Module,
        Other
    } kind = Kind::Other;
};

// ============================================================================
// TYPE ALIASES FOR CALLBACKS
// ============================================================================

using DebugEventCallback = std::function<void(const DebugEvent&)>;
using BreakpointCallback = std::function<void(const Breakpoint&)>;
using VariableCallback = std::function<void(const Variable&)>;
using OutputCallback = std::function<void(const std::string&)>;
using ErrorCallback = std::function<void(const std::string&)>;
using ProgressCallback = std::function<void(float)>;

} // namespace IDE
} // namespace UltraCanvas
