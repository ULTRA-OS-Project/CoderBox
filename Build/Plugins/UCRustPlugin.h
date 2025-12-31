// Apps/IDE/Build/Plugins/UCRustPlugin.h
// Rust/Cargo Compiler Plugin for ULTRA IDE
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
#include <set>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// RUST PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief Rust toolchain type
 */
enum class RustToolchain {
    Stable,
    Beta,
    Nightly,
    Custom
};

/**
 * @brief Rust edition
 */
enum class RustEdition {
    Edition2015,
    Edition2018,
    Edition2021,
    Edition2024
};

/**
 * @brief Cargo build profile
 */
enum class CargoBuildProfile {
    Dev,            // Development (debug)
    Release,        // Release optimized
    Test,           // Test profile
    Bench,          // Benchmark profile
    Custom          // Custom profile from Cargo.toml
};

/**
 * @brief Rust target triple
 */
struct RustTarget {
    std::string triple;         // e.g., "x86_64-unknown-linux-gnu"
    std::string arch;           // e.g., "x86_64"
    std::string vendor;         // e.g., "unknown"
    std::string os;             // e.g., "linux"
    std::string env;            // e.g., "gnu"
    bool isInstalled = false;
    
    static RustTarget Parse(const std::string& triple);
    std::string ToString() const { return triple; }
};

/**
 * @brief Rust-specific configuration options
 */
struct RustPluginConfig {
    // Toolchain paths
    std::string rustcPath;              // Path to rustc
    std::string cargoPath;              // Path to cargo
    std::string rustupPath;             // Path to rustup
    std::string rustfmtPath;            // Path to rustfmt
    std::string clippyDriverPath;       // Path to clippy-driver
    
    // Toolchain selection
    RustToolchain toolchain = RustToolchain::Stable;
    std::string customToolchainName;    // For custom toolchain
    
    // Default edition
    RustEdition defaultEdition = RustEdition::Edition2021;
    
    // Build options
    CargoBuildProfile defaultProfile = CargoBuildProfile::Dev;
    std::string targetTriple;           // Cross-compilation target
    int jobs = 0;                       // Parallel jobs (0 = auto)
    bool allFeatures = false;           // --all-features
    bool noDefaultFeatures = false;     // --no-default-features
    std::vector<std::string> features;  // --features
    
    // Output options
    bool colorOutput = true;            // --color=always
    bool verboseOutput = false;         // -v or -vv
    bool timings = false;               // --timings
    bool buildPlan = false;             // --build-plan (unstable)
    
    // Lint options
    bool enableClippy = true;           // Run clippy for linting
    std::string clippyConfig;           // clippy.toml path
    bool warningsAsErrors = false;      // -D warnings
    bool denyAllWarnings = false;       // --deny warnings
    
    // Test options
    bool testThreaded = true;           // Run tests in parallel
    int testThreads = 0;                // Number of test threads (0 = auto)
    bool nocapture = false;             // --nocapture (show stdout)
    bool showOutput = false;            // --show-output
    
    // Documentation
    bool documentPrivate = false;       // --document-private-items
    bool openDocs = false;              // --open after doc generation
    
    // Misc
    bool offline = false;               // --offline
    bool locked = false;                // --locked (Cargo.lock)
    bool frozen = false;                // --frozen
    
    /**
     * @brief Get default configuration
     */
    static RustPluginConfig Default();
};

// ============================================================================
// CARGO PACKAGE INFO
// ============================================================================

/**
 * @brief Cargo package metadata
 */
struct CargoPackageInfo {
    std::string name;
    std::string version;
    std::string authors;
    std::string description;
    std::string license;
    std::string repository;
    std::string homepage;
    std::string documentation;
    RustEdition edition = RustEdition::Edition2021;
    
    std::vector<std::string> keywords;
    std::vector<std::string> categories;
    
    // Dependencies
    struct Dependency {
        std::string name;
        std::string version;
        std::string source;         // crates.io, git, path
        bool optional = false;
        bool devDependency = false;
        bool buildDependency = false;
        std::vector<std::string> features;
    };
    std::vector<Dependency> dependencies;
    
    // Targets
    struct Target {
        std::string name;
        std::string kind;           // bin, lib, example, test, bench
        std::string crateType;      // lib, rlib, dylib, cdylib, staticlib, proc-macro
        std::string path;
        bool doctest = true;
    };
    std::vector<Target> targets;
    
    // Workspace info
    bool isWorkspace = false;
    std::vector<std::string> workspaceMembers;
};

// ============================================================================
// RUST LINT RESULT
// ============================================================================

/**
 * @brief Clippy/rustc lint result
 */
struct RustLintResult {
    std::string tool;                   // clippy, rustc
    std::vector<CompilerMessage> messages;
    int errorCount = 0;
    int warningCount = 0;
    int noteCount = 0;
    int helpCount = 0;
    bool passed = false;
    
    // Clippy categories
    int correctnessCount = 0;           // clippy::correctness
    int suspiciousCount = 0;            // clippy::suspicious  
    int styleCount = 0;                 // clippy::style
    int complexityCount = 0;            // clippy::complexity
    int perfCount = 0;                  // clippy::perf
    int pedanticCount = 0;              // clippy::pedantic
};

// ============================================================================
// RUST TEST RESULT
// ============================================================================

/**
 * @brief Test execution result
 */
struct RustTestResult {
    std::string name;
    std::string module;
    
    enum class Status {
        Passed,
        Failed,
        Ignored,
        Measured,       // Benchmark
        FilteredOut
    };
    
    Status status = Status::Passed;
    std::string errorMessage;
    std::string stdout;
    float duration = 0.0f;          // Seconds
    
    // For benchmarks
    int64_t nsPerIter = 0;
    int64_t deviation = 0;
    
    std::string GetStatusString() const {
        switch (status) {
            case Status::Passed: return "ok";
            case Status::Failed: return "FAILED";
            case Status::Ignored: return "ignored";
            case Status::Measured: return "bench";
            case Status::FilteredOut: return "filtered";
            default: return "unknown";
        }
    }
};

/**
 * @brief Overall test run summary
 */
struct RustTestSummary {
    std::vector<RustTestResult> tests;
    int passed = 0;
    int failed = 0;
    int ignored = 0;
    int measured = 0;
    int filteredOut = 0;
    float totalDuration = 0.0f;
    bool allPassed = false;
};

// ============================================================================
// CARGO BUILD ARTIFACT
// ============================================================================

/**
 * @brief Build artifact information
 */
struct CargoBuildArtifact {
    std::string packageName;
    std::string targetName;
    std::string targetKind;         // bin, lib, example, test, bench
    std::string crateType;          // bin, lib, rlib, dylib, cdylib, staticlib
    std::string filePath;
    std::vector<std::string> filenames;
    bool fresh = false;             // Was already built (cached)
};

// ============================================================================
// RUST COMPILER PLUGIN
// ============================================================================

/**
 * @brief Rust/Cargo Compiler Plugin Implementation
 * 
 * Supports:
 * - Rust compilation via rustc
 * - Cargo project management
 * - Cross-compilation
 * - Clippy linting
 * - rustfmt formatting
 * - cargo test/bench
 * - cargo doc
 * - Workspace support
 * 
 * Error Format Parsed:
 *   error[E0382]: borrow of moved value: `x`
 *    --> src/main.rs:10:5
 *     |
 *   9 |     let y = x;
 *     |             - value moved here
 *  10 |     println!("{}", x);
 *     |                    ^ value borrowed here after move
 */
class UCRustPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Constructor
     */
    UCRustPlugin();
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCRustPlugin(const RustPluginConfig& config);
    
    ~UCRustPlugin() override;
    
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
    
    // ===== RUST-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const RustPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const RustPluginConfig& config);
    
    /**
     * @brief Detect Rust installation
     */
    bool DetectInstallation();
    
    /**
     * @brief Get Rust version info
     */
    struct RustVersionInfo {
        int major = 0;
        int minor = 0;
        int patch = 0;
        std::string channel;        // stable, beta, nightly
        std::string commitHash;
        std::string commitDate;
        std::string hostTriple;
        std::string llvmVersion;
        
        std::string ToString() const {
            return std::to_string(major) + "." + 
                   std::to_string(minor) + "." + 
                   std::to_string(patch);
        }
        
        bool AtLeast(int maj, int min, int pat = 0) const {
            if (major > maj) return true;
            if (major < maj) return false;
            if (minor > min) return true;
            if (minor < min) return false;
            return patch >= pat;
        }
    };
    
    RustVersionInfo GetVersionInfo();
    
    // ===== TOOLCHAIN MANAGEMENT =====
    
    /**
     * @brief List installed toolchains
     */
    std::vector<std::string> ListToolchains();
    
    /**
     * @brief Get active toolchain
     */
    std::string GetActiveToolchain();
    
    /**
     * @brief Set active toolchain
     */
    bool SetActiveToolchain(const std::string& toolchain);
    
    /**
     * @brief Install toolchain
     */
    bool InstallToolchain(const std::string& toolchain,
                          std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Update toolchains
     */
    bool UpdateToolchains(std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== TARGET MANAGEMENT =====
    
    /**
     * @brief List installed targets
     */
    std::vector<RustTarget> ListInstalledTargets();
    
    /**
     * @brief List available targets
     */
    std::vector<RustTarget> ListAvailableTargets();
    
    /**
     * @brief Add target
     */
    bool AddTarget(const std::string& target,
                   std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Remove target
     */
    bool RemoveTarget(const std::string& target);
    
    // ===== CARGO OPERATIONS =====
    
    /**
     * @brief Initialize new Cargo project
     */
    bool CargoNew(const std::string& path, bool isLib = false,
                  std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Initialize Cargo in existing directory
     */
    bool CargoInit(const std::string& path, bool isLib = false,
                   std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Build project
     */
    BuildResult CargoBuild(const std::string& projectPath,
                           const BuildConfiguration& config,
                           std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Build and run
     */
    BuildResult CargoRun(const std::string& projectPath,
                         const std::vector<std::string>& args = {},
                         std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Check project (faster than build)
     */
    BuildResult CargoCheck(const std::string& projectPath,
                           std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Clean build artifacts
     */
    bool CargoClean(const std::string& projectPath,
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Update dependencies
     */
    bool CargoUpdate(const std::string& projectPath,
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Fetch dependencies
     */
    bool CargoFetch(const std::string& projectPath,
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Get package metadata
     */
    CargoPackageInfo CargoMetadata(const std::string& projectPath);
    
    /**
     * @brief Search crates.io
     */
    std::vector<CargoPackageInfo> CargoSearch(const std::string& query, int limit = 10);
    
    /**
     * @brief Add dependency
     */
    bool CargoAdd(const std::string& projectPath, const std::string& dependency,
                  bool isDev = false, bool isBuild = false,
                  std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Remove dependency
     */
    bool CargoRemove(const std::string& projectPath, const std::string& dependency,
                     std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== LINTING (CLIPPY) =====
    
    /**
     * @brief Run clippy
     */
    RustLintResult RunClippy(const std::string& projectPath,
                             std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Fix clippy warnings automatically
     */
    bool ClippyFix(const std::string& projectPath,
                   std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== FORMATTING =====
    
    /**
     * @brief Check formatting
     */
    RustLintResult CheckFormat(const std::string& projectPath,
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Format code
     */
    bool Format(const std::string& projectPath,
                std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Format single file
     */
    bool FormatFile(const std::string& filePath,
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== TESTING =====
    
    /**
     * @brief Run tests
     */
    RustTestSummary CargoTest(const std::string& projectPath,
                              const std::string& testFilter = "",
                              std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run specific test
     */
    RustTestSummary RunTest(const std::string& projectPath,
                            const std::string& testName,
                            std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run benchmarks
     */
    RustTestSummary CargoBench(const std::string& projectPath,
                               const std::string& benchFilter = "",
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief List tests
     */
    std::vector<std::string> ListTests(const std::string& projectPath);
    
    // ===== DOCUMENTATION =====
    
    /**
     * @brief Generate documentation
     */
    bool CargoDoc(const std::string& projectPath, bool openBrowser = false,
                  std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run doc tests
     */
    RustTestSummary CargoDoctest(const std::string& projectPath,
                                 std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== PUBLISHING =====
    
    /**
     * @brief Package for publishing
     */
    bool CargoPackage(const std::string& projectPath,
                      std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Publish to crates.io
     */
    bool CargoPublish(const std::string& projectPath, bool dryRun = false,
                      std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== EXPANSION & ANALYSIS =====
    
    /**
     * @brief Expand macros
     */
    std::string CargoExpand(const std::string& projectPath,
                            const std::string& itemPath = "");
    
    /**
     * @brief Show dependency tree
     */
    std::string CargoTree(const std::string& projectPath,
                          bool duplicates = false, bool inverted = false);
    
    /**
     * @brief Audit dependencies for vulnerabilities
     */
    struct AuditResult {
        int vulnerabilities = 0;
        int warnings = 0;
        std::vector<std::string> advisories;
        bool passed = false;
    };
    AuditResult CargoAudit(const std::string& projectPath,
                           std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== CALLBACKS =====
    
    std::function<void(const RustLintResult&)> onLintComplete;
    std::function<void(const RustTestSummary&)> onTestComplete;
    std::function<void(const CargoBuildArtifact&)> onArtifactBuilt;
    std::function<void(const std::string&)> onBuildMessage;

private:
    // ===== INTERNAL METHODS =====
    
    std::string GetRustcExecutable() const;
    std::string GetCargoExecutable() const;
    std::vector<std::string> GetCargoBaseArgs() const;
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output,
                        std::string& error,
                        int& exitCode,
                        const std::string& workingDir = "");
    
    bool ExecuteCommandAsync(const std::vector<std::string>& args,
                             std::function<void(const std::string&)> onOutputLine,
                             std::function<void(int)> onComplete,
                             const std::string& workingDir = "");
    
    CompilerMessage ParseRustcMessage(const std::string& jsonLine);
    CompilerMessage ParseCargoMessage(const std::string& jsonLine);
    RustTestResult ParseTestResult(const std::string& line);
    CargoBuildArtifact ParseArtifact(const std::string& jsonLine);
    
    void ParseJsonDiagnostic(const std::string& json, std::vector<CompilerMessage>& messages);
    
    // ===== STATE =====
    
    RustPluginConfig pluginConfig;
    RustVersionInfo versionInfo;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    
    std::thread buildThread;
    
    // Process handle (platform-specific)
    void* currentProcess = nullptr;
    
    // Cached data
    bool installationDetected = false;
    std::string cachedVersion;
    std::vector<std::string> cachedToolchains;
    std::vector<RustTarget> cachedTargets;
    
    // JSON parsing state
    std::string jsonBuffer;
    bool inMultilineJson = false;
    int jsonBraceCount = 0;
};

// ============================================================================
// RUST EDITION UTILITIES
// ============================================================================

inline std::string RustEditionToString(RustEdition edition) {
    switch (edition) {
        case RustEdition::Edition2015: return "2015";
        case RustEdition::Edition2018: return "2018";
        case RustEdition::Edition2021: return "2021";
        case RustEdition::Edition2024: return "2024";
        default: return "2021";
    }
}

inline RustEdition StringToRustEdition(const std::string& str) {
    if (str == "2015") return RustEdition::Edition2015;
    if (str == "2018") return RustEdition::Edition2018;
    if (str == "2021") return RustEdition::Edition2021;
    if (str == "2024") return RustEdition::Edition2024;
    return RustEdition::Edition2021;
}

inline std::string CargoBuildProfileToString(CargoBuildProfile profile) {
    switch (profile) {
        case CargoBuildProfile::Dev: return "dev";
        case CargoBuildProfile::Release: return "release";
        case CargoBuildProfile::Test: return "test";
        case CargoBuildProfile::Bench: return "bench";
        default: return "dev";
    }
}

} // namespace IDE
} // namespace UltraCanvas
