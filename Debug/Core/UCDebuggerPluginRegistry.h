// Apps/IDE/Debug/Core/UCDebuggerPluginRegistry.h
// Debugger Plugin Registry for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "IUCDebuggerPlugin.h"
#include <map>
#include <memory>
#include <mutex>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// DEBUGGER PLUGIN REGISTRY
// ============================================================================

/**
 * @brief Singleton registry for debugger plugins
 * 
 * Manages registration and lookup of debugger plugins.
 * Similar to UCCompilerPluginRegistry for consistency.
 */
class UCDebuggerPluginRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the registry instance
     */
    static UCDebuggerPluginRegistry& Instance() {
        static UCDebuggerPluginRegistry instance;
        return instance;
    }
    
    // Prevent copying
    UCDebuggerPluginRegistry(const UCDebuggerPluginRegistry&) = delete;
    UCDebuggerPluginRegistry& operator=(const UCDebuggerPluginRegistry&) = delete;
    
    // =========================================================================
    // PLUGIN REGISTRATION
    // =========================================================================
    
    /**
     * @brief Register a debugger plugin
     * @param plugin Shared pointer to plugin instance
     */
    void RegisterPlugin(std::shared_ptr<IUCDebuggerPlugin> plugin) {
        if (!plugin) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        DebuggerType type = plugin->GetDebuggerType();
        plugins_[type] = plugin;
    }
    
    /**
     * @brief Unregister a debugger plugin by type
     * @param type Debugger type to unregister
     */
    void UnregisterPlugin(DebuggerType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        plugins_.erase(type);
    }
    
    /**
     * @brief Clear all registered plugins
     */
    void ClearPlugins() {
        std::lock_guard<std::mutex> lock(mutex_);
        plugins_.clear();
    }
    
    // =========================================================================
    // PLUGIN LOOKUP
    // =========================================================================
    
    /**
     * @brief Get a plugin by type
     * @param type Debugger type
     * @return Shared pointer to plugin, or nullptr if not found
     */
    std::shared_ptr<IUCDebuggerPlugin> GetPlugin(DebuggerType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = plugins_.find(type);
        return (it != plugins_.end()) ? it->second : nullptr;
    }
    
    /**
     * @brief Get all registered plugins
     * @return Vector of all plugins
     */
    std::vector<std::shared_ptr<IUCDebuggerPlugin>> GetAllPlugins() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<IUCDebuggerPlugin>> result;
        result.reserve(plugins_.size());
        for (const auto& pair : plugins_) {
            result.push_back(pair.second);
        }
        return result;
    }
    
    /**
     * @brief Get all available plugins (debugger is installed)
     * @return Vector of available plugins
     */
    std::vector<std::shared_ptr<IUCDebuggerPlugin>> GetAvailablePlugins() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<IUCDebuggerPlugin>> result;
        for (const auto& pair : plugins_) {
            if (pair.second && pair.second->IsAvailable()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    /**
     * @brief Check if a debugger type is registered
     * @param type Debugger type
     * @return true if registered
     */
    bool HasPlugin(DebuggerType type) {
        std::lock_guard<std::mutex> lock(mutex_);
        return plugins_.find(type) != plugins_.end();
    }
    
    /**
     * @brief Check if a debugger type is available
     * @param type Debugger type
     * @return true if registered and available
     */
    bool IsPluginAvailable(DebuggerType type) {
        auto plugin = GetPlugin(type);
        return plugin && plugin->IsAvailable();
    }
    
    // =========================================================================
    // AUTOMATIC SELECTION
    // =========================================================================
    
    /**
     * @brief Get the best debugger for the current platform
     * @return Best available debugger, or nullptr if none available
     */
    std::shared_ptr<IUCDebuggerPlugin> GetBestDebugger() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Platform-specific preference
#ifdef __APPLE__
        // macOS: Prefer LLDB
        auto lldb = GetPluginUnlocked(DebuggerType::LLDB);
        if (lldb && lldb->IsAvailable()) return lldb;
        
        auto gdb = GetPluginUnlocked(DebuggerType::GDB);
        if (gdb && gdb->IsAvailable()) return gdb;
#else
        // Linux/Windows: Prefer GDB
        auto gdb = GetPluginUnlocked(DebuggerType::GDB);
        if (gdb && gdb->IsAvailable()) return gdb;
        
        auto lldb = GetPluginUnlocked(DebuggerType::LLDB);
        if (lldb && lldb->IsAvailable()) return lldb;
#endif
        
        // Fall back to any available debugger
        for (const auto& pair : plugins_) {
            if (pair.second && pair.second->IsAvailable()) {
                return pair.second;
            }
        }
        
        return nullptr;
    }
    
    /**
     * @brief Get the best debugger for a specific executable
     * @param executablePath Path to executable
     * @return Best debugger for this executable
     */
    std::shared_ptr<IUCDebuggerPlugin> GetDebuggerForExecutable(
            const std::string& executablePath) {
        // For now, just return the best available debugger
        // Future: Could examine executable format (ELF, Mach-O, PE) to decide
        return GetBestDebugger();
    }
    
    // =========================================================================
    // INFORMATION
    // =========================================================================
    
    /**
     * @brief Get the number of registered plugins
     * @return Number of plugins
     */
    size_t GetPluginCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return plugins_.size();
    }
    
    /**
     * @brief Get debug information about all plugins
     * @return Debug info string
     */
    std::string GetDebugInfo() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string info = "Debugger Plugin Registry:\n";
        
        for (const auto& pair : plugins_) {
            auto& plugin = pair.second;
            if (!plugin) continue;
            
            info += "  - " + plugin->GetPluginName();
            info += " (" + plugin->GetDebuggerName() + ")";
            info += " v" + plugin->GetPluginVersion();
            
            if (plugin->IsAvailable()) {
                info += " [Available: " + plugin->GetDebuggerPath() + "]";
                info += " Version: " + plugin->GetDebuggerVersion();
            } else {
                info += " [Not Available]";
            }
            info += "\n";
        }
        
        if (plugins_.empty()) {
            info += "  (no plugins registered)\n";
        }
        
        return info;
    }
    
private:
    UCDebuggerPluginRegistry() = default;
    
    std::shared_ptr<IUCDebuggerPlugin> GetPluginUnlocked(DebuggerType type) {
        auto it = plugins_.find(type);
        return (it != plugins_.end()) ? it->second : nullptr;
    }
    
    std::map<DebuggerType, std::shared_ptr<IUCDebuggerPlugin>> plugins_;
    mutable std::mutex mutex_;
};

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

/**
 * @brief Get the debugger plugin registry (convenience function)
 */
inline UCDebuggerPluginRegistry& GetDebuggerRegistry() {
    return UCDebuggerPluginRegistry::Instance();
}

/**
 * @brief Register built-in debugger plugins
 * 
 * Call this during IDE initialization to register GDB and LLDB plugins.
 * Implementation in UCDebugManager.cpp
 */
void RegisterBuiltInDebuggerPlugins();

} // namespace IDE
} // namespace UltraCanvas
