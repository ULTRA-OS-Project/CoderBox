// Apps/IDE/Build/IUCCompilerPlugin.h
// Abstract compiler plugin interface for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// COMPILER IDENTIFICATION
// ============================================================================

/**
 * @brief Supported compiler types
 */
enum class CompilerType {
    Unknown = 0,
    GCC,                    // GNU C Compiler
    GPlusPlus,              // GNU C++ Compiler
    FreePascal,             // Free Pascal Compiler
    Rust,                   // Rust/Cargo (future)
    Python,                 // Python interpreter (future)
    Clang,                  // Clang/LLVM (future)
    Custom                  // User-defined compiler
};

/**
 * @brief Convert CompilerType to string
 */
inline std::string CompilerTypeToString(CompilerType type) {
    switch (type) {
        case CompilerType::GCC:         return "GCC";
        case CompilerType::GPlusPlus:   return "G++";
        case CompilerType::FreePascal:  return "FreePascal";
        case CompilerType::Rust:        return "Rust";
        case CompilerType::Python:      return "Python";
        case CompilerType::Clang:       return "Clang";
        case CompilerType::Custom:      return "Custom";
        default:                        return "Unknown";
    }
}

/**
 * @brief Convert string to CompilerType
 */
inline CompilerType StringToCompilerType(const std::string& str) {
    if (str == "GCC")           return CompilerType::GCC;
    if (str == "G++" || str == "GPlusPlus") return CompilerType::GPlusPlus;
    if (str == "FreePascal" || str == "FPC") return CompilerType::FreePascal;
    if (str == "Rust")          return CompilerType::Rust;
    if (str == "Python")        return CompilerType::Python;
    if (str == "Clang")         return CompilerType::Clang;
    if (str == "Custom")        return CompilerType::Custom;
    return CompilerType::Unknown;
}

// ============================================================================
// COMPILER MESSAGE TYPES
// ============================================================================

/**
 * @brief Compiler message severity levels
 */
enum class CompilerMessageType {
    Info,                   // Informational message
    Note,                   // Additional note
    Warning,                // Warning (compilation continues)
    Error,                  // Error (compilation may continue)
    FatalError              // Fatal error (compilation stops)
};

/**
 * @brief Convert CompilerMessageType to string
 */
inline std::string CompilerMessageTypeToString(CompilerMessageType type) {
    switch (type) {
        case CompilerMessageType::Info:         return "Info";
        case CompilerMessageType::Note:         return "Note";
        case CompilerMessageType::Warning:      return "Warning";
        case CompilerMessageType::Error:        return "Error";
        case CompilerMessageType::FatalError:   return "Fatal Error";
        default:                                return "Unknown";
    }
}

// ============================================================================
// PROJECT FILE TYPES
// ============================================================================

/**
 * @brief Project file type classification
 */
enum class ProjectFileType {
    Unknown = 0,
    SourceC,                // .c
    SourceCpp,              // .cpp, .cc, .cxx, .c++
    SourcePascal,           // .pas, .pp, .p, .lpr
    SourceRust,             // .rs
    SourcePython,           // .py
    Header,                 // .h, .hpp, .hxx, .h++
    Resource,               // .rc, .res
    CMakeFile,              // CMakeLists.txt, *.cmake
    Makefile,               // Makefile, *.mk
    BuildConfig,            // Build configuration files
    Documentation,          // .md, .txt, .rst, .adoc
    Data,                   // .json, .xml, .yaml, .yml
    Other
};

/**
 * @brief Detect file type from file path/extension
 */
inline ProjectFileType DetectProjectFileType(const std::string& filePath) {
    // Extract extension
    size_t dotPos = filePath.rfind('.');
    size_t slashPos = filePath.rfind('/');
    if (slashPos == std::string::npos) slashPos = filePath.rfind('\\');
    
    std::string fileName = (slashPos != std::string::npos) ? 
                           filePath.substr(slashPos + 1) : filePath;
    
    // Check for special filenames first
    if (fileName == "CMakeLists.txt") return ProjectFileType::CMakeFile;
    if (fileName == "Makefile" || fileName == "makefile") return ProjectFileType::Makefile;
    
    if (dotPos == std::string::npos || dotPos < slashPos) {
        return ProjectFileType::Unknown;
    }
    
    std::string ext = filePath.substr(dotPos + 1);
    // Convert to lowercase
    for (char& c : ext) c = std::tolower(c);
    
    // C sources
    if (ext == "c") return ProjectFileType::SourceC;
    
    // C++ sources
    if (ext == "cpp" || ext == "cc" || ext == "cxx" || ext == "c++") {
        return ProjectFileType::SourceCpp;
    }
    
    // Pascal sources
    if (ext == "pas" || ext == "pp" || ext == "p" || ext == "lpr" || ext == "dpr") {
        return ProjectFileType::SourcePascal;
    }
    
    // Rust sources
    if (ext == "rs") return ProjectFileType::SourceRust;
    
    // Python sources
    if (ext == "py" || ext == "pyw") return ProjectFileType::SourcePython;
    
    // Headers
    if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "h++" || ext == "hh") {
        return ProjectFileType::Header;
    }
    
    // Resources
    if (ext == "rc" || ext == "res") return ProjectFileType::Resource;
    
    // CMake
    if (ext == "cmake") return ProjectFileType::CMakeFile;
    
    // Makefiles
    if (ext == "mk") return ProjectFileType::Makefile;
    
    // Documentation
    if (ext == "md" || ext == "txt" || ext == "rst" || ext == "adoc") {
        return ProjectFileType::Documentation;
    }
    
    // Data files
    if (ext == "json" || ext == "xml" || ext == "yaml" || ext == "yml") {
        return ProjectFileType::Data;
    }
    
    return ProjectFileType::Other;
}

/**
 * @brief Check if file type is a compilable source
 */
inline bool IsSourceFile(ProjectFileType type) {
    return type == ProjectFileType::SourceC ||
           type == ProjectFileType::SourceCpp ||
           type == ProjectFileType::SourcePascal ||
           type == ProjectFileType::SourceRust ||
           type == ProjectFileType::SourcePython;
}

// ============================================================================
// BUILD OUTPUT TYPE
// ============================================================================

/**
 * @brief Build output type
 */
enum class BuildOutputType {
    Executable,             // Executable binary
    StaticLibrary,          // Static library (.a, .lib)
    SharedLibrary           // Shared/dynamic library (.so, .dll, .dylib)
};

/**
 * @brief Convert BuildOutputType to string
 */
inline std::string BuildOutputTypeToString(BuildOutputType type) {
    switch (type) {
        case BuildOutputType::Executable:       return "Executable";
        case BuildOutputType::StaticLibrary:    return "StaticLibrary";
        case BuildOutputType::SharedLibrary:    return "SharedLibrary";
        default:                                return "Unknown";
    }
}

/**
 * @brief Convert string to BuildOutputType
 */
inline BuildOutputType StringToBuildOutputType(const std::string& str) {
    if (str == "Executable")        return BuildOutputType::Executable;
    if (str == "StaticLibrary")     return BuildOutputType::StaticLibrary;
    if (str == "SharedLibrary")     return BuildOutputType::SharedLibrary;
    return BuildOutputType::Executable;
}

// ============================================================================
// OUTPUT TAB TYPE (for IDE output console)
// ============================================================================

/**
 * @brief Output console tab types
 */
enum class OutputTabType {
    Build,                  // Raw compiler output
    Errors,                 // Errors only
    Warnings,               // Warnings only
    Messages,               // Info/notes
    Application,            // Running program output
    Debug                   // Debug messages
};

// ============================================================================
// COMPILER MESSAGE STRUCTURE
// ============================================================================

/**
 * @brief Represents a single compiler message (error, warning, note, etc.)
 */
struct CompilerMessage {
    CompilerMessageType type = CompilerMessageType::Info;
    std::string filePath;           // Full or relative path to source file
    int line = 0;                   // Line number (1-based, 0 = unknown)
    int column = 0;                 // Column number (1-based, 0 = unknown)
    std::string code;               // Error/warning code (e.g., "C4996", "W001")
    std::string message;            // Human-readable message text
    std::string rawLine;            // Original compiler output line
    
    /**
     * @brief Check if this is an error message
     */
    bool IsError() const {
        return type == CompilerMessageType::Error || 
               type == CompilerMessageType::FatalError;
    }
    
    /**
     * @brief Check if this is a warning message
     */
    bool IsWarning() const {
        return type == CompilerMessageType::Warning;
    }
    
    /**
     * @brief Check if this is a note/info message
     */
    bool IsNote() const {
        return type == CompilerMessageType::Note || 
               type == CompilerMessageType::Info;
    }
    
    /**
     * @brief Check if message has valid location information
     */
    bool HasLocation() const {
        return !filePath.empty() && line > 0;
    }
    
    /**
     * @brief Format message for display
     */
    std::string Format() const {
        std::string result;
        if (!filePath.empty()) {
            result += filePath;
            if (line > 0) {
                result += ":" + std::to_string(line);
                if (column > 0) {
                    result += ":" + std::to_string(column);
                }
            }
            result += ": ";
        }
        result += CompilerMessageTypeToString(type) + ": ";
        if (!code.empty()) {
            result += "[" + code + "] ";
        }
        result += message;
        return result;
    }
};

// ============================================================================
// BUILD CONFIGURATION STRUCTURE
// ============================================================================

/**
 * @brief Build configuration (Debug, Release, etc.)
 */
struct BuildConfiguration {
    std::string name = "Debug";             // Configuration name
    std::string outputDirectory = "build";  // Output directory (relative to project root)
    std::string outputName;                 // Output file name (without extension)
    BuildOutputType outputType = BuildOutputType::Executable;
    
    // Source files (can be empty if using project's file list)
    std::vector<std::string> sourceFiles;
    
    // Include paths
    std::vector<std::string> includePaths;
    
    // Library paths
    std::vector<std::string> libraryPaths;
    
    // Libraries to link
    std::vector<std::string> libraries;
    
    // Preprocessor defines
    std::vector<std::string> defines;
    
    // Additional compiler flags
    std::vector<std::string> compilerFlags;
    
    // Additional linker flags
    std::vector<std::string> linkerFlags;
    
    // Optimization level (0-3)
    int optimizationLevel = 0;
    
    // Include debug symbols
    bool debugSymbols = true;
    
    // Enable compiler warnings
    bool enableWarnings = true;
    
    // Treat warnings as errors
    bool treatWarningsAsErrors = false;
    
    /**
     * @brief Get output file extension based on type and platform
     */
    std::string GetOutputExtension() const {
        switch (outputType) {
            case BuildOutputType::Executable:
                #ifdef _WIN32
                return ".exe";
                #else
                return "";
                #endif
            case BuildOutputType::StaticLibrary:
                #ifdef _WIN32
                return ".lib";
                #else
                return ".a";
                #endif
            case BuildOutputType::SharedLibrary:
                #ifdef _WIN32
                return ".dll";
                #elif defined(__APPLE__)
                return ".dylib";
                #else
                return ".so";
                #endif
        }
        return "";
    }
    
    /**
     * @brief Get full output path
     */
    std::string GetOutputPath() const {
        std::string path = outputDirectory;
        if (!path.empty() && path.back() != '/' && path.back() != '\\') {
            path += "/";
        }
        
        // Add library prefix for libraries on Unix
        #ifndef _WIN32
        if (outputType == BuildOutputType::StaticLibrary || 
            outputType == BuildOutputType::SharedLibrary) {
            path += "lib";
        }
        #endif
        
        path += outputName;
        path += GetOutputExtension();
        return path;
    }
};

// ============================================================================
// BUILD RESULT STRUCTURE
// ============================================================================

/**
 * @brief Result of a build operation
 */
struct BuildResult {
    bool success = false;               // Overall build success
    int exitCode = -1;                  // Compiler exit code
    std::string outputFile;             // Path to generated output file
    std::vector<CompilerMessage> messages;  // All compiler messages
    int errorCount = 0;                 // Number of errors
    int warningCount = 0;               // Number of warnings
    double buildTimeSeconds = 0.0;      // Total build time
    std::string rawOutput;              // Complete raw compiler output
    
    /**
     * @brief Get all error messages
     */
    std::vector<CompilerMessage> GetErrors() const {
        std::vector<CompilerMessage> errors;
        for (const auto& msg : messages) {
            if (msg.IsError()) {
                errors.push_back(msg);
            }
        }
        return errors;
    }
    
    /**
     * @brief Get all warning messages
     */
    std::vector<CompilerMessage> GetWarnings() const {
        std::vector<CompilerMessage> warnings;
        for (const auto& msg : messages) {
            if (msg.IsWarning()) {
                warnings.push_back(msg);
            }
        }
        return warnings;
    }
    
    /**
     * @brief Get all note/info messages
     */
    std::vector<CompilerMessage> GetNotes() const {
        std::vector<CompilerMessage> notes;
        for (const auto& msg : messages) {
            if (msg.IsNote()) {
                notes.push_back(msg);
            }
        }
        return notes;
    }
    
    /**
     * @brief Get summary string
     */
    std::string GetSummary() const {
        std::string summary;
        if (success) {
            summary = "Build succeeded";
        } else {
            summary = "Build failed";
        }
        summary += " - " + std::to_string(errorCount) + " error(s), " +
                   std::to_string(warningCount) + " warning(s)";
        summary += " [" + std::to_string(buildTimeSeconds) + "s]";
        return summary;
    }
};

// ============================================================================
// PROJECT FILE STRUCTURE
// ============================================================================

/**
 * @brief Represents a single file in the project
 */
struct ProjectFile {
    std::string absolutePath;           // Full absolute path
    std::string relativePath;           // Path relative to project root
    std::string fileName;               // File name only (no path)
    ProjectFileType type = ProjectFileType::Unknown;
    bool isOpen = false;                // Currently open in editor
    bool isModified = false;            // Has unsaved changes
    
    /**
     * @brief Detect type from path
     */
    void DetectType() {
        type = DetectProjectFileType(absolutePath);
    }
    
    /**
     * @brief Check if this is a compilable source file
     */
    bool IsSource() const {
        return IsSourceFile(type);
    }
};

// ============================================================================
// PROJECT FOLDER STRUCTURE
// ============================================================================

/**
 * @brief Represents a folder in the project hierarchy
 */
struct ProjectFolder {
    std::string name;                   // Folder name
    std::string absolutePath;           // Full absolute path
    std::string relativePath;           // Path relative to project root
    std::vector<ProjectFile> files;     // Files in this folder
    std::vector<ProjectFolder> subfolders;  // Subfolders
    bool isExpanded = true;             // UI expansion state
    
    /**
     * @brief Get total file count (recursive)
     */
    int GetTotalFileCount() const {
        int count = static_cast<int>(files.size());
        for (const auto& sub : subfolders) {
            count += sub.GetTotalFileCount();
        }
        return count;
    }
    
    /**
     * @brief Get all source files (recursive)
     */
    std::vector<const ProjectFile*> GetSourceFiles() const {
        std::vector<const ProjectFile*> sources;
        for (const auto& file : files) {
            if (file.IsSource()) {
                sources.push_back(&file);
            }
        }
        for (const auto& sub : subfolders) {
            auto subSources = sub.GetSourceFiles();
            sources.insert(sources.end(), subSources.begin(), subSources.end());
        }
        return sources;
    }
};

// ============================================================================
// BUILD JOB STRUCTURE
// ============================================================================

/**
 * @brief Represents a build job in the build queue
 */
struct BuildJob {
    std::string projectPath;            // Path to project file
    std::vector<std::string> sourceFiles;   // Specific files to build (empty = all)
    BuildConfiguration configuration;   // Build configuration to use
    bool cleanBuild = false;            // Perform clean build
    bool runAfterBuild = false;         // Run executable after successful build
    
    /**
     * @brief Check if this is a single-file build
     */
    bool IsSingleFileBuild() const {
        return sourceFiles.size() == 1;
    }
    
    /**
     * @brief Check if this is a full project build
     */
    bool IsFullBuild() const {
        return sourceFiles.empty();
    }
};

// ============================================================================
// COMPILER PLUGIN INTERFACE
// ============================================================================

/**
 * @brief Abstract interface for compiler plugins
 * 
 * All compiler plugins must implement this interface.
 * Plugins are responsible for:
 * - Detecting if the compiler is available
 * - Building command line arguments
 * - Executing the compiler
 * - Parsing compiler output
 */
class IUCCompilerPlugin {
public:
    virtual ~IUCCompilerPlugin() = default;
    
    // ===== PLUGIN IDENTIFICATION =====
    
    /**
     * @brief Get plugin display name
     */
    virtual std::string GetPluginName() const = 0;
    
    /**
     * @brief Get plugin version string
     */
    virtual std::string GetPluginVersion() const = 0;
    
    /**
     * @brief Get compiler type this plugin handles
     */
    virtual CompilerType GetCompilerType() const = 0;
    
    /**
     * @brief Get compiler display name
     */
    virtual std::string GetCompilerName() const = 0;
    
    // ===== COMPILER DETECTION =====
    
    /**
     * @brief Check if the compiler is available on this system
     */
    virtual bool IsAvailable() = 0;
    
    /**
     * @brief Get path to compiler executable
     */
    virtual std::string GetCompilerPath() = 0;
    
    /**
     * @brief Set custom compiler path
     */
    virtual void SetCompilerPath(const std::string& path) = 0;
    
    /**
     * @brief Get compiler version string
     */
    virtual std::string GetCompilerVersion() = 0;
    
    /**
     * @brief Get list of supported file extensions
     */
    virtual std::vector<std::string> GetSupportedExtensions() const = 0;
    
    /**
     * @brief Check if this plugin can compile the given file
     */
    virtual bool CanCompile(const std::string& filePath) const = 0;
    
    // ===== COMPILATION =====
    
    /**
     * @brief Compile files asynchronously
     * @param sourceFiles List of source files to compile
     * @param config Build configuration
     * @param onComplete Callback when build completes
     * @param onOutputLine Callback for each line of compiler output
     * @param onProgress Callback for progress updates (0.0 to 1.0)
     */
    virtual void CompileAsync(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config,
        std::function<void(const BuildResult&)> onComplete,
        std::function<void(const std::string&)> onOutputLine,
        std::function<void(float)> onProgress
    ) = 0;
    
    /**
     * @brief Compile files synchronously (blocking)
     * @param sourceFiles List of source files to compile
     * @param config Build configuration
     * @return Build result
     */
    virtual BuildResult CompileSync(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) = 0;
    
    // ===== BUILD CONTROL =====
    
    /**
     * @brief Cancel ongoing build
     */
    virtual void Cancel() = 0;
    
    /**
     * @brief Check if a build is currently in progress
     */
    virtual bool IsBuildInProgress() const = 0;
    
    // ===== OUTPUT PARSING =====
    
    /**
     * @brief Parse a single line of compiler output
     * @param line Raw output line from compiler
     * @return Parsed compiler message (type may be Info if not recognized)
     */
    virtual CompilerMessage ParseOutputLine(const std::string& line) = 0;
    
    // ===== COMMAND LINE GENERATION =====
    
    /**
     * @brief Generate compiler command line arguments
     * @param sourceFiles Source files to compile
     * @param config Build configuration
     * @return Vector of command line arguments
     */
    virtual std::vector<std::string> GenerateCommandLine(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) = 0;

protected:
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> buildInProgress{false};
    std::string compilerPath;
    mutable std::mutex pluginMutex;
};

// ============================================================================
// COMPILER PLUGIN REGISTRY
// ============================================================================

/**
 * @brief Singleton registry for compiler plugins
 */
class UCCompilerPluginRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static UCCompilerPluginRegistry& Instance() {
        static UCCompilerPluginRegistry instance;
        return instance;
    }
    
    /**
     * @brief Register a compiler plugin
     * @param plugin Plugin to register
     */
    void RegisterPlugin(std::shared_ptr<IUCCompilerPlugin> plugin) {
        if (!plugin) return;
        std::lock_guard<std::mutex> lock(registryMutex);
        plugins[plugin->GetCompilerType()] = plugin;
    }
    
    /**
     * @brief Unregister a compiler plugin
     * @param type Compiler type to unregister
     */
    void UnregisterPlugin(CompilerType type) {
        std::lock_guard<std::mutex> lock(registryMutex);
        plugins.erase(type);
    }
    
    /**
     * @brief Get plugin by compiler type
     * @param type Compiler type
     * @return Plugin pointer or nullptr if not found
     */
    std::shared_ptr<IUCCompilerPlugin> GetPlugin(CompilerType type) {
        std::lock_guard<std::mutex> lock(registryMutex);
        auto it = plugins.find(type);
        if (it != plugins.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    /**
     * @brief Get plugin that can compile the given file
     * @param filePath Path to source file
     * @return Plugin pointer or nullptr if no plugin can handle the file
     */
    std::shared_ptr<IUCCompilerPlugin> GetPluginForFile(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(registryMutex);
        for (const auto& pair : plugins) {
            if (pair.second->CanCompile(filePath)) {
                return pair.second;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get all registered plugins
     */
    std::vector<std::shared_ptr<IUCCompilerPlugin>> GetAllPlugins() {
        std::lock_guard<std::mutex> lock(registryMutex);
        std::vector<std::shared_ptr<IUCCompilerPlugin>> result;
        for (const auto& pair : plugins) {
            result.push_back(pair.second);
        }
        return result;
    }
    
    /**
     * @brief Get all available plugins (compiler is installed)
     */
    std::vector<std::shared_ptr<IUCCompilerPlugin>> GetAvailablePlugins() {
        std::lock_guard<std::mutex> lock(registryMutex);
        std::vector<std::shared_ptr<IUCCompilerPlugin>> result;
        for (const auto& pair : plugins) {
            if (pair.second->IsAvailable()) {
                result.push_back(pair.second);
            }
        }
        return result;
    }
    
    /**
     * @brief Check if any plugins are registered
     */
    bool HasPlugins() const {
        std::lock_guard<std::mutex> lock(registryMutex);
        return !plugins.empty();
    }
    
    /**
     * @brief Get count of registered plugins
     */
    size_t GetPluginCount() const {
        std::lock_guard<std::mutex> lock(registryMutex);
        return plugins.size();
    }

private:
    UCCompilerPluginRegistry() = default;
    UCCompilerPluginRegistry(const UCCompilerPluginRegistry&) = delete;
    UCCompilerPluginRegistry& operator=(const UCCompilerPluginRegistry&) = delete;
    
    std::map<CompilerType, std::shared_ptr<IUCCompilerPlugin>> plugins;
    mutable std::mutex registryMutex;
};

} // namespace IDE
} // namespace UltraCanvas
