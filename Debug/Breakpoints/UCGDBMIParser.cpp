// Apps/IDE/Debug/GDB/UCGDBMIParser.cpp
// GDB/MI Protocol Parser Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGDBMIParser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// GDBMIValue IMPLEMENTATION
// ============================================================================

std::string GDBMIValue::GetString(const std::string& key, const std::string& def) const {
    if (type != Type::Tuple) return def;
    auto it = tupleValue.find(key);
    if (it == tupleValue.end()) return def;
    if (it->second.type != Type::String) return def;
    return it->second.stringValue;
}

int GDBMIValue::GetInt(const std::string& key, int def) const {
    std::string s = GetString(key, "");
    if (s.empty()) return def;
    try {
        return std::stoi(s);
    } catch (...) {
        return def;
    }
}

uint64_t GDBMIValue::GetUInt64(const std::string& key, uint64_t def) const {
    std::string s = GetString(key, "");
    if (s.empty()) return def;
    try {
        if (s.substr(0, 2) == "0x" || s.substr(0, 2) == "0X") {
            return std::stoull(s, nullptr, 16);
        }
        return std::stoull(s);
    } catch (...) {
        return def;
    }
}

const GDBMIValue* GDBMIValue::Get(const std::string& key) const {
    if (type != Type::Tuple) return nullptr;
    auto it = tupleValue.find(key);
    return (it != tupleValue.end()) ? &it->second : nullptr;
}

bool GDBMIValue::Has(const std::string& key) const {
    if (type != Type::Tuple) return false;
    return tupleValue.find(key) != tupleValue.end();
}

// ============================================================================
// GDBMIRecord IMPLEMENTATION
// ============================================================================

bool GDBMIRecord::IsAsync() const {
    return type == GDBMIRecordType::AsyncExec ||
           type == GDBMIRecordType::AsyncStatus ||
           type == GDBMIRecordType::AsyncNotify;
}

bool GDBMIRecord::IsStream() const {
    return type == GDBMIRecordType::ConsoleStream ||
           type == GDBMIRecordType::TargetStream ||
           type == GDBMIRecordType::LogStream;
}

std::string GDBMIRecord::GetError() const {
    if (!IsError()) return "";
    auto it = results.find("msg");
    if (it != results.end() && it->second.IsString()) {
        return it->second.stringValue;
    }
    return "Unknown error";
}

// ============================================================================
// UCGDBMIParser IMPLEMENTATION
// ============================================================================

UCGDBMIParser::UCGDBMIParser() = default;
UCGDBMIParser::~UCGDBMIParser() = default;

GDBMIRecord UCGDBMIParser::ParseLine(const std::string& line) {
    GDBMIRecord record;
    record.rawLine = line;
    
    if (line.empty()) {
        return record;
    }
    
    // Check for (gdb) prompt
    if (line == "(gdb)" || line == "(gdb) ") {
        record.type = GDBMIRecordType::Prompt;
        return record;
    }
    
    size_t pos = 0;
    
    // Parse optional token
    while (pos < line.size() && std::isdigit(line[pos])) {
        pos++;
    }
    if (pos > 0) {
        record.token = std::stoi(line.substr(0, pos));
    }
    
    if (pos >= line.size()) {
        return record;
    }
    
    char prefix = line[pos];
    pos++;
    
    switch (prefix) {
        case '^': {
            // Result record: ^done, ^running, ^connected, ^error, ^exit
            record.type = GDBMIRecordType::Result;
            size_t classEnd = line.find(',', pos);
            if (classEnd == std::string::npos) {
                classEnd = line.size();
            }
            std::string resultClass = line.substr(pos, classEnd - pos);
            record.resultClass = ParseResultClass(resultClass);
            pos = classEnd;
            
            // Parse results
            if (pos < line.size() && line[pos] == ',') {
                pos++;
                record.results = ParseTuple(line, pos);
            }
            break;
        }
        
        case '*': {
            // Async exec record: *stopped, *running
            record.type = GDBMIRecordType::AsyncExec;
            size_t classEnd = line.find(',', pos);
            if (classEnd == std::string::npos) {
                classEnd = line.size();
            }
            record.asyncClassName = line.substr(pos, classEnd - pos);
            record.asyncClass = ParseAsyncClass(record.asyncClassName);
            pos = classEnd;
            
            if (pos < line.size() && line[pos] == ',') {
                pos++;
                record.results = ParseTuple(line, pos);
            }
            break;
        }
        
        case '+': {
            // Async status record
            record.type = GDBMIRecordType::AsyncStatus;
            size_t classEnd = line.find(',', pos);
            if (classEnd == std::string::npos) {
                classEnd = line.size();
            }
            record.asyncClassName = line.substr(pos, classEnd - pos);
            pos = classEnd;
            
            if (pos < line.size() && line[pos] == ',') {
                pos++;
                record.results = ParseTuple(line, pos);
            }
            break;
        }
        
        case '=': {
            // Async notify record
            record.type = GDBMIRecordType::AsyncNotify;
            size_t classEnd = line.find(',', pos);
            if (classEnd == std::string::npos) {
                classEnd = line.size();
            }
            record.asyncClassName = line.substr(pos, classEnd - pos);
            record.asyncClass = ParseAsyncClass(record.asyncClassName);
            pos = classEnd;
            
            if (pos < line.size() && line[pos] == ',') {
                pos++;
                record.results = ParseTuple(line, pos);
            }
            break;
        }
        
        case '~': {
            // Console stream output
            record.type = GDBMIRecordType::ConsoleStream;
            record.streamContent = ParseCString(line, pos);
            break;
        }
        
        case '@': {
            // Target stream output
            record.type = GDBMIRecordType::TargetStream;
            record.streamContent = ParseCString(line, pos);
            break;
        }
        
        case '&': {
            // Log stream output
            record.type = GDBMIRecordType::LogStream;
            record.streamContent = ParseCString(line, pos);
            break;
        }
        
        default:
            // Unknown format, treat as console output
            record.type = GDBMIRecordType::ConsoleStream;
            record.streamContent = line;
            break;
    }
    
    return record;
}

std::vector<GDBMIRecord> UCGDBMIParser::ParseOutput(const std::string& output) {
    std::vector<GDBMIRecord> records;
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove trailing CR if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            records.push_back(ParseLine(line));
        }
    }
    
    return records;
}

GDBMIValue UCGDBMIParser::ParseValue(const std::string& str, size_t& pos) {
    SkipWhitespace(str, pos);
    
    if (pos >= str.size()) {
        return GDBMIValue("");
    }
    
    char c = str[pos];
    
    if (c == '"') {
        // C-string
        return GDBMIValue(ParseCString(str, pos));
    } else if (c == '{') {
        // Tuple
        GDBMIValue val;
        val.type = GDBMIValue::Type::Tuple;
        val.tupleValue = ParseTuple(str, pos);
        return val;
    } else if (c == '[') {
        // List
        GDBMIValue val;
        val.type = GDBMIValue::Type::List;
        val.listValue = ParseList(str, pos);
        return val;
    } else {
        // Unquoted string (until comma or end)
        return GDBMIValue(ParseString(str, pos));
    }
}

GDBMITuple UCGDBMIParser::ParseTuple(const std::string& str, size_t& pos) {
    GDBMITuple tuple;
    
    SkipWhitespace(str, pos);
    
    // Check for opening brace (optional in some contexts)
    bool hasBrace = false;
    if (pos < str.size() && str[pos] == '{') {
        hasBrace = true;
        pos++;
    }
    
    while (pos < str.size()) {
        SkipWhitespace(str, pos);
        
        if (hasBrace && str[pos] == '}') {
            pos++;
            break;
        }
        
        // Parse key
        std::string key;
        while (pos < str.size() && str[pos] != '=' && str[pos] != ',' && 
               str[pos] != '}' && !std::isspace(str[pos])) {
            key += str[pos++];
        }
        
        if (key.empty()) {
            if (pos < str.size() && str[pos] == ',') {
                pos++;
                continue;
            }
            break;
        }
        
        SkipWhitespace(str, pos);
        
        if (pos < str.size() && str[pos] == '=') {
            pos++;
            SkipWhitespace(str, pos);
            tuple[key] = ParseValue(str, pos);
        }
        
        SkipWhitespace(str, pos);
        
        if (pos < str.size() && str[pos] == ',') {
            pos++;
        } else if (!hasBrace) {
            break;
        }
    }
    
    return tuple;
}

GDBMIList UCGDBMIParser::ParseList(const std::string& str, size_t& pos) {
    GDBMIList list;
    
    SkipWhitespace(str, pos);
    
    if (pos >= str.size() || str[pos] != '[') {
        return list;
    }
    pos++;
    
    while (pos < str.size()) {
        SkipWhitespace(str, pos);
        
        if (str[pos] == ']') {
            pos++;
            break;
        }
        
        // Check if this is a result (key=value) or just a value
        size_t lookAhead = pos;
        while (lookAhead < str.size() && str[lookAhead] != '=' && 
               str[lookAhead] != ',' && str[lookAhead] != ']') {
            lookAhead++;
        }
        
        if (lookAhead < str.size() && str[lookAhead] == '=') {
            // It's a tuple element
            GDBMIValue tupleVal;
            tupleVal.type = GDBMIValue::Type::Tuple;
            
            std::string key;
            while (pos < str.size() && str[pos] != '=') {
                key += str[pos++];
            }
            pos++; // skip '='
            
            tupleVal.tupleValue[key] = ParseValue(str, pos);
            list.push_back(tupleVal);
        } else {
            // Just a value
            list.push_back(ParseValue(str, pos));
        }
        
        SkipWhitespace(str, pos);
        
        if (pos < str.size() && str[pos] == ',') {
            pos++;
        }
    }
    
    return list;
}

std::string UCGDBMIParser::ParseString(const std::string& str, size_t& pos) {
    std::string result;
    
    while (pos < str.size() && str[pos] != ',' && str[pos] != '}' && 
           str[pos] != ']' && !std::isspace(str[pos])) {
        result += str[pos++];
    }
    
    return result;
}

std::string UCGDBMIParser::ParseCString(const std::string& str, size_t& pos) {
    if (pos >= str.size() || str[pos] != '"') {
        return "";
    }
    pos++; // skip opening quote
    
    std::string result;
    
    while (pos < str.size() && str[pos] != '"') {
        if (str[pos] == '\\' && pos + 1 < str.size()) {
            pos++;
            switch (str[pos]) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += str[pos]; break;
            }
        } else {
            result += str[pos];
        }
        pos++;
    }
    
    if (pos < str.size()) {
        pos++; // skip closing quote
    }
    
    return result;
}

GDBMIResultClass UCGDBMIParser::ParseResultClass(const std::string& cls) {
    if (cls == "done") return GDBMIResultClass::Done;
    if (cls == "running") return GDBMIResultClass::Running;
    if (cls == "connected") return GDBMIResultClass::Connected;
    if (cls == "error") return GDBMIResultClass::Error;
    if (cls == "exit") return GDBMIResultClass::Exit;
    return GDBMIResultClass::Done;
}

GDBMIAsyncClass UCGDBMIParser::ParseAsyncClass(const std::string& cls) {
    if (cls == "stopped") return GDBMIAsyncClass::Stopped;
    if (cls == "running") return GDBMIAsyncClass::Running;
    if (cls == "thread-group-added") return GDBMIAsyncClass::ThreadGroupAdded;
    if (cls == "thread-group-started") return GDBMIAsyncClass::ThreadGroupStarted;
    if (cls == "thread-group-exited") return GDBMIAsyncClass::ThreadGroupExited;
    if (cls == "thread-created") return GDBMIAsyncClass::ThreadCreated;
    if (cls == "thread-exited") return GDBMIAsyncClass::ThreadExited;
    if (cls == "library-loaded") return GDBMIAsyncClass::LibraryLoaded;
    if (cls == "library-unloaded") return GDBMIAsyncClass::LibraryUnloaded;
    if (cls == "breakpoint-created") return GDBMIAsyncClass::BreakpointCreated;
    if (cls == "breakpoint-modified") return GDBMIAsyncClass::BreakpointModified;
    if (cls == "breakpoint-deleted") return GDBMIAsyncClass::BreakpointDeleted;
    return GDBMIAsyncClass::Other;
}

void UCGDBMIParser::SkipWhitespace(const std::string& str, size_t& pos) {
    while (pos < str.size() && std::isspace(str[pos])) {
        pos++;
    }
}

std::string UCGDBMIParser::Escape(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 10);
    
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

std::string UCGDBMIParser::Unescape(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            i++;
            switch (str[i]) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

// ============================================================================
// GDBMICommandBuilder IMPLEMENTATION
// ============================================================================

GDBMICommandBuilder& GDBMICommandBuilder::SetToken(int token) {
    token_ = token;
    return *this;
}

GDBMICommandBuilder& GDBMICommandBuilder::SetCommand(const std::string& cmd) {
    command_ = cmd;
    return *this;
}

GDBMICommandBuilder& GDBMICommandBuilder::AddOption(const std::string& opt) {
    options_.push_back(opt);
    return *this;
}

GDBMICommandBuilder& GDBMICommandBuilder::AddOption(const std::string& opt, const std::string& value) {
    options_.push_back(opt + " " + value);
    return *this;
}

GDBMICommandBuilder& GDBMICommandBuilder::AddArgument(const std::string& arg) {
    arguments_.push_back(arg);
    return *this;
}

GDBMICommandBuilder& GDBMICommandBuilder::AddQuotedArgument(const std::string& arg) {
    arguments_.push_back("\"" + UCGDBMIParser::Escape(arg) + "\"");
    return *this;
}

std::string GDBMICommandBuilder::Build() const {
    std::ostringstream cmd;
    
    if (token_ >= 0) {
        cmd << token_;
    }
    
    cmd << command_;
    
    for (const auto& opt : options_) {
        cmd << " " << opt;
    }
    
    for (const auto& arg : arguments_) {
        cmd << " " << arg;
    }
    
    return cmd.str();
}

// Static command builders
std::string GDBMICommandBuilder::BreakInsert(const std::string& location, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-break-insert");
    if (token >= 0) builder.SetToken(token);
    builder.AddQuotedArgument(location);
    return builder.Build();
}

std::string GDBMICommandBuilder::BreakDelete(int bpNum, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-break-delete");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(bpNum));
    return builder.Build();
}

std::string GDBMICommandBuilder::BreakEnable(int bpNum, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-break-enable");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(bpNum));
    return builder.Build();
}

std::string GDBMICommandBuilder::BreakDisable(int bpNum, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-break-disable");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(bpNum));
    return builder.Build();
}

std::string GDBMICommandBuilder::BreakCondition(int bpNum, const std::string& cond, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-break-condition");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(bpNum));
    builder.AddQuotedArgument(cond);
    return builder.Build();
}

std::string GDBMICommandBuilder::ExecRun(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-exec-run");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::ExecContinue(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-exec-continue");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::ExecNext(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-exec-next");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::ExecStep(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-exec-step");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::ExecFinish(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-exec-finish");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::ExecInterrupt(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-exec-interrupt");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::StackListFrames(int maxDepth, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-stack-list-frames");
    if (token >= 0) builder.SetToken(token);
    if (maxDepth > 0) {
        builder.AddArgument("0");
        builder.AddArgument(std::to_string(maxDepth - 1));
    }
    return builder.Build();
}

std::string GDBMICommandBuilder::StackListVariables(int printValues, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-stack-list-variables");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(printValues));
    return builder.Build();
}

std::string GDBMICommandBuilder::StackListLocals(int printValues, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-stack-list-locals");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(printValues));
    return builder.Build();
}

std::string GDBMICommandBuilder::StackListArguments(int printValues, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-stack-list-arguments");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(printValues));
    return builder.Build();
}

std::string GDBMICommandBuilder::ThreadInfo(int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-thread-info");
    if (token >= 0) builder.SetToken(token);
    return builder.Build();
}

std::string GDBMICommandBuilder::ThreadSelect(int threadId, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-thread-select");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(std::to_string(threadId));
    return builder.Build();
}

std::string GDBMICommandBuilder::DataEvaluateExpression(const std::string& expr, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-data-evaluate-expression");
    if (token >= 0) builder.SetToken(token);
    builder.AddQuotedArgument(expr);
    return builder.Build();
}

std::string GDBMICommandBuilder::DataReadMemory(uint64_t addr, size_t count, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-data-read-memory-bytes");
    if (token >= 0) builder.SetToken(token);
    
    std::ostringstream addrStr;
    addrStr << "0x" << std::hex << addr;
    builder.AddArgument(addrStr.str());
    builder.AddArgument(std::to_string(count));
    return builder.Build();
}

std::string GDBMICommandBuilder::DataDisassemble(uint64_t start, uint64_t end, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-data-disassemble");
    if (token >= 0) builder.SetToken(token);
    
    std::ostringstream startStr, endStr;
    startStr << "0x" << std::hex << start;
    endStr << "0x" << std::hex << end;
    
    builder.AddOption("-s", startStr.str());
    builder.AddOption("-e", endStr.str());
    builder.AddArgument("0"); // mode: disassembly only
    return builder.Build();
}

std::string GDBMICommandBuilder::VarCreate(const std::string& name, const std::string& expr, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-var-create");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(name);
    builder.AddArgument("@");  // floating variable
    builder.AddQuotedArgument(expr);
    return builder.Build();
}

std::string GDBMICommandBuilder::VarDelete(const std::string& name, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-var-delete");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(name);
    return builder.Build();
}

std::string GDBMICommandBuilder::VarListChildren(const std::string& name, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-var-list-children");
    if (token >= 0) builder.SetToken(token);
    builder.AddOption("--all-values");
    builder.AddArgument(name);
    return builder.Build();
}

std::string GDBMICommandBuilder::VarEvaluate(const std::string& name, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-var-evaluate-expression");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(name);
    return builder.Build();
}

std::string GDBMICommandBuilder::VarAssign(const std::string& name, const std::string& value, int token) {
    GDBMICommandBuilder builder;
    builder.SetCommand("-var-assign");
    if (token >= 0) builder.SetToken(token);
    builder.AddArgument(name);
    builder.AddQuotedArgument(value);
    return builder.Build();
}

} // namespace IDE
} // namespace UltraCanvas
