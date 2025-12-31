// Apps/IDE/Debug/Breakpoints/UCBreakpointManager.cpp
// Breakpoint Manager Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCBreakpointManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace UltraCanvas {
namespace IDE {

UCBreakpointManager::UCBreakpointManager() : nextId_(1) {}

UCBreakpointManager::~UCBreakpointManager() = default;

int UCBreakpointManager::GenerateId() {
    return nextId_++;
}

void UCBreakpointManager::NotifyAdd(const UCBreakpoint& bp) {
    if (onBreakpointAdd) onBreakpointAdd(bp);
}

void UCBreakpointManager::NotifyRemove(int id) {
    if (onBreakpointRemove) onBreakpointRemove(id);
}

void UCBreakpointManager::NotifyChange(const UCBreakpoint& bp) {
    if (onBreakpointChange) onBreakpointChange(bp);
}

// ============================================================================
// BREAKPOINT CREATION
// ============================================================================

int UCBreakpointManager::AddBreakpoint(const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already exists
    for (const auto& pair : breakpoints_) {
        if (pair.second.MatchesLocation(file, line)) {
            return pair.first;
        }
    }
    
    int id = GenerateId();
    UCBreakpoint bp = UCBreakpoint::CreateLine(file, line);
    bp.SetId(id);
    
    breakpoints_[id] = bp;
    fileBreakpoints_[file].insert(id);
    
    NotifyAdd(bp);
    return id;
}

int UCBreakpointManager::AddFunctionBreakpoint(const std::string& functionName) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already exists
    for (const auto& pair : breakpoints_) {
        if (pair.second.MatchesFunction(functionName)) {
            return pair.first;
        }
    }
    
    int id = GenerateId();
    UCBreakpoint bp = UCBreakpoint::CreateFunction(functionName);
    bp.SetId(id);
    
    breakpoints_[id] = bp;
    
    NotifyAdd(bp);
    return id;
}

int UCBreakpointManager::AddConditionalBreakpoint(const std::string& file, int line,
                                                   const std::string& condition) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int id = GenerateId();
    UCBreakpoint bp = UCBreakpoint::CreateConditional(file, line, condition);
    bp.SetId(id);
    
    breakpoints_[id] = bp;
    fileBreakpoints_[file].insert(id);
    
    NotifyAdd(bp);
    return id;
}

int UCBreakpointManager::AddLogpoint(const std::string& file, int line,
                                      const std::string& logMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int id = GenerateId();
    UCBreakpoint bp = UCBreakpoint::CreateLogpoint(file, line, logMessage);
    bp.SetId(id);
    
    breakpoints_[id] = bp;
    fileBreakpoints_[file].insert(id);
    
    NotifyAdd(bp);
    return id;
}

int UCBreakpointManager::AddAddressBreakpoint(uint64_t address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already exists
    for (const auto& pair : breakpoints_) {
        if (pair.second.MatchesAddress(address)) {
            return pair.first;
        }
    }
    
    int id = GenerateId();
    UCBreakpoint bp = UCBreakpoint::CreateAddress(address);
    bp.SetId(id);
    
    breakpoints_[id] = bp;
    
    NotifyAdd(bp);
    return id;
}

// ============================================================================
// BREAKPOINT REMOVAL
// ============================================================================

bool UCBreakpointManager::RemoveBreakpoint(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = breakpoints_.find(id);
    if (it == breakpoints_.end()) return false;
    
    // Remove from file map
    const std::string& file = it->second.GetFile();
    if (!file.empty()) {
        fileBreakpoints_[file].erase(id);
        if (fileBreakpoints_[file].empty()) {
            fileBreakpoints_.erase(file);
        }
    }
    
    // Remove from debugger ID map
    int debuggerId = it->second.GetDebuggerId();
    if (debuggerId >= 0) {
        debuggerIdMap_.erase(debuggerId);
    }
    
    breakpoints_.erase(it);
    NotifyRemove(id);
    return true;
}

bool UCBreakpointManager::RemoveBreakpoint(const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto it = breakpoints_.begin(); it != breakpoints_.end(); ++it) {
        if (it->second.MatchesLocation(file, line)) {
            int id = it->first;
            
            // Remove from file map
            fileBreakpoints_[file].erase(id);
            if (fileBreakpoints_[file].empty()) {
                fileBreakpoints_.erase(file);
            }
            
            breakpoints_.erase(it);
            NotifyRemove(id);
            return true;
        }
    }
    return false;
}

void UCBreakpointManager::RemoveAllBreakpoints() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<int> ids;
    for (const auto& pair : breakpoints_) {
        ids.push_back(pair.first);
    }
    
    breakpoints_.clear();
    fileBreakpoints_.clear();
    debuggerIdMap_.clear();
    
    for (int id : ids) {
        NotifyRemove(id);
    }
}

void UCBreakpointManager::RemoveBreakpointsInFile(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = fileBreakpoints_.find(file);
    if (it == fileBreakpoints_.end()) return;
    
    std::vector<int> ids(it->second.begin(), it->second.end());
    
    for (int id : ids) {
        breakpoints_.erase(id);
        NotifyRemove(id);
    }
    
    fileBreakpoints_.erase(it);
}

int UCBreakpointManager::ToggleBreakpoint(const std::string& file, int line) {
    // Don't lock here - called methods will lock
    if (HasBreakpointAt(file, line)) {
        RemoveBreakpoint(file, line);
        return 0;  // Removed
    } else {
        return AddBreakpoint(file, line);  // Added
    }
}

// ============================================================================
// BREAKPOINT ACCESS
// ============================================================================

UCBreakpoint* UCBreakpointManager::GetBreakpoint(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    return (it != breakpoints_.end()) ? &it->second : nullptr;
}

const UCBreakpoint* UCBreakpointManager::GetBreakpoint(int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    return (it != breakpoints_.end()) ? &it->second : nullptr;
}

UCBreakpoint* UCBreakpointManager::GetBreakpointAt(const std::string& file, int line) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : breakpoints_) {
        if (pair.second.MatchesLocation(file, line)) {
            return &pair.second;
        }
    }
    return nullptr;
}

const UCBreakpoint* UCBreakpointManager::GetBreakpointAt(const std::string& file, int line) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : breakpoints_) {
        if (pair.second.MatchesLocation(file, line)) {
            return &pair.second;
        }
    }
    return nullptr;
}

std::vector<UCBreakpoint> UCBreakpointManager::GetAllBreakpoints() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UCBreakpoint> result;
    result.reserve(breakpoints_.size());
    for (const auto& pair : breakpoints_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<Breakpoint> UCBreakpointManager::GetAllBreakpointData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Breakpoint> result;
    result.reserve(breakpoints_.size());
    for (const auto& pair : breakpoints_) {
        result.push_back(pair.second.GetData());
    }
    return result;
}

std::vector<UCBreakpoint> UCBreakpointManager::GetBreakpointsInFile(const std::string& file) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UCBreakpoint> result;
    
    auto it = fileBreakpoints_.find(file);
    if (it != fileBreakpoints_.end()) {
        for (int id : it->second) {
            auto bpIt = breakpoints_.find(id);
            if (bpIt != breakpoints_.end()) {
                result.push_back(bpIt->second);
            }
        }
    }
    return result;
}

std::set<int> UCBreakpointManager::GetBreakpointLines(const std::string& file) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<int> lines;
    
    auto it = fileBreakpoints_.find(file);
    if (it != fileBreakpoints_.end()) {
        for (int id : it->second) {
            auto bpIt = breakpoints_.find(id);
            if (bpIt != breakpoints_.end()) {
                lines.insert(bpIt->second.GetLine());
            }
        }
    }
    return lines;
}

bool UCBreakpointManager::HasBreakpointAt(const std::string& file, int line) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : breakpoints_) {
        if (pair.second.MatchesLocation(file, line)) {
            return true;
        }
    }
    return false;
}

size_t UCBreakpointManager::GetBreakpointCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return breakpoints_.size();
}

// ============================================================================
// BREAKPOINT MODIFICATION
// ============================================================================

bool UCBreakpointManager::SetBreakpointEnabled(int id, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    if (it == breakpoints_.end()) return false;
    
    it->second.SetEnabled(enabled);
    NotifyChange(it->second);
    return true;
}

bool UCBreakpointManager::SetBreakpointCondition(int id, const std::string& condition) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    if (it == breakpoints_.end()) return false;
    
    it->second.SetCondition(condition);
    NotifyChange(it->second);
    return true;
}

bool UCBreakpointManager::SetBreakpointIgnoreCount(int id, int count) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    if (it == breakpoints_.end()) return false;
    
    it->second.SetIgnoreCount(count);
    NotifyChange(it->second);
    return true;
}

bool UCBreakpointManager::SetBreakpointLogMessage(int id, const std::string& logMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    if (it == breakpoints_.end()) return false;
    
    it->second.SetLogMessage(logMessage);
    NotifyChange(it->second);
    return true;
}

void UCBreakpointManager::UpdateBreakpoint(const Breakpoint& bp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find by debugger ID first
    auto* existing = FindByDebuggerId(bp.debuggerId);
    if (existing) {
        existing->SetState(bp.state);
        existing->SetVerified(bp.verified);
        if (!bp.errorMessage.empty()) {
            existing->SetErrorMessage(bp.errorMessage);
        }
        NotifyChange(*existing);
        return;
    }
    
    // Find by local ID
    auto it = breakpoints_.find(bp.id);
    if (it != breakpoints_.end()) {
        it->second.SetDebuggerId(bp.debuggerId);
        it->second.SetState(bp.state);
        it->second.SetVerified(bp.verified);
        if (bp.debuggerId >= 0) {
            debuggerIdMap_[bp.debuggerId] = bp.id;
        }
        NotifyChange(it->second);
    }
}

void UCBreakpointManager::RecordBreakpointHit(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = breakpoints_.find(id);
    if (it != breakpoints_.end()) {
        it->second.IncrementHitCount();
        if (onBreakpointHit) {
            onBreakpointHit(it->second);
        }
    }
}

// ============================================================================
// DEBUGGER SYNCHRONIZATION
// ============================================================================

void UCBreakpointManager::SyncToDebugger(IUCDebuggerPlugin* debugger) {
    if (!debugger) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : breakpoints_) {
        UCBreakpoint& bp = pair.second;
        if (!bp.IsEnabled()) continue;
        
        Breakpoint result;
        switch (bp.GetType()) {
            case BreakpointType::LineBreakpoint:
            case BreakpointType::ConditionalBreakpoint:
                if (bp.GetCondition().empty()) {
                    result = debugger->AddBreakpoint(bp.GetFile(), bp.GetLine());
                } else {
                    result = debugger->AddConditionalBreakpoint(
                        bp.GetFile(), bp.GetLine(), bp.GetCondition());
                }
                break;
                
            case BreakpointType::FunctionBreakpoint:
                result = debugger->AddFunctionBreakpoint(bp.GetFunctionName());
                break;
                
            case BreakpointType::AddressBreakpoint:
                result = debugger->AddAddressBreakpoint(bp.GetAddress());
                break;
                
            case BreakpointType::Logpoint:
                result = debugger->AddLogpoint(
                    bp.GetFile(), bp.GetLine(), bp.GetLogMessage());
                break;
                
            default:
                continue;
        }
        
        if (result.debuggerId >= 0) {
            bp.SetDebuggerId(result.debuggerId);
            bp.SetVerified(result.verified);
            bp.SetState(result.state);
            debuggerIdMap_[result.debuggerId] = bp.GetId();
        }
    }
}

void UCBreakpointManager::OnDebuggerBreakpointUpdate(int debuggerId, bool verified, int actualLine) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto* bp = FindByDebuggerId(debuggerId);
    if (bp) {
        bp->SetVerified(verified);
        if (verified && actualLine > 0 && actualLine != bp->GetLine()) {
            SourceLocation loc = bp->GetLocation();
            loc.line = actualLine;
            bp->SetLocation(loc);
        }
        NotifyChange(*bp);
    }
}

void UCBreakpointManager::ClearDebuggerIds() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    debuggerIdMap_.clear();
    for (auto& pair : breakpoints_) {
        pair.second.SetDebuggerId(-1);
        pair.second.SetState(BreakpointState::Pending);
    }
}

UCBreakpoint* UCBreakpointManager::FindByDebuggerId(int debuggerId) {
    auto it = debuggerIdMap_.find(debuggerId);
    if (it != debuggerIdMap_.end()) {
        auto bpIt = breakpoints_.find(it->second);
        if (bpIt != breakpoints_.end()) {
            return &bpIt->second;
        }
    }
    return nullptr;
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool UCBreakpointManager::SaveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    
    file << ToJson();
    return true;
}

bool UCBreakpointManager::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return FromJson(buffer.str());
}

std::string UCBreakpointManager::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream json;
    json << "{\n  \"breakpoints\": [\n";
    
    bool first = true;
    for (const auto& pair : breakpoints_) {
        const UCBreakpoint& bp = pair.second;
        
        if (!first) json << ",\n";
        first = false;
        
        json << "    {\n";
        json << "      \"id\": " << bp.GetId() << ",\n";
        json << "      \"type\": \"" << BreakpointTypeToString(bp.GetType()) << "\",\n";
        json << "      \"file\": \"" << bp.GetFile() << "\",\n";
        json << "      \"line\": " << bp.GetLine() << ",\n";
        json << "      \"function\": \"" << bp.GetFunctionName() << "\",\n";
        json << "      \"condition\": \"" << bp.GetCondition() << "\",\n";
        json << "      \"logMessage\": \"" << bp.GetLogMessage() << "\",\n";
        json << "      \"enabled\": " << (bp.IsEnabled() ? "true" : "false") << ",\n";
        json << "      \"ignoreCount\": " << bp.GetIgnoreCount() << "\n";
        json << "    }";
    }
    
    json << "\n  ]\n}";
    return json.str();
}

bool UCBreakpointManager::FromJson(const std::string& json) {
    // Simple JSON parsing - in production, use a proper JSON library
    // This is a placeholder that shows the expected format
    // TODO: Implement proper JSON parsing using jsoncpp
    return false;
}

} // namespace IDE
} // namespace UltraCanvas
