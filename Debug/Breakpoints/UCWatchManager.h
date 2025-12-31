// Apps/IDE/Debug/Breakpoints/UCWatchManager.h
// Watch Expression Manager for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Types/UCDebugStructs.h"
#include "../Core/IUCDebuggerPlugin.h"
#include <vector>
#include <map>
#include <mutex>
#include <functional>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Watch expression entry
 */
struct WatchEntry {
    int id = -1;
    std::string expression;
    Variable value;
    bool isExpanded = false;
    bool hasError = false;
    std::string errorMessage;
    
    bool IsValid() const { return id >= 0 && !expression.empty(); }
};

/**
 * @brief Manages watch expressions for the debug session
 */
class UCWatchManager {
public:
    UCWatchManager();
    ~UCWatchManager();
    
    // Watch operations
    int AddWatch(const std::string& expression);
    bool RemoveWatch(int id);
    bool RemoveWatch(const std::string& expression);
    void RemoveAllWatches();
    bool UpdateWatchExpression(int id, const std::string& expression);
    
    // Watch access
    WatchEntry* GetWatch(int id);
    const WatchEntry* GetWatch(int id) const;
    std::vector<WatchEntry> GetAllWatches() const;
    size_t GetWatchCount() const;
    bool HasWatch(const std::string& expression) const;
    
    // Value updates
    void RefreshAll(IUCDebuggerPlugin* debugger);
    void RefreshWatch(int id, IUCDebuggerPlugin* debugger);
    void ClearAllValues();
    void UpdateWatchValue(int id, const Variable& value);
    void SetWatchError(int id, const std::string& errorMessage);
    
    // Expansion
    bool ToggleExpanded(int id);
    bool SetExpanded(int id, bool expanded);
    std::vector<Variable> GetExpandedChildren(int id, IUCDebuggerPlugin* debugger);
    
    // Persistence
    bool SaveToFile(const std::string& filePath) const;
    bool LoadFromFile(const std::string& filePath);
    
    // Callbacks
    std::function<void(const WatchEntry&)> onWatchAdd;
    std::function<void(int)> onWatchRemove;
    std::function<void(const WatchEntry&)> onWatchUpdate;
    
private:
    int GenerateId();
    void NotifyAdd(const WatchEntry& entry);
    void NotifyRemove(int id);
    void NotifyUpdate(const WatchEntry& entry);
    
    std::map<int, WatchEntry> watches_;
    int nextId_ = 1;
    mutable std::mutex mutex_;
};

} // namespace IDE
} // namespace UltraCanvas
