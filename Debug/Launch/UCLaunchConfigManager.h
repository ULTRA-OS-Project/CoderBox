// Apps/IDE/Debug/Launch/UCLaunchConfigManager.h
// Launch Configuration Manager for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCLaunchConfiguration.h"
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Launch configuration template type
 */
enum class LaunchTemplateType {
    CppLaunch,          // C++ Launch
    CppAttach,          // C++ Attach to process
    CppRemote,          // C++ Remote debugging
    CppCoreDump,        // C++ Core dump
    PythonLaunch,       // Python Launch
    PythonAttach,       // Python Attach
    RustLaunch,         // Rust Launch
    GoLaunch,           // Go Launch
    NodeLaunch,         // Node.js Launch
    Custom              // Empty custom
};

/**
 * @brief Launch configuration manager
 * 
 * Manages launch configurations for a project or workspace.
 * Handles persistence, templates, and configuration selection.
 */
class UCLaunchConfigManager {
public:
    UCLaunchConfigManager();
    ~UCLaunchConfigManager();
    
    // =========================================================================
    // CONFIGURATION MANAGEMENT
    // =========================================================================
    
    /**
     * @brief Add a new configuration
     * @return ID of the added configuration
     */
    std::string AddConfiguration(std::unique_ptr<UCLaunchConfiguration> config);
    
    /**
     * @brief Remove a configuration by ID
     */
    bool RemoveConfiguration(const std::string& id);
    
    /**
     * @brief Get a configuration by ID
     */
    UCLaunchConfiguration* GetConfiguration(const std::string& id);
    const UCLaunchConfiguration* GetConfiguration(const std::string& id) const;
    
    /**
     * @brief Get a configuration by name
     */
    UCLaunchConfiguration* GetConfigurationByName(const std::string& name);
    
    /**
     * @brief Get all configurations
     */
    std::vector<UCLaunchConfiguration*> GetAllConfigurations();
    std::vector<const UCLaunchConfiguration*> GetAllConfigurations() const;
    
    /**
     * @brief Get configuration count
     */
    size_t GetConfigurationCount() const { return configurations_.size(); }
    
    /**
     * @brief Check if a configuration exists
     */
    bool HasConfiguration(const std::string& id) const;
    
    /**
     * @brief Clear all configurations
     */
    void ClearConfigurations();
    
    // =========================================================================
    // DEFAULT CONFIGURATION
    // =========================================================================
    
    /**
     * @brief Set the default configuration
     */
    void SetDefaultConfiguration(const std::string& id);
    
    /**
     * @brief Get the default configuration
     */
    UCLaunchConfiguration* GetDefaultConfiguration();
    const UCLaunchConfiguration* GetDefaultConfiguration() const;
    
    /**
     * @brief Get the default configuration ID
     */
    std::string GetDefaultConfigurationId() const { return defaultConfigId_; }
    
    // =========================================================================
    // ACTIVE CONFIGURATION
    // =========================================================================
    
    /**
     * @brief Set the active (selected) configuration
     */
    void SetActiveConfiguration(const std::string& id);
    
    /**
     * @brief Get the active configuration
     */
    UCLaunchConfiguration* GetActiveConfiguration();
    const UCLaunchConfiguration* GetActiveConfiguration() const;
    
    /**
     * @brief Get the active configuration ID
     */
    std::string GetActiveConfigurationId() const { return activeConfigId_; }
    
    // =========================================================================
    // TEMPLATES
    // =========================================================================
    
    /**
     * @brief Create a configuration from a template
     */
    std::unique_ptr<UCLaunchConfiguration> CreateFromTemplate(LaunchTemplateType type);
    
    /**
     * @brief Get available template types
     */
    std::vector<LaunchTemplateType> GetAvailableTemplates() const;
    
    /**
     * @brief Get template name
     */
    static std::string GetTemplateName(LaunchTemplateType type);
    
    /**
     * @brief Get template description
     */
    static std::string GetTemplateDescription(LaunchTemplateType type);
    
    // =========================================================================
    // PERSISTENCE
    // =========================================================================
    
    /**
     * @brief Save configurations to file
     * @param filePath Path to save (default: .vscode/launch.json style)
     */
    bool SaveToFile(const std::string& filePath);
    
    /**
     * @brief Load configurations from file
     */
    bool LoadFromFile(const std::string& filePath);
    
    /**
     * @brief Set the project directory for relative paths
     */
    void SetProjectDirectory(const std::string& dir) { projectDirectory_ = dir; }
    std::string GetProjectDirectory() const { return projectDirectory_; }
    
    /**
     * @brief Get the default launch.json path
     */
    std::string GetDefaultConfigPath() const;
    
    // =========================================================================
    // RECENT CONFIGURATIONS
    // =========================================================================
    
    /**
     * @brief Add to recent configurations
     */
    void AddToRecent(const std::string& id);
    
    /**
     * @brief Get recent configuration IDs
     */
    std::vector<std::string> GetRecentConfigurations() const;
    
    /**
     * @brief Clear recent configurations
     */
    void ClearRecent() { recentConfigIds_.clear(); }
    
    // =========================================================================
    // VALIDATION
    // =========================================================================
    
    /**
     * @brief Validate all configurations
     * @return Map of config ID to error message (empty if valid)
     */
    std::map<std::string, std::string> ValidateAll() const;
    
    // =========================================================================
    // DUPLICATE
    // =========================================================================
    
    /**
     * @brief Duplicate a configuration
     * @return ID of the new configuration
     */
    std::string DuplicateConfiguration(const std::string& id);
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const std::string& id)> onConfigurationAdded;
    std::function<void(const std::string& id)> onConfigurationRemoved;
    std::function<void(const std::string& id)> onConfigurationChanged;
    std::function<void(const std::string& id)> onActiveConfigurationChanged;
    std::function<void()> onConfigurationsLoaded;
    
private:
    void NotifyConfigurationAdded(const std::string& id);
    void NotifyConfigurationRemoved(const std::string& id);
    void NotifyConfigurationChanged(const std::string& id);
    void NotifyActiveChanged(const std::string& id);
    
    std::map<std::string, std::unique_ptr<UCLaunchConfiguration>> configurations_;
    std::string defaultConfigId_;
    std::string activeConfigId_;
    std::string projectDirectory_;
    std::vector<std::string> recentConfigIds_;
    
    mutable std::mutex mutex_;
    static const size_t MAX_RECENT = 10;
};

} // namespace IDE
} // namespace UltraCanvas
