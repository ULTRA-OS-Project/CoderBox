// Apps/IDE/Debug/GDB/UCGDBPluginCommands.cpp
// GDB Plugin Command Handling for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGDBPlugin.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <future>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

GDBMIRecord UCGDBPlugin::SendCommand(const std::string& command) {
    if (!process_ || !process_->IsRunning()) {
        GDBMIRecord error;
        error.type = GDBMIRecordType::Result;
        error.resultClass = GDBMIResultClass::Error;
        error.results["msg"] = GDBMIValue("Process not running");
        return error;
    }
    
    int token = GetNextToken();
    std::string fullCommand = std::to_string(token) + command;
    
    // Create promise for response
    std::promise<GDBMIRecord> promise;
    std::future<GDBMIRecord> future = promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(commandMutex_);
        pendingCommands_[token] = std::move(promise);
    }
    
    // Send command
    process_->SendLine(fullCommand);
    
    // Wait for response with timeout
    auto status = future.wait_for(std::chrono::milliseconds(config_.commandTimeout));
    
    if (status == std::future_status::timeout) {
        std::lock_guard<std::mutex> lock(commandMutex_);
        pendingCommands_.erase(token);
        
        GDBMIRecord timeout;
        timeout.type = GDBMIRecordType::Result;
        timeout.resultClass = GDBMIResultClass::Error;
        timeout.results["msg"] = GDBMIValue("Command timeout");
        return timeout;
    }
    
    return future.get();
}

GDBMIRecord UCGDBPlugin::SendCommandAsync(const std::string& command) {
    // For now, same as sync
    return SendCommand(command);
}

bool UCGDBPlugin::SendCommandNoWait(const std::string& command) {
    if (!process_ || !process_->IsRunning()) {
        return false;
    }
    
    int token = GetNextToken();
    std::string fullCommand = std::to_string(token) + command;
    return process_->SendLine(fullCommand);
}

// ============================================================================
// OUTPUT PROCESSING
// ============================================================================

void UCGDBPlugin::ProcessOutput() {
    while (!stopOutputThread_ && process_ && process_->IsRunning()) {
        std::string line = process_->ReadLine(100);
        if (line.empty()) continue;
        
        GDBMIRecord record = parser_->ParseLine(line);
        
        if (record.type == GDBMIRecordType::Result && record.token >= 0) {
            // This is a response to a command
            std::lock_guard<std::mutex> lock(commandMutex_);
            auto it = pendingCommands_.find(record.token);
            if (it != pendingCommands_.end()) {
                it->second.set_value(record);
                pendingCommands_.erase(it);
            }
        } else if (record.IsAsync()) {
            HandleAsyncRecord(record);
        } else if (record.IsStream()) {
            if (onOutput) {
                onOutput(record.streamContent);
            }
        }
    }
}

void UCGDBPlugin::HandleAsyncRecord(const GDBMIRecord& record) {
    switch (record.asyncClass) {
        case GDBMIAsyncClass::Stopped:
            HandleStopEvent(record);
            break;
            
        case GDBMIAsyncClass::Running:
            HandleRunningEvent(record);
            break;
            
        case GDBMIAsyncClass::BreakpointCreated:
        case GDBMIAsyncClass::BreakpointModified:
        case GDBMIAsyncClass::BreakpointDeleted:
            HandleBreakpointEvent(record);
            break;
            
        case GDBMIAsyncClass::ThreadCreated:
        case GDBMIAsyncClass::ThreadExited:
            HandleThreadEvent(record);
            break;
            
        case GDBMIAsyncClass::LibraryLoaded:
        case GDBMIAsyncClass::LibraryUnloaded:
            HandleLibraryEvent(record);
            break;
            
        default:
            break;
    }
}

void UCGDBPlugin::HandleStopEvent(const GDBMIRecord& record) {
    SetState(DebugSessionState::Paused);
    
    StopReason reason = StopReason::UnknownStop;
    SourceLocation location;
    
    // Parse reason
    std::string reasonStr;
    auto reasonIt = record.results.find("reason");
    if (reasonIt != record.results.end() && reasonIt->second.IsString()) {
        reasonStr = reasonIt->second.stringValue;
    }
    
    if (reasonStr == "breakpoint-hit") {
        reason = StopReason::Breakpoint;
        
        // Get breakpoint ID
        auto bkptnoIt = record.results.find("bkptno");
        if (bkptnoIt != record.results.end()) {
            int bpNum = std::stoi(bkptnoIt->second.stringValue);
            for (auto& pair : breakpoints_) {
                if (pair.second.debuggerId == bpNum) {
                    pair.second.hitCount++;
                    if (onBreakpointHit) {
                        onBreakpointHit(pair.second);
                    }
                    break;
                }
            }
        }
    } else if (reasonStr == "watchpoint-trigger") {
        reason = StopReason::Watchpoint;
    } else if (reasonStr == "end-stepping-range" || reasonStr == "function-finished") {
        reason = StopReason::Step;
    } else if (reasonStr == "signal-received") {
        reason = StopReason::Signal;
    } else if (reasonStr == "exited-normally") {
        reason = StopReason::ExitNormal;
        SetState(DebugSessionState::Terminated);
    } else if (reasonStr == "exited" || reasonStr == "exited-signalled") {
        reason = StopReason::ExitError;
        SetState(DebugSessionState::Terminated);
    }
    
    // Parse frame for location
    auto frameIt = record.results.find("frame");
    if (frameIt != record.results.end() && frameIt->second.IsTuple()) {
        const auto& frame = frameIt->second.tupleValue;
        
        auto fileIt = frame.find("fullname");
        if (fileIt == frame.end()) {
            fileIt = frame.find("file");
        }
        if (fileIt != frame.end() && fileIt->second.IsString()) {
            location.filePath = fileIt->second.stringValue;
        }
        
        auto lineIt = frame.find("line");
        if (lineIt != frame.end() && lineIt->second.IsString()) {
            location.line = std::stoi(lineIt->second.stringValue);
        }
    }
    
    // Update thread ID
    auto threadIt = record.results.find("thread-id");
    if (threadIt != record.results.end() && threadIt->second.IsString()) {
        currentThreadId_ = std::stoi(threadIt->second.stringValue);
    }
    
    if (onTargetStop) {
        onTargetStop(reason, location);
    }
    
    // Send debug event
    if (onDebugEvent) {
        DebugEvent event(DebugEventType::TargetStop);
        event.stopReason = reason;
        event.location = location;
        event.threadId = currentThreadId_;
        onDebugEvent(event);
    }
}

void UCGDBPlugin::HandleRunningEvent(const GDBMIRecord& record) {
    SetState(DebugSessionState::Running);
    
    if (onTargetContinue) {
        onTargetContinue();
    }
}

void UCGDBPlugin::HandleBreakpointEvent(const GDBMIRecord& record) {
    auto bkptIt = record.results.find("bkpt");
    if (bkptIt == record.results.end() || !bkptIt->second.IsTuple()) {
        return;
    }
    
    const auto& bkpt = bkptIt->second.tupleValue;
    
    auto numIt = bkpt.find("number");
    if (numIt == bkpt.end()) return;
    
    int bpNum = std::stoi(numIt->second.stringValue);
    
    // Find our breakpoint
    for (auto& pair : breakpoints_) {
        if (pair.second.debuggerId == bpNum) {
            // Update state
            auto enabledIt = bkpt.find("enabled");
            if (enabledIt != bkpt.end()) {
                bool enabled = enabledIt->second.stringValue == "y";
                pair.second.enabled = enabled;
            }
            
            if (onBreakpointChange) {
                onBreakpointChange(pair.second);
            }
            break;
        }
    }
}

void UCGDBPlugin::HandleThreadEvent(const GDBMIRecord& record) {
    auto idIt = record.results.find("id");
    if (idIt == record.results.end()) return;
    
    int threadId = std::stoi(idIt->second.stringValue);
    
    if (record.asyncClass == GDBMIAsyncClass::ThreadCreated) {
        if (onThreadCreate) {
            ThreadInfo info;
            info.id = threadId;
            onThreadCreate(info);
        }
    } else if (record.asyncClass == GDBMIAsyncClass::ThreadExited) {
        if (onThreadExit) {
            onThreadExit(threadId);
        }
    }
}

void UCGDBPlugin::HandleLibraryEvent(const GDBMIRecord& record) {
    auto idIt = record.results.find("id");
    auto targetIt = record.results.find("target-name");
    
    std::string libPath;
    if (targetIt != record.results.end()) {
        libPath = targetIt->second.stringValue;
    }
    
    if (record.asyncClass == GDBMIAsyncClass::LibraryLoaded) {
        if (onModuleLoad) {
            ModuleInfo info;
            info.path = libPath;
            size_t pos = libPath.find_last_of("/\\");
            info.name = (pos != std::string::npos) ? libPath.substr(pos + 1) : libPath;
            onModuleLoad(info);
        }
    } else if (record.asyncClass == GDBMIAsyncClass::LibraryUnloaded) {
        if (onModuleUnload) {
            onModuleUnload(libPath);
        }
    }
}

// ============================================================================
// BREAKPOINTS
// ============================================================================

Breakpoint UCGDBPlugin::AddBreakpoint(const std::string& file, int line) {
    std::string location = file + ":" + std::to_string(line);
    std::string cmd = GDBMICommandBuilder::BreakInsert(location);
    
    GDBMIRecord result = SendCommand(cmd);
    return ParseBreakpointResult(result);
}

Breakpoint UCGDBPlugin::AddFunctionBreakpoint(const std::string& functionName) {
    std::string cmd = GDBMICommandBuilder::BreakInsert(functionName);
    GDBMIRecord result = SendCommand(cmd);
    return ParseBreakpointResult(result);
}

Breakpoint UCGDBPlugin::AddConditionalBreakpoint(const std::string& file, int line,
                                                  const std::string& condition) {
    std::string location = file + ":" + std::to_string(line);
    
    // Insert breakpoint
    GDBMIRecord result = SendCommand(GDBMICommandBuilder::BreakInsert(location));
    Breakpoint bp = ParseBreakpointResult(result);
    
    if (bp.debuggerId >= 0 && !condition.empty()) {
        // Set condition
        SendCommand(GDBMICommandBuilder::BreakCondition(bp.debuggerId, condition));
        bp.condition = condition;
    }
    
    return bp;
}

Breakpoint UCGDBPlugin::AddAddressBreakpoint(uint64_t address) {
    std::ostringstream addrStr;
    addrStr << "*0x" << std::hex << address;
    
    GDBMIRecord result = SendCommand(GDBMICommandBuilder::BreakInsert(addrStr.str()));
    return ParseBreakpointResult(result);
}

Breakpoint UCGDBPlugin::AddLogpoint(const std::string& file, int line,
                                    const std::string& logMessage) {
    // GDB doesn't have native logpoints, so we use dprintf
    std::string location = file + ":" + std::to_string(line);
    std::string cmd = "-dprintf-insert \"" + UCGDBMIParser::Escape(location) + 
                      "\" \"" + UCGDBMIParser::Escape(logMessage) + "\\n\"";
    
    GDBMIRecord result = SendCommand(cmd);
    Breakpoint bp = ParseBreakpointResult(result);
    bp.type = BreakpointType::Logpoint;
    bp.logMessage = logMessage;
    return bp;
}

bool UCGDBPlugin::RemoveBreakpoint(int breakpointId) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    GDBMIRecord result = SendCommand(
        GDBMICommandBuilder::BreakDelete(it->second.debuggerId));
    
    if (result.IsSuccess()) {
        breakpoints_.erase(it);
        return true;
    }
    return false;
}

bool UCGDBPlugin::SetBreakpointEnabled(int breakpointId, bool enabled) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    std::string cmd = enabled ? 
        GDBMICommandBuilder::BreakEnable(it->second.debuggerId) :
        GDBMICommandBuilder::BreakDisable(it->second.debuggerId);
    
    GDBMIRecord result = SendCommand(cmd);
    if (result.IsSuccess()) {
        it->second.enabled = enabled;
        return true;
    }
    return false;
}

bool UCGDBPlugin::SetBreakpointCondition(int breakpointId, const std::string& condition) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    GDBMIRecord result = SendCommand(
        GDBMICommandBuilder::BreakCondition(it->second.debuggerId, condition));
    
    if (result.IsSuccess()) {
        it->second.condition = condition;
        return true;
    }
    return false;
}

bool UCGDBPlugin::SetBreakpointIgnoreCount(int breakpointId, int count) {
    auto it = breakpoints_.find(breakpointId);
    if (it == breakpoints_.end()) return false;
    
    std::string cmd = "-break-after " + std::to_string(it->second.debuggerId) + 
                      " " + std::to_string(count);
    
    GDBMIRecord result = SendCommand(cmd);
    if (result.IsSuccess()) {
        it->second.ignoreCount = count;
        return true;
    }
    return false;
}

std::vector<Breakpoint> UCGDBPlugin::GetBreakpoints() const {
    std::vector<Breakpoint> result;
    result.reserve(breakpoints_.size());
    for (const auto& pair : breakpoints_) {
        result.push_back(pair.second);
    }
    return result;
}

Breakpoint UCGDBPlugin::GetBreakpoint(int breakpointId) const {
    auto it = breakpoints_.find(breakpointId);
    if (it != breakpoints_.end()) {
        return it->second;
    }
    return Breakpoint();
}

Breakpoint UCGDBPlugin::ParseBreakpointResult(const GDBMIRecord& record) {
    Breakpoint bp;
    bp.id = nextBreakpointId_++;
    
    if (record.IsError()) {
        bp.state = BreakpointState::Invalid;
        bp.errorMessage = record.GetError();
        return bp;
    }
    
    auto bkptIt = record.results.find("bkpt");
    if (bkptIt == record.results.end() || !bkptIt->second.IsTuple()) {
        bp.state = BreakpointState::Invalid;
        return bp;
    }
    
    const auto& bkpt = bkptIt->second.tupleValue;
    
    // Get debugger's breakpoint number
    auto numIt = bkpt.find("number");
    if (numIt != bkpt.end()) {
        bp.debuggerId = std::stoi(numIt->second.stringValue);
    }
    
    // Get location
    auto fileIt = bkpt.find("fullname");
    if (fileIt == bkpt.end()) {
        fileIt = bkpt.find("file");
    }
    if (fileIt != bkpt.end()) {
        bp.location.filePath = fileIt->second.stringValue;
    }
    
    auto lineIt = bkpt.find("line");
    if (lineIt != bkpt.end()) {
        bp.location.line = std::stoi(lineIt->second.stringValue);
    }
    
    auto funcIt = bkpt.find("func");
    if (funcIt != bkpt.end()) {
        bp.functionName = funcIt->second.stringValue;
    }
    
    auto addrIt = bkpt.find("addr");
    if (addrIt != bkpt.end() && addrIt->second.stringValue != "<PENDING>") {
        bp.address = std::stoull(addrIt->second.stringValue, nullptr, 16);
    }
    
    // Check if pending
    auto pendingIt = bkpt.find("pending");
    if (pendingIt != bkpt.end()) {
        bp.state = BreakpointState::Pending;
        bp.pending = true;
    } else {
        bp.state = BreakpointState::Active;
        bp.verified = true;
        bp.pending = false;
    }
    
    // Store breakpoint
    breakpoints_[bp.id] = bp;
    
    return bp;
}

// ============================================================================
// WATCHPOINTS
// ============================================================================

Watchpoint UCGDBPlugin::AddWatchpoint(const std::string& expression,
                                       WatchpointAccess access) {
    Watchpoint wp;
    wp.id = nextWatchpointId_++;
    wp.expression = expression;
    wp.access = access;
    
    std::string accessFlag;
    switch (access) {
        case WatchpointAccess::Read:
            accessFlag = "-r";
            break;
        case WatchpointAccess::ReadWrite:
            accessFlag = "-a";
            break;
        case WatchpointAccess::Write:
        default:
            accessFlag = "";
            break;
    }
    
    std::string cmd = "-break-watch " + accessFlag + " \"" + 
                      UCGDBMIParser::Escape(expression) + "\"";
    
    GDBMIRecord result = SendCommand(cmd);
    
    if (result.IsSuccess()) {
        auto wptIt = result.results.find("wpt");
        if (wptIt != result.results.end() && wptIt->second.IsTuple()) {
            auto numIt = wptIt->second.tupleValue.find("number");
            if (numIt != wptIt->second.tupleValue.end()) {
                wp.debuggerId = std::stoi(numIt->second.stringValue);
            }
        }
        watchpoints_[wp.id] = wp;
    }
    
    return wp;
}

bool UCGDBPlugin::RemoveWatchpoint(int watchpointId) {
    auto it = watchpoints_.find(watchpointId);
    if (it == watchpoints_.end()) return false;
    
    GDBMIRecord result = SendCommand(
        GDBMICommandBuilder::BreakDelete(it->second.debuggerId));
    
    if (result.IsSuccess()) {
        watchpoints_.erase(it);
        return true;
    }
    return false;
}

std::vector<Watchpoint> UCGDBPlugin::GetWatchpoints() const {
    std::vector<Watchpoint> result;
    result.reserve(watchpoints_.size());
    for (const auto& pair : watchpoints_) {
        result.push_back(pair.second);
    }
    return result;
}

} // namespace IDE
} // namespace UltraCanvas
