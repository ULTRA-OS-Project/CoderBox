// Apps/IDE/Debug/DAP/UCDAPTypes.h
// Debug Adapter Protocol Types for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
// Reference: https://microsoft.github.io/debug-adapter-protocol/specification
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <variant>
#include <cstdint>

namespace UltraCanvas {
namespace IDE {
namespace DAP {

// ============================================================================
// BASE TYPES
// ============================================================================

using JsonValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<struct JsonObject>,
    std::map<std::string, struct JsonObject>
>;

struct JsonObject {
    std::map<std::string, JsonValue> properties;
    
    template<typename T>
    std::optional<T> Get(const std::string& key) const;
    
    void Set(const std::string& key, JsonValue value);
    bool Has(const std::string& key) const;
    std::string ToJson() const;
    static JsonObject FromJson(const std::string& json);
};

// ============================================================================
// PROTOCOL MESSAGE TYPES
// ============================================================================

/**
 * @brief Base class for all DAP messages
 */
struct ProtocolMessage {
    int seq = 0;                        // Sequence number
    std::string type;                   // "request", "response", "event"
    
    virtual ~ProtocolMessage() = default;
    virtual std::string ToJson() const;
};

/**
 * @brief Request message from client to adapter
 */
struct Request : ProtocolMessage {
    std::string command;                // Command name
    JsonObject arguments;               // Command arguments
    
    Request() { type = "request"; }
    std::string ToJson() const override;
};

/**
 * @brief Response message from adapter to client
 */
struct Response : ProtocolMessage {
    int request_seq = 0;                // Sequence of request
    bool success = true;
    std::string command;                // Command being responded to
    std::string message;                // Error message if !success
    JsonObject body;                    // Response body
    
    Response() { type = "response"; }
    std::string ToJson() const override;
};

/**
 * @brief Event message from adapter to client
 */
struct Event : ProtocolMessage {
    std::string event;                  // Event type
    JsonObject body;                    // Event body
    
    Event() { type = "event"; }
    Event(const std::string& eventName) : event(eventName) { type = "event"; }
    std::string ToJson() const override;
};

// ============================================================================
// CAPABILITIES
// ============================================================================

/**
 * @brief Capabilities of the debug adapter
 */
struct Capabilities {
    bool supportsConfigurationDoneRequest = true;
    bool supportsFunctionBreakpoints = true;
    bool supportsConditionalBreakpoints = true;
    bool supportsHitConditionalBreakpoints = true;
    bool supportsEvaluateForHovers = true;
    bool supportsSetVariable = true;
    bool supportsSetExpression = false;
    bool supportsStepBack = false;
    bool supportsRestartFrame = false;
    bool supportsGotoTargetsRequest = false;
    bool supportsStepInTargetsRequest = false;
    bool supportsCompletionsRequest = true;
    bool supportsModulesRequest = true;
    bool supportsRestartRequest = true;
    bool supportsExceptionOptions = true;
    bool supportsValueFormattingOptions = true;
    bool supportsExceptionInfoRequest = true;
    bool supportTerminateDebuggee = true;
    bool supportSuspendDebuggee = true;
    bool supportsDelayedStackTraceLoading = true;
    bool supportsLoadedSourcesRequest = true;
    bool supportsLogPoints = true;
    bool supportsTerminateThreadsRequest = true;
    bool supportsDataBreakpoints = true;
    bool supportsReadMemoryRequest = true;
    bool supportsWriteMemoryRequest = true;
    bool supportsDisassembleRequest = true;
    bool supportsCancelRequest = true;
    bool supportsBreakpointLocationsRequest = true;
    bool supportsClipboardContext = false;
    bool supportsSteppingGranularity = true;
    bool supportsInstructionBreakpoints = true;
    bool supportsExceptionFilterOptions = true;
    bool supportsSingleThreadExecutionRequests = true;
    
    std::vector<std::string> exceptionBreakpointFilters;
    std::vector<std::string> additionalModuleColumns;
    std::vector<std::string> supportedChecksumAlgorithms;
    
    JsonObject ToJson() const;
};

// ============================================================================
// SOURCE & LOCATION
// ============================================================================

/**
 * @brief Source descriptor
 */
struct Source {
    std::string name;                   // Short name
    std::string path;                   // Full path
    int sourceReference = 0;            // For sources without path
    std::string presentationHint;       // "normal", "emphasize", "deemphasize"
    std::string origin;                 // Origin description
    std::vector<Source> sources;        // Related sources
    JsonObject adapterData;             // Adapter-specific data
    std::vector<std::string> checksums;
    
    JsonObject ToJson() const;
    static Source FromJson(const JsonObject& obj);
};

/**
 * @brief Breakpoint location
 */
struct BreakpointLocation {
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    
    JsonObject ToJson() const;
};

/**
 * @brief Source breakpoint
 */
struct SourceBreakpoint {
    int line = 0;
    int column = 0;
    std::string condition;
    std::string hitCondition;
    std::string logMessage;
    
    static SourceBreakpoint FromJson(const JsonObject& obj);
};

/**
 * @brief Function breakpoint
 */
struct FunctionBreakpoint {
    std::string name;
    std::string condition;
    std::string hitCondition;
    
    static FunctionBreakpoint FromJson(const JsonObject& obj);
};

/**
 * @brief Data breakpoint
 */
struct DataBreakpoint {
    std::string dataId;
    std::string accessType;             // "read", "write", "readWrite"
    std::string condition;
    std::string hitCondition;
    
    static DataBreakpoint FromJson(const JsonObject& obj);
};

/**
 * @brief Instruction breakpoint
 */
struct InstructionBreakpoint {
    std::string instructionReference;   // Memory address as string
    int offset = 0;
    std::string condition;
    std::string hitCondition;
    
    static InstructionBreakpoint FromJson(const JsonObject& obj);
};

/**
 * @brief Breakpoint response
 */
struct Breakpoint {
    int id = 0;
    bool verified = false;
    std::string message;
    Source source;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string instructionReference;
    int offset = 0;
    
    JsonObject ToJson() const;
};

// ============================================================================
// STACK & THREADS
// ============================================================================

/**
 * @brief Stack frame
 */
struct StackFrame {
    int id = 0;
    std::string name;
    Source source;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    bool canRestart = false;
    std::string instructionPointerReference;
    int moduleId = 0;
    std::string presentationHint;       // "normal", "label", "subtle"
    
    JsonObject ToJson() const;
};

/**
 * @brief Thread
 */
struct Thread {
    int id = 0;
    std::string name;
    
    JsonObject ToJson() const;
};

// ============================================================================
// VARIABLES & SCOPES
// ============================================================================

/**
 * @brief Variable scope
 */
struct Scope {
    std::string name;
    std::string presentationHint;       // "arguments", "locals", "registers"
    int variablesReference = 0;
    int namedVariables = 0;
    int indexedVariables = 0;
    bool expensive = false;
    Source source;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    
    JsonObject ToJson() const;
};

/**
 * @brief Variable
 */
struct Variable {
    std::string name;
    std::string value;
    std::string type;
    std::string presentationHint;
    std::string evaluateName;
    int variablesReference = 0;
    int namedVariables = 0;
    int indexedVariables = 0;
    std::string memoryReference;
    
    JsonObject ToJson() const;
};

/**
 * @brief Variable presentation hint
 */
struct VariablePresentationHint {
    std::string kind;                   // "property", "method", "class", etc.
    std::vector<std::string> attributes; // "static", "constant", "readOnly", etc.
    std::string visibility;             // "public", "private", "protected"
    bool lazy = false;
    
    JsonObject ToJson() const;
};

// ============================================================================
// MODULES
// ============================================================================

/**
 * @brief Module (shared library/DLL)
 */
struct Module {
    int id = 0;
    std::string name;
    std::string path;
    bool isOptimized = false;
    bool isUserCode = true;
    std::string version;
    std::string symbolStatus;           // "Symbols loaded", "Symbols not found", etc.
    std::string symbolFilePath;
    std::string dateTimeStamp;
    std::string addressRange;
    
    JsonObject ToJson() const;
};

// ============================================================================
// EXCEPTIONS
// ============================================================================

/**
 * @brief Exception breakpoint filter
 */
struct ExceptionBreakpointsFilter {
    std::string filter;
    std::string label;
    std::string description;
    bool defaultValue = false;
    bool supportsCondition = false;
    std::string conditionDescription;
    
    JsonObject ToJson() const;
};

/**
 * @brief Exception filter options
 */
struct ExceptionFilterOptions {
    std::string filterId;
    std::string condition;
    
    static ExceptionFilterOptions FromJson(const JsonObject& obj);
};

/**
 * @brief Exception path segment
 */
struct ExceptionPathSegment {
    bool negate = false;
    std::vector<std::string> names;
    
    static ExceptionPathSegment FromJson(const JsonObject& obj);
};

/**
 * @brief Exception options
 */
struct ExceptionOptions {
    std::vector<ExceptionPathSegment> path;
    std::string breakMode;              // "never", "always", "unhandled", "userUnhandled"
    
    static ExceptionOptions FromJson(const JsonObject& obj);
};

/**
 * @brief Exception details
 */
struct ExceptionDetails {
    std::string message;
    std::string typeName;
    std::string fullTypeName;
    std::string evaluateName;
    std::string stackTrace;
    std::vector<ExceptionDetails> innerException;
    
    JsonObject ToJson() const;
};

// ============================================================================
// MEMORY & DISASSEMBLY
// ============================================================================

/**
 * @brief Memory read result
 */
struct MemoryContents {
    std::string address;                // Start address
    int unreadableBytes = 0;
    std::string data;                   // Base64 encoded
    
    JsonObject ToJson() const;
};

/**
 * @brief Disassembled instruction
 */
struct DisassembledInstruction {
    std::string address;
    std::string instructionBytes;
    std::string instruction;
    std::string symbol;
    Source location;
    int line = 0;
    int column = 0;
    int endLine = 0;
    int endColumn = 0;
    
    JsonObject ToJson() const;
};

// ============================================================================
// EVALUATE & COMPLETIONS
// ============================================================================

/**
 * @brief Evaluate context
 */
enum class EvaluateContext {
    Watch,
    Repl,
    Hover,
    Clipboard,
    Variables
};

/**
 * @brief Value format
 */
struct ValueFormat {
    bool hex = false;
    
    static ValueFormat FromJson(const JsonObject& obj);
};

/**
 * @brief Completion item
 */
struct CompletionItem {
    std::string label;
    std::string text;
    std::string sortText;
    std::string detail;
    std::string type;                   // "method", "function", "field", etc.
    int start = 0;
    int length = 0;
    int selectionStart = 0;
    int selectionLength = 0;
    
    JsonObject ToJson() const;
};

// ============================================================================
// STEPPING
// ============================================================================

/**
 * @brief Stepping granularity
 */
enum class SteppingGranularity {
    Statement,
    Line,
    Instruction
};

std::string SteppingGranularityToString(SteppingGranularity granularity);
SteppingGranularity StringToSteppingGranularity(const std::string& str);

// ============================================================================
// EVENTS
// ============================================================================

/**
 * @brief Stop reason
 */
enum class DAPStopReason {
    Step,
    Breakpoint,
    Exception,
    Pause,
    Entry,
    Goto,
    FunctionBreakpoint,
    DataBreakpoint,
    InstructionBreakpoint
};

std::string DAPStopReasonToString(DAPStopReason reason);

/**
 * @brief Output category
 */
enum class OutputCategory {
    Console,
    Important,
    Stdout,
    Stderr,
    Telemetry
};

std::string OutputCategoryToString(OutputCategory category);

/**
 * @brief Invalidated areas
 */
enum class InvalidatedAreas {
    All,
    Stacks,
    Threads,
    Variables
};

std::string InvalidatedAreasToString(InvalidatedAreas area);

} // namespace DAP
} // namespace IDE
} // namespace UltraCanvas
