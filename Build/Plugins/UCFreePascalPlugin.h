// Apps/IDE/Build/Plugins/UCFreePascalPlugin.h
// FreePascal Compiler Plugin for ULTRA IDE
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

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// FREEPASCAL PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief FreePascal-specific configuration options
 */
struct FreePascalPluginConfig {
    // Compiler path
    std::string fpcPath;                    // Path to fpc executable
    
    // Compiler mode
    enum class SyntaxMode {
        FPC,            // Default FPC mode
        ObjFPC,         // Object Pascal mode (-Mobjfpc)
        Delphi,         // Delphi compatibility (-Mdelphi)
        DelphiUnicode,  // Delphi Unicode (-Mdelphiunicode)
        TP,             // Turbo Pascal (-Mtp)
        MacPas          // Mac Pascal (-Mmacpas)
    };
    SyntaxMode syntaxMode = SyntaxMode::ObjFPC;
    
    // Code generation options
    bool smartLinking = true;               // -CX (smart linking)
    bool stackChecking = false;             // -Ct (stack checking)
    bool rangeChecking = false;             // -Cr (range checking)
    bool overflowChecking = false;          // -Co (overflow checking)
    bool ioChecking = true;                 // -Ci (I/O checking)
    
    // Target options
    std::string targetOS;                   // -T<os> (target OS)
    std::string targetCPU;                  // -P<cpu> (target CPU)
    
    // Output options
    bool generateAssembly = false;          // -a (generate assembly)
    bool stripSymbols = false;              // -Xs (strip symbols)
    
    // Unit paths
    std::vector<std::string> unitPaths;     // -Fu<path>
    std::vector<std::string> includePaths;  // -Fi<path>
    std::vector<std::string> libraryPaths;  // -Fl<path>
    
    // Verbosity
    enum class Verbosity {
        Quiet,          // -v0
        Errors,         // -ve
        ErrorsWarnings, // -vew
        All,            // -vewnh (errors, warnings, notes, hints)
        Debug           // -va (all messages)
    };
    Verbosity verbosity = Verbosity::All;
    
    /**
     * @brief Get default configuration
     */
    static FreePascalPluginConfig Default();
};

// ============================================================================
// FREEPASCAL COMPILER PLUGIN
// ============================================================================

/**
 * @brief FreePascal Compiler Plugin Implementation
 * 
 * Supports:
 * - Pascal compilation (fpc)
 * - Multiple syntax modes (ObjFPC, Delphi, TP)
 * - Unit compilation
 * - Executable and library generation
 * 
 * Error Format Parsed:
 *   file(line,column) Type: message
 *   file(line) Type: message
 */
class UCFreePascalPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Default constructor
     */
    UCFreePascalPlugin();
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCFreePascalPlugin(const FreePascalPluginConfig& config);
    
    ~UCFreePascalPlugin() override;
    
    // ===== PLUGIN IDENTIFICATION =====
    
    std::string GetPluginName() const override;
    std::string GetPluginVersion() const override;
    CompilerType GetCompilerType() const override;
    std::string GetCompilerName() const override;
    
    // ===== COMPILER DETECTION =====
    
    bool IsAvailable() override;
    std::string GetCompilerPath() override;
    void SetCompilerPath(const std::string& path) override;
    std::string GetCompilerVersion() override;
    std::vector<std::string> GetSupportedExtensions() const override;
    bool CanCompile(const std::string& filePath) const override;
    
    // ===== COMPILATION =====
    
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
    
    // ===== BUILD CONTROL =====
    
    void Cancel() override;
    bool IsBuildInProgress() const override;
    
    // ===== OUTPUT PARSING =====
    
    CompilerMessage ParseOutputLine(const std::string& line) override;
    
    // ===== COMMAND LINE GENERATION =====
    
    std::vector<std::string> GenerateCommandLine(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) override;
    
    // ===== FREEPASCAL-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const FreePascalPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const FreePascalPluginConfig& config) { pluginConfig = config; }
    
    /**
     * @brief Detect FreePascal installation
     */
    bool DetectInstallation();
    
    /**
     * @brief Get FreePascal installation directory
     */
    std::string GetInstallationDirectory() const;
    
    /**
     * @brief Get list of available syntax modes
     */
    std::vector<std::pair<FreePascalPluginConfig::SyntaxMode, std::string>> GetSyntaxModes() const;
    
    /**
     * @brief Get list of available target operating systems
     */
    std::vector<std::string> GetTargetOperatingSystems() const;
    
    /**
     * @brief Get list of available target CPUs
     */
    std::vector<std::string> GetTargetCPUs() const;

private:
    // ===== INTERNAL METHODS =====
    
    /**
     * @brief Build compiler arguments
     */
    std::vector<std::string> BuildCompileArgs(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    );
    
    /**
     * @brief Get syntax mode flag
     */
    std::string GetSyntaxModeFlag() const;
    
    /**
     * @brief Get verbosity flags
     */
    std::string GetVerbosityFlag() const;
    
    /**
     * @brief Get optimization flag from level
     */
    std::string GetOptimizationFlag(int level) const;
    
    /**
     * @brief Execute compiler and capture output
     */
    int ExecuteCompiler(
        const std::vector<std::string>& args,
        const std::string& workDir,
        std::function<void(const std::string&)> outputCallback
    );
    
    /**
     * @brief Create output directory if needed
     */
    bool EnsureDirectoryExists(const std::string& path);
    
    // ===== STATE =====
    
    FreePascalPluginConfig pluginConfig;
    std::string cachedVersion;
    bool versionCached = false;
    
    std::unique_ptr<FreePascalOutputParser> outputParser;
    
    std::thread buildThread;
    pid_t currentProcessId = 0;
    
    // Callbacks for async build
    std::function<void(const BuildResult&)> asyncOnComplete;
    std::function<void(const std::string&)> asyncOnOutputLine;
    std::function<void(float)> asyncOnProgress;
};

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

/**
 * @brief Create FreePascal plugin
 */
std::shared_ptr<UCFreePascalPlugin> CreateFreePascalPlugin();

/**
 * @brief Register FreePascal plugin with the registry
 */
void RegisterFreePascalPlugin();

} // namespace IDE
} // namespace UltraCanvas
