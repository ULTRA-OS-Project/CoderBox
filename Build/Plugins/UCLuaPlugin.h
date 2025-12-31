// Apps/IDE/Build/Plugins/UCLuaPlugin.h
// Lua Interpreter/Compiler Plugin for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../IUCCompilerPlugin.h"
#include "../UCBuildOutput.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>
#include <functional>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// LUA VERSION/IMPLEMENTATION
// ============================================================================

/**
 * @brief Lua implementation type
 */
enum class LuaImplementation {
    StandardLua,        // PUC-Rio Lua (lua.org)
    LuaJIT,             // LuaJIT (luajit.org)
    LuaU,               // Luau (Roblox)
    MoonScript,         // MoonScript (transpiles to Lua)
    Teal,               // Teal (typed Lua)
    Unknown
};

/**
 * @brief Lua version
 */
enum class LuaVersion {
    Lua51,              // Lua 5.1
    Lua52,              // Lua 5.2
    Lua53,              // Lua 5.3
    Lua54,              // Lua 5.4 (current)
    LuaJIT21,           // LuaJIT 2.1 (5.1 compatible)
    Luau,               // Luau (5.1 based)
    Unknown
};

// ============================================================================
// LUA PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief Lua-specific configuration options
 */
struct LuaPluginConfig {
    // Interpreter paths
    std::string luaPath;                // Path to lua interpreter
    std::string luacPath;               // Path to luac compiler
    std::string luajitPath;             // Path to luajit
    std::string luauPath;               // Path to luau
    std::string tealPath;               // Path to tl (Teal compiler)
    std::string moonscriptPath;         // Path to moonc
    
    // Active implementation
    LuaImplementation activeImpl = LuaImplementation::StandardLua;
    
    // Execution options
    bool interactiveMode = false;       // -i (enter interactive after script)
    bool showVersion = false;           // -v
    bool showWarnings = true;           // -W (Lua 5.4+)
    bool strictMode = false;            // Require strict variable declarations
    
    // LuaJIT options
    bool jitEnabled = true;             // JIT compilation on
    bool jitDumpBytecode = false;       // -bl (list bytecode)
    bool jitDumpTrace = false;          // -jdump (trace JIT)
    int jitOptLevel = 3;                // -O0 to -O3
    
    // Bytecode compilation
    bool stripDebugInfo = false;        // -s (strip debug info from bytecode)
    bool textBytecode = false;          // Output bytecode as text listing
    
    // Module paths
    std::vector<std::string> packagePaths;   // LUA_PATH additions
    std::vector<std::string> cPaths;         // LUA_CPATH additions
    
    // Debugging
    bool enableDebugHooks = false;      // Enable debug library hooks
    bool enableProfiler = false;        // Enable profiling
    int memoryLimit = 0;                // Memory limit in MB (0 = unlimited)
    
    // Teal options
    bool tealStrict = true;             // Teal strict mode
    bool tealGenDeclarations = false;   // Generate .d.tl files
    
    // Environment variables
    std::map<std::string, std::string> envVars;
    
    /**
     * @brief Get default configuration
     */
    static LuaPluginConfig Default();
};

// ============================================================================
// LUA ANALYSIS STRUCTURES
// ============================================================================

/**
 * @brief Lua syntax error info
 */
struct LuaSyntaxError {
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;
    std::string nearToken;              // "near 'xxx'"
};

/**
 * @brief Lua runtime error info
 */
struct LuaRuntimeError {
    std::string file;
    int line = 0;
    std::string message;
    std::vector<std::string> traceback;
};

/**
 * @brief Lua function info
 */
struct LuaFunctionInfo {
    std::string name;
    std::string file;
    int startLine = 0;
    int endLine = 0;
    std::vector<std::string> parameters;
    bool isLocal = false;
    bool isMethod = false;              // function obj:method()
    std::string parentTable;            // For table.func definitions
};

/**
 * @brief Lua module info
 */
struct LuaModuleInfo {
    std::string name;
    std::string file;
    std::vector<std::string> requires;  // Required modules
    std::vector<std::string> exports;   // Exported names
};

/**
 * @brief Lua global variable info
 */
struct LuaGlobalInfo {
    std::string name;
    std::string file;
    int line = 0;
    std::string type;                   // Inferred type if possible
};

/**
 * @brief Lua bytecode info
 */
struct LuaBytecodeInfo {
    std::string sourceFile;
    std::string outputFile;
    size_t sourceSize = 0;
    size_t bytecodeSize = 0;
    int numFunctions = 0;
    int numUpvalues = 0;
    bool stripped = false;
};

// ============================================================================
// LUA REPL SESSION
// ============================================================================

/**
 * @brief Interactive Lua REPL session
 */
class LuaReplSession {
public:
    bool isRunning = false;
    std::string lastResult;
    std::string lastError;
    std::vector<std::string> history;
    
    std::function<void(const std::string&)> onOutput;
    std::function<void(const std::string&)> onError;
    std::function<void()> onExit;
    
    // Pipe communication (Unix)
    int inputFd = -1;
    int outputFd = -1;
    pid_t pid = 0;
    std::thread readerThread;
};

// ============================================================================
// LUA PLUGIN
// ============================================================================

/**
 * @brief Lua Interpreter/Compiler Plugin Implementation
 * 
 * Supports:
 * - Standard Lua 5.1-5.4
 * - LuaJIT 2.1
 * - Luau (Roblox Lua)
 * - Teal (typed Lua)
 * - MoonScript
 * - Bytecode compilation
 * - Interactive REPL
 * - Syntax checking
 * - Module dependency analysis
 * 
 * Error Format Parsed:
 *   lua: file.lua:10: syntax error near 'end'
 *   file.lua:10: attempt to call a nil value
 */
class UCLuaPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Constructor
     */
    UCLuaPlugin();
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCLuaPlugin(const LuaPluginConfig& config);
    
    ~UCLuaPlugin() override;
    
    // ===== PLUGIN IDENTIFICATION =====
    
    std::string GetPluginName() const override;
    std::string GetPluginVersion() const override;
    CompilerType GetCompilerType() const override;
    std::string GetCompilerName() const override;
    
    // ===== INTERPRETER DETECTION =====
    
    bool IsAvailable() override;
    std::string GetCompilerPath() override;
    void SetCompilerPath(const std::string& path) override;
    std::string GetCompilerVersion() override;
    std::vector<std::string> GetSupportedExtensions() const override;
    bool CanCompile(const std::string& filePath) const override;
    
    // ===== EXECUTION (CompileSync runs the script) =====
    
    void CompileAsync(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config,
        std::function<void(const BuildResult&)> onComplete,
        std::function<void(const std::string&)> onOutputLine,
        std::function<void(float)> onProgress
    ) override;
    
    BuildResult CompileSync(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) override;
    
    // ===== RUN CONTROL =====
    
    void Cancel() override;
    bool IsBuildInProgress() const override;
    
    // ===== OUTPUT PARSING =====
    
    CompilerMessage ParseOutputLine(const std::string& line) override;
    
    // ===== COMMAND LINE GENERATION =====
    
    std::vector<std::string> GenerateCommandLine(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) override;
    
    // ===== LUA-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const LuaPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const LuaPluginConfig& config);
    
    /**
     * @brief Detect Lua installations
     */
    bool DetectInstallation();
    
    /**
     * @brief Get available implementations
     */
    std::vector<LuaImplementation> GetAvailableImplementations();
    
    /**
     * @brief Set active implementation
     */
    bool SetActiveImplementation(LuaImplementation impl);
    
    /**
     * @brief Get detected Lua version
     */
    LuaVersion GetLuaVersion() const { return detectedVersion; }
    
    // ===== SCRIPT EXECUTION =====
    
    /**
     * @brief Run a Lua script
     */
    BuildResult RunScript(const std::string& scriptFile,
                          const std::vector<std::string>& args = {},
                          std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run Lua code directly
     */
    BuildResult RunCode(const std::string& code,
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run script with timeout
     */
    BuildResult RunScriptWithTimeout(const std::string& scriptFile,
                                     int timeoutSeconds,
                                     std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== BYTECODE COMPILATION =====
    
    /**
     * @brief Compile to bytecode
     */
    LuaBytecodeInfo CompileToBytecode(const std::string& sourceFile,
                                      const std::string& outputFile = "");
    
    /**
     * @brief Dump bytecode listing
     */
    std::string DumpBytecode(const std::string& sourceFile);
    
    /**
     * @brief Check if file is bytecode
     */
    bool IsBytecodeFile(const std::string& file);
    
    // ===== SYNTAX CHECKING =====
    
    /**
     * @brief Check syntax without running
     */
    BuildResult CheckSyntax(const std::string& sourceFile);
    
    /**
     * @brief Check syntax of code string
     */
    BuildResult CheckSyntaxCode(const std::string& code);
    
    /**
     * @brief Parse syntax error from output
     */
    LuaSyntaxError ParseSyntaxError(const std::string& output);
    
    // ===== SOURCE ANALYSIS =====
    
    /**
     * @brief Scan for functions in file
     */
    std::vector<LuaFunctionInfo> ScanFunctions(const std::string& sourceFile);
    
    /**
     * @brief Scan for global variables
     */
    std::vector<LuaGlobalInfo> ScanGlobals(const std::string& sourceFile);
    
    /**
     * @brief Scan for require() calls
     */
    std::vector<std::string> ScanRequires(const std::string& sourceFile);
    
    /**
     * @brief Get module info
     */
    LuaModuleInfo AnalyzeModule(const std::string& sourceFile);
    
    // ===== REPL =====
    
    /**
     * @brief Start interactive REPL
     */
    bool StartRepl(std::function<void(const std::string&)> onOutput);
    
    /**
     * @brief Send input to REPL
     */
    bool SendReplInput(const std::string& input);
    
    /**
     * @brief Stop REPL
     */
    void StopRepl();
    
    /**
     * @brief Check if REPL is running
     */
    bool IsReplRunning() const;
    
    // ===== TEAL (TYPED LUA) =====
    
    /**
     * @brief Compile Teal to Lua
     */
    BuildResult CompileTeal(const std::string& tealFile,
                            const std::string& outputFile = "");
    
    /**
     * @brief Type-check Teal file
     */
    BuildResult CheckTeal(const std::string& tealFile);
    
    /**
     * @brief Generate Teal declaration file
     */
    bool GenerateTealDeclaration(const std::string& luaFile,
                                 const std::string& outputFile = "");
    
    // ===== MOONSCRIPT =====
    
    /**
     * @brief Compile MoonScript to Lua
     */
    BuildResult CompileMoonScript(const std::string& moonFile,
                                  const std::string& outputFile = "");
    
    /**
     * @brief Check MoonScript syntax
     */
    BuildResult CheckMoonScript(const std::string& moonFile);
    
    // ===== LUAJIT SPECIFIC =====
    
    /**
     * @brief Run with LuaJIT
     */
    BuildResult RunWithJIT(const std::string& scriptFile,
                           std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Get JIT status/stats
     */
    std::string GetJITStatus();
    
    /**
     * @brief Dump JIT trace
     */
    std::string DumpJITTrace(const std::string& scriptFile);
    
    // ===== MODULE MANAGEMENT =====
    
    /**
     * @brief Find module file
     */
    std::string FindModule(const std::string& moduleName);
    
    /**
     * @brief Get effective package.path
     */
    std::string GetPackagePath();
    
    /**
     * @brief Get effective package.cpath
     */
    std::string GetPackageCPath();
    
    // ===== CALLBACKS =====
    
    std::function<void(const std::string&)> onScriptOutput;
    std::function<void(const LuaRuntimeError&)> onRuntimeError;
    std::function<void(const LuaBytecodeInfo&)> onBytecodeCompiled;

private:
    // ===== INTERNAL METHODS =====
    
    std::string GetInterpreterExecutable() const;
    std::string GetCompilerExecutable() const;
    bool DetectImplementation(LuaImplementation impl, const std::string& path);
    
    LuaVersion ParseVersion(const std::string& versionString);
    LuaRuntimeError ParseRuntimeError(const std::string& output);
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output,
                        std::string& error,
                        int& exitCode,
                        const std::string& input = "",
                        const std::string& workingDir = "");
    
    std::string BuildPackagePath() const;
    std::string BuildCPath() const;
    
    void ParseFunctionsFromSource(const std::string& source,
                                  const std::string& file,
                                  std::vector<LuaFunctionInfo>& functions);
    
    // ===== STATE =====
    
    LuaPluginConfig pluginConfig;
    
    std::map<LuaImplementation, std::string> detectedImpls;
    std::map<LuaImplementation, std::string> implVersions;
    LuaVersion detectedVersion = LuaVersion::Unknown;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    
    std::thread buildThread;
    void* currentProcess = nullptr;
    
    // REPL state
    std::unique_ptr<LuaReplSession> replSession;
    std::thread replThread;
    
    // Cached data
    bool installationDetected = false;
    std::string cachedVersion;
    std::map<std::string, LuaModuleInfo> moduleCache;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

inline std::string LuaImplementationToString(LuaImplementation impl) {
    switch (impl) {
        case LuaImplementation::StandardLua: return "lua";
        case LuaImplementation::LuaJIT:      return "luajit";
        case LuaImplementation::LuaU:        return "luau";
        case LuaImplementation::MoonScript:  return "moonscript";
        case LuaImplementation::Teal:        return "teal";
        default: return "unknown";
    }
}

inline std::string LuaVersionToString(LuaVersion ver) {
    switch (ver) {
        case LuaVersion::Lua51:    return "5.1";
        case LuaVersion::Lua52:    return "5.2";
        case LuaVersion::Lua53:    return "5.3";
        case LuaVersion::Lua54:    return "5.4";
        case LuaVersion::LuaJIT21: return "LuaJIT 2.1";
        case LuaVersion::Luau:     return "Luau";
        default: return "unknown";
    }
}

} // namespace IDE
} // namespace UltraCanvas
