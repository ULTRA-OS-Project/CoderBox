// Apps/IDE/Build/Plugins/UCGoPlugin.h
// Go Compiler Plugin for ULTRA IDE
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
// GO VERSION / BUILD MODE
// ============================================================================

/**
 * @brief Go build mode
 */
enum class GoBuildMode {
    Default,            // Standard executable
    Archive,            // Build archive (.a)
    CArchive,           // C archive for cgo
    CShared,            // C shared library
    Shared,             // Go shared library
    Exe,                // Executable (explicit)
    Pie,                // Position-independent executable
    Plugin              // Go plugin (.so)
};

/**
 * @brief Go module mode
 */
enum class GoModuleMode {
    Auto,               // Automatic (GO111MODULE=auto)
    On,                 // Module-aware mode (GO111MODULE=on)
    Off                 // GOPATH mode (GO111MODULE=off)
};

/**
 * @brief Go OS targets
 */
enum class GoOS {
    Linux, Windows, Darwin, FreeBSD, OpenBSD, NetBSD,
    Android, iOS, JS, WASI, Plan9, AIX, Solaris, Illumos, Dragonfly
};

/**
 * @brief Go architecture targets
 */
enum class GoArch {
    AMD64, ARM64, ARM, I386, MIPS, MIPS64, MIPSLE, MIPS64LE,
    PPC64, PPC64LE, RISCV64, S390X, WASM, LOONG64
};

// ============================================================================
// GO PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief Go-specific configuration options
 */
struct GoPluginConfig {
    // Paths
    std::string goRoot;                 // GOROOT
    std::string goPath;                 // GOPATH
    std::string goBin;                  // Path to go binary
    std::string goCache;                // GOCACHE
    std::string goModCache;             // GOMODCACHE
    
    // Module settings
    GoModuleMode moduleMode = GoModuleMode::On;
    std::string goProxy = "https://proxy.golang.org,direct";
    std::string goPrivate;              // GOPRIVATE
    std::string goNoProxy;              // GONOPROXY
    std::string goSumDB = "sum.golang.org";
    
    // Build settings
    GoBuildMode buildMode = GoBuildMode::Default;
    bool race = false;                  // -race (race detector)
    bool msan = false;                  // -msan (memory sanitizer)
    bool asan = false;                  // -asan (address sanitizer)
    bool cover = false;                 // -cover (coverage)
    std::string coverMode = "set";      // set, count, atomic
    
    // Cross-compilation
    GoOS targetOS = GoOS::Linux;
    GoArch targetArch = GoArch::AMD64;
    bool crossCompile = false;
    
    // CGO
    bool cgoEnabled = true;             // CGO_ENABLED
    std::string cc;                     // C compiler for cgo
    std::string cxx;                    // C++ compiler for cgo
    std::vector<std::string> cflags;    // CGO_CFLAGS
    std::vector<std::string> cxxflags;  // CGO_CXXFLAGS
    std::vector<std::string> ldflags;   // CGO_LDFLAGS
    
    // Linker flags
    std::vector<std::string> gcflags;   // -gcflags
    std::vector<std::string> ldFlags;   // -ldflags
    std::vector<std::string> asmflags;  // -asmflags
    bool trimpath = false;              // -trimpath
    
    // Build tags
    std::vector<std::string> tags;      // -tags
    
    // Output
    std::string outputPath;             // -o
    bool verbose = false;               // -v
    bool printCommands = false;         // -x
    bool work = false;                  // -work (keep temp dir)
    
    // Optimization
    bool disableInlining = false;       // -gcflags=-l
    bool disableOptimizations = false;  // -gcflags=-N
    
    // Testing
    int testTimeout = 600;              // Test timeout in seconds
    bool testVerbose = false;           // -v for tests
    bool testShort = false;             // -short
    int testCount = 1;                  // -count
    std::string testRun;                // -run pattern
    std::string testBench;              // -bench pattern
    bool testCover = false;             // -cover
    std::string testCoverProfile;       // -coverprofile
    
    /**
     * @brief Get default configuration
     */
    static GoPluginConfig Default();
};

// ============================================================================
// GO ANALYSIS STRUCTURES
// ============================================================================

/**
 * @brief Go module info (go.mod)
 */
struct GoModuleInfo {
    std::string modulePath;             // Module path
    std::string goVersion;              // Go version required
    std::vector<std::pair<std::string, std::string>> require;  // Dependencies
    std::vector<std::string> replace;   // Replace directives
    std::vector<std::string> exclude;   // Exclude directives
    std::vector<std::string> retract;   // Retract directives
};

/**
 * @brief Go package info
 */
struct GoPackageInfo {
    std::string name;                   // Package name
    std::string importPath;             // Import path
    std::string dir;                    // Directory
    std::vector<std::string> goFiles;   // .go files
    std::vector<std::string> cgoFiles;  // Cgo files
    std::vector<std::string> testFiles; // _test.go files
    std::vector<std::string> imports;   // Imported packages
    std::vector<std::string> deps;      // All dependencies
    bool isStandard = false;            // Standard library
    bool isCommand = false;             // main package
};

/**
 * @brief Go function info
 */
struct GoFunctionInfo {
    std::string name;
    std::string file;
    int startLine = 0;
    int endLine = 0;
    std::string receiver;               // Method receiver (if any)
    std::vector<std::string> params;
    std::vector<std::string> results;
    bool isExported = false;
    bool isMethod = false;
    bool isTest = false;
    bool isBenchmark = false;
    bool isExample = false;
};

/**
 * @brief Go type info
 */
struct GoTypeInfo {
    std::string name;
    std::string file;
    int line = 0;
    std::string kind;                   // struct, interface, alias, etc.
    bool isExported = false;
    std::vector<std::string> methods;
    std::vector<std::string> fields;    // For structs
    std::vector<std::string> embeds;    // Embedded types
};

/**
 * @brief Go test result
 */
struct GoTestResult {
    std::string name;
    std::string package;
    bool passed = false;
    float duration = 0;                 // seconds
    std::string output;
    std::string failureMessage;
};

/**
 * @brief Go benchmark result
 */
struct GoBenchmarkResult {
    std::string name;
    int iterations = 0;
    float nsPerOp = 0;
    float bytesPerOp = 0;
    float allocsPerOp = 0;
};

/**
 * @brief Go coverage info
 */
struct GoCoverageInfo {
    std::string package;
    float coveragePercent = 0;
    int coveredStatements = 0;
    int totalStatements = 0;
    std::map<std::string, float> fileCoverage;
};

/**
 * @brief Go dependency info
 */
struct GoDependencyInfo {
    std::string path;
    std::string version;
    std::string sum;                    // go.sum hash
    bool indirect = false;
    bool isUpdate = false;              // Update available
    std::string latestVersion;
};

// ============================================================================
// GO COMPILER PLUGIN
// ============================================================================

/**
 * @brief Go Compiler Plugin Implementation
 * 
 * Supports:
 * - Go 1.x (1.18+)
 * - Module system (go mod)
 * - Cross-compilation
 * - CGO
 * - Race detector
 * - Testing and benchmarks
 * - Code coverage
 * - Package management
 * - Code formatting (gofmt, goimports)
 * - Linting (go vet, staticcheck)
 * - Documentation (godoc)
 * 
 * Error Format Parsed:
 *   main.go:10:5: undefined: foo
 *   # package/name
 *   ./file.go:10: error message
 */
class UCGoPlugin : public IUCCompilerPlugin {
public:
    UCGoPlugin();
    explicit UCGoPlugin(const GoPluginConfig& config);
    ~UCGoPlugin() override;
    
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
    
    // ===== GO-SPECIFIC METHODS =====
    
    const GoPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const GoPluginConfig& config);
    
    bool DetectInstallation();
    std::string GetGoRoot() const { return detectedGoRoot; }
    std::string GetGoPath() const { return detectedGoPath; }
    
    // ===== GO BUILD COMMANDS =====
    
    /**
     * @brief Build packages
     */
    BuildResult Build(const std::string& packagePath = "./...",
                      std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Build and install
     */
    BuildResult Install(const std::string& packagePath,
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run package
     */
    BuildResult Run(const std::string& packageOrFile,
                    const std::vector<std::string>& args = {},
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Clean build cache
     */
    bool Clean(const std::string& packagePath = "",
               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Generate output (go generate)
     */
    BuildResult Generate(const std::string& packagePath = "./...",
                         std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== MODULE COMMANDS =====
    
    /**
     * @brief Initialize new module
     */
    bool ModInit(const std::string& modulePath,
                 std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Tidy module (add missing, remove unused)
     */
    bool ModTidy(std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Download dependencies
     */
    bool ModDownload(const std::string& module = "",
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Verify dependencies
     */
    bool ModVerify(std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Get dependency
     */
    bool Get(const std::string& packagePath,
             std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Update dependency
     */
    bool GetUpdate(const std::string& packagePath = "./...",
                   std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Parse go.mod
     */
    GoModuleInfo ParseGoMod(const std::string& goModPath = "go.mod");
    
    /**
     * @brief List dependencies
     */
    std::vector<GoDependencyInfo> ListDependencies(bool direct = true);
    
    /**
     * @brief Check for updates
     */
    std::vector<GoDependencyInfo> CheckUpdates();
    
    // ===== TESTING =====
    
    /**
     * @brief Run tests
     */
    BuildResult Test(const std::string& packagePath = "./...",
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run specific test
     */
    BuildResult RunTest(const std::string& testName,
                        const std::string& packagePath = "./...",
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run benchmarks
     */
    BuildResult Benchmark(const std::string& benchPattern = ".",
                          const std::string& packagePath = "./...",
                          std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run with coverage
     */
    BuildResult TestWithCoverage(const std::string& packagePath = "./...",
                                 const std::string& coverProfile = "coverage.out",
                                 std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Parse test results
     */
    std::vector<GoTestResult> ParseTestResults(const std::string& output);
    
    /**
     * @brief Parse benchmark results
     */
    std::vector<GoBenchmarkResult> ParseBenchmarkResults(const std::string& output);
    
    /**
     * @brief Parse coverage
     */
    GoCoverageInfo ParseCoverage(const std::string& coverProfile);
    
    /**
     * @brief Generate HTML coverage report
     */
    bool GenerateCoverageHTML(const std::string& coverProfile,
                              const std::string& outputHTML,
                              std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== CODE QUALITY =====
    
    /**
     * @brief Format code (gofmt)
     */
    bool Format(const std::string& path = ".",
                bool write = true,
                std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Format with imports (goimports)
     */
    bool FormatImports(const std::string& path = ".",
                       bool write = true,
                       std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run go vet
     */
    BuildResult Vet(const std::string& packagePath = "./...",
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run staticcheck (if installed)
     */
    BuildResult Staticcheck(const std::string& packagePath = "./...",
                            std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run golint (if installed)
     */
    BuildResult Lint(const std::string& packagePath = "./...",
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== PACKAGE ANALYSIS =====
    
    /**
     * @brief List packages
     */
    std::vector<GoPackageInfo> ListPackages(const std::string& pattern = "./...");
    
    /**
     * @brief Get package info
     */
    GoPackageInfo GetPackageInfo(const std::string& packagePath);
    
    /**
     * @brief List imports
     */
    std::vector<std::string> ListImports(const std::string& packagePath);
    
    // ===== SOURCE ANALYSIS =====
    
    /**
     * @brief Parse functions from file
     */
    std::vector<GoFunctionInfo> ParseFunctions(const std::string& file);
    
    /**
     * @brief Parse types from file
     */
    std::vector<GoTypeInfo> ParseTypes(const std::string& file);
    
    /**
     * @brief Get package name from file
     */
    std::string GetPackageName(const std::string& file);
    
    /**
     * @brief Get imports from file
     */
    std::vector<std::string> GetFileImports(const std::string& file);
    
    // ===== DOCUMENTATION =====
    
    /**
     * @brief Start godoc server
     */
    bool StartDocServer(int port = 6060);
    
    /**
     * @brief Get package documentation
     */
    std::string GetDoc(const std::string& packagePath);
    
    // ===== TOOL MANAGEMENT =====
    
    /**
     * @brief Install Go tool
     */
    bool InstallTool(const std::string& toolPath,
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief List environment
     */
    std::map<std::string, std::string> GetEnv();
    
    /**
     * @brief Get specific env var
     */
    std::string GetEnvVar(const std::string& name);
    
    // ===== CROSS COMPILATION =====
    
    /**
     * @brief Build for specific target
     */
    BuildResult BuildForTarget(const std::string& packagePath,
                               GoOS targetOS,
                               GoArch targetArch,
                               const std::string& outputPath = "",
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Get supported targets
     */
    std::vector<std::pair<GoOS, GoArch>> GetSupportedTargets();
    
    // ===== CALLBACKS =====
    
    std::function<void(const std::string&)> onBuildOutput;
    std::function<void(const GoTestResult&)> onTestComplete;
    std::function<void(const GoBenchmarkResult&)> onBenchmarkComplete;

private:
    std::string GetGoExecutable() const;
    std::string GoOSToString(GoOS os) const;
    std::string GoArchToString(GoArch arch) const;
    std::string BuildModeToString(GoBuildMode mode) const;
    
    std::vector<std::string> GetBuildEnv() const;
    std::vector<std::string> GetBuildFlags() const;
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output, std::string& error, int& exitCode,
                        const std::string& workingDir = "",
                        const std::vector<std::string>& env = {});
    
    GoPluginConfig pluginConfig;
    
    std::string detectedVersion;
    std::string detectedGoRoot;
    std::string detectedGoPath;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    std::thread buildThread;
    void* currentProcess = nullptr;
    
    bool installationDetected = false;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

inline std::string GoOSToString(GoOS os) {
    switch (os) {
        case GoOS::Linux: return "linux";
        case GoOS::Windows: return "windows";
        case GoOS::Darwin: return "darwin";
        case GoOS::FreeBSD: return "freebsd";
        case GoOS::Android: return "android";
        case GoOS::iOS: return "ios";
        case GoOS::JS: return "js";
        case GoOS::WASI: return "wasip1";
        default: return "linux";
    }
}

inline std::string GoArchToString(GoArch arch) {
    switch (arch) {
        case GoArch::AMD64: return "amd64";
        case GoArch::ARM64: return "arm64";
        case GoArch::ARM: return "arm";
        case GoArch::I386: return "386";
        case GoArch::WASM: return "wasm";
        case GoArch::RISCV64: return "riscv64";
        case GoArch::PPC64LE: return "ppc64le";
        default: return "amd64";
    }
}

} // namespace IDE
} // namespace UltraCanvas
