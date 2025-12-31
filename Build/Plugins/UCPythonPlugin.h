// Apps/IDE/Build/Plugins/UCPythonPlugin.h
// Python Interpreter Plugin for ULTRA IDE
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
// PYTHON PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief Python environment type
 */
enum class PythonEnvironmentType {
    System,             // System Python installation
    Venv,               // Python venv
    Virtualenv,         // virtualenv
    Conda,              // Conda environment
    Pyenv,              // pyenv
    Custom              // Custom path
};

/**
 * @brief Python-specific configuration options
 */
struct PythonPluginConfig {
    // Interpreter paths
    std::string pythonPath;             // Path to python executable
    std::string pipPath;                // Path to pip executable
    std::string python3Path;            // Path to python3 (if different)
    
    // Environment
    PythonEnvironmentType envType = PythonEnvironmentType::System;
    std::string virtualEnvPath;         // Path to virtual environment
    std::string condaEnvName;           // Conda environment name
    
    // Runtime options
    bool unbufferedOutput = true;       // -u flag (unbuffered stdout/stderr)
    bool optimized = false;             // -O flag (basic optimizations)
    bool optimizedMore = false;         // -OO flag (remove docstrings too)
    bool ignoreEnvironment = false;     // -E flag (ignore PYTHON* env vars)
    bool dontWriteBytecode = false;     // -B flag (don't write .pyc files)
    bool verboseImport = false;         // -v flag (verbose import)
    bool warningsAsErrors = false;      // -W error
    bool isolatedMode = false;          // -I flag (isolated mode)
    
    // Linting & Type Checking
    bool enablePylint = true;           // Run pylint for linting
    bool enableMypy = false;            // Run mypy for type checking
    bool enableFlake8 = false;          // Run flake8 for style checking
    bool enableBlack = false;           // Check formatting with black
    std::string pylintRcPath;           // Custom pylintrc path
    std::string mypyIniPath;            // Custom mypy.ini path
    
    // Testing
    bool enablePytest = true;           // Use pytest for testing
    bool enableUnittest = false;        // Use unittest for testing
    std::string testDirectory = "tests"; // Default test directory
    
    // Package management
    std::string requirementsFile = "requirements.txt";
    bool autoInstallDeps = false;       // Auto-install missing dependencies
    
    // Debug options
    int debugPort = 5678;               // Default debugpy port
    bool waitForDebugger = false;       // Wait for debugger to attach
    
    /**
     * @brief Get default configuration
     */
    static PythonPluginConfig Default();
};

// ============================================================================
// PYTHON PACKAGE INFO
// ============================================================================

/**
 * @brief Information about an installed Python package
 */
struct PythonPackageInfo {
    std::string name;
    std::string version;
    std::string location;
    std::string summary;
    std::vector<std::string> dependencies;
    bool isEditable = false;            // Installed with -e
    
    std::string ToString() const {
        return name + "==" + version;
    }
};

// ============================================================================
// PYTHON LINTING RESULT
// ============================================================================

/**
 * @brief Linting/type checking result
 */
struct PythonLintResult {
    std::string tool;                   // pylint, mypy, flake8, etc.
    std::vector<CompilerMessage> messages;
    int errorCount = 0;
    int warningCount = 0;
    int conventionCount = 0;
    int refactorCount = 0;
    float score = 0.0f;                 // pylint score (0-10)
    bool passed = false;
};

// ============================================================================
// PYTHON TEST RESULT
// ============================================================================

/**
 * @brief Test execution result
 */
struct PythonTestResult {
    std::string testName;
    std::string testFile;
    std::string testClass;
    std::string testMethod;
    
    enum class Status {
        Passed,
        Failed,
        Error,
        Skipped,
        ExpectedFailure,
        UnexpectedSuccess
    };
    
    Status status = Status::Passed;
    std::string errorMessage;
    std::string traceback;
    float duration = 0.0f;              // Seconds
    
    std::string GetStatusString() const {
        switch (status) {
            case Status::Passed: return "PASSED";
            case Status::Failed: return "FAILED";
            case Status::Error: return "ERROR";
            case Status::Skipped: return "SKIPPED";
            case Status::ExpectedFailure: return "XFAIL";
            case Status::UnexpectedSuccess: return "XPASS";
            default: return "UNKNOWN";
        }
    }
};

/**
 * @brief Overall test run summary
 */
struct PythonTestSummary {
    std::vector<PythonTestResult> tests;
    int passed = 0;
    int failed = 0;
    int errors = 0;
    int skipped = 0;
    float totalDuration = 0.0f;
    bool allPassed = false;
};

// ============================================================================
// PYTHON INTERPRETER PLUGIN
// ============================================================================

/**
 * @brief Python Interpreter Plugin Implementation
 * 
 * Supports:
 * - Python 3.x script execution
 * - Virtual environment management
 * - Package management (pip)
 * - Linting (pylint, flake8)
 * - Type checking (mypy)
 * - Testing (pytest, unittest)
 * - Debugging (debugpy)
 * 
 * Error Format Parsed:
 *   Traceback (most recent call last):
 *     File "filename.py", line N, in <module>
 *   ErrorType: message
 *   
 *   filename.py:N: error: message (mypy)
 *   filename.py:N:C: E001 message (flake8/pylint)
 */
class UCPythonPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Constructor
     */
    UCPythonPlugin();
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCPythonPlugin(const PythonPluginConfig& config);
    
    ~UCPythonPlugin() override;
    
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
    
    // ===== EXECUTION (Build = Run for Python) =====
    
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
    
    // ===== PYTHON-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const PythonPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const PythonPluginConfig& config);
    
    /**
     * @brief Detect Python installation
     */
    bool DetectInstallation();
    
    /**
     * @brief Get Python version info
     */
    struct PythonVersionInfo {
        int major = 0;
        int minor = 0;
        int patch = 0;
        std::string releaseLevel;       // alpha, beta, candidate, final
        std::string implementation;     // CPython, PyPy, etc.
        std::string platform;
        
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
    
    PythonVersionInfo GetVersionInfo();
    
    // ===== VIRTUAL ENVIRONMENT =====
    
    /**
     * @brief Create virtual environment
     */
    bool CreateVirtualEnv(const std::string& path, bool useSystemPackages = false);
    
    /**
     * @brief Activate virtual environment
     */
    bool ActivateVirtualEnv(const std::string& path);
    
    /**
     * @brief Deactivate current virtual environment
     */
    void DeactivateVirtualEnv();
    
    /**
     * @brief Check if virtual environment is active
     */
    bool IsVirtualEnvActive() const { return !activeVenvPath.empty(); }
    
    /**
     * @brief Get active virtual environment path
     */
    std::string GetActiveVirtualEnv() const { return activeVenvPath; }
    
    // ===== PACKAGE MANAGEMENT =====
    
    /**
     * @brief Install package
     */
    bool InstallPackage(const std::string& packageSpec,
                        std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Install packages from requirements file
     */
    bool InstallRequirements(const std::string& requirementsPath,
                             std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Uninstall package
     */
    bool UninstallPackage(const std::string& packageName,
                          std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief List installed packages
     */
    std::vector<PythonPackageInfo> ListPackages();
    
    /**
     * @brief Get package info
     */
    PythonPackageInfo GetPackageInfo(const std::string& packageName);
    
    /**
     * @brief Check if package is installed
     */
    bool IsPackageInstalled(const std::string& packageName);
    
    /**
     * @brief Freeze packages to requirements format
     */
    std::string FreezePackages();
    
    // ===== LINTING & TYPE CHECKING =====
    
    /**
     * @brief Run pylint on files
     */
    PythonLintResult RunPylint(const std::vector<std::string>& files,
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run mypy on files
     */
    PythonLintResult RunMypy(const std::vector<std::string>& files,
                             std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run flake8 on files
     */
    PythonLintResult RunFlake8(const std::vector<std::string>& files,
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Check formatting with black
     */
    PythonLintResult CheckBlackFormatting(const std::vector<std::string>& files,
                                          std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Format files with black
     */
    bool FormatWithBlack(const std::vector<std::string>& files,
                         std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== TESTING =====
    
    /**
     * @brief Run tests with pytest
     */
    PythonTestSummary RunPytest(const std::string& testPath = "",
                                const std::vector<std::string>& args = {},
                                std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run tests with unittest
     */
    PythonTestSummary RunUnittest(const std::string& testPath = "",
                                  std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Discover tests
     */
    std::vector<std::string> DiscoverTests(const std::string& testPath = "");
    
    // ===== DEBUGGING =====
    
    /**
     * @brief Start debug server
     */
    bool StartDebugServer(const std::string& scriptPath,
                          int port = 5678,
                          bool waitForClient = true);
    
    /**
     * @brief Stop debug server
     */
    void StopDebugServer();
    
    /**
     * @brief Check if debug server is running
     */
    bool IsDebugServerRunning() const { return debugServerRunning; }
    
    // ===== REPL =====
    
    /**
     * @brief Start interactive REPL
     */
    bool StartREPL(std::function<void(const std::string&)> onOutput,
                   std::function<void()> onReady);
    
    /**
     * @brief Send command to REPL
     */
    bool SendToREPL(const std::string& command);
    
    /**
     * @brief Stop REPL
     */
    void StopREPL();
    
    /**
     * @brief Check if REPL is running
     */
    bool IsREPLRunning() const { return replRunning; }
    
    // ===== CALLBACKS =====
    
    std::function<void(const PythonLintResult&)> onLintComplete;
    std::function<void(const PythonTestSummary&)> onTestComplete;
    std::function<void(const std::string&)> onPackageInstalled;
    std::function<void(const std::string&)> onREPLOutput;

private:
    // ===== INTERNAL METHODS =====
    
    std::string GetPythonExecutable() const;
    std::string GetPipExecutable() const;
    std::vector<std::string> GetBaseCommandLine() const;
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output,
                        std::string& error,
                        int& exitCode);
    
    bool ExecuteCommandAsync(const std::vector<std::string>& args,
                             std::function<void(const std::string&)> onOutputLine,
                             std::function<void(int)> onComplete);
    
    CompilerMessage ParsePythonTraceback(const std::string& traceback);
    CompilerMessage ParsePylintMessage(const std::string& line);
    CompilerMessage ParseMypyMessage(const std::string& line);
    CompilerMessage ParseFlake8Message(const std::string& line);
    
    PythonTestResult ParsePytestResult(const std::string& output);
    
    void DetectEnvironmentType();
    void UpdateEnvironmentPaths();
    
    // ===== STATE =====
    
    PythonPluginConfig pluginConfig;
    PythonVersionInfo versionInfo;
    
    std::string activeVenvPath;
    std::map<std::string, std::string> environmentVariables;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> debugServerRunning{false};
    std::atomic<bool> replRunning{false};
    
    std::thread buildThread;
    std::thread debugThread;
    std::thread replThread;
    
    // Process handles (platform-specific)
    void* currentProcess = nullptr;
    void* debugProcess = nullptr;
    void* replProcess = nullptr;
    
    // Cached data
    bool installationDetected = false;
    std::string cachedVersion;
    std::vector<PythonPackageInfo> cachedPackages;
    bool packagesCached = false;
    
    // Traceback parsing state
    bool inTraceback = false;
    std::string currentTraceback;
    std::string currentTracebackFile;
    int currentTracebackLine = 0;
};

} // namespace IDE
} // namespace UltraCanvas
