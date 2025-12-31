// Apps/IDE/Project/UCIDEProject.h
// IDE Project data structure with JSON serialization
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Build/IUCCompilerPlugin.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// PROJECT FORMAT CONSTANTS
// ============================================================================

constexpr const char* UCIDE_PROJECT_EXTENSION = ".ucproj";
constexpr const char* UCIDE_PROJECT_VERSION = "1.0.0";
constexpr int UCIDE_FORMAT_VERSION = 1;

// ============================================================================
// CMAKE INTEGRATION STRUCTURES
// ============================================================================

/**
 * @brief CMake configuration settings
 */
struct CMakeSettings {
    bool enabled = false;               // Whether CMake is used for this project
    std::string cmakeListsPath;         // Path to CMakeLists.txt (relative)
    std::string buildDirectory = "build"; // CMake build directory
    std::string activeTarget;           // Currently selected CMake target
    std::string generator;              // CMake generator (e.g., "Ninja", "Unix Makefiles")
    std::map<std::string, std::string> variables;  // CMake variables (-D options)
    
    /**
     * @brief Get build type from variables
     */
    std::string GetBuildType() const {
        auto it = variables.find("CMAKE_BUILD_TYPE");
        if (it != variables.end()) {
            return it->second;
        }
        return "Debug";
    }
    
    /**
     * @brief Set build type in variables
     */
    void SetBuildType(const std::string& type) {
        variables["CMAKE_BUILD_TYPE"] = type;
    }
};

/**
 * @brief CMake target information
 */
struct CMakeTarget {
    std::string name;                   // Target name
    enum class Type {
        Executable,
        StaticLibrary,
        SharedLibrary,
        Interface,
        Unknown
    } type = Type::Unknown;
    
    std::vector<std::string> sources;   // Source files
    std::vector<std::string> includeDirs; // Include directories
    std::vector<std::string> defines;   // Compile definitions
    std::vector<std::string> linkLibraries; // Link libraries
};

// ============================================================================
// EDITOR SETTINGS
// ============================================================================

/**
 * @brief Editor settings for the project
 */
struct EditorSettings {
    int tabSize = 4;                    // Tab width in spaces
    bool insertSpaces = true;           // Use spaces instead of tabs
    bool trimTrailingWhitespace = true; // Trim trailing whitespace on save
    std::string defaultEncoding = "UTF-8"; // Default file encoding
    bool showWhitespace = false;        // Show whitespace characters
    bool wordWrap = false;              // Enable word wrap
};

// ============================================================================
// SOURCE GROUP
// ============================================================================

/**
 * @brief Source file grouping for display
 */
struct SourceGroup {
    std::string name;                   // Group display name
    std::vector<std::string> patterns;  // Glob patterns for matching files
    
    SourceGroup() = default;
    SourceGroup(const std::string& n, std::initializer_list<std::string> p)
        : name(n), patterns(p) {}
};

// ============================================================================
// PROJECT CLASS
// ============================================================================

/**
 * @brief Main project data structure
 * 
 * Represents a complete ULTRA IDE project with all settings,
 * configurations, and file information.
 */
class UCIDEProject {
public:
    // ===== PROJECT METADATA =====
    std::string name;                   // Project name
    std::string version = "1.0.0";      // Project version (semver)
    std::string description;            // Project description
    std::string author;                 // Project author
    std::string projectFilePath;        // Full path to .ucproj file
    std::string rootDirectory;          // Project root directory
    std::string createdDate;            // ISO 8601 creation date
    std::string modifiedDate;           // ISO 8601 modification date
    
    // ===== COMPILER SETTINGS =====
    CompilerType primaryCompiler = CompilerType::GPlusPlus;
    std::string compilerPath;           // Custom compiler path (empty = auto-detect)
    
    // ===== BUILD CONFIGURATIONS =====
    std::vector<BuildConfiguration> configurations;
    int activeConfigurationIndex = 0;
    
    // ===== PROJECT FILES =====
    ProjectFolder rootFolder;
    std::vector<std::string> excludePatterns = {
        "build/*", ".git/*", "*.o", "*.obj", "*.a", "*.so", "*.dll",
        "cmake-build-*/*", ".idea/*", ".vscode/*"
    };
    std::vector<SourceGroup> sourceGroups;
    
    // ===== CMAKE INTEGRATION =====
    CMakeSettings cmake;
    std::vector<CMakeTarget> cmakeTargets;
    
    // ===== EDITOR SETTINGS =====
    EditorSettings editor;
    
    // ===== OPEN FILES STATE =====
    std::vector<std::string> openFiles;     // Currently open files
    std::string activeFile;                  // Currently active/focused file
    
public:
    // ===== CONSTRUCTORS =====
    UCIDEProject() = default;
    ~UCIDEProject() = default;
    
    // ===== FILE OPERATIONS =====
    
    /**
     * @brief Load project from file
     * @param path Path to .ucproj file
     * @return true if successful
     */
    bool LoadFromFile(const std::string& path);
    
    /**
     * @brief Save project to file
     * @param path Path to save to (empty = use current projectFilePath)
     * @return true if successful
     */
    bool SaveToFile(const std::string& path = "");
    
    // ===== CONFIGURATION MANAGEMENT =====
    
    /**
     * @brief Get active build configuration
     */
    BuildConfiguration& GetActiveConfiguration();
    const BuildConfiguration& GetActiveConfiguration() const;
    
    /**
     * @brief Add a new build configuration
     */
    void AddConfiguration(const BuildConfiguration& config);
    
    /**
     * @brief Remove a build configuration by index
     */
    void RemoveConfiguration(int index);
    
    /**
     * @brief Set active configuration by index
     */
    void SetActiveConfiguration(int index);
    
    /**
     * @brief Set active configuration by name
     */
    void SetActiveConfiguration(const std::string& name);
    
    /**
     * @brief Find configuration by name
     * @return Index or -1 if not found
     */
    int FindConfiguration(const std::string& name) const;
    
    // ===== FILE TREE MANAGEMENT =====
    
    /**
     * @brief Refresh the file tree by scanning the directory
     */
    void RefreshFileTree();
    
    /**
     * @brief Add a file to the project
     */
    void AddFile(const std::string& filePath);
    
    /**
     * @brief Remove a file from the project
     */
    void RemoveFile(const std::string& filePath);
    
    /**
     * @brief Find a file by relative path
     * @return Pointer to ProjectFile or nullptr
     */
    ProjectFile* FindFile(const std::string& relativePath);
    const ProjectFile* FindFile(const std::string& relativePath) const;
    
    /**
     * @brief Get all source files
     */
    std::vector<ProjectFile*> GetSourceFiles();
    std::vector<const ProjectFile*> GetSourceFiles() const;
    
    /**
     * @brief Get all modified (unsaved) files
     */
    std::vector<ProjectFile*> GetModifiedFiles();
    
    /**
     * @brief Check if path matches any exclude pattern
     */
    bool MatchesExcludePattern(const std::string& path) const;
    
    // ===== CMAKE INTEGRATION =====
    
    /**
     * @brief Check if CMakeLists.txt exists in project
     */
    bool DetectCMakeProject();
    
    /**
     * @brief Parse CMakeLists.txt to extract targets
     */
    bool ParseCMakeLists();
    
    /**
     * @brief Get list of CMake target names
     */
    std::vector<std::string> GetCMakeTargetNames() const;
    
    /**
     * @brief Get CMake target by name
     */
    const CMakeTarget* GetCMakeTarget(const std::string& name) const;
    
    // ===== JSON SERIALIZATION =====
    
    /**
     * @brief Serialize project to JSON string
     */
    std::string ToJSON() const;
    
    /**
     * @brief Deserialize project from JSON string
     * @return true if successful
     */
    bool FromJSON(const std::string& json);
    
    // ===== UTILITY =====
    
    /**
     * @brief Get project file path relative to root
     */
    std::string GetRelativePath(const std::string& absolutePath) const;
    
    /**
     * @brief Get absolute path from relative
     */
    std::string GetAbsolutePath(const std::string& relativePath) const;
    
    /**
     * @brief Update modification timestamp
     */
    void Touch();
    
    /**
     * @brief Check if project has unsaved changes
     */
    bool HasUnsavedChanges() const;
    
    /**
     * @brief Mark project as having unsaved changes
     */
    void SetModified(bool modified = true);
    
    // ===== FACTORY METHODS =====
    
    /**
     * @brief Create a new empty project
     */
    static std::shared_ptr<UCIDEProject> Create(
        const std::string& name,
        const std::string& rootPath,
        CompilerType compiler = CompilerType::GPlusPlus
    );
    
    /**
     * @brief Create project from existing CMakeLists.txt
     */
    static std::shared_ptr<UCIDEProject> CreateFromCMake(
        const std::string& cmakeListsPath
    );
    
    /**
     * @brief Create default Debug configuration
     */
    static BuildConfiguration CreateDebugConfiguration(const std::string& outputName);
    
    /**
     * @brief Create default Release configuration
     */
    static BuildConfiguration CreateReleaseConfiguration(const std::string& outputName);

private:
    // Private helper methods
    void ScanDirectory(const std::string& path, ProjectFolder& folder, int depth = 0);
    bool isModified = false;
    
    // Maximum directory scan depth to prevent infinite recursion
    static constexpr int MAX_SCAN_DEPTH = 20;
};

// ============================================================================
// PROJECT MANAGER CLASS
// ============================================================================

/**
 * @brief Manages project lifecycle and recent projects
 */
class UCIDEProjectManager {
public:
    /**
     * @brief Get singleton instance
     */
    static UCIDEProjectManager& Instance();
    
    // ===== PROJECT OPERATIONS =====
    
    /**
     * @brief Create a new project
     */
    std::shared_ptr<UCIDEProject> CreateProject(
        const std::string& name,
        const std::string& path,
        CompilerType compiler = CompilerType::GPlusPlus
    );
    
    /**
     * @brief Open existing project
     */
    std::shared_ptr<UCIDEProject> OpenProject(const std::string& projectFilePath);
    
    /**
     * @brief Import CMake project
     */
    std::shared_ptr<UCIDEProject> ImportCMakeProject(const std::string& cmakeListsPath);
    
    /**
     * @brief Save project
     */
    bool SaveProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Close project
     */
    bool CloseProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Close all projects
     */
    void CloseAllProjects();
    
    // ===== ACTIVE PROJECT =====
    
    /**
     * @brief Get currently active project
     */
    std::shared_ptr<UCIDEProject> GetActiveProject() const { return activeProject; }
    
    /**
     * @brief Set active project
     */
    void SetActiveProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Get all open projects
     */
    const std::vector<std::shared_ptr<UCIDEProject>>& GetOpenProjects() const {
        return openProjects;
    }
    
    // ===== RECENT PROJECTS =====
    
    /**
     * @brief Get list of recent project paths
     */
    std::vector<std::string> GetRecentProjects() const { return recentProjects; }
    
    /**
     * @brief Add project to recent list
     */
    void AddToRecentProjects(const std::string& path);
    
    /**
     * @brief Remove from recent list
     */
    void RemoveFromRecentProjects(const std::string& path);
    
    /**
     * @brief Clear recent projects list
     */
    void ClearRecentProjects();
    
    /**
     * @brief Load recent projects from settings
     */
    void LoadRecentProjects();
    
    /**
     * @brief Save recent projects to settings
     */
    void SaveRecentProjects();
    
    // ===== CALLBACKS =====
    
    std::function<void(std::shared_ptr<UCIDEProject>)> onProjectOpen;
    std::function<void(std::shared_ptr<UCIDEProject>)> onProjectClose;
    std::function<void(std::shared_ptr<UCIDEProject>)> onProjectSave;
    std::function<void(std::shared_ptr<UCIDEProject>)> onActiveProjectChange;
    std::function<void(const std::string&)> onProjectError;

private:
    UCIDEProjectManager() = default;
    UCIDEProjectManager(const UCIDEProjectManager&) = delete;
    UCIDEProjectManager& operator=(const UCIDEProjectManager&) = delete;
    
    std::vector<std::shared_ptr<UCIDEProject>> openProjects;
    std::shared_ptr<UCIDEProject> activeProject;
    std::vector<std::string> recentProjects;
    
    static constexpr int MAX_RECENT_PROJECTS = 10;
};

} // namespace IDE
} // namespace UltraCanvas
