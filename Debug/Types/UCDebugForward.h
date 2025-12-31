// Apps/IDE/Debug/Types/UCDebugForward.h
// Forward Declarations for ULTRA IDE Debugger
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// ENUMERATIONS (forward declared for pointer/reference use)
// ============================================================================

enum class DebuggerType;
enum class DebugSessionState;
enum class StopReason;
enum class BreakpointType;
enum class BreakpointState;
enum class WatchpointAccess;
enum class VariableCategory;
enum class StepType;
enum class MemoryFormat;
enum class MemoryUnitSize;
enum class RegisterCategory;
enum class EvaluationContext;
enum class DebugEventType;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct SourceLocation;
struct Breakpoint;
struct Watchpoint;
struct StackFrame;
struct ThreadInfo;
struct Variable;
struct Register;
struct MemoryRegion;
struct DisassemblyLine;
struct ModuleInfo;
struct ExceptionInfo;
struct DebugLaunchConfig;
struct DebugEvent;
struct EvaluationResult;
struct DebugCompletionItem;

// ============================================================================
// CORE CLASSES
// ============================================================================

// Plugin interface
class IUCDebuggerPlugin;

// Plugin registry
class UCDebuggerPluginRegistry;

// Session management
class UCDebugManager;
class UCDebugSession;

// Breakpoint management
class UCBreakpointManager;
class UCWatchManager;

// Variable inspection
class UCVariableInspector;
class UCVariableFormatter;

// Stack/thread management
class UCCallStack;

// ============================================================================
// GDB PLUGIN
// ============================================================================

class UCGDBPlugin;
class UCGDBMIParser;
class UCGDBProcess;
struct GDBMIRecord;
struct GDBMIValue;

// ============================================================================
// LLDB PLUGIN
// ============================================================================

class UCLLDBPlugin;
class UCLLDBBridge;

// ============================================================================
// UI COMPONENTS
// ============================================================================

class UCDebugToolbar;
class UCBreakpointPanel;
class UCCallStackPanel;
class UCVariablesPanel;
class UCWatchPanel;
class UCDebugConsole;
class UCRegistersPanel;
class UCMemoryPanel;
class UCDisassemblyPanel;

} // namespace IDE
} // namespace UltraCanvas
