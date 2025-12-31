// Apps/IDE/Debug/DAP/UCDAPTypes.cpp
// Debug Adapter Protocol Types Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDAPTypes.h"
#include <sstream>
#include <iomanip>

namespace UltraCanvas {
namespace IDE {
namespace DAP {

// ============================================================================
// JSON HELPERS
// ============================================================================

static std::string EscapeJsonString(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

// ============================================================================
// PROTOCOL MESSAGES
// ============================================================================

std::string ProtocolMessage::ToJson() const {
    std::ostringstream ss;
    ss << "{\"seq\":" << seq << ",\"type\":\"" << type << "\"";
    return ss.str();
}

std::string Request::ToJson() const {
    std::ostringstream ss;
    ss << ProtocolMessage::ToJson();
    ss << ",\"command\":\"" << command << "\"";
    if (!arguments.properties.empty()) {
        ss << ",\"arguments\":" << arguments.ToJson();
    }
    ss << "}";
    return ss.str();
}

std::string Response::ToJson() const {
    std::ostringstream ss;
    ss << ProtocolMessage::ToJson();
    ss << ",\"request_seq\":" << request_seq;
    ss << ",\"success\":" << (success ? "true" : "false");
    ss << ",\"command\":\"" << command << "\"";
    if (!success && !message.empty()) {
        ss << ",\"message\":\"" << EscapeJsonString(message) << "\"";
    }
    if (!body.properties.empty()) {
        ss << ",\"body\":" << body.ToJson();
    }
    ss << "}";
    return ss.str();
}

std::string Event::ToJson() const {
    std::ostringstream ss;
    ss << ProtocolMessage::ToJson();
    ss << ",\"event\":\"" << event << "\"";
    if (!body.properties.empty()) {
        ss << ",\"body\":" << body.ToJson();
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// JSON OBJECT
// ============================================================================

std::string JsonObject::ToJson() const {
    std::ostringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& prop : properties) {
        if (!first) ss << ",";
        first = false;
        ss << "\"" << prop.first << "\":";
        
        std::visit([&ss](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                ss << "null";
            } else if constexpr (std::is_same_v<T, bool>) {
                ss << (arg ? "true" : "false");
            } else if constexpr (std::is_same_v<T, int64_t>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, double>) {
                ss << arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                ss << "\"" << EscapeJsonString(arg) << "\"";
            }
        }, prop.second);
    }
    ss << "}";
    return ss.str();
}

void JsonObject::Set(const std::string& key, JsonValue value) {
    properties[key] = value;
}

bool JsonObject::Has(const std::string& key) const {
    return properties.find(key) != properties.end();
}

// ============================================================================
// CAPABILITIES
// ============================================================================

JsonObject Capabilities::ToJson() const {
    JsonObject obj;
    obj.Set("supportsConfigurationDoneRequest", supportsConfigurationDoneRequest);
    obj.Set("supportsFunctionBreakpoints", supportsFunctionBreakpoints);
    obj.Set("supportsConditionalBreakpoints", supportsConditionalBreakpoints);
    obj.Set("supportsHitConditionalBreakpoints", supportsHitConditionalBreakpoints);
    obj.Set("supportsEvaluateForHovers", supportsEvaluateForHovers);
    obj.Set("supportsSetVariable", supportsSetVariable);
    obj.Set("supportsRestartRequest", supportsRestartRequest);
    obj.Set("supportsExceptionOptions", supportsExceptionOptions);
    obj.Set("supportsValueFormattingOptions", supportsValueFormattingOptions);
    obj.Set("supportsExceptionInfoRequest", supportsExceptionInfoRequest);
    obj.Set("supportTerminateDebuggee", supportTerminateDebuggee);
    obj.Set("supportsDelayedStackTraceLoading", supportsDelayedStackTraceLoading);
    obj.Set("supportsLoadedSourcesRequest", supportsLoadedSourcesRequest);
    obj.Set("supportsLogPoints", supportsLogPoints);
    obj.Set("supportsDataBreakpoints", supportsDataBreakpoints);
    obj.Set("supportsReadMemoryRequest", supportsReadMemoryRequest);
    obj.Set("supportsWriteMemoryRequest", supportsWriteMemoryRequest);
    obj.Set("supportsDisassembleRequest", supportsDisassembleRequest);
    obj.Set("supportsSteppingGranularity", supportsSteppingGranularity);
    obj.Set("supportsInstructionBreakpoints", supportsInstructionBreakpoints);
    obj.Set("supportsModulesRequest", supportsModulesRequest);
    obj.Set("supportsCompletionsRequest", supportsCompletionsRequest);
    obj.Set("supportsCancelRequest", supportsCancelRequest);
    obj.Set("supportsBreakpointLocationsRequest", supportsBreakpointLocationsRequest);
    obj.Set("supportsSingleThreadExecutionRequests", supportsSingleThreadExecutionRequests);
    return obj;
}

// ============================================================================
// SOURCE
// ============================================================================

JsonObject Source::ToJson() const {
    JsonObject obj;
    if (!name.empty()) obj.Set("name", name);
    if (!path.empty()) obj.Set("path", path);
    if (sourceReference > 0) obj.Set("sourceReference", static_cast<int64_t>(sourceReference));
    if (!presentationHint.empty()) obj.Set("presentationHint", presentationHint);
    if (!origin.empty()) obj.Set("origin", origin);
    return obj;
}

Source Source::FromJson(const JsonObject& obj) {
    Source s;
    // Simplified parsing
    return s;
}

// ============================================================================
// BREAKPOINTS
// ============================================================================

JsonObject BreakpointLocation::ToJson() const {
    JsonObject obj;
    obj.Set("line", static_cast<int64_t>(line));
    if (column > 0) obj.Set("column", static_cast<int64_t>(column));
    if (endLine > 0) obj.Set("endLine", static_cast<int64_t>(endLine));
    if (endColumn > 0) obj.Set("endColumn", static_cast<int64_t>(endColumn));
    return obj;
}

SourceBreakpoint SourceBreakpoint::FromJson(const JsonObject& obj) {
    SourceBreakpoint bp;
    // Simplified parsing
    return bp;
}

FunctionBreakpoint FunctionBreakpoint::FromJson(const JsonObject& obj) {
    FunctionBreakpoint bp;
    return bp;
}

DataBreakpoint DataBreakpoint::FromJson(const JsonObject& obj) {
    DataBreakpoint bp;
    return bp;
}

InstructionBreakpoint InstructionBreakpoint::FromJson(const JsonObject& obj) {
    InstructionBreakpoint bp;
    return bp;
}

JsonObject Breakpoint::ToJson() const {
    JsonObject obj;
    obj.Set("id", static_cast<int64_t>(id));
    obj.Set("verified", verified);
    if (!message.empty()) obj.Set("message", message);
    if (!source.path.empty()) obj.Set("source", std::string(source.ToJson().ToJson()));
    if (line > 0) obj.Set("line", static_cast<int64_t>(line));
    if (column > 0) obj.Set("column", static_cast<int64_t>(column));
    if (endLine > 0) obj.Set("endLine", static_cast<int64_t>(endLine));
    if (endColumn > 0) obj.Set("endColumn", static_cast<int64_t>(endColumn));
    if (!instructionReference.empty()) obj.Set("instructionReference", instructionReference);
    return obj;
}

// ============================================================================
// STACK & THREADS
// ============================================================================

JsonObject StackFrame::ToJson() const {
    JsonObject obj;
    obj.Set("id", static_cast<int64_t>(id));
    obj.Set("name", name);
    if (!source.path.empty() || !source.name.empty()) {
        // Nested object would require proper JSON building
    }
    obj.Set("line", static_cast<int64_t>(line));
    obj.Set("column", static_cast<int64_t>(column));
    if (endLine > 0) obj.Set("endLine", static_cast<int64_t>(endLine));
    if (endColumn > 0) obj.Set("endColumn", static_cast<int64_t>(endColumn));
    if (canRestart) obj.Set("canRestart", true);
    if (!instructionPointerReference.empty()) {
        obj.Set("instructionPointerReference", instructionPointerReference);
    }
    if (!presentationHint.empty()) obj.Set("presentationHint", presentationHint);
    return obj;
}

JsonObject Thread::ToJson() const {
    JsonObject obj;
    obj.Set("id", static_cast<int64_t>(id));
    obj.Set("name", name);
    return obj;
}

// ============================================================================
// VARIABLES & SCOPES
// ============================================================================

JsonObject Scope::ToJson() const {
    JsonObject obj;
    obj.Set("name", name);
    if (!presentationHint.empty()) obj.Set("presentationHint", presentationHint);
    obj.Set("variablesReference", static_cast<int64_t>(variablesReference));
    if (namedVariables > 0) obj.Set("namedVariables", static_cast<int64_t>(namedVariables));
    if (indexedVariables > 0) obj.Set("indexedVariables", static_cast<int64_t>(indexedVariables));
    obj.Set("expensive", expensive);
    return obj;
}

JsonObject Variable::ToJson() const {
    JsonObject obj;
    obj.Set("name", name);
    obj.Set("value", value);
    if (!type.empty()) obj.Set("type", type);
    if (!evaluateName.empty()) obj.Set("evaluateName", evaluateName);
    obj.Set("variablesReference", static_cast<int64_t>(variablesReference));
    if (namedVariables > 0) obj.Set("namedVariables", static_cast<int64_t>(namedVariables));
    if (indexedVariables > 0) obj.Set("indexedVariables", static_cast<int64_t>(indexedVariables));
    if (!memoryReference.empty()) obj.Set("memoryReference", memoryReference);
    return obj;
}

JsonObject VariablePresentationHint::ToJson() const {
    JsonObject obj;
    if (!kind.empty()) obj.Set("kind", kind);
    if (!visibility.empty()) obj.Set("visibility", visibility);
    if (lazy) obj.Set("lazy", true);
    return obj;
}

// ============================================================================
// MODULES
// ============================================================================

JsonObject Module::ToJson() const {
    JsonObject obj;
    obj.Set("id", static_cast<int64_t>(id));
    obj.Set("name", name);
    if (!path.empty()) obj.Set("path", path);
    obj.Set("isOptimized", isOptimized);
    obj.Set("isUserCode", isUserCode);
    if (!version.empty()) obj.Set("version", version);
    if (!symbolStatus.empty()) obj.Set("symbolStatus", symbolStatus);
    if (!symbolFilePath.empty()) obj.Set("symbolFilePath", symbolFilePath);
    if (!dateTimeStamp.empty()) obj.Set("dateTimeStamp", dateTimeStamp);
    if (!addressRange.empty()) obj.Set("addressRange", addressRange);
    return obj;
}

// ============================================================================
// EXCEPTIONS
// ============================================================================

JsonObject ExceptionBreakpointsFilter::ToJson() const {
    JsonObject obj;
    obj.Set("filter", filter);
    obj.Set("label", label);
    if (!description.empty()) obj.Set("description", description);
    obj.Set("default", defaultValue);
    if (supportsCondition) obj.Set("supportsCondition", true);
    if (!conditionDescription.empty()) obj.Set("conditionDescription", conditionDescription);
    return obj;
}

ExceptionFilterOptions ExceptionFilterOptions::FromJson(const JsonObject& obj) {
    ExceptionFilterOptions opt;
    return opt;
}

ExceptionPathSegment ExceptionPathSegment::FromJson(const JsonObject& obj) {
    ExceptionPathSegment seg;
    return seg;
}

ExceptionOptions ExceptionOptions::FromJson(const JsonObject& obj) {
    ExceptionOptions opt;
    return opt;
}

JsonObject ExceptionDetails::ToJson() const {
    JsonObject obj;
    if (!message.empty()) obj.Set("message", message);
    if (!typeName.empty()) obj.Set("typeName", typeName);
    if (!fullTypeName.empty()) obj.Set("fullTypeName", fullTypeName);
    if (!evaluateName.empty()) obj.Set("evaluateName", evaluateName);
    if (!stackTrace.empty()) obj.Set("stackTrace", stackTrace);
    return obj;
}

// ============================================================================
// MEMORY & DISASSEMBLY
// ============================================================================

JsonObject MemoryContents::ToJson() const {
    JsonObject obj;
    obj.Set("address", address);
    if (unreadableBytes > 0) obj.Set("unreadableBytes", static_cast<int64_t>(unreadableBytes));
    if (!data.empty()) obj.Set("data", data);
    return obj;
}

JsonObject DisassembledInstruction::ToJson() const {
    JsonObject obj;
    obj.Set("address", address);
    if (!instructionBytes.empty()) obj.Set("instructionBytes", instructionBytes);
    obj.Set("instruction", instruction);
    if (!symbol.empty()) obj.Set("symbol", symbol);
    if (line > 0) obj.Set("line", static_cast<int64_t>(line));
    if (column > 0) obj.Set("column", static_cast<int64_t>(column));
    return obj;
}

// ============================================================================
// COMPLETIONS
// ============================================================================

JsonObject CompletionItem::ToJson() const {
    JsonObject obj;
    obj.Set("label", label);
    if (!text.empty()) obj.Set("text", text);
    if (!sortText.empty()) obj.Set("sortText", sortText);
    if (!detail.empty()) obj.Set("detail", detail);
    if (!type.empty()) obj.Set("type", type);
    if (start > 0) obj.Set("start", static_cast<int64_t>(start));
    if (length > 0) obj.Set("length", static_cast<int64_t>(length));
    return obj;
}

ValueFormat ValueFormat::FromJson(const JsonObject& obj) {
    ValueFormat fmt;
    return fmt;
}

// ============================================================================
// ENUMS
// ============================================================================

std::string SteppingGranularityToString(SteppingGranularity granularity) {
    switch (granularity) {
        case SteppingGranularity::Statement: return "statement";
        case SteppingGranularity::Line: return "line";
        case SteppingGranularity::Instruction: return "instruction";
    }
    return "statement";
}

SteppingGranularity StringToSteppingGranularity(const std::string& str) {
    if (str == "line") return SteppingGranularity::Line;
    if (str == "instruction") return SteppingGranularity::Instruction;
    return SteppingGranularity::Statement;
}

std::string DAPStopReasonToString(DAPStopReason reason) {
    switch (reason) {
        case DAPStopReason::Step: return "step";
        case DAPStopReason::Breakpoint: return "breakpoint";
        case DAPStopReason::Exception: return "exception";
        case DAPStopReason::Pause: return "pause";
        case DAPStopReason::Entry: return "entry";
        case DAPStopReason::Goto: return "goto";
        case DAPStopReason::FunctionBreakpoint: return "function breakpoint";
        case DAPStopReason::DataBreakpoint: return "data breakpoint";
        case DAPStopReason::InstructionBreakpoint: return "instruction breakpoint";
    }
    return "breakpoint";
}

std::string OutputCategoryToString(OutputCategory category) {
    switch (category) {
        case OutputCategory::Console: return "console";
        case OutputCategory::Important: return "important";
        case OutputCategory::Stdout: return "stdout";
        case OutputCategory::Stderr: return "stderr";
        case OutputCategory::Telemetry: return "telemetry";
    }
    return "console";
}

std::string InvalidatedAreasToString(InvalidatedAreas area) {
    switch (area) {
        case InvalidatedAreas::All: return "all";
        case InvalidatedAreas::Stacks: return "stacks";
        case InvalidatedAreas::Threads: return "threads";
        case InvalidatedAreas::Variables: return "variables";
    }
    return "all";
}

} // namespace DAP
} // namespace IDE
} // namespace UltraCanvas
