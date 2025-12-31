// Apps/IDE/Debug/Breakpoints/UCWatchManager.cpp
// Watch Expression Manager Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCWatchManager.h"
#include <fstream>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

UCWatchManager::UCWatchManager() : nextId_(1) {}
UCWatchManager::~UCWatchManager() = default;

int UCWatchManager::GenerateId() {
    return nextId_++;
}

void UCWatchManager::NotifyAdd(const WatchEntry& entry) {
    if (onWatchAdd) onWatchAdd(entry);
}

void UCWatchManager::NotifyRemove(int id) {
    if (onWatchRemove) onWatchRemove(id);
}

void UCWatchManager::NotifyUpdate(const WatchEntry& entry) {
    if (onWatchUpdate) onWatchUpdate(entry);
}

// ============================================================================
// WATCH OPERATIONS
// ============================================================================

int UCWatchManager::AddWatch(const std::string& expression) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already exists
    for (const auto& pair : watches_) {
        if (pair.second.expression == expression) {
            return pair.first;
        }
    }
    
    int id = GenerateId();
    WatchEntry entry;
    entry.id = id;
    entry.expression = expression;
    
    watches_[id] = entry;
    NotifyAdd(entry);
    
    return id;
}

bool UCWatchManager::RemoveWatch(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watches_.find(id);
    if (it == watches_.end()) return false;
    
    watches_.erase(it);
    NotifyRemove(id);
    return true;
}

bool UCWatchManager::RemoveWatch(const std::string& expression) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto it = watches_.begin(); it != watches_.end(); ++it) {
        if (it->second.expression == expression) {
            int id = it->first;
            watches_.erase(it);
            NotifyRemove(id);
            return true;
        }
    }
    return false;
}

void UCWatchManager::RemoveAllWatches() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<int> ids;
    for (const auto& pair : watches_) {
        ids.push_back(pair.first);
    }
    
    watches_.clear();
    
    for (int id : ids) {
        NotifyRemove(id);
    }
}

bool UCWatchManager::UpdateWatchExpression(int id, const std::string& expression) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watches_.find(id);
    if (it == watches_.end()) return false;
    
    it->second.expression = expression;
    it->second.value = Variable();  // Clear value
    it->second.hasError = false;
    it->second.errorMessage.clear();
    
    NotifyUpdate(it->second);
    return true;
}

// ============================================================================
// WATCH ACCESS
// ============================================================================

WatchEntry* UCWatchManager::GetWatch(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = watches_.find(id);
    return (it != watches_.end()) ? &it->second : nullptr;
}

const WatchEntry* UCWatchManager::GetWatch(int id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = watches_.find(id);
    return (it != watches_.end()) ? &it->second : nullptr;
}

std::vector<WatchEntry> UCWatchManager::GetAllWatches() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WatchEntry> result;
    result.reserve(watches_.size());
    for (const auto& pair : watches_) {
        result.push_back(pair.second);
    }
    return result;
}

size_t UCWatchManager::GetWatchCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return watches_.size();
}

bool UCWatchManager::HasWatch(const std::string& expression) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& pair : watches_) {
        if (pair.second.expression == expression) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// VALUE UPDATES
// ============================================================================

void UCWatchManager::RefreshAll(IUCDebuggerPlugin* debugger) {
    if (!debugger) return;
    
    std::vector<int> ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& pair : watches_) {
            ids.push_back(pair.first);
        }
    }
    
    for (int id : ids) {
        RefreshWatch(id, debugger);
    }
}

void UCWatchManager::RefreshWatch(int id, IUCDebuggerPlugin* debugger) {
    if (!debugger) return;
    
    std::string expression;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = watches_.find(id);
        if (it == watches_.end()) return;
        expression = it->second.expression;
    }
    
    EvaluationResult result = debugger->Evaluate(expression);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = watches_.find(id);
        if (it == watches_.end()) return;
        
        if (result.success) {
            it->second.value = result.variable;
            it->second.hasError = false;
            it->second.errorMessage.clear();
        } else {
            it->second.hasError = true;
            it->second.errorMessage = result.errorMessage;
        }
        
        NotifyUpdate(it->second);
    }
}

void UCWatchManager::ClearAllValues() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : watches_) {
        pair.second.value = Variable();
        pair.second.hasError = false;
        pair.second.errorMessage.clear();
        NotifyUpdate(pair.second);
    }
}

void UCWatchManager::UpdateWatchValue(int id, const Variable& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watches_.find(id);
    if (it == watches_.end()) return;
    
    it->second.value = value;
    it->second.hasError = false;
    it->second.errorMessage.clear();
    
    NotifyUpdate(it->second);
}

void UCWatchManager::SetWatchError(int id, const std::string& errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watches_.find(id);
    if (it == watches_.end()) return;
    
    it->second.hasError = true;
    it->second.errorMessage = errorMessage;
    
    NotifyUpdate(it->second);
}

// ============================================================================
// EXPANSION
// ============================================================================

bool UCWatchManager::ToggleExpanded(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watches_.find(id);
    if (it == watches_.end()) return false;
    
    it->second.isExpanded = !it->second.isExpanded;
    NotifyUpdate(it->second);
    return true;
}

bool UCWatchManager::SetExpanded(int id, bool expanded) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = watches_.find(id);
    if (it == watches_.end()) return false;
    
    it->second.isExpanded = expanded;
    NotifyUpdate(it->second);
    return true;
}

std::vector<Variable> UCWatchManager::GetExpandedChildren(int id, IUCDebuggerPlugin* debugger) {
    if (!debugger) return {};
    
    int variablesRef;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = watches_.find(id);
        if (it == watches_.end()) return {};
        variablesRef = it->second.value.variablesReference;
    }
    
    if (variablesRef <= 0) return {};
    
    return debugger->ExpandVariable(variablesRef);
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool UCWatchManager::SaveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    file << "{\n  \"watches\": [\n";
    
    bool first = true;
    for (const auto& pair : watches_) {
        if (!first) file << ",\n";
        first = false;
        
        file << "    { \"expression\": \"" << pair.second.expression << "\" }";
    }
    
    file << "\n  ]\n}\n";
    return true;
}

bool UCWatchManager::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    // Simple JSON parsing - in production use a proper JSON library
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // Find expressions and add them
    size_t pos = 0;
    while ((pos = content.find("\"expression\"", pos)) != std::string::npos) {
        size_t colonPos = content.find(':', pos);
        if (colonPos == std::string::npos) break;
        
        size_t quoteStart = content.find('"', colonPos + 1);
        if (quoteStart == std::string::npos) break;
        
        size_t quoteEnd = content.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) break;
        
        std::string expression = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        AddWatch(expression);
        
        pos = quoteEnd + 1;
    }
    
    return true;
}

} // namespace IDE
} // namespace UltraCanvas
