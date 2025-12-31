// Apps/IDE/Build/Plugins/UCGCCPlugin.h
// GCC/G++ Compiler Plugin for ULTRA IDE
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
// GCC PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief GCC-specific configuration options
 */
struct GCCPluginConfig {
    // Compiler paths
    std::string gccPath;            // Path to gcc executable
    std::string gppPath;            // Path to g++ executable
    std::string arPath;             // Path to ar (archiver)
    std::string ldPath;             // Path to ld (linker)
    
    // Default flags
    std::string cStandard = "c11";          // C standard (-std=c11)
    std::string cppStandard = "c++20";      // C++ standard (-std=c++20)
    
    // Diagnostic options
    bool colorDiagnostics = true;           // -fdiagnostics-color=always
    bool showColumn = true;                 // -fshow-column
    bool caretDiagnostics = true;          // -fcaret-diagnostics
    
    // Build options
    int parallelJobs = 0;                   // -j (0 = auto-detect CPU count)
    bool verboseOutput = false;             // -v
    bool saveTemps = false;                 // -save-temps
    
    /**
     * @brief Get default configuration
     */
    static GCCPluginConfig Default();
};

// ============================================================================
// GCC COMPILER PLUGIN
// ============================================================================

/**
 * @brief GCC/G++ Compiler Plugin Implementation
 * 
 * Supports:
 * - C compilation (gcc)
 * - C++ compilation (g++)
 * - Static library creation (ar)
 * - Shared library creation (gcc/g++ -shared)
 * - Executable linking
 * 
 * Error Format Parsed:
 *   file:line:column: type: message
 *   file:line: type: message
 */
class UCGCCPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Constructor
     * @param useCpp If true, defaults to G++ mode; false = GCC mode
     */
    explicit UCGCCPlugin(bool useCpp = true);
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCGCCPlugin(const GCCPluginConfig& config, bool useCpp = true);
    
    ~UCGCCPlugin() override;
    
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
    
    // ===== GCC-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const GCCPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const GCCPluginConfig& config) { pluginConfig = config; }
    
    /**
     * @brief Set whether to use C++ compiler (g++) or C compiler (gcc)
     */
    void SetUseCpp(bool useCpp) { this->useCpp = useCpp; }
    bool GetUseCpp() const { return useCpp; }
    
    /**
     * @brief Detect GCC installation
     */
    bool DetectInstallation();
    
    /**
     * @brief Get list of supported warning flags
     */
    std::vector<std::string> GetAvailableWarningFlags() const;
    
    /**
     * @brief Get list of supported optimization levels
     */
    std::vector<std::string> GetOptimizationLevels() const;

private:
    // ===== INTERNAL METHODS =====
    
    /**
     * @brief Build compiler arguments for compilation
     */
    std::vector<std::string> BuildCompileArgs(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    );
    
    /**
     * @brief Build linker arguments
     */
    std::vector<std::string> BuildLinkArgs(
        const std::vector<std::string>& objectFiles,
        const BuildConfiguration& config
    );
    
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
     * @brief Get object file path from source file
     */
    std::string GetObjectFilePath(const std::string& sourceFile, 
                                  const std::string& outputDir) const;
    
    /**
     * @brief Create output directory if needed
     */
    bool EnsureDirectoryExists(const std::string& path);
    
    // ===== STATE =====
    
    GCCPluginConfig pluginConfig;
    bool useCpp;                            // Use g++ instead of gcc
    std::string cachedVersion;              // Cached compiler version
    bool versionCached = false;
    
    std::unique_ptr<GCCOutputParser> outputParser;
    
    std::thread buildThread;
    pid_t currentProcessId = 0;             // For cancellation (POSIX)
    
    // Callbacks for async build
    std::function<void(const BuildResult&)> asyncOnComplete;
    std::function<void(const std::string&)> asyncOnOutputLine;
    std::function<void(float)> asyncOnProgress;
};

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

/**
 * @brief Create GCC plugin (C compiler)
 */
std::shared_ptr<UCGCCPlugin> CreateGCCPlugin();

/**
 * @brief Create G++ plugin (C++ compiler)
 */
std::shared_ptr<UCGCCPlugin> CreateGPlusPlusPlugin();

/**
 * @brief Register GCC plugins with the registry
 */
void RegisterGCCPlugins();

} // namespace IDE
} // namespace UltraCanvas
