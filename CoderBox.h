// Apps/CoderBox/CoderBox.h
// CoderBox - Main Application Business Logic
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox
#pragma once

// ============================================================================
// CODERBOX - Integrated Development Environment for ULTRA OS
// ============================================================================
//
// This file contains the business logic layer for CoderBox:
// - Plugin registration and management
// - Build orchestration
// - Project lifecycle management
// - Configuration management
//
// The UI layer is implemented in UI/UCCoderBoxApplication.h
//
// Supported Languages:
// - C/C++ (GCC/G++/Clang)
// - FreePascal (Object Pascal, Delphi mode)
// - Rust (Cargo integration)
// - Python (interpreter + virtual environments)
// - Java (JDK compilation)
// - Go (go build/run)
// - C# (.NET/Mono)
// - Fortran (gfortran/ifort/flang)
// - Lua (interpreter)
//
// ============================================================================

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

// IDE Component includes
#include "Project/UCCoderBoxProject.h"
#include "Build/UCBuildManager.h"
#include "Build/UCBuildOutput.h"
#include "Build/IUCCompilerPlugin.h"

// Compiler Plugin includes
#include "Build/Plugins/UCGCCPlugin.h"
#include "Build/Plugins/UCFreePascalPlugin.h"
#include "Build/Plugins/UCRustPlugin.h"
#include "Build/Plugins/UCPythonPlugin.h"
#include "Build/Plugins/UCJavaPlugin.h"
#include "Build/Plugins/UCGoPlugin.h"
#include "Build/Plugins/UCCSharpPlugin.h"
#include "Build/Plugins/UCFortranPlugin.h"
#include "Build/Plugins/UCLuaPlugin.h"

namespace UltraCanvas {
namespace CoderBox {

// ============================================================================
// CODERBOX VERSION INFO
// ============================================================================

constexpr const char* CODERBOX_VERSION = "2.0.0";
constexpr const char* CODERBOX_NAME = "CoderBox";
constexpr const char* CODERBOX_BUILD_DATE = __DATE__;
constexpr int CODERBOX_VERSION_MAJOR = 2;
constexpr int CODERBOX_VERSION_MINOR = 0;
constexpr int CODERBOX_VERSION_PATCH = 0;

// ============================================================================
// CODERBOX CONFIGURATION
// ============================================================================

/**
 * @brief CoderBox application configuration
 * 
 * This contains settings for the business logic layer.
 * UI-specific settings are in CoderBoxConfiguration (UCCoderBoxApplication.h)
 */
struct CoderBoxConfig {
    // Build settings
    bool autoBuildOnSave = false;
    bool showBuildNotifications = true;
    bool clearOutputBeforeBuild = true;
    int maxParallelBuilds = 1;
    
    // Project settings
    std::string defaultProjectPath;
    std::vector<std::string> recentProjects;
    int maxRecentProjects = 10;
    
    // Plugin settings
    bool autoDetectCompilers = true;
    std::map<std::string, std::string> customCompilerPaths;
    
    /**
     * @brief Get default configuration
     */
    static CoderBoxConfig Default();
    
    /**
     * @brief Load configuration from file
     */
    bool LoadFromFile(const std::string& path);
    
    /**
     * @brief Save configuration to file
     */
    bool SaveToFile(const std::string& path) const;
};

// ============================================================================
// CODERBOX STATE
// ============================================================================

/**
 * @brief Current state of the IDE
 */
enum class CoderBoxState {
    Initializing,
    Ready,
    Building,
    Running,
    Debugging,
    ShuttingDown
};

/**
 * @brief Convert CoderBoxState to string
 */
inline std::string CoderBoxStateToString(CoderBoxState state) {
    switch (state) {
        case CoderBoxState::Initializing:  return "Initializing";
        case CoderBoxState::Ready:         return "Ready";
        case CoderBoxState::Building:      return "Building";
        case CoderBoxState::Running:       return "Running";
        case CoderBoxState::Debugging:     return "Debugging";
        case CoderBoxState::ShuttingDown:  return "Shutting Down";
        default:                           return "Unknown";
    }
}

// ============================================================================
// CODERBOX MAIN CLASS (BUSINESS LOGIC)
// ============================================================================

/**
 * @brief CoderBox Business Logic Class
 * 
 * This class orchestrates all non-UI IDE functionality:
 * - Plugin registration and management
 * - Project lifecycle
 * - Build orchestration
 * 
 * The UI layer (UCCoderBoxApplication) uses this class for all
 * business operations.
 */
class CoderBox {
public:
    /**
     * @brief Get singleton instance
     */
    static CoderBox& Instance();
    
    // ===== LIFECYCLE =====
    
    /**
     * @brief Initialize the business logic layer
     * @param config Configuration options
     * @return true if initialization succeeded
     */
    bool Initialize(const CoderBoxConfig& config = CoderBoxConfig::Default());
    
    /**
     * @brief Shutdown the business logic layer
     */
    void Shutdown();
    
    /**
     * @brief Check if initialized
     */
    bool IsInitialized() const { return initialized; }
    
    /**
     * @brief Get current state
     */
    CoderBoxState GetState() const { return currentState; }
    
    // ===== PLUGIN MANAGEMENT =====
    
    /**
     * @brief Register all built-in compiler plugins
     */
    void RegisterBuiltInPlugins();
    
    /**
     * @brief Register a custom compiler plugin
     */
    void RegisterPlugin(std::shared_ptr<IUCCompilerPlugin> plugin);
    
    /**
     * @brief Unregister a plugin by compiler type
     */
    void UnregisterPlugin(CompilerType type);
    
    /**
     * @brief Get list of all registered plugins
     */
    std::vector<std::shared_ptr<IUCCompilerPlugin>> GetRegisteredPlugins() const;
    
    /**
     * @brief Get list of available plugins (compiler found)
     */
    std::vector<std::shared_ptr<IUCCompilerPlugin>> GetAvailablePlugins() const;
    
    /**
     * @brief Detect all available compilers
     */
    void DetectCompilers();
    
    /**
     * @brief Print plugin status to console
     */
    void PrintPluginStatus() const;
    
    // ===== PROJECT MANAGEMENT =====
    
    /**
     * @brief Create a new project
     */
    std::shared_ptr<UCCoderBoxProject> NewProject(
        const std::string& name,
        const std::string& path,
        CompilerType compiler = CompilerType::GPlusPlus
    );
    
    /**
     * @brief Open an existing project
     */
    std::shared_ptr<UCCoderBoxProject> OpenProject(const std::string& projectPath);
    
    /**
     * @brief Import a CMake project
     */
    std::shared_ptr<UCCoderBoxProject> ImportCMakeProject(const std::string& cmakeListsPath);
    
    /**
     * @brief Save current project
     */
    bool SaveProject();
    
    /**
     * @brief Close current project
     */
    bool CloseProject();
    
    /**
     * @brief Get active project
     */
    std::shared_ptr<UCCoderBoxProject> GetActiveProject() const;
    
    /**
     * @brief Set active project
     */
    void SetActiveProject(std::shared_ptr<UCCoderBoxProject> project);
    
    /**
     * @brief Get recent projects list
     */
    std::vector<std::string> GetRecentProjects() const;
    
    /**
     * @brief Add to recent projects
     */
    void AddToRecentProjects(const std::string& projectPath);
    
    // ===== BUILD OPERATIONS =====
    
    /**
     * @brief Build the active project
     */
    void Build();
    
    /**
     * @brief Rebuild the active project (clean + build)
     */
    void Rebuild();
    
    /**
     * @brief Clean the active project
     */
    void Clean();
    
    /**
     * @brief Build and run the active project
     */
    void BuildAndRun();
    
    /**
     * @brief Run the last built executable
     */
    void Run();
    
    /**
     * @brief Stop the running program
     */
    void Stop();
    
    /**
     * @brief Cancel current build
     */
    void CancelBuild();
    
    /**
     * @brief Check if a build is in progress
     */
    bool IsBuildInProgress() const;
    
    /**
     * @brief Get last build result
     */
    const BuildResult& GetLastBuildResult() const;
    
    // ===== CONFIGURATION =====
    
    /**
     * @brief Get configuration
     */
    CoderBoxConfig& GetConfig() { return config; }
    const CoderBoxConfig& GetConfig() const { return config; }
    
    /**
     * @brief Save configuration
     */
    bool SaveConfig();
    
    /**
     * @brief Load configuration
     */
    bool LoadConfig();
    
    /**
     * @brief Get configuration file path
     */
    std::string GetConfigFilePath() const;
    
    // ===== UTILITIES =====
    
    /**
     * @brief Get version string
     */
    std::string GetVersionString() const;
    
    /**
     * @brief Get build information
     */
    std::string GetBuildInfo() const;
    
    /**
     * @brief Print welcome message to console
     */
    void PrintWelcome() const;
    
    // ===== CALLBACKS =====
    
    std::function<void(CoderBoxState)> onStateChange;
    std::function<void(std::shared_ptr<UCCoderBoxProject>)> onProjectOpen;
    std::function<void(std::shared_ptr<UCCoderBoxProject>)> onProjectClose;
    std::function<void(const BuildResult&)> onBuildComplete;
    std::function<void(const std::string&)> onBuildOutput;
    std::function<void(const CompilerMessage&)> onCompilerMessage;
    std::function<void(const std::string&)> onError;

private:
    CoderBox();
    ~CoderBox();
    
    CoderBox(const CoderBox&) = delete;
    CoderBox& operator=(const CoderBox&) = delete;
    
    // ===== INTERNAL METHODS =====
    
    void SetState(CoderBoxState state);
    void SetupBuildCallbacks();
    void SetupProjectCallbacks();
    
    bool InitializeBuildSystem();
    bool InitializeProjectSystem();
    
    // ===== STATE =====
    
    bool initialized = false;
    CoderBoxState currentState = CoderBoxState::Initializing;
    
    CoderBoxConfig config;
    
    std::shared_ptr<UCCoderBoxProject> activeProject;
    
    BuildResult lastBuildResult;
    
    // Running process handle
    void* runningProcess = nullptr;
};

// ============================================================================
// GLOBAL CONVENIENCE FUNCTIONS
// ============================================================================

/**
 * @brief Register all built-in compiler plugins
 */
void RegisterAllCompilerPlugins();

/**
 * @brief Detect and print available compilers
 */
void DetectAndPrintCompilers();

/**
 * @brief Get CoderBox instance (convenience function)
 */
inline CoderBox& GetCoderBox() {
    return CoderBox::Instance();
}

} // namespace CoderBox
} // namespace UltraCanvas
