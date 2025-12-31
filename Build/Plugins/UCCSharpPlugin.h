// Apps/IDE/Build/Plugins/UCCSharpPlugin.h
// C#/.NET Compiler Plugin for ULTRA IDE
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
// .NET SDK / RUNTIME TYPE
// ============================================================================

/**
 * @brief .NET implementation type
 */
enum class DotNetType {
    DotNetSDK,          // Modern .NET SDK (5, 6, 7, 8, 9+)
    DotNetCore,         // .NET Core (2.x, 3.x)
    DotNetFramework,    // .NET Framework (Windows only)
    Mono,               // Mono runtime (cross-platform)
    Unknown
};

/**
 * @brief .NET version
 */
enum class DotNetVersion {
    Net48,              // .NET Framework 4.8
    NetCore31,          // .NET Core 3.1 (LTS, EOL)
    Net50,              // .NET 5.0
    Net60,              // .NET 6.0 (LTS)
    Net70,              // .NET 7.0
    Net80,              // .NET 8.0 (LTS)
    Net90,              // .NET 9.0 (current)
    Unknown
};

/**
 * @brief C# language version
 */
enum class CSharpVersion {
    CSharp7   = 7,      // C# 7.0
    CSharp7_1 = 71,     // C# 7.1
    CSharp7_2 = 72,     // C# 7.2
    CSharp7_3 = 73,     // C# 7.3
    CSharp8   = 8,      // C# 8.0
    CSharp9   = 9,      // C# 9.0
    CSharp10  = 10,     // C# 10.0
    CSharp11  = 11,     // C# 11.0
    CSharp12  = 12,     // C# 12.0
    CSharp13  = 13,     // C# 13.0 (current)
    Latest    = 99,     // Latest supported
    Preview   = 100,    // Preview features
    Unknown   = 0
};

/**
 * @brief Project output type
 */
enum class DotNetOutputType {
    Exe,                // Console application
    WinExe,             // Windows GUI application
    Library,            // Class library (DLL)
    Module,             // .netmodule
    WinMDObj            // Windows Runtime metadata
};

/**
 * @brief Target framework
 */
enum class TargetFramework {
    Net48,              // net48
    NetCoreApp31,       // netcoreapp3.1
    Net50,              // net5.0
    Net60,              // net6.0
    Net70,              // net7.0
    Net80,              // net8.0
    Net90,              // net9.0
    NetStandard20,      // netstandard2.0
    NetStandard21,      // netstandard2.1
    Unknown
};

// ============================================================================
// C# PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief C#-specific configuration options
 */
struct CSharpPluginConfig {
    // SDK/Runtime paths
    std::string dotnetPath;             // Path to dotnet CLI
    std::string cscPath;                // Path to csc.exe (Roslyn)
    std::string monoPath;               // Path to mono/mcs
    std::string msbuildPath;            // Path to MSBuild
    
    // Active implementation
    DotNetType activeDotNet = DotNetType::DotNetSDK;
    
    // Language version
    CSharpVersion langVersion = CSharpVersion::Latest;
    bool nullable = true;               // Nullable reference types
    bool implicitUsings = true;         // Implicit global usings
    
    // Compilation options
    DotNetOutputType outputType = DotNetOutputType::Exe;
    TargetFramework targetFramework = TargetFramework::Net80;
    std::string runtimeIdentifier;      // e.g., "win-x64", "linux-x64"
    bool selfContained = false;         // Self-contained deployment
    bool singleFile = false;            // Single-file publish
    bool readyToRun = false;            // R2R compilation
    bool trimmed = false;               // IL trimming
    bool aot = false;                   // Native AOT (NET 7+)
    
    // Optimization
    bool optimize = true;               // -optimize
    bool debug = true;                  // -debug
    std::string debugType = "portable"; // portable, full, pdbonly
    bool deterministic = true;          // Deterministic build
    
    // Warnings
    int warningLevel = 4;               // 0-4, or 9999 for all
    bool warningsAsErrors = false;      // -warnaserror
    std::vector<std::string> disabledWarnings;  // -nowarn
    std::vector<std::string> warningsAsErrorsList; // Specific warnings as errors
    
    // References
    std::vector<std::string> references;    // -reference
    std::vector<std::string> packageReferences; // NuGet packages
    std::vector<std::string> projectReferences; // Project references
    
    // Preprocessor
    std::vector<std::string> defines;   // -define
    
    // Resources
    std::vector<std::string> embeddedResources;
    std::vector<std::string> linkedResources;
    
    // Output
    std::string outputDirectory;
    std::string assemblyName;
    std::string rootNamespace;
    
    // Code analysis
    bool enableAnalyzers = true;
    bool enforceCodeStyleInBuild = false;
    std::string analysisLevel = "latest"; // latest, preview, 5.0, 6.0, etc.
    
    // Runtime options
    std::vector<std::string> runtimeArgs;
    std::map<std::string, std::string> environmentVars;
    
    /**
     * @brief Get default configuration
     */
    static CSharpPluginConfig Default();
};

// ============================================================================
// C# ANALYSIS STRUCTURES
// ============================================================================

/**
 * @brief C# namespace info
 */
struct CSharpNamespaceInfo {
    std::string name;
    std::vector<std::string> types;
    std::vector<std::string> nestedNamespaces;
};

/**
 * @brief C# type info
 */
struct CSharpTypeInfo {
    std::string name;
    std::string fullName;
    std::string namespaceName;
    std::string sourceFile;
    int startLine = 0;
    int endLine = 0;
    
    bool isPublic = false;
    bool isInternal = false;
    bool isPrivate = false;
    bool isProtected = false;
    
    bool isClass = false;
    bool isStruct = false;
    bool isInterface = false;
    bool isEnum = false;
    bool isDelegate = false;
    bool isRecord = false;           // C# 9+
    bool isRecordStruct = false;     // C# 10+
    
    bool isAbstract = false;
    bool isSealed = false;
    bool isStatic = false;
    bool isPartial = false;
    
    std::string baseType;
    std::vector<std::string> interfaces;
    std::vector<std::string> genericParameters;
};

/**
 * @brief C# method info
 */
struct CSharpMethodInfo {
    std::string name;
    std::string returnType;
    std::vector<std::string> parameters;
    std::string typeName;
    int startLine = 0;
    int endLine = 0;
    
    bool isPublic = false;
    bool isPrivate = false;
    bool isProtected = false;
    bool isInternal = false;
    
    bool isStatic = false;
    bool isVirtual = false;
    bool isOverride = false;
    bool isAbstract = false;
    bool isSealed = false;
    bool isAsync = false;
    bool isExtern = false;
    bool isPartial = false;
    
    std::vector<std::string> genericParameters;
    std::vector<std::string> attributes;
};

/**
 * @brief C# property info
 */
struct CSharpPropertyInfo {
    std::string name;
    std::string type;
    std::string typeName;
    int line = 0;
    
    bool hasGetter = false;
    bool hasSetter = false;
    bool hasInit = false;            // C# 9+
    bool isRequired = false;         // C# 11+
    bool isAutoProperty = false;
    
    bool isPublic = false;
    bool isPrivate = false;
    bool isStatic = false;
    bool isVirtual = false;
};

/**
 * @brief C# project info (.csproj)
 */
struct CSharpProjectInfo {
    std::string projectFile;
    std::string projectName;
    std::string assemblyName;
    std::string rootNamespace;
    
    TargetFramework targetFramework = TargetFramework::Unknown;
    std::vector<TargetFramework> targetFrameworks; // Multi-targeting
    DotNetOutputType outputType = DotNetOutputType::Library;
    CSharpVersion langVersion = CSharpVersion::Unknown;
    
    bool nullable = false;
    bool implicitUsings = false;
    
    std::vector<std::string> sourceFiles;
    std::vector<std::string> packageReferences;
    std::vector<std::string> projectReferences;
    std::vector<std::string> references;
};

/**
 * @brief NuGet package info
 */
struct NuGetPackageInfo {
    std::string id;
    std::string version;
    std::string description;
    std::string authors;
    std::string license;
    std::vector<std::string> dependencies;
    bool isInstalled = false;
};

/**
 * @brief Solution info (.sln)
 */
struct SolutionInfo {
    std::string solutionFile;
    std::string solutionName;
    std::vector<std::string> projects;
    std::vector<std::string> configurations; // Debug, Release
    std::vector<std::string> platforms;      // Any CPU, x64, x86
};

// ============================================================================
// C# COMPILER PLUGIN
// ============================================================================

/**
 * @brief C#/.NET Compiler Plugin Implementation
 * 
 * Supports:
 * - .NET SDK (5, 6, 7, 8, 9)
 * - .NET Core (2.x, 3.x)
 * - .NET Framework (4.x, Windows)
 * - Mono (cross-platform)
 * - Roslyn compiler (csc)
 * - MSBuild
 * - NuGet package management
 * - Solution/Project handling
 * 
 * Error Format Parsed:
 *   Program.cs(10,5): error CS1002: ; expected
 */
class UCCSharpPlugin : public IUCCompilerPlugin {
public:
    UCCSharpPlugin();
    explicit UCCSharpPlugin(const CSharpPluginConfig& config);
    ~UCCSharpPlugin() override;
    
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
    
    // ===== C#-SPECIFIC METHODS =====
    
    const CSharpPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const CSharpPluginConfig& config);
    
    /**
     * @brief Detect .NET installation
     */
    bool DetectInstallation();
    
    /**
     * @brief Get detected .NET type
     */
    DotNetType GetDotNetType() const { return detectedDotNetType; }
    
    /**
     * @brief Get detected .NET version
     */
    DotNetVersion GetDotNetVersion() const { return detectedVersion; }
    
    /**
     * @brief List installed SDKs
     */
    std::vector<std::string> ListInstalledSDKs();
    
    /**
     * @brief List installed runtimes
     */
    std::vector<std::string> ListInstalledRuntimes();
    
    // ===== DOTNET CLI OPERATIONS =====
    
    /**
     * @brief Create new project
     */
    bool CreateProject(const std::string& projectDir,
                       const std::string& templateName,
                       const std::string& projectName = "",
                       std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Create new solution
     */
    bool CreateSolution(const std::string& solutionDir,
                        const std::string& solutionName = "",
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Build project/solution
     */
    BuildResult Build(const std::string& projectOrSolution,
                      const std::string& configuration = "Debug",
                      std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Clean project/solution
     */
    bool Clean(const std::string& projectOrSolution,
               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Restore NuGet packages
     */
    bool Restore(const std::string& projectOrSolution,
                 std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Publish project
     */
    BuildResult Publish(const std::string& project,
                        const std::string& outputDir = "",
                        const std::string& configuration = "Release",
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run project
     */
    BuildResult Run(const std::string& projectOrDll,
                    const std::vector<std::string>& args = {},
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run tests
     */
    BuildResult Test(const std::string& project,
                     const std::string& filter = "",
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Pack as NuGet package
     */
    bool Pack(const std::string& project,
              const std::string& outputDir = "",
              std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== NUGET OPERATIONS =====
    
    /**
     * @brief Add NuGet package
     */
    bool AddPackage(const std::string& project,
                    const std::string& packageId,
                    const std::string& version = "",
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Remove NuGet package
     */
    bool RemovePackage(const std::string& project,
                       const std::string& packageId,
                       std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief List installed packages
     */
    std::vector<NuGetPackageInfo> ListPackages(const std::string& project);
    
    /**
     * @brief Search NuGet packages
     */
    std::vector<NuGetPackageInfo> SearchPackages(const std::string& query, int take = 10);
    
    /**
     * @brief Update packages
     */
    bool UpdatePackages(const std::string& project,
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== PROJECT/SOLUTION MANAGEMENT =====
    
    /**
     * @brief Add project to solution
     */
    bool AddProjectToSolution(const std::string& solution,
                              const std::string& project,
                              std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Remove project from solution
     */
    bool RemoveProjectFromSolution(const std::string& solution,
                                   const std::string& project,
                                   std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Add project reference
     */
    bool AddProjectReference(const std::string& project,
                             const std::string& referenceProject,
                             std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Parse .csproj file
     */
    CSharpProjectInfo ParseProjectFile(const std::string& csprojFile);
    
    /**
     * @brief Parse .sln file
     */
    SolutionInfo ParseSolutionFile(const std::string& slnFile);
    
    // ===== SOURCE ANALYSIS =====
    
    /**
     * @brief Parse type info from source
     */
    std::vector<CSharpTypeInfo> ParseTypes(const std::string& sourceFile);
    
    /**
     * @brief Get methods in a type
     */
    std::vector<CSharpMethodInfo> GetMethods(const std::string& sourceFile);
    
    /**
     * @brief Get properties in a type
     */
    std::vector<CSharpPropertyInfo> GetProperties(const std::string& sourceFile);
    
    /**
     * @brief Get using directives
     */
    std::vector<std::string> GetUsings(const std::string& sourceFile);
    
    /**
     * @brief Get namespace from file
     */
    std::string GetNamespace(const std::string& sourceFile);
    
    // ===== ROSLYN DIRECT COMPILATION =====
    
    /**
     * @brief Compile using csc directly
     */
    BuildResult CompileWithRoslyn(const std::vector<std::string>& sourceFiles,
                                  const std::string& outputFile,
                                  std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Compile single file (scripting)
     */
    BuildResult CompileScript(const std::string& scriptFile,
                              std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== TOOL MANAGEMENT =====
    
    /**
     * @brief Install global tool
     */
    bool InstallTool(const std::string& toolId,
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief List installed tools
     */
    std::vector<std::string> ListTools();
    
    /**
     * @brief Run dotnet tool
     */
    BuildResult RunTool(const std::string& toolName,
                        const std::vector<std::string>& args,
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== CODE FORMATTING =====
    
    /**
     * @brief Format code (dotnet format)
     */
    bool FormatCode(const std::string& projectOrSolution,
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Verify formatting
     */
    BuildResult VerifyFormat(const std::string& projectOrSolution,
                             std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== CALLBACKS =====
    
    std::function<void(const std::string&)> onBuildOutput;
    std::function<void(const std::string&)> onRuntimeOutput;
    std::function<void(const NuGetPackageInfo&)> onPackageRestored;

private:
    std::string GetDotNetExecutable() const;
    std::string GetCscExecutable() const;
    std::string GetMonoExecutable() const;
    
    bool DetectDotNetSDK();
    bool DetectMono();
    DotNetVersion ParseDotNetVersion(const std::string& versionStr);
    
    std::string TargetFrameworkToString(TargetFramework tf) const;
    std::string CSharpVersionToString(CSharpVersion ver) const;
    std::string OutputTypeToString(DotNetOutputType type) const;
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output, std::string& error, int& exitCode,
                        const std::string& workingDir = "");
    
    CSharpPluginConfig pluginConfig;
    
    DotNetType detectedDotNetType = DotNetType::Unknown;
    DotNetVersion detectedVersion = DotNetVersion::Unknown;
    std::string detectedVersionString;
    std::vector<std::string> installedSDKs;
    std::vector<std::string> installedRuntimes;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    std::thread buildThread;
    void* currentProcess = nullptr;
    
    bool installationDetected = false;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

inline std::string DotNetTypeToString(DotNetType type) {
    switch (type) {
        case DotNetType::DotNetSDK:       return ".NET SDK";
        case DotNetType::DotNetCore:      return ".NET Core";
        case DotNetType::DotNetFramework: return ".NET Framework";
        case DotNetType::Mono:            return "Mono";
        default: return "Unknown";
    }
}

inline std::string DotNetVersionToString(DotNetVersion ver) {
    switch (ver) {
        case DotNetVersion::Net48:     return "4.8";
        case DotNetVersion::NetCore31: return "3.1";
        case DotNetVersion::Net50:     return "5.0";
        case DotNetVersion::Net60:     return "6.0";
        case DotNetVersion::Net70:     return "7.0";
        case DotNetVersion::Net80:     return "8.0";
        case DotNetVersion::Net90:     return "9.0";
        default: return "unknown";
    }
}

} // namespace IDE
} // namespace UltraCanvas
