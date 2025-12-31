// Apps/IDE/Build/CMakeIntegration/UCCMakeParser.h
// Enhanced CMake Parser for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../IUCCompilerPlugin.h"
#include "../../Project/UCIDEProject.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <regex>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// CMAKE COMMAND TYPES
// ============================================================================

/**
 * @brief Types of CMake commands
 */
enum class CMakeCommandType {
    Unknown,
    
    // Project
    Project,
    CMakeMinimumRequired,
    
    // Targets
    AddExecutable,
    AddLibrary,
    AddCustomTarget,
    
    // Target properties
    TargetSources,
    TargetIncludeDirectories,
    TargetCompileDefinitions,
    TargetCompileOptions,
    TargetLinkLibraries,
    TargetLinkOptions,
    SetTargetProperties,
    
    // Global
    IncludeDirectories,
    AddDefinitions,
    LinkLibraries,
    LinkDirectories,
    
    // Variables
    Set,
    Option,
    List,
    
    // Subdirectories
    AddSubdirectory,
    Include,
    
    // Control flow
    If,
    ElseIf,
    Else,
    EndIf,
    ForEach,
    EndForEach,
    While,
    EndWhile,
    Function,
    EndFunction,
    Macro,
    EndMacro,
    
    // Other
    Message,
    FindPackage,
    FindLibrary,
    Configure,
    Install
};

// ============================================================================
// CMAKE PARSED COMMAND
// ============================================================================

/**
 * @brief Represents a parsed CMake command
 */
struct CMakeParsedCommand {
    CMakeCommandType type = CMakeCommandType::Unknown;
    std::string name;                           // Command name as written
    std::vector<std::string> arguments;         // Command arguments
    int lineNumber = 0;                         // Line in file
    std::string rawText;                        // Original command text
    
    /**
     * @brief Get argument at index (empty if out of bounds)
     */
    std::string GetArg(size_t index) const {
        if (index < arguments.size()) {
            return arguments[index];
        }
        return "";
    }
    
    /**
     * @brief Check if argument exists
     */
    bool HasArg(const std::string& arg) const {
        for (const auto& a : arguments) {
            if (a == arg) return true;
        }
        return false;
    }
    
    /**
     * @brief Get all arguments after a keyword
     */
    std::vector<std::string> GetArgsAfter(const std::string& keyword) const {
        std::vector<std::string> result;
        bool found = false;
        for (const auto& arg : arguments) {
            if (found) {
                // Stop at next keyword (uppercase)
                if (!arg.empty() && std::isupper(arg[0])) {
                    break;
                }
                result.push_back(arg);
            } else if (arg == keyword) {
                found = true;
            }
        }
        return result;
    }
};

// ============================================================================
// CMAKE PARSED TARGET
// ============================================================================

/**
 * @brief Extended target information from CMake
 */
struct CMakeParsedTarget {
    std::string name;
    CMakeTarget::Type type = CMakeTarget::Type::Unknown;
    
    // Source files
    std::vector<std::string> sources;
    std::vector<std::string> headers;
    
    // Build settings
    std::vector<std::string> includeDirectories;
    std::vector<std::string> compileDefinitions;
    std::vector<std::string> compileOptions;
    std::vector<std::string> linkLibraries;
    std::vector<std::string> linkOptions;
    
    // Properties
    std::map<std::string, std::string> properties;
    
    // Dependencies
    std::vector<std::string> dependencies;
    
    // Location in file
    int definedAtLine = 0;
    std::string definedInFile;
    
    /**
     * @brief Check if target has sources
     */
    bool HasSources() const {
        return !sources.empty();
    }
    
    /**
     * @brief Get all source files (sources + headers)
     */
    std::vector<std::string> GetAllSources() const {
        std::vector<std::string> all = sources;
        all.insert(all.end(), headers.begin(), headers.end());
        return all;
    }
};

// ============================================================================
// CMAKE PARSED PROJECT
// ============================================================================

/**
 * @brief Complete parsed CMake project
 */
struct CMakeParsedProject {
    // Project info
    std::string name;
    std::string version;
    std::string description;
    std::string cmakeMinVersion;
    std::vector<std::string> languages;
    
    // Targets
    std::vector<CMakeParsedTarget> targets;
    
    // Variables
    std::map<std::string, std::string> variables;
    std::map<std::string, std::string> cacheVariables;
    std::map<std::string, std::string> options;
    
    // Directories
    std::vector<std::string> subdirectories;
    std::vector<std::string> includeDirectories;
    std::vector<std::string> linkDirectories;
    
    // Global settings
    std::vector<std::string> globalDefinitions;
    std::vector<std::string> globalLinkLibraries;
    
    // Find packages
    std::vector<std::string> requiredPackages;
    std::vector<std::string> optionalPackages;
    
    // Commands (all parsed)
    std::vector<CMakeParsedCommand> commands;
    
    // File info
    std::string rootDirectory;
    std::string mainFile;
    std::vector<std::string> includedFiles;
    
    // Errors/warnings during parsing
    std::vector<std::string> parseErrors;
    std::vector<std::string> parseWarnings;
    
    /**
     * @brief Find target by name
     */
    const CMakeParsedTarget* FindTarget(const std::string& targetName) const {
        for (const auto& target : targets) {
            if (target.name == targetName) {
                return &target;
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get target names
     */
    std::vector<std::string> GetTargetNames() const {
        std::vector<std::string> names;
        for (const auto& target : targets) {
            names.push_back(target.name);
        }
        return names;
    }
    
    /**
     * @brief Get variable value
     */
    std::string GetVariable(const std::string& name) const {
        auto it = variables.find(name);
        if (it != variables.end()) {
            return it->second;
        }
        return "";
    }
};

// ============================================================================
// CMAKE PARSER
// ============================================================================

/**
 * @brief CMake file parser
 * 
 * Parses CMakeLists.txt files to extract project structure,
 * targets, and build configuration.
 */
class UCCMakeParser {
public:
    UCCMakeParser();
    ~UCCMakeParser() = default;
    
    // ===== PARSING =====
    
    /**
     * @brief Parse CMakeLists.txt file
     * @param filePath Path to CMakeLists.txt
     * @return Parsed project information
     */
    CMakeParsedProject ParseFile(const std::string& filePath);
    
    /**
     * @brief Parse CMake content from string
     * @param content CMake content
     * @param workingDir Working directory for includes
     * @return Parsed project information
     */
    CMakeParsedProject ParseString(const std::string& content, 
                                    const std::string& workingDir = "");
    
    /**
     * @brief Parse single command from string
     */
    CMakeParsedCommand ParseCommand(const std::string& commandText);
    
    // ===== VARIABLE EXPANSION =====
    
    /**
     * @brief Expand variables in string
     */
    std::string ExpandVariables(const std::string& input, 
                                 const CMakeParsedProject& project);
    
    /**
     * @brief Set variable for expansion
     */
    void SetVariable(const std::string& name, const std::string& value);
    
    /**
     * @brief Get variable value
     */
    std::string GetVariable(const std::string& name) const;
    
    // ===== PROJECT CONVERSION =====
    
    /**
     * @brief Convert parsed project to UCIDEProject
     */
    std::shared_ptr<UCIDEProject> ToIDEProject(const CMakeParsedProject& parsed);
    
    /**
     * @brief Update UCIDEProject from parsed CMake
     */
    void UpdateIDEProject(std::shared_ptr<UCIDEProject> project,
                          const CMakeParsedProject& parsed);
    
    // ===== CALLBACKS =====
    
    /**
     * @brief Called when parsing starts
     */
    std::function<void(const std::string& file)> onParseStart;
    
    /**
     * @brief Called when a command is parsed
     */
    std::function<void(const CMakeParsedCommand&)> onCommand;
    
    /**
     * @brief Called when a target is found
     */
    std::function<void(const CMakeParsedTarget&)> onTarget;
    
    /**
     * @brief Called on parse error
     */
    std::function<void(const std::string& error, int line)> onError;
    
    /**
     * @brief Called on parse warning
     */
    std::function<void(const std::string& warning, int line)> onWarning;

private:
    // ===== INTERNAL PARSING =====
    
    /**
     * @brief Tokenize CMake content
     */
    std::vector<std::string> Tokenize(const std::string& content);
    
    /**
     * @brief Parse commands from tokens
     */
    std::vector<CMakeParsedCommand> ParseCommands(const std::string& content);
    
    /**
     * @brief Get command type from name
     */
    CMakeCommandType GetCommandType(const std::string& name) const;
    
    /**
     * @brief Process parsed command into project
     */
    void ProcessCommand(const CMakeParsedCommand& cmd, CMakeParsedProject& project);
    
    /**
     * @brief Process target command
     */
    void ProcessTargetCommand(const CMakeParsedCommand& cmd, CMakeParsedProject& project);
    
    /**
     * @brief Process variable command (set, option)
     */
    void ProcessVariableCommand(const CMakeParsedCommand& cmd, CMakeParsedProject& project);
    
    /**
     * @brief Parse argument list (handles quotes, parentheses)
     */
    std::vector<std::string> ParseArguments(const std::string& argsText);
    
    /**
     * @brief Handle generator expressions
     */
    std::string HandleGeneratorExpression(const std::string& expr);
    
    /**
     * @brief Read file content
     */
    std::string ReadFile(const std::string& filePath);
    
    /**
     * @brief Handle include() command
     */
    void ProcessInclude(const std::string& includePath, 
                        const std::string& workingDir,
                        CMakeParsedProject& project);
    
    /**
     * @brief Handle add_subdirectory() command
     */
    void ProcessSubdirectory(const std::string& subdir,
                              const std::string& workingDir,
                              CMakeParsedProject& project);
    
    // ===== STATE =====
    
    std::map<std::string, std::string> variables;
    std::string currentFile;
    int currentLine = 0;
    int nestingLevel = 0;
    
    // CMake built-in variables
    std::map<std::string, std::string> builtInVariables;
    
    // Command name to type mapping
    std::map<std::string, CMakeCommandType> commandTypes;
    
    // Regex patterns
    std::regex commandPattern;
    std::regex variablePattern;
    std::regex generatorExprPattern;
};

// ============================================================================
// CMAKE BUILD RUNNER
// ============================================================================

/**
 * @brief Runs CMake configure and build commands
 */
class UCCMakeBuildRunner {
public:
    UCCMakeBuildRunner();
    ~UCCMakeBuildRunner() = default;
    
    // ===== CMAKE COMMANDS =====
    
    /**
     * @brief Run cmake configure step
     * @param sourceDir Source directory (with CMakeLists.txt)
     * @param buildDir Build directory
     * @param generator Generator to use (e.g., "Ninja", "Unix Makefiles")
     * @param options Additional options
     * @return Exit code
     */
    int Configure(const std::string& sourceDir,
                  const std::string& buildDir,
                  const std::string& generator = "",
                  const std::map<std::string, std::string>& options = {});
    
    /**
     * @brief Run cmake build
     * @param buildDir Build directory
     * @param target Target to build (empty = all)
     * @param config Configuration (Debug, Release)
     * @param jobs Number of parallel jobs
     * @return Exit code
     */
    int Build(const std::string& buildDir,
              const std::string& target = "",
              const std::string& config = "",
              int jobs = 0);
    
    /**
     * @brief Run cmake clean
     */
    int Clean(const std::string& buildDir);
    
    /**
     * @brief Run cmake install
     */
    int Install(const std::string& buildDir,
                const std::string& prefix = "");
    
    /**
     * @brief Get available generators
     */
    std::vector<std::string> GetAvailableGenerators();
    
    /**
     * @brief Check if cmake is available
     */
    bool IsCMakeAvailable();
    
    /**
     * @brief Get cmake version
     */
    std::string GetCMakeVersion();
    
    // ===== CALLBACKS =====
    
    std::function<void(const std::string&)> onOutput;
    std::function<void(const std::string&)> onError;
    std::function<void(float)> onProgress;
    
private:
    std::string cmakePath;
    
    int ExecuteCMake(const std::vector<std::string>& args,
                     const std::string& workDir);
};

} // namespace IDE
} // namespace UltraCanvas
