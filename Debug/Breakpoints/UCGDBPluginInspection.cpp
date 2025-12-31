// Apps/IDE/Debug/GDB/UCGDBPluginInspection.cpp
// GDB Plugin Inspection Functions for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGDBPlugin.h"
#include <sstream>
#include <iomanip>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// THREADS
// ============================================================================

std::vector<ThreadInfo> UCGDBPlugin::GetThreads() {
    std::vector<ThreadInfo> threads;
    
    GDBMIRecord result = SendCommand("-thread-info");
    if (!result.IsSuccess()) return threads;
    
    auto threadsIt = result.results.find("threads");
    if (threadsIt == result.results.end() || !threadsIt->second.IsList()) {
        return threads;
    }
    
    for (const auto& threadVal : threadsIt->second.listValue) {
        if (threadVal.IsTuple()) {
            threads.push_back(ParseThreadResult(threadVal.tupleValue));
        }
    }
    
    auto currentIt = result.results.find("current-thread-id");
    if (currentIt != result.results.end()) {
        int currentId = std::stoi(currentIt->second.stringValue);
        for (auto& thread : threads) {
            thread.isCurrent = (thread.id == currentId);
        }
    }
    
    return threads;
}

ThreadInfo UCGDBPlugin::GetCurrentThread() {
    auto threads = GetThreads();
    for (const auto& thread : threads) {
        if (thread.isCurrent) return thread;
    }
    return threads.empty() ? ThreadInfo() : threads[0];
}

bool UCGDBPlugin::SwitchThread(int threadId) {
    GDBMIRecord result = SendCommand(GDBMICommandBuilder::ThreadSelect(threadId));
    if (result.IsSuccess()) {
        currentThreadId_ = threadId;
        return true;
    }
    return false;
}

ThreadInfo UCGDBPlugin::ParseThreadResult(const std::map<std::string, GDBMIValue>& thread) {
    ThreadInfo info;
    
    auto idIt = thread.find("id");
    if (idIt != thread.end()) info.id = std::stoi(idIt->second.stringValue);
    
    auto targetIdIt = thread.find("target-id");
    if (targetIdIt != thread.end()) info.name = targetIdIt->second.stringValue;
    
    auto stateIt = thread.find("state");
    if (stateIt != thread.end()) {
        info.state = (stateIt->second.stringValue == "stopped") ? 
            DebugSessionState::Paused : DebugSessionState::Running;
    }
    
    auto frameIt = thread.find("frame");
    if (frameIt != thread.end() && frameIt->second.IsTuple()) {
        const auto& frame = frameIt->second.tupleValue;
        auto funcIt = frame.find("func");
        if (funcIt != frame.end()) info.currentFunction = funcIt->second.stringValue;
        
        auto fileIt = frame.find("fullname");
        if (fileIt == frame.end()) fileIt = frame.find("file");
        if (fileIt != frame.end()) info.currentLocation.filePath = fileIt->second.stringValue;
        
        auto lineIt = frame.find("line");
        if (lineIt != frame.end()) info.currentLocation.line = std::stoi(lineIt->second.stringValue);
    }
    
    return info;
}

// ============================================================================
// STACK
// ============================================================================

std::vector<StackFrame> UCGDBPlugin::GetCallStack(int maxFrames) {
    return GetCallStack(currentThreadId_, maxFrames);
}

std::vector<StackFrame> UCGDBPlugin::GetCallStack(int threadId, int maxFrames) {
    std::vector<StackFrame> frames;
    
    if (threadId != currentThreadId_) SwitchThread(threadId);
    
    GDBMIRecord result = SendCommand(GDBMICommandBuilder::StackListFrames(maxFrames));
    if (!result.IsSuccess()) return frames;
    
    auto stackIt = result.results.find("stack");
    if (stackIt == result.results.end() || !stackIt->second.IsList()) return frames;
    
    for (const auto& frameVal : stackIt->second.listValue) {
        if (frameVal.IsTuple()) {
            auto frameIt = frameVal.tupleValue.find("frame");
            if (frameIt != frameVal.tupleValue.end() && frameIt->second.IsTuple()) {
                frames.push_back(ParseFrameResult(frameIt->second.tupleValue));
            } else {
                frames.push_back(ParseFrameResult(frameVal.tupleValue));
            }
        }
    }
    
    return frames;
}

bool UCGDBPlugin::SwitchFrame(int frameLevel) {
    GDBMIRecord result = SendCommand("-stack-select-frame " + std::to_string(frameLevel));
    if (result.IsSuccess()) {
        currentFrameLevel_ = frameLevel;
        return true;
    }
    return false;
}

StackFrame UCGDBPlugin::GetCurrentFrame() {
    auto frames = GetCallStack(1);
    return frames.empty() ? StackFrame() : frames[0];
}

StackFrame UCGDBPlugin::ParseFrameResult(const std::map<std::string, GDBMIValue>& frame) {
    StackFrame sf;
    
    auto levelIt = frame.find("level");
    if (levelIt != frame.end()) sf.level = std::stoi(levelIt->second.stringValue);
    
    auto funcIt = frame.find("func");
    if (funcIt != frame.end()) sf.functionName = funcIt->second.stringValue;
    
    auto fileIt = frame.find("fullname");
    if (fileIt == frame.end()) fileIt = frame.find("file");
    if (fileIt != frame.end()) sf.location.filePath = fileIt->second.stringValue;
    
    auto lineIt = frame.find("line");
    if (lineIt != frame.end()) sf.location.line = std::stoi(lineIt->second.stringValue);
    
    auto addrIt = frame.find("addr");
    if (addrIt != frame.end()) {
        sf.address = std::stoull(addrIt->second.stringValue, nullptr, 16);
    }
    
    auto argsIt = frame.find("args");
    if (argsIt != frame.end() && argsIt->second.IsList()) {
        std::ostringstream args;
        bool first = true;
        for (const auto& arg : argsIt->second.listValue) {
            if (!first) args << ", ";
            first = false;
            if (arg.IsTuple()) {
                auto nameIt = arg.tupleValue.find("name");
                auto valueIt = arg.tupleValue.find("value");
                if (nameIt != arg.tupleValue.end()) {
                    args << nameIt->second.stringValue;
                    if (valueIt != arg.tupleValue.end()) {
                        args << "=" << valueIt->second.stringValue;
                    }
                }
            }
        }
        sf.args = args.str();
    }
    
    sf.hasDebugInfo = !sf.location.filePath.empty();
    sf.threadId = currentThreadId_;
    
    return sf;
}

// ============================================================================
// VARIABLES
// ============================================================================

std::vector<Variable> UCGDBPlugin::GetLocals() {
    std::vector<Variable> vars;
    
    GDBMIRecord result = SendCommand(GDBMICommandBuilder::StackListLocals(2));
    if (!result.IsSuccess()) return vars;
    
    auto localsIt = result.results.find("locals");
    if (localsIt == result.results.end() || !localsIt->second.IsList()) return vars;
    
    for (const auto& varVal : localsIt->second.listValue) {
        if (varVal.IsTuple()) {
            vars.push_back(ParseVariableResult(varVal.tupleValue));
        }
    }
    
    return vars;
}

std::vector<Variable> UCGDBPlugin::GetArguments() {
    std::vector<Variable> vars;
    
    GDBMIRecord result = SendCommand(GDBMICommandBuilder::StackListArguments(2));
    if (!result.IsSuccess()) return vars;
    
    auto stackArgsIt = result.results.find("stack-args");
    if (stackArgsIt == result.results.end() || !stackArgsIt->second.IsList()) return vars;
    
    for (const auto& frameVal : stackArgsIt->second.listValue) {
        if (!frameVal.IsTuple()) continue;
        
        auto argsIt = frameVal.tupleValue.find("args");
        if (argsIt == frameVal.tupleValue.end() || !argsIt->second.IsList()) continue;
        
        for (const auto& argVal : argsIt->second.listValue) {
            if (argVal.IsTuple()) {
                vars.push_back(ParseVariableResult(argVal.tupleValue));
            }
        }
        break; // Only current frame
    }
    
    return vars;
}

std::vector<Variable> UCGDBPlugin::GetGlobals() {
    // GDB doesn't have a direct command for globals
    // Would need to parse symbol table
    return {};
}

EvaluationResult UCGDBPlugin::Evaluate(const std::string& expression) {
    return Evaluate(expression, EvaluationContext::Watch);
}

EvaluationResult UCGDBPlugin::Evaluate(const std::string& expression, EvaluationContext context) {
    EvaluationResult result;
    
    GDBMIRecord response = SendCommand(
        GDBMICommandBuilder::DataEvaluateExpression(expression));
    
    if (response.IsError()) {
        result.success = false;
        result.errorMessage = response.GetError();
        return result;
    }
    
    result.success = true;
    auto valueIt = response.results.find("value");
    if (valueIt != response.results.end()) {
        result.variable.name = expression;
        result.variable.value = valueIt->second.stringValue;
    }
    
    return result;
}

bool UCGDBPlugin::SetVariable(const std::string& name, const std::string& value) {
    std::string cmd = "-gdb-set var " + name + "=" + value;
    GDBMIRecord result = SendCommand(cmd);
    return result.IsSuccess();
}

std::vector<Variable> UCGDBPlugin::ExpandVariable(int variablesReference) {
    std::vector<Variable> children;
    
    auto it = variableObjects_.find(variablesReference);
    if (it == variableObjects_.end()) return children;
    
    GDBMIRecord result = SendCommand(
        GDBMICommandBuilder::VarListChildren(it->second));
    if (!result.IsSuccess()) return children;
    
    auto childrenIt = result.results.find("children");
    if (childrenIt == result.results.end() || !childrenIt->second.IsList()) return children;
    
    for (const auto& childVal : childrenIt->second.listValue) {
        if (!childVal.IsTuple()) continue;
        
        auto childIt = childVal.tupleValue.find("child");
        if (childIt != childVal.tupleValue.end() && childIt->second.IsTuple()) {
            Variable var = ParseVariableResult(childIt->second.tupleValue);
            children.push_back(var);
        }
    }
    
    return children;
}

Variable UCGDBPlugin::ParseVariableResult(const std::map<std::string, GDBMIValue>& var) {
    Variable v;
    
    auto nameIt = var.find("name");
    if (nameIt != var.end()) v.name = nameIt->second.stringValue;
    
    auto valueIt = var.find("value");
    if (valueIt != var.end()) v.value = valueIt->second.stringValue;
    
    auto typeIt = var.find("type");
    if (typeIt != var.end()) v.type = typeIt->second.stringValue;
    
    auto numchildIt = var.find("numchild");
    if (numchildIt != var.end()) {
        v.numChildren = std::stoi(numchildIt->second.stringValue);
        v.hasChildren = v.numChildren > 0;
    }
    
    // Determine category from type
    if (!v.type.empty()) {
        if (v.type.find('*') != std::string::npos) {
            v.category = VariableCategory::Pointer;
            v.isPointer = true;
        } else if (v.type.find('&') != std::string::npos) {
            v.category = VariableCategory::Reference;
            v.isReference = true;
        } else if (v.type.find('[') != std::string::npos) {
            v.category = VariableCategory::Array;
        } else if (v.type.find("struct") != std::string::npos) {
            v.category = VariableCategory::Struct;
        } else if (v.type.find("class") != std::string::npos) {
            v.category = VariableCategory::Class;
        } else if (v.type == "int" || v.type == "float" || v.type == "double" ||
                   v.type == "char" || v.type == "bool" || v.type == "long") {
            v.category = VariableCategory::Primitive;
        }
    }
    
    // Create variable object for expansion if has children
    if (v.hasChildren) {
        std::string varObjName = "var" + std::to_string(nextVariableRef_);
        GDBMIRecord createResult = SendCommand(
            GDBMICommandBuilder::VarCreate(varObjName, v.name));
        
        if (createResult.IsSuccess()) {
            v.variablesReference = nextVariableRef_;
            variableObjects_[nextVariableRef_] = varObjName;
            nextVariableRef_++;
        }
    }
    
    return v;
}

// ============================================================================
// REGISTERS
// ============================================================================

std::vector<Register> UCGDBPlugin::GetRegisters() {
    std::vector<Register> registers;
    
    // Get register names first
    GDBMIRecord namesResult = SendCommand("-data-list-register-names");
    if (!namesResult.IsSuccess()) return registers;
    
    auto namesIt = namesResult.results.find("register-names");
    if (namesIt == namesResult.results.end() || !namesIt->second.IsList()) return registers;
    
    std::vector<std::string> names;
    for (const auto& nameVal : namesIt->second.listValue) {
        names.push_back(nameVal.stringValue);
    }
    
    // Get register values
    GDBMIRecord valuesResult = SendCommand("-data-list-register-values x");
    if (!valuesResult.IsSuccess()) return registers;
    
    auto valuesIt = valuesResult.results.find("register-values");
    if (valuesIt == valuesResult.results.end() || !valuesIt->second.IsList()) return registers;
    
    for (const auto& regVal : valuesIt->second.listValue) {
        if (!regVal.IsTuple()) continue;
        
        Register reg;
        
        auto numIt = regVal.tupleValue.find("number");
        auto valueIt = regVal.tupleValue.find("value");
        
        if (numIt != regVal.tupleValue.end() && valueIt != regVal.tupleValue.end()) {
            int regNum = std::stoi(numIt->second.stringValue);
            if (regNum >= 0 && regNum < static_cast<int>(names.size()) && 
                !names[regNum].empty()) {
                reg.name = names[regNum];
                reg.value = valueIt->second.stringValue;
                registers.push_back(reg);
            }
        }
    }
    
    return registers;
}

std::vector<Register> UCGDBPlugin::GetRegisters(RegisterCategory category) {
    // GDB doesn't categorize registers; return all
    return GetRegisters();
}

bool UCGDBPlugin::SetRegister(const std::string& name, const std::string& value) {
    std::string cmd = "-gdb-set $" + name + "=" + value;
    GDBMIRecord result = SendCommand(cmd);
    return result.IsSuccess();
}

// ============================================================================
// MEMORY
// ============================================================================

MemoryRegion UCGDBPlugin::ReadMemory(uint64_t address, size_t size) {
    MemoryRegion region;
    region.address = address;
    region.size = size;
    
    GDBMIRecord result = SendCommand(
        GDBMICommandBuilder::DataReadMemory(address, size));
    
    if (!result.IsSuccess()) return region;
    
    auto memoryIt = result.results.find("memory");
    if (memoryIt == result.results.end() || !memoryIt->second.IsList()) return region;
    
    for (const auto& blockVal : memoryIt->second.listValue) {
        if (!blockVal.IsTuple()) continue;
        
        auto contentsIt = blockVal.tupleValue.find("contents");
        if (contentsIt == blockVal.tupleValue.end()) continue;
        
        std::string hexData = contentsIt->second.stringValue;
        for (size_t i = 0; i < hexData.length(); i += 2) {
            if (i + 1 < hexData.length()) {
                uint8_t byte = static_cast<uint8_t>(
                    std::stoul(hexData.substr(i, 2), nullptr, 16));
                region.data.push_back(byte);
            }
        }
    }
    
    return region;
}

bool UCGDBPlugin::WriteMemory(uint64_t address, const std::vector<uint8_t>& data) {
    std::ostringstream hexData;
    for (uint8_t byte : data) {
        hexData << std::hex << std::setw(2) << std::setfill('0') 
                << static_cast<int>(byte);
    }
    
    std::ostringstream cmd;
    cmd << "-data-write-memory-bytes 0x" << std::hex << address 
        << " " << hexData.str();
    
    GDBMIRecord result = SendCommand(cmd.str());
    return result.IsSuccess();
}

// ============================================================================
// DISASSEMBLY
// ============================================================================

std::vector<DisassemblyLine> UCGDBPlugin::GetDisassembly(uint64_t address, int count) {
    std::vector<DisassemblyLine> lines;
    
    uint64_t endAddr = address + (count * 16); // Estimate
    GDBMIRecord result = SendCommand(
        GDBMICommandBuilder::DataDisassemble(address, endAddr));
    
    if (!result.IsSuccess()) return lines;
    
    auto asmIt = result.results.find("asm_insns");
    if (asmIt == result.results.end() || !asmIt->second.IsList()) return lines;
    
    for (const auto& lineVal : asmIt->second.listValue) {
        if (!lineVal.IsTuple()) continue;
        
        DisassemblyLine line;
        
        auto addrIt = lineVal.tupleValue.find("address");
        if (addrIt != lineVal.tupleValue.end()) {
            line.address = std::stoull(addrIt->second.stringValue, nullptr, 16);
        }
        
        auto instIt = lineVal.tupleValue.find("inst");
        if (instIt != lineVal.tupleValue.end()) {
            line.instruction = instIt->second.stringValue;
        }
        
        auto funcIt = lineVal.tupleValue.find("func-name");
        if (funcIt != lineVal.tupleValue.end()) {
            line.functionName = funcIt->second.stringValue;
        }
        
        lines.push_back(line);
        
        if (static_cast<int>(lines.size()) >= count) break;
    }
    
    return lines;
}

std::vector<DisassemblyLine> UCGDBPlugin::GetFunctionDisassembly(const std::string& functionName) {
    std::vector<DisassemblyLine> lines;
    
    std::string cmd = "-data-disassemble -f \"" + 
                      UCGDBMIParser::Escape(functionName) + "\" 0";
    GDBMIRecord result = SendCommand(cmd);
    
    if (!result.IsSuccess()) return lines;
    
    auto asmIt = result.results.find("asm_insns");
    if (asmIt == result.results.end() || !asmIt->second.IsList()) return lines;
    
    for (const auto& lineVal : asmIt->second.listValue) {
        if (!lineVal.IsTuple()) continue;
        
        DisassemblyLine line;
        
        auto addrIt = lineVal.tupleValue.find("address");
        if (addrIt != lineVal.tupleValue.end()) {
            line.address = std::stoull(addrIt->second.stringValue, nullptr, 16);
        }
        
        auto instIt = lineVal.tupleValue.find("inst");
        if (instIt != lineVal.tupleValue.end()) {
            line.instruction = instIt->second.stringValue;
        }
        
        line.functionName = functionName;
        lines.push_back(line);
    }
    
    return lines;
}

// ============================================================================
// MODULES
// ============================================================================

std::vector<ModuleInfo> UCGDBPlugin::GetModules() {
    std::vector<ModuleInfo> modules;
    
    GDBMIRecord result = SendCommand("-file-list-shared-libraries");
    if (!result.IsSuccess()) return modules;
    
    auto libsIt = result.results.find("shared-libraries");
    if (libsIt == result.results.end() || !libsIt->second.IsList()) return modules;
    
    for (const auto& libVal : libsIt->second.listValue) {
        if (!libVal.IsTuple()) continue;
        
        ModuleInfo mod;
        
        auto nameIt = libVal.tupleValue.find("target-name");
        if (nameIt != libVal.tupleValue.end()) {
            mod.path = nameIt->second.stringValue;
            size_t pos = mod.path.find_last_of("/\\");
            mod.name = (pos != std::string::npos) ? mod.path.substr(pos + 1) : mod.path;
        }
        
        auto fromIt = libVal.tupleValue.find("from");
        if (fromIt != libVal.tupleValue.end()) {
            mod.baseAddress = std::stoull(fromIt->second.stringValue, nullptr, 16);
        }
        
        auto toIt = libVal.tupleValue.find("to");
        if (toIt != libVal.tupleValue.end()) {
            uint64_t endAddr = std::stoull(toIt->second.stringValue, nullptr, 16);
            mod.size = endAddr - mod.baseAddress;
        }
        
        auto symbolsIt = libVal.tupleValue.find("symbols-loaded");
        if (symbolsIt != libVal.tupleValue.end()) {
            mod.hasDebugInfo = (symbolsIt->second.stringValue == "1");
        }
        
        modules.push_back(mod);
    }
    
    return modules;
}

// ============================================================================
// COMPLETIONS
// ============================================================================

std::vector<DebugCompletionItem> UCGDBPlugin::GetCompletions(const std::string& text, int column) {
    std::vector<DebugCompletionItem> completions;
    
    std::string cmd = "-complete \"" + UCGDBMIParser::Escape(text) + "\"";
    GDBMIRecord result = SendCommand(cmd);
    
    if (!result.IsSuccess()) return completions;
    
    auto matchesIt = result.results.find("matches");
    if (matchesIt == result.results.end() || !matchesIt->second.IsList()) return completions;
    
    for (const auto& matchVal : matchesIt->second.listValue) {
        if (matchVal.IsString()) {
            DebugCompletionItem item;
            item.label = matchVal.stringValue;
            item.insertText = matchVal.stringValue;
            completions.push_back(item);
        }
    }
    
    return completions;
}

} // namespace IDE
} // namespace UltraCanvas
