// Apps/IDE/Debug/Types/UCDebugEnums.h
// Debug Enumeration Definitions for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// DEBUGGER TYPE
// ============================================================================

/**
 * @brief Supported debugger backends
 */
enum class DebuggerType {
    UnknownDebugger = 0,    // Avoid conflict with X11's None
    GDB,                    // GNU Debugger (Linux, Windows via MinGW)
    LLDB,                   // LLVM Debugger (macOS, Linux)
    WinDbg,                 // Windows Debugger (future)
    CDB,                    // Windows Console Debugger (future)
    CustomDebugger          // Custom debugger plugin
};

/**
 * @brief Convert DebuggerType to display string
 */
inline std::string DebuggerTypeToString(DebuggerType type) {
    switch (type) {
        case DebuggerType::GDB:            return "GDB";
        case DebuggerType::LLDB:           return "LLDB";
        case DebuggerType::WinDbg:         return "WinDbg";
        case DebuggerType::CDB:            return "CDB";
        case DebuggerType::CustomDebugger: return "Custom";
        default:                           return "Unknown";
    }
}

/**
 * @brief Convert string to DebuggerType
 */
inline DebuggerType StringToDebuggerType(const std::string& str) {
    if (str == "GDB" || str == "gdb")       return DebuggerType::GDB;
    if (str == "LLDB" || str == "lldb")     return DebuggerType::LLDB;
    if (str == "WinDbg" || str == "windbg") return DebuggerType::WinDbg;
    if (str == "CDB" || str == "cdb")       return DebuggerType::CDB;
    if (str == "Custom" || str == "custom") return DebuggerType::CustomDebugger;
    return DebuggerType::UnknownDebugger;
}

// ============================================================================
// DEBUG SESSION STATE
// ============================================================================

/**
 * @brief Current state of a debug session
 */
enum class DebugSessionState {
    Inactive,       // No debug session active
    Starting,       // Debugger process starting
    Running,        // Target program running
    Paused,         // Target paused (breakpoint, step, signal)
    Stepping,       // Currently stepping
    Stopping,       // Session terminating
    Terminated,     // Target has exited
    ErrorState      // Error state (avoid conflict with Error macro)
};

/**
 * @brief Convert DebugSessionState to string
 */
inline std::string DebugSessionStateToString(DebugSessionState state) {
    switch (state) {
        case DebugSessionState::Inactive:   return "Inactive";
        case DebugSessionState::Starting:   return "Starting";
        case DebugSessionState::Running:    return "Running";
        case DebugSessionState::Paused:     return "Paused";
        case DebugSessionState::Stepping:   return "Stepping";
        case DebugSessionState::Stopping:   return "Stopping";
        case DebugSessionState::Terminated: return "Terminated";
        case DebugSessionState::ErrorState: return "Error";
        default:                            return "Unknown";
    }
}

/**
 * @brief Check if session is active (can receive commands)
 */
inline bool IsSessionActive(DebugSessionState state) {
    return state == DebugSessionState::Running ||
           state == DebugSessionState::Paused ||
           state == DebugSessionState::Stepping;
}

/**
 * @brief Check if target is stopped (can inspect state)
 */
inline bool IsTargetStopped(DebugSessionState state) {
    return state == DebugSessionState::Paused;
}

// ============================================================================
// STOP REASON
// ============================================================================

/**
 * @brief Reason why the debugger stopped
 */
enum class StopReason {
    UnknownStop,        // Unknown reason (avoid X11 conflict)
    Breakpoint,         // Hit a breakpoint
    Watchpoint,         // Watchpoint triggered
    Step,               // Step completed
    Signal,             // Signal received (SIGSEGV, SIGINT, etc.)
    Exception,          // Exception thrown
    ExitNormal,         // Program exited normally (exit code 0)
    ExitError,          // Program exited with error
    Detached,           // Debugger detached
    UserRequest,        // User requested pause/interrupt
    FunctionFinished,   // Step out completed
    LocationReached,    // Run to cursor completed
    Fork,               // Process forked
    Exec,               // Process exec'd
    Syscall             // Syscall entry/exit
};

/**
 * @brief Convert StopReason to string
 */
inline std::string StopReasonToString(StopReason reason) {
    switch (reason) {
        case StopReason::Breakpoint:        return "Breakpoint";
        case StopReason::Watchpoint:        return "Watchpoint";
        case StopReason::Step:              return "Step";
        case StopReason::Signal:            return "Signal";
        case StopReason::Exception:         return "Exception";
        case StopReason::ExitNormal:        return "Exit (Normal)";
        case StopReason::ExitError:         return "Exit (Error)";
        case StopReason::Detached:          return "Detached";
        case StopReason::UserRequest:       return "User Request";
        case StopReason::FunctionFinished:  return "Function Finished";
        case StopReason::LocationReached:   return "Location Reached";
        case StopReason::Fork:              return "Fork";
        case StopReason::Exec:              return "Exec";
        case StopReason::Syscall:           return "Syscall";
        default:                            return "Unknown";
    }
}

// ============================================================================
// BREAKPOINT TYPES
// ============================================================================

/**
 * @brief Type of breakpoint
 */
enum class BreakpointType {
    LineBreakpoint,         // Source line breakpoint
    FunctionBreakpoint,     // Function entry breakpoint
    AddressBreakpoint,      // Memory address breakpoint
    ConditionalBreakpoint,  // Breakpoint with condition
    Logpoint,               // Log message without stopping
    DataBreakpoint,         // Data breakpoint (watchpoint)
    ExceptionBreakpoint     // Exception breakpoint
};

/**
 * @brief Convert BreakpointType to string
 */
inline std::string BreakpointTypeToString(BreakpointType type) {
    switch (type) {
        case BreakpointType::LineBreakpoint:        return "Line";
        case BreakpointType::FunctionBreakpoint:    return "Function";
        case BreakpointType::AddressBreakpoint:     return "Address";
        case BreakpointType::ConditionalBreakpoint: return "Conditional";
        case BreakpointType::Logpoint:              return "Logpoint";
        case BreakpointType::DataBreakpoint:        return "Data";
        case BreakpointType::ExceptionBreakpoint:   return "Exception";
        default:                                     return "Unknown";
    }
}

/**
 * @brief Breakpoint state
 */
enum class BreakpointState {
    Pending,        // Not yet set in debugger
    Verified,       // Set and verified by debugger
    Active,         // Set and enabled
    Disabled,       // Set but disabled
    Invalid,        // Could not be set (bad location)
    Deleted         // Marked for deletion
};

/**
 * @brief Convert BreakpointState to string
 */
inline std::string BreakpointStateToString(BreakpointState state) {
    switch (state) {
        case BreakpointState::Pending:  return "Pending";
        case BreakpointState::Verified: return "Verified";
        case BreakpointState::Active:   return "Active";
        case BreakpointState::Disabled: return "Disabled";
        case BreakpointState::Invalid:  return "Invalid";
        case BreakpointState::Deleted:  return "Deleted";
        default:                        return "Unknown";
    }
}

// ============================================================================
// WATCHPOINT TYPES
// ============================================================================

/**
 * @brief Watchpoint access type
 */
enum class WatchpointAccess {
    Read,           // Break on read
    Write,          // Break on write
    ReadWrite       // Break on read or write
};

/**
 * @brief Convert WatchpointAccess to string
 */
inline std::string WatchpointAccessToString(WatchpointAccess access) {
    switch (access) {
        case WatchpointAccess::Read:      return "Read";
        case WatchpointAccess::Write:     return "Write";
        case WatchpointAccess::ReadWrite: return "Read/Write";
        default:                          return "Unknown";
    }
}

// ============================================================================
// VARIABLE TYPES
// ============================================================================

/**
 * @brief Variable value type category
 */
enum class VariableCategory {
    UnknownCategory,    // Unknown type
    Primitive,          // int, float, char, bool, etc.
    Pointer,            // Pointer type
    Reference,          // Reference type
    Array,              // Array type (fixed size)
    DynamicArray,       // Dynamic array
    Struct,             // Struct type
    Class,              // Class type
    Union,              // Union type
    Enum,               // Enumeration
    Function,           // Function pointer
    String,             // String type (char*, std::string)
    WideString,         // Wide string (wchar_t*, std::wstring)
    Container,          // STL container (vector, map, etc.)
    SmartPointer,       // Smart pointer (unique_ptr, shared_ptr)
    Optional,           // std::optional
    Variant,            // std::variant
    Tuple               // std::tuple
};

/**
 * @brief Convert VariableCategory to string
 */
inline std::string VariableCategoryToString(VariableCategory cat) {
    switch (cat) {
        case VariableCategory::Primitive:     return "Primitive";
        case VariableCategory::Pointer:       return "Pointer";
        case VariableCategory::Reference:     return "Reference";
        case VariableCategory::Array:         return "Array";
        case VariableCategory::DynamicArray:  return "Dynamic Array";
        case VariableCategory::Struct:        return "Struct";
        case VariableCategory::Class:         return "Class";
        case VariableCategory::Union:         return "Union";
        case VariableCategory::Enum:          return "Enum";
        case VariableCategory::Function:      return "Function";
        case VariableCategory::String:        return "String";
        case VariableCategory::WideString:    return "Wide String";
        case VariableCategory::Container:     return "Container";
        case VariableCategory::SmartPointer:  return "Smart Pointer";
        case VariableCategory::Optional:      return "Optional";
        case VariableCategory::Variant:       return "Variant";
        case VariableCategory::Tuple:         return "Tuple";
        default:                              return "Unknown";
    }
}

// ============================================================================
// STEP TYPE
// ============================================================================

/**
 * @brief Type of step operation
 */
enum class StepType {
    StepOver,           // Step over (next line, skip function calls)
    StepInto,           // Step into (next line, enter function calls)
    StepOut,            // Step out (run until current function returns)
    StepInstruction,    // Single instruction step
    StepOverInstruction // Step over single instruction
};

/**
 * @brief Convert StepType to string
 */
inline std::string StepTypeToString(StepType type) {
    switch (type) {
        case StepType::StepOver:            return "Step Over";
        case StepType::StepInto:            return "Step Into";
        case StepType::StepOut:             return "Step Out";
        case StepType::StepInstruction:     return "Step Instruction";
        case StepType::StepOverInstruction: return "Step Over Instruction";
        default:                            return "Unknown";
    }
}

// ============================================================================
// MEMORY ACCESS
// ============================================================================

/**
 * @brief Memory display format
 */
enum class MemoryFormat {
    Hex,            // Hexadecimal
    Decimal,        // Decimal
    Octal,          // Octal
    Binary,         // Binary
    Float,          // Floating point
    Instruction     // Disassembly
};

/**
 * @brief Memory unit size
 */
enum class MemoryUnitSize {
    Byte,           // 1 byte
    HalfWord,       // 2 bytes
    Word,           // 4 bytes
    DoubleWord,     // 8 bytes
    QuadWord        // 16 bytes
};

// ============================================================================
// REGISTER CATEGORY
// ============================================================================

/**
 * @brief Register category for grouping
 */
enum class RegisterCategory {
    General,        // General purpose registers
    FloatingPoint,  // Floating point registers
    Vector,         // SIMD/Vector registers (SSE, AVX)
    Segment,        // Segment registers (x86)
    Control,        // Control registers
    Debug,          // Debug registers
    System,         // System registers
    Other           // Other registers
};

// ============================================================================
// EVALUATION CONTEXT
// ============================================================================

/**
 * @brief Context for expression evaluation
 */
enum class EvaluationContext {
    Watch,          // Watch expression (persistent)
    Hover,          // Hover tooltip (temporary)
    Repl,           // Debug console REPL
    Conditional,    // Breakpoint condition
    Logpoint        // Logpoint message
};

// ============================================================================
// DEBUG EVENT TYPE
// ============================================================================

/**
 * @brief Type of debug event for callbacks
 */
enum class DebugEventType {
    SessionStart,           // Debug session started
    SessionEnd,             // Debug session ended
    TargetStop,             // Target stopped
    TargetContinue,         // Target continued
    BreakpointHit,          // Breakpoint was hit
    BreakpointAdded,        // Breakpoint added
    BreakpointRemoved,      // Breakpoint removed
    BreakpointChanged,      // Breakpoint modified
    WatchpointHit,          // Watchpoint triggered
    ExceptionThrown,        // Exception was thrown
    OutputReceived,         // Program output received
    ErrorOccurred,          // Error occurred
    ThreadCreated,          // New thread created
    ThreadExited,           // Thread exited
    ModuleLoaded,           // Shared library loaded
    ModuleUnloaded          // Shared library unloaded
};

/**
 * @brief Convert DebugEventType to string
 */
inline std::string DebugEventTypeToString(DebugEventType type) {
    switch (type) {
        case DebugEventType::SessionStart:      return "Session Start";
        case DebugEventType::SessionEnd:        return "Session End";
        case DebugEventType::TargetStop:        return "Target Stop";
        case DebugEventType::TargetContinue:    return "Target Continue";
        case DebugEventType::BreakpointHit:     return "Breakpoint Hit";
        case DebugEventType::BreakpointAdded:   return "Breakpoint Added";
        case DebugEventType::BreakpointRemoved: return "Breakpoint Removed";
        case DebugEventType::BreakpointChanged: return "Breakpoint Changed";
        case DebugEventType::WatchpointHit:     return "Watchpoint Hit";
        case DebugEventType::ExceptionThrown:   return "Exception Thrown";
        case DebugEventType::OutputReceived:    return "Output Received";
        case DebugEventType::ErrorOccurred:     return "Error Occurred";
        case DebugEventType::ThreadCreated:     return "Thread Created";
        case DebugEventType::ThreadExited:      return "Thread Exited";
        case DebugEventType::ModuleLoaded:      return "Module Loaded";
        case DebugEventType::ModuleUnloaded:    return "Module Unloaded";
        default:                                return "Unknown";
    }
}

} // namespace IDE
} // namespace UltraCanvas
