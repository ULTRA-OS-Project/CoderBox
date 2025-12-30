// Apps/IDE/UltraIDE.h
// Main ULTRA IDE Application Header
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

// ============================================================================
// ULTRA IDE - Integrated Development Environment for ULTRA OS
// ============================================================================
//
// Features:
// - Multi-language compiler support via plugin architecture
// - Background compilation with real-time output parsing
// - CMake project integration
// - Syntax-highlighted code editor
// - Tabbed output console (Build/Errors/Warnings/Messages)
// - Project management with JSON serialization
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

// Core UltraCanvas includes
// #include "UltraCanvas/Core/UltraCanvasApplication.h"
// #include "UltraCanvas/Core/UltraCanvasWindow.h"

// IDE Component includes
#include "Project/UCIDEProject.h"
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
namespace IDE {

// ============================================================================
// ULTRA IDE VERSION INFO
// ============================================================================

constexpr const char* ULTRAIDE_VERSION = "1.0.0";
constexpr const char* ULTRAIDE_NAME = "ULTRA IDE";
constexpr const char* ULTRAIDE_BUILD_DATE = __DATE__;
constexpr int ULTRAIDE_VERSION_MAJOR = 1;
constexpr int ULTRAIDE_VERSION_MINOR = 0;
constexpr int ULTRAIDE_VERSION_PATCH = 0;

// ============================================================================
// IDE CONFIGURATION
// ============================================================================

/**
 * @brief IDE application configuration
 */
struct UltraIDEConfig {
    // Window settings
    std::string windowTitle = "ULTRA IDE";
    int windowWidth = 1400;
    int windowHeight = 900;
    bool maximized = false;
    bool rememberWindowState = true;
    
    // Theme settings
    std::string themeName = "dark";
    std::string fontFamily = "Consolas";
    int fontSize = 12;
    
    // Editor settings
    int tabSize = 4;
    bool insertSpaces = true;
    bool autoIndent = true;
    bool showLineNumbers = true;
    bool highlightCurrentLine = true;
    bool wordWrap = false;
    
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
    static UltraIDEConfig Default();
    
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
// IDE STATE
// ============================================================================

/**
 * @brief Current state of the IDE
 */
enum class IDEState {
    Initializing,
    Ready,
    Building,
    Running,
    Debugging,
    ShuttingDown
};

/**
 * @brief Convert IDEState to string
 */
inline std::string IDEStateToString(IDEState state) {
    switch (state) {
        case IDEState::Initializing:  return "Initializing";
        case IDEState::Ready:         return "Ready";
        case IDEState::Building:      return "Building";
        case IDEState::Running:       return "Running";
        case IDEState::Debugging:     return "Debugging";
        case IDEState::ShuttingDown:  return "Shutting Down";
        default:                      return "Unknown";
    }
}

// ============================================================================
// ULTRA IDE MAIN CLASS
// ============================================================================

/**
 * @brief Main ULTRA IDE Application Class
 * 
 * This class orchestrates all IDE functionality:
 * - Plugin registration and management
 * - Project lifecycle
 * - Build orchestration
 * - UI coordination
 */
class UltraIDE {
public:
    /**
     * @brief Get singleton instance
     */
    static UltraIDE& Instance();
    
    // ===== LIFECYCLE =====
    
    /**
     * @brief Initialize the IDE application
     * @param config Configuration options
     * @return true if initialization succeeded
     */
    bool Initialize(const UltraIDEConfig& config = UltraIDEConfig::Default());
    
    /**
     * @brief Run the IDE main loop
     * @return Exit code
     */
    int Run();
    
    /**
     * @brief Shutdown the IDE
     */
    void Shutdown();
    
    /**
     * @brief Check if IDE is running
     */
    bool IsRunning() const { return running; }
    
    /**
     * @brief Get current IDE state
     */
    IDEState GetState() const { return currentState; }
    
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
    std::shared_ptr<UCIDEProject> NewProject(
        const std::string& name,
        const std::string& path,
        CompilerType compiler = CompilerType::GPlusPlus
    );
    
    /**
     * @brief Open an existing project
     */
    std::shared_ptr<UCIDEProject> OpenProject(const std::string& projectPath);
    
    /**
     * @brief Import a CMake project
     */
    std::shared_ptr<UCIDEProject> ImportCMakeProject(const std::string& cmakeListsPath);
    
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
    std::shared_ptr<UCIDEProject> GetActiveProject() const;
    
    /**
     * @brief Get recent projects list
     */
    std::vector<std::string> GetRecentProjects() const;
    
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
    
    // ===== FILE OPERATIONS =====
    
    /**
     * @brief Open a file in the editor
     */
    void OpenFile(const std::string& filePath);
    
    /**
     * @brief Save the active file
     */
    void SaveFile();
    
    /**
     * @brief Save all open files
     */
    void SaveAllFiles();
    
    /**
     * @brief Close a file
     */
    void CloseFile(const std::string& filePath);
    
    /**
     * @brief Get list of open files
     */
    std::vector<std::string> GetOpenFiles() const;
    
    /**
     * @brief Get active file path
     */
    std::string GetActiveFile() const;
    
    // ===== CONFIGURATION =====
    
    /**
     * @brief Get IDE configuration
     */
    UltraIDEConfig& GetConfig() { return config; }
    const UltraIDEConfig& GetConfig() const { return config; }
    
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
     * @brief Get IDE version string
     */
    std::string GetVersionString() const;
    
    /**
     * @brief Get build information
     */
    std::string GetBuildInfo() const;
    
    /**
     * @brief Print welcome message
     */
    void PrintWelcome() const;
    
    // ===== CALLBACKS =====
    
    std::function<void(IDEState)> onStateChange;
    std::function<void(std::shared_ptr<UCIDEProject>)> onProjectOpen;
    std::function<void(std::shared_ptr<UCIDEProject>)> onProjectClose;
    std::function<void(const BuildResult&)> onBuildComplete;
    std::function<void(const std::string&)> onBuildOutput;
    std::function<void(const CompilerMessage&)> onCompilerMessage;
    std::function<void(const std::string&)> onFileOpen;
    std::function<void(const std::string&)> onFileSave;
    std::function<void(const std::string&)> onError;

private:
    UltraIDE();
    ~UltraIDE();
    
    UltraIDE(const UltraIDE&) = delete;
    UltraIDE& operator=(const UltraIDE&) = delete;
    
    // ===== INTERNAL METHODS =====
    
    void SetState(IDEState state);
    void SetupBuildCallbacks();
    void SetupProjectCallbacks();
    
    bool InitializeUI();
    bool InitializeBuildSystem();
    bool InitializeProjectSystem();
    
    void ProcessEvents();
    void UpdateUI();
    
    // ===== STATE =====
    
    bool initialized = false;
    bool running = false;
    IDEState currentState = IDEState::Initializing;
    
    UltraIDEConfig config;
    
    std::shared_ptr<UCIDEProject> activeProject;
    std::vector<std::string> openFiles;
    std::string activeFile;
    
    BuildResult lastBuildResult;
    
    // Running process handle
    void* runningProcess = nullptr;
};

// ============================================================================
// GLOBAL REGISTRATION FUNCTIONS
// ============================================================================

/**
 * @brief Register all built-in compiler plugins
 * 
 * This function registers all standard compiler plugins with the
 * UCCompilerPluginRegistry. It should be called during IDE initialization.
 * 
 * Registered plugins:
 * - UCGCCPlugin (GCC and G++)
 * - UCFreePascalPlugin
 * - UCRustPlugin
 * - UCPythonPlugin
 * - UCJavaPlugin
 * - UCGoPlugin
 * - UCCSharpPlugin
 * - UCFortranPlugin
 * - UCLuaPlugin
 */
void RegisterAllCompilerPlugins();

/**
 * @brief Detect and print available compilers
 */
void DetectAndPrintCompilers();

/**
 * @brief Get IDE instance (convenience function)
 */
inline UltraIDE& GetIDE() {
    return UltraIDE::Instance();
}

} // namespace IDE
} // namespace UltraCanvas
