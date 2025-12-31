// Apps/IDE/Build/Plugins/UCFortranPlugin.h
// Fortran Compiler Plugin for ULTRA IDE (gfortran/ifort/flang)
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

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// FORTRAN COMPILER TYPE
// ============================================================================

/**
 * @brief Supported Fortran compilers
 */
enum class FortranCompilerType {
    GFortran,           // GNU Fortran (gfortran)
    IntelFortran,       // Intel Fortran (ifort/ifx)
    Flang,              // LLVM Flang
    NAGFortran,         // NAG Fortran
    PGIFortran,         // PGI/NVIDIA Fortran (nvfortran)
    Unknown
};

/**
 * @brief Fortran language standard
 */
enum class FortranStandard {
    Fortran77,          // FORTRAN 77 (legacy)
    Fortran90,          // Fortran 90
    Fortran95,          // Fortran 95
    Fortran2003,        // Fortran 2003
    Fortran2008,        // Fortran 2008
    Fortran2018,        // Fortran 2018 (current)
    Fortran2023,        // Fortran 2023 (latest)
    GNU                 // GNU extensions
};

// ============================================================================
// FORTRAN PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief Fortran-specific configuration options
 */
struct FortranPluginConfig {
    // Compiler paths
    std::string gfortranPath;           // Path to gfortran
    std::string ifortPath;              // Path to ifort/ifx
    std::string flangPath;              // Path to flang
    
    // Active compiler
    FortranCompilerType activeCompiler = FortranCompilerType::GFortran;
    
    // Language standard
    FortranStandard standard = FortranStandard::Fortran2018;
    
    // Source form
    bool freeForm = true;               // Free-form source (default for .f90+)
    bool fixedForm = false;             // Fixed-form source (legacy .f, .for)
    int fixedLineLength = 72;           // Line length for fixed form (72 or 132)
    
    // Preprocessing
    bool enablePreprocessor = false;    // -cpp (C preprocessor)
    std::vector<std::string> defines;   // -D definitions
    std::vector<std::string> undefines; // -U undefinitions
    
    // Warnings
    bool wallWarnings = true;           // -Wall
    bool wextraWarnings = false;        // -Wextra
    bool pedantic = false;              // -pedantic
    bool warningsAsErrors = false;      // -Werror
    bool implicitNone = true;           // -fimplicit-none (require declarations)
    
    // Runtime checks (debug)
    bool boundsCheck = false;           // -fbounds-check / -fcheck=bounds
    bool nullPointerCheck = false;      // -fcheck=pointer
    bool arrayTempsWarning = false;     // -Warray-temporaries
    bool reallocLhs = true;             // -frealloc-lhs (auto realloc arrays)
    bool initLocal = false;             // -finit-local-zero (init locals to zero)
    
    // Optimization
    bool fastMath = false;              // -ffast-math
    bool openmp = false;                // -fopenmp
    bool coarray = false;               // -fcoarray=single/lib
    std::string coarrayMode = "single"; // single, lib
    
    // Debug
    bool backtraceOnError = true;       // -fbacktrace
    bool debugRuntime = false;          // -g with runtime checks
    
    // Module handling
    std::string moduleOutputDir;        // -J<dir> (module output directory)
    std::vector<std::string> modulePaths; // -I<dir> (module search paths)
    
    // Linking
    std::vector<std::string> libraries; // -l<lib>
    std::vector<std::string> libraryPaths; // -L<path>
    bool staticLinking = false;         // -static
    
    /**
     * @brief Get default configuration
     */
    static FortranPluginConfig Default();
};

// ============================================================================
// FORTRAN MODULE INFO
// ============================================================================

/**
 * @brief Information about a Fortran module
 */
struct FortranModuleInfo {
    std::string name;                   // Module name
    std::string sourceFile;             // Source file containing module
    std::string modFile;                // .mod file path
    std::vector<std::string> uses;      // Modules this module uses
    std::vector<std::string> usedBy;    // Modules that use this module
    bool isSubmodule = false;           // Is this a submodule?
    std::string parentModule;           // Parent module (for submodules)
};

/**
 * @brief Fortran program unit types
 */
enum class FortranUnitType {
    Program,
    Module,
    Submodule,
    Subroutine,
    Function,
    BlockData
};

/**
 * @brief Information about a Fortran program unit
 */
struct FortranProgramUnit {
    std::string name;
    FortranUnitType type;
    std::string sourceFile;
    int startLine = 0;
    int endLine = 0;
    std::vector<std::string> arguments;     // For subroutines/functions
    std::string returnType;                 // For functions
};

// ============================================================================
// FORTRAN COMPILER PLUGIN
// ============================================================================

/**
 * @brief Fortran Compiler Plugin Implementation
 * 
 * Supports:
 * - GNU Fortran (gfortran)
 * - Intel Fortran (ifort/ifx)
 * - LLVM Flang
 * - Fortran 77 through Fortran 2018/2023
 * - Free-form and fixed-form source
 * - Module dependency tracking
 * - OpenMP support
 * - Coarray Fortran
 * 
 * Error Format Parsed (gfortran):
 *   file.f90:10:5:
 *   
 *      10 |   call subroutine(x)
 *         |     1
 *   Error: ...
 */
class UCFortranPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Constructor
     */
    UCFortranPlugin();
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCFortranPlugin(const FortranPluginConfig& config);
    
    ~UCFortranPlugin() override;
    
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
    
    // ===== FORTRAN-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const FortranPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const FortranPluginConfig& config);
    
    /**
     * @brief Detect Fortran compilers
     */
    bool DetectInstallation();
    
    /**
     * @brief Get available compilers
     */
    std::vector<FortranCompilerType> GetAvailableCompilers();
    
    /**
     * @brief Set active compiler
     */
    bool SetActiveCompiler(FortranCompilerType compiler);
    
    /**
     * @brief Get active compiler type
     */
    FortranCompilerType GetActiveCompiler() const { return pluginConfig.activeCompiler; }
    
    // ===== MODULE MANAGEMENT =====
    
    /**
     * @brief Scan source files for modules
     */
    std::vector<FortranModuleInfo> ScanModules(const std::vector<std::string>& sourceFiles);
    
    /**
     * @brief Get module dependencies for a file
     */
    std::vector<std::string> GetModuleDependencies(const std::string& sourceFile);
    
    /**
     * @brief Determine compilation order based on dependencies
     */
    std::vector<std::string> GetCompilationOrder(const std::vector<std::string>& sourceFiles);
    
    /**
     * @brief Find module file (.mod)
     */
    std::string FindModuleFile(const std::string& moduleName);
    
    // ===== SOURCE ANALYSIS =====
    
    /**
     * @brief Detect source form (free or fixed)
     */
    bool IsFixedForm(const std::string& filePath) const;
    
    /**
     * @brief Scan source for program units
     */
    std::vector<FortranProgramUnit> ScanProgramUnits(const std::string& sourceFile);
    
    /**
     * @brief Check syntax without full compilation
     */
    BuildResult CheckSyntax(const std::string& sourceFile);
    
    // ===== PREPROCESSING =====
    
    /**
     * @brief Run preprocessor only
     */
    std::string Preprocess(const std::string& sourceFile);
    
    // ===== CALLBACKS =====
    
    std::function<void(const FortranModuleInfo&)> onModuleCompiled;
    std::function<void(const std::string&, float)> onFileProgress;

private:
    // ===== INTERNAL METHODS =====
    
    std::string GetCompilerExecutable() const;
    std::string GetCompilerName(FortranCompilerType type) const;
    bool DetectCompiler(FortranCompilerType type, const std::string& path);
    
    std::vector<std::string> GetStandardFlags() const;
    std::vector<std::string> GetWarningFlags() const;
    std::vector<std::string> GetDebugFlags() const;
    std::vector<std::string> GetOptimizationFlags(int level) const;
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output,
                        std::string& error,
                        int& exitCode);
    
    CompilerMessage ParseGFortranMessage(const std::string& line);
    CompilerMessage ParseIntelMessage(const std::string& line);
    CompilerMessage ParseFlangMessage(const std::string& line);
    
    void ParseModuleFromSource(const std::string& sourceFile, 
                               std::vector<FortranModuleInfo>& modules);
    
    // ===== STATE =====
    
    FortranPluginConfig pluginConfig;
    
    std::map<FortranCompilerType, std::string> detectedCompilers;
    std::map<FortranCompilerType, std::string> compilerVersions;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    
    std::thread buildThread;
    void* currentProcess = nullptr;
    
    // Cached data
    bool installationDetected = false;
    std::string cachedVersion;
    std::map<std::string, FortranModuleInfo> moduleCache;
    
    // Multi-line error parsing state
    std::string pendingFile;
    int pendingLine = 0;
    int pendingColumn = 0;
    bool inErrorBlock = false;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

inline std::string FortranStandardToString(FortranStandard std) {
    switch (std) {
        case FortranStandard::Fortran77:   return "legacy";
        case FortranStandard::Fortran90:   return "f90";
        case FortranStandard::Fortran95:   return "f95";
        case FortranStandard::Fortran2003: return "f2003";
        case FortranStandard::Fortran2008: return "f2008";
        case FortranStandard::Fortran2018: return "f2018";
        case FortranStandard::Fortran2023: return "f2023";
        case FortranStandard::GNU:         return "gnu";
        default: return "f2018";
    }
}

inline std::string FortranCompilerTypeToString(FortranCompilerType type) {
    switch (type) {
        case FortranCompilerType::GFortran:     return "gfortran";
        case FortranCompilerType::IntelFortran: return "ifort";
        case FortranCompilerType::Flang:        return "flang";
        case FortranCompilerType::NAGFortran:   return "nagfor";
        case FortranCompilerType::PGIFortran:   return "nvfortran";
        default: return "unknown";
    }
}

} // namespace IDE
} // namespace UltraCanvas
