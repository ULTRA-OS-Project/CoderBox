// Apps/IDE/Debug/GDB/UCGDBMIParser.h
// GDB/MI Protocol Parser for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace UltraCanvas {
namespace IDE {

// GDB/MI Value (string, list, or tuple)
struct GDBMIValue;
using GDBMIList = std::vector<GDBMIValue>;
using GDBMITuple = std::map<std::string, GDBMIValue>;

struct GDBMIValue {
    enum class Type { String, List, Tuple };
    Type type = Type::String;
    std::string stringValue;
    GDBMIList listValue;
    GDBMITuple tupleValue;
    
    GDBMIValue() = default;
    explicit GDBMIValue(const std::string& s) : type(Type::String), stringValue(s) {}
    
    bool IsString() const { return type == Type::String; }
    bool IsList() const { return type == Type::List; }
    bool IsTuple() const { return type == Type::Tuple; }
    
    std::string GetString(const std::string& key, const std::string& def = "") const;
    int GetInt(const std::string& key, int def = 0) const;
    uint64_t GetUInt64(const std::string& key, uint64_t def = 0) const;
    const GDBMIValue* Get(const std::string& key) const;
    bool Has(const std::string& key) const;
};

// GDB/MI Record Types
enum class GDBMIRecordType {
    Result, AsyncExec, AsyncStatus, AsyncNotify,
    ConsoleStream, TargetStream, LogStream, Prompt
};

enum class GDBMIResultClass { Done, Running, Connected, Error, Exit };

enum class GDBMIAsyncClass {
    Stopped, Running, ThreadGroupAdded, ThreadGroupStarted, ThreadGroupExited,
    ThreadCreated, ThreadExited, LibraryLoaded, LibraryUnloaded,
    BreakpointCreated, BreakpointModified, BreakpointDeleted, Other
};

// Parsed GDB/MI Record
struct GDBMIRecord {
    GDBMIRecordType type = GDBMIRecordType::Prompt;
    int token = -1;
    GDBMIResultClass resultClass = GDBMIResultClass::Done;
    GDBMIAsyncClass asyncClass = GDBMIAsyncClass::Other;
    std::string asyncClassName;
    std::string streamContent;
    GDBMITuple results;
    std::string rawLine;
    
    bool IsResult() const { return type == GDBMIRecordType::Result; }
    bool IsAsync() const;
    bool IsStream() const;
    bool IsSuccess() const { return IsResult() && resultClass == GDBMIResultClass::Done; }
    bool IsError() const { return IsResult() && resultClass == GDBMIResultClass::Error; }
    std::string GetError() const;
};

// GDB/MI Parser
class UCGDBMIParser {
public:
    UCGDBMIParser();
    ~UCGDBMIParser();
    
    GDBMIRecord ParseLine(const std::string& line);
    std::vector<GDBMIRecord> ParseOutput(const std::string& output);
    
    static std::string Escape(const std::string& str);
    static std::string Unescape(const std::string& str);
    
private:
    GDBMIValue ParseValue(const std::string& str, size_t& pos);
    GDBMITuple ParseTuple(const std::string& str, size_t& pos);
    GDBMIList ParseList(const std::string& str, size_t& pos);
    std::string ParseString(const std::string& str, size_t& pos);
    std::string ParseCString(const std::string& str, size_t& pos);
    
    GDBMIResultClass ParseResultClass(const std::string& cls);
    GDBMIAsyncClass ParseAsyncClass(const std::string& cls);
    
    void SkipWhitespace(const std::string& str, size_t& pos);
};

// GDB/MI Command Builder
class GDBMICommandBuilder {
public:
    GDBMICommandBuilder& SetToken(int token);
    GDBMICommandBuilder& SetCommand(const std::string& cmd);
    GDBMICommandBuilder& AddOption(const std::string& opt);
    GDBMICommandBuilder& AddOption(const std::string& opt, const std::string& value);
    GDBMICommandBuilder& AddArgument(const std::string& arg);
    GDBMICommandBuilder& AddQuotedArgument(const std::string& arg);
    
    std::string Build() const;
    
    // Common commands
    static std::string BreakInsert(const std::string& location, int token = -1);
    static std::string BreakDelete(int bpNum, int token = -1);
    static std::string BreakEnable(int bpNum, int token = -1);
    static std::string BreakDisable(int bpNum, int token = -1);
    static std::string BreakCondition(int bpNum, const std::string& cond, int token = -1);
    
    static std::string ExecRun(int token = -1);
    static std::string ExecContinue(int token = -1);
    static std::string ExecNext(int token = -1);
    static std::string ExecStep(int token = -1);
    static std::string ExecFinish(int token = -1);
    static std::string ExecInterrupt(int token = -1);
    
    static std::string StackListFrames(int maxDepth = -1, int token = -1);
    static std::string StackListVariables(int printValues = 1, int token = -1);
    static std::string StackListLocals(int printValues = 1, int token = -1);
    static std::string StackListArguments(int printValues = 1, int token = -1);
    
    static std::string ThreadInfo(int token = -1);
    static std::string ThreadSelect(int threadId, int token = -1);
    
    static std::string DataEvaluateExpression(const std::string& expr, int token = -1);
    static std::string DataReadMemory(uint64_t addr, size_t count, int token = -1);
    static std::string DataDisassemble(uint64_t start, uint64_t end, int token = -1);
    
    static std::string VarCreate(const std::string& name, const std::string& expr, int token = -1);
    static std::string VarDelete(const std::string& name, int token = -1);
    static std::string VarListChildren(const std::string& name, int token = -1);
    static std::string VarEvaluate(const std::string& name, int token = -1);
    static std::string VarAssign(const std::string& name, const std::string& value, int token = -1);
    
private:
    int token_ = -1;
    std::string command_;
    std::vector<std::string> options_;
    std::vector<std::string> arguments_;
};

} // namespace IDE
} // namespace UltraCanvas
