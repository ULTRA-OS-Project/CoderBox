// Apps/IDE/Build/Plugins/UCPythonPlugin.cpp
// Python Interpreter Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCPythonPlugin.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

PythonPluginConfig PythonPluginConfig::Default() {
    PythonPluginConfig config;
    
#ifdef _WIN32
    config.pythonPath = "python";
    config.pipPath = "pip";
    config.python3Path = "python";
#else
    config.pythonPath = "/usr/bin/python3";
    config.pipPath = "/usr/bin/pip3";
    config.python3Path = "/usr/bin/python3";
#endif
    
    config.envType = PythonEnvironmentType::System;
    config.unbufferedOutput = true;
    config.enablePylint = true;
    config.enablePytest = true;
    
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCPythonPlugin::UCPythonPlugin() 
    : pluginConfig(PythonPluginConfig::Default()) {
    DetectInstallation();
}

UCPythonPlugin::UCPythonPlugin(const PythonPluginConfig& config) 
    : pluginConfig(config) {
    DetectInstallation();
}

UCPythonPlugin::~UCPythonPlugin() {
    Cancel();
    StopDebugServer();
    StopREPL();
    
    if (buildThread.joinable()) {
        buildThread.join();
    }
    if (debugThread.joinable()) {
        debugThread.join();
    }
    if (replThread.joinable()) {
        replThread.join();
    }
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCPythonPlugin::GetPluginName() const {
    return "Python Interpreter Plugin";
}

std::string UCPythonPlugin::GetPluginVersion() const {
    return "1.0.0";
}

CompilerType UCPythonPlugin::GetCompilerType() const {
    return CompilerType::Python;
}

std::string UCPythonPlugin::GetCompilerName() const {
    if (!versionInfo.implementation.empty()) {
        return versionInfo.implementation + " " + versionInfo.ToString();
    }
    return "Python";
}

// ============================================================================
// INTERPRETER DETECTION
// ============================================================================

bool UCPythonPlugin::IsAvailable() {
    if (installationDetected) {
        return !cachedVersion.empty();
    }
    return DetectInstallation();
}

std::string UCPythonPlugin::GetCompilerPath() {
    return GetPythonExecutable();
}

void UCPythonPlugin::SetCompilerPath(const std::string& path) {
    pluginConfig.pythonPath = path;
    installationDetected = false;
    DetectInstallation();
}

std::string UCPythonPlugin::GetCompilerVersion() {
    if (!cachedVersion.empty()) {
        return cachedVersion;
    }
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand({GetPythonExecutable(), "--version"}, output, error, exitCode)) {
        // Python 3.x prints to stdout, Python 2.x prints to stderr
        std::string version = output.empty() ? error : output;
        
        // Parse "Python X.Y.Z"
        size_t pos = version.find("Python ");
        if (pos != std::string::npos) {
            cachedVersion = version.substr(pos + 7);
            // Trim whitespace
            cachedVersion.erase(cachedVersion.find_last_not_of(" \n\r\t") + 1);
        }
    }
    
    return cachedVersion;
}

std::vector<std::string> UCPythonPlugin::GetSupportedExtensions() const {
    return {"py", "pyw", "pyi"};
}

bool UCPythonPlugin::CanCompile(const std::string& filePath) const {
    std::string ext;
    size_t dotPos = filePath.rfind('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    
    auto supported = GetSupportedExtensions();
    return std::find(supported.begin(), supported.end(), ext) != supported.end();
}

bool UCPythonPlugin::DetectInstallation() {
    installationDetected = true;
    
    // Try configured path first
    std::string output, error;
    int exitCode;
    
    std::vector<std::string> pathsToTry = {
        pluginConfig.pythonPath,
        pluginConfig.python3Path,
#ifdef _WIN32
        "python",
        "python3",
        "C:\\Python312\\python.exe",
        "C:\\Python311\\python.exe",
        "C:\\Python310\\python.exe",
        "C:\\Python39\\python.exe",
#else
        "/usr/bin/python3",
        "/usr/bin/python",
        "/usr/local/bin/python3",
        "/usr/local/bin/python",
        "/opt/homebrew/bin/python3",
#endif
    };
    
    for (const auto& path : pathsToTry) {
        if (path.empty()) continue;
        
        if (ExecuteCommand({path, "--version"}, output, error, exitCode)) {
            if (exitCode == 0) {
                pluginConfig.pythonPath = path;
                GetCompilerVersion();
                GetVersionInfo();
                return true;
            }
        }
    }
    
    return false;
}

UCPythonPlugin::PythonVersionInfo UCPythonPlugin::GetVersionInfo() {
    if (versionInfo.major > 0) {
        return versionInfo;
    }
    
    std::string output, error;
    int exitCode;
    
    // Get detailed version info using Python itself
    std::string script = 
        "import sys; import platform; "
        "print(sys.version_info.major, sys.version_info.minor, sys.version_info.micro, "
        "sys.version_info.releaselevel, platform.python_implementation(), sys.platform)";
    
    if (ExecuteCommand({GetPythonExecutable(), "-c", script}, output, error, exitCode)) {
        std::istringstream iss(output);
        std::string relLevel, impl, plat;
        iss >> versionInfo.major >> versionInfo.minor >> versionInfo.patch 
            >> relLevel >> impl >> plat;
        versionInfo.releaseLevel = relLevel;
        versionInfo.implementation = impl;
        versionInfo.platform = plat;
    }
    
    return versionInfo;
}

// ============================================================================
// EXECUTION
// ============================================================================

void UCPythonPlugin::CompileAsync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config,
    std::function<void(const BuildResult&)> onComplete,
    std::function<void(const std::string&)> onOutputLine,
    std::function<void(float)> onProgress) {
    
    if (buildInProgress) {
        BuildResult result;
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({
            CompilerMessageType::Error,
            "",
            0, 0,
            "Another Python process is already running"
        });
        if (onComplete) onComplete(result);
        return;
    }
    
    buildInProgress = true;
    cancelRequested = false;
    
    buildThread = std::thread([this, sourceFiles, config, onComplete, onOutputLine, onProgress]() {
        BuildResult result = CompileSync(sourceFiles, config);
        buildInProgress = false;
        if (onComplete) onComplete(result);
    });
}

BuildResult UCPythonPlugin::CompileSync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config) {
    
    BuildResult result;
    result.success = false;
    
    if (sourceFiles.empty()) {
        result.messages.push_back({
            CompilerMessageType::Error,
            "",
            0, 0,
            "No Python files specified"
        });
        result.errorCount = 1;
        return result;
    }
    
    // Generate command line
    auto commandLine = GenerateCommandLine(sourceFiles, config);
    
    std::string output, error;
    int exitCode;
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Execute Python
    bool executed = ExecuteCommand(commandLine, output, error, exitCode);
    
    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration<float>(endTime - startTime).count();
    
    if (!executed) {
        result.messages.push_back({
            CompilerMessageType::FatalError,
            "",
            0, 0,
            "Failed to execute Python interpreter"
        });
        result.errorCount = 1;
        return result;
    }
    
    // Parse output
    std::istringstream outputStream(output + "\n" + error);
    std::string line;
    
    while (std::getline(outputStream, line)) {
        if (line.empty()) continue;
        
        CompilerMessage msg = ParseOutputLine(line);
        if (msg.type != CompilerMessageType::Info || !msg.message.empty()) {
            result.messages.push_back(msg);
            
            if (msg.type == CompilerMessageType::Error || 
                msg.type == CompilerMessageType::FatalError) {
                result.errorCount++;
            } else if (msg.type == CompilerMessageType::Warning) {
                result.warningCount++;
            }
        }
    }
    
    result.success = (exitCode == 0);
    result.outputPath = sourceFiles[0];  // Main script
    
    return result;
}

void UCPythonPlugin::Cancel() {
    cancelRequested = true;
    
    // Kill current process if running
    if (currentProcess) {
#ifdef _WIN32
        TerminateProcess(currentProcess, 1);
#else
        kill(reinterpret_cast<pid_t>(reinterpret_cast<intptr_t>(currentProcess)), SIGTERM);
#endif
        currentProcess = nullptr;
    }
}

bool UCPythonPlugin::IsBuildInProgress() const {
    return buildInProgress;
}

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCPythonPlugin::ParseOutputLine(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    msg.message = line;
    msg.line = 0;
    msg.column = 0;
    
    // Check for traceback start
    if (line.find("Traceback (most recent call last):") != std::string::npos) {
        inTraceback = true;
        currentTraceback = line + "\n";
        return msg;
    }
    
    // Accumulate traceback
    if (inTraceback) {
        currentTraceback += line + "\n";
        
        // Parse file/line from traceback
        // Format: File "filename.py", line N, in <module>
        std::regex fileLineRegex(R"(File "([^"]+)", line (\d+))");
        std::smatch match;
        if (std::regex_search(line, match, fileLineRegex)) {
            currentTracebackFile = match[1].str();
            currentTracebackLine = std::stoi(match[2].str());
        }
        
        // Check for error line (ErrorType: message)
        std::regex errorRegex(R"(^(\w+Error|\w+Exception): (.+)$)");
        if (std::regex_search(line, match, errorRegex)) {
            inTraceback = false;
            
            msg.type = CompilerMessageType::Error;
            msg.file = currentTracebackFile;
            msg.line = currentTracebackLine;
            msg.message = match[1].str() + ": " + match[2].str();
            msg.rawOutput = currentTraceback;
            
            currentTraceback.clear();
            currentTracebackFile.clear();
            currentTracebackLine = 0;
            
            return msg;
        }
        
        return msg;
    }
    
    // Parse SyntaxError format
    // File "filename.py", line N
    //     code
    //     ^
    // SyntaxError: message
    std::regex syntaxErrorRegex(R"(^SyntaxError: (.+)$)");
    std::smatch match;
    if (std::regex_search(line, match, syntaxErrorRegex)) {
        msg.type = CompilerMessageType::Error;
        msg.message = "SyntaxError: " + match[1].str();
        msg.file = currentTracebackFile;
        msg.line = currentTracebackLine;
        return msg;
    }
    
    // Parse warning format
    // filename.py:N: WarningType: message
    std::regex warningRegex(R"(^(.+):(\d+): (\w+Warning): (.+)$)");
    if (std::regex_search(line, match, warningRegex)) {
        msg.type = CompilerMessageType::Warning;
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.message = match[3].str() + ": " + match[4].str();
        return msg;
    }
    
    // Parse DeprecationWarning etc from stderr
    std::regex deprecationRegex(R"(^(.+):(\d+): (\w+): (.+)$)");
    if (std::regex_search(line, match, deprecationRegex)) {
        std::string warningType = match[3].str();
        if (warningType.find("Warning") != std::string::npos || 
            warningType.find("Deprecat") != std::string::npos) {
            msg.type = CompilerMessageType::Warning;
            msg.file = match[1].str();
            msg.line = std::stoi(match[2].str());
            msg.message = warningType + ": " + match[4].str();
            return msg;
        }
    }
    
    return msg;
}

CompilerMessage UCPythonPlugin::ParsePylintMessage(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    
    // Pylint format: filename.py:line:column: code: message
    std::regex pylintRegex(R"(^(.+):(\d+):(\d+): ([CRWEF]\d+): (.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, pylintRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = std::stoi(match[3].str());
        std::string code = match[4].str();
        msg.message = "[" + code + "] " + match[5].str();
        
        // Determine severity by code prefix
        char severity = code[0];
        switch (severity) {
            case 'E': // Error
            case 'F': // Fatal
                msg.type = CompilerMessageType::Error;
                break;
            case 'W': // Warning
                msg.type = CompilerMessageType::Warning;
                break;
            case 'C': // Convention
            case 'R': // Refactor
            default:
                msg.type = CompilerMessageType::Info;
                break;
        }
    }
    
    return msg;
}

CompilerMessage UCPythonPlugin::ParseMypyMessage(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    
    // Mypy format: filename.py:line: error: message
    //              filename.py:line: note: message
    std::regex mypyRegex(R"(^(.+):(\d+): (error|warning|note): (.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, mypyRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        std::string level = match[3].str();
        msg.message = match[4].str();
        
        if (level == "error") {
            msg.type = CompilerMessageType::Error;
        } else if (level == "warning") {
            msg.type = CompilerMessageType::Warning;
        } else {
            msg.type = CompilerMessageType::Note;
        }
    }
    
    return msg;
}

CompilerMessage UCPythonPlugin::ParseFlake8Message(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    
    // Flake8 format: filename.py:line:column: code message
    std::regex flake8Regex(R"(^(.+):(\d+):(\d+): ([A-Z]\d+) (.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, flake8Regex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = std::stoi(match[3].str());
        std::string code = match[4].str();
        msg.message = "[" + code + "] " + match[5].str();
        
        // E = Error, W = Warning, F = PyFlakes, C = McCabe, N = pep8-naming
        char codeType = code[0];
        if (codeType == 'E' || codeType == 'F') {
            msg.type = CompilerMessageType::Error;
        } else {
            msg.type = CompilerMessageType::Warning;
        }
    }
    
    return msg;
}

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCPythonPlugin::GenerateCommandLine(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config) {
    
    std::vector<std::string> args = GetBaseCommandLine();
    
    // Add configuration-specific options
    if (config.optimizationLevel > 0) {
        if (pluginConfig.optimizedMore) {
            args.push_back("-OO");
        } else {
            args.push_back("-O");
        }
    }
    
    // Warnings
    if (pluginConfig.warningsAsErrors) {
        args.push_back("-W");
        args.push_back("error");
    }
    
    // Add main script
    if (!sourceFiles.empty()) {
        args.push_back(sourceFiles[0]);
    }
    
    // Add script arguments from extra args
    for (const auto& arg : config.extraArgs) {
        args.push_back(arg);
    }
    
    return args;
}

std::vector<std::string> UCPythonPlugin::GetBaseCommandLine() const {
    std::vector<std::string> args;
    
    args.push_back(GetPythonExecutable());
    
    // Unbuffered output for real-time streaming
    if (pluginConfig.unbufferedOutput) {
        args.push_back("-u");
    }
    
    // Don't write bytecode
    if (pluginConfig.dontWriteBytecode) {
        args.push_back("-B");
    }
    
    // Ignore environment variables
    if (pluginConfig.ignoreEnvironment) {
        args.push_back("-E");
    }
    
    // Isolated mode
    if (pluginConfig.isolatedMode) {
        args.push_back("-I");
    }
    
    // Verbose imports
    if (pluginConfig.verboseImport) {
        args.push_back("-v");
    }
    
    return args;
}

std::string UCPythonPlugin::GetPythonExecutable() const {
    // If virtual environment is active, use its Python
    if (!activeVenvPath.empty()) {
#ifdef _WIN32
        return activeVenvPath + "\\Scripts\\python.exe";
#else
        return activeVenvPath + "/bin/python";
#endif
    }
    
    return pluginConfig.pythonPath;
}

std::string UCPythonPlugin::GetPipExecutable() const {
    // If virtual environment is active, use its pip
    if (!activeVenvPath.empty()) {
#ifdef _WIN32
        return activeVenvPath + "\\Scripts\\pip.exe";
#else
        return activeVenvPath + "/bin/pip";
#endif
    }
    
    return pluginConfig.pipPath;
}

// ============================================================================
// VIRTUAL ENVIRONMENT
// ============================================================================

bool UCPythonPlugin::CreateVirtualEnv(const std::string& path, bool useSystemPackages) {
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "venv"
    };
    
    if (useSystemPackages) {
        args.push_back("--system-site-packages");
    }
    
    args.push_back(path);
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode)) {
        return exitCode == 0;
    }
    
    return false;
}

bool UCPythonPlugin::ActivateVirtualEnv(const std::string& path) {
    // Check if path exists
    std::string pythonPath;
#ifdef _WIN32
    pythonPath = path + "\\Scripts\\python.exe";
#else
    pythonPath = path + "/bin/python";
#endif
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand({pythonPath, "--version"}, output, error, exitCode)) {
        if (exitCode == 0) {
            activeVenvPath = path;
            pluginConfig.envType = PythonEnvironmentType::Venv;
            
            // Clear cached data
            cachedVersion.clear();
            packagesCached = false;
            cachedPackages.clear();
            
            // Refresh version info
            GetCompilerVersion();
            
            return true;
        }
    }
    
    return false;
}

void UCPythonPlugin::DeactivateVirtualEnv() {
    activeVenvPath.clear();
    pluginConfig.envType = PythonEnvironmentType::System;
    
    // Clear cached data
    cachedVersion.clear();
    packagesCached = false;
    cachedPackages.clear();
}

// ============================================================================
// PACKAGE MANAGEMENT
// ============================================================================

bool UCPythonPlugin::InstallPackage(const std::string& packageSpec,
                                    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {
        GetPipExecutable(),
        "install",
        packageSpec
    };
    
    std::string output, error;
    int exitCode;
    
    bool success = ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    if (success && exitCode == 0) {
        packagesCached = false;  // Invalidate cache
        if (onPackageInstalled) {
            onPackageInstalled(packageSpec);
        }
        return true;
    }
    
    return false;
}

bool UCPythonPlugin::InstallRequirements(const std::string& requirementsPath,
                                         std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {
        GetPipExecutable(),
        "install",
        "-r", requirementsPath
    };
    
    std::string output, error;
    int exitCode;
    
    bool success = ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    if (success && exitCode == 0) {
        packagesCached = false;
        return true;
    }
    
    return false;
}

bool UCPythonPlugin::UninstallPackage(const std::string& packageName,
                                      std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {
        GetPipExecutable(),
        "uninstall",
        "-y",
        packageName
    };
    
    std::string output, error;
    int exitCode;
    
    bool success = ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    if (success && exitCode == 0) {
        packagesCached = false;
        return true;
    }
    
    return false;
}

std::vector<PythonPackageInfo> UCPythonPlugin::ListPackages() {
    if (packagesCached) {
        return cachedPackages;
    }
    
    cachedPackages.clear();
    
    std::vector<std::string> args = {
        GetPipExecutable(),
        "list",
        "--format=freeze"
    };
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode) && exitCode == 0) {
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            
            // Format: package==version
            size_t pos = line.find("==");
            if (pos != std::string::npos) {
                PythonPackageInfo info;
                info.name = line.substr(0, pos);
                info.version = line.substr(pos + 2);
                cachedPackages.push_back(info);
            }
        }
        
        packagesCached = true;
    }
    
    return cachedPackages;
}

PythonPackageInfo UCPythonPlugin::GetPackageInfo(const std::string& packageName) {
    PythonPackageInfo info;
    info.name = packageName;
    
    std::vector<std::string> args = {
        GetPipExecutable(),
        "show",
        packageName
    };
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode) && exitCode == 0) {
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            size_t colonPos = line.find(": ");
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 2);
                
                if (key == "Version") {
                    info.version = value;
                } else if (key == "Summary") {
                    info.summary = value;
                } else if (key == "Location") {
                    info.location = value;
                } else if (key == "Requires") {
                    // Parse comma-separated dependencies
                    std::istringstream depStream(value);
                    std::string dep;
                    while (std::getline(depStream, dep, ',')) {
                        // Trim whitespace
                        dep.erase(0, dep.find_first_not_of(" "));
                        dep.erase(dep.find_last_not_of(" ") + 1);
                        if (!dep.empty()) {
                            info.dependencies.push_back(dep);
                        }
                    }
                } else if (key == "Editable project location") {
                    info.isEditable = true;
                }
            }
        }
    }
    
    return info;
}

bool UCPythonPlugin::IsPackageInstalled(const std::string& packageName) {
    auto packages = ListPackages();
    for (const auto& pkg : packages) {
        if (pkg.name == packageName) {
            return true;
        }
    }
    return false;
}

std::string UCPythonPlugin::FreezePackages() {
    std::vector<std::string> args = {
        GetPipExecutable(),
        "freeze"
    };
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode) && exitCode == 0) {
        return output;
    }
    
    return "";
}

// ============================================================================
// LINTING & TYPE CHECKING
// ============================================================================

PythonLintResult UCPythonPlugin::RunPylint(const std::vector<std::string>& files,
                                           std::function<void(const std::string&)> onOutput) {
    PythonLintResult result;
    result.tool = "pylint";
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "pylint",
        "--output-format=text"
    };
    
    if (!pluginConfig.pylintRcPath.empty()) {
        args.push_back("--rcfile=" + pluginConfig.pylintRcPath);
    }
    
    for (const auto& file : files) {
        args.push_back(file);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
    }
    
    // Parse output
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        CompilerMessage msg = ParsePylintMessage(line);
        if (!msg.file.empty()) {
            result.messages.push_back(msg);
            
            switch (msg.type) {
                case CompilerMessageType::Error:
                    result.errorCount++;
                    break;
                case CompilerMessageType::Warning:
                    result.warningCount++;
                    break;
                default:
                    break;
            }
        }
        
        // Parse score
        if (line.find("Your code has been rated at") != std::string::npos) {
            std::regex scoreRegex(R"(rated at (-?\d+\.?\d*)/10)");
            std::smatch match;
            if (std::regex_search(line, match, scoreRegex)) {
                result.score = std::stof(match[1].str());
            }
        }
    }
    
    result.passed = (result.errorCount == 0);
    
    if (onLintComplete) {
        onLintComplete(result);
    }
    
    return result;
}

PythonLintResult UCPythonPlugin::RunMypy(const std::vector<std::string>& files,
                                         std::function<void(const std::string&)> onOutput) {
    PythonLintResult result;
    result.tool = "mypy";
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "mypy"
    };
    
    if (!pluginConfig.mypyIniPath.empty()) {
        args.push_back("--config-file=" + pluginConfig.mypyIniPath);
    }
    
    for (const auto& file : files) {
        args.push_back(file);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
    }
    
    // Parse output
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        CompilerMessage msg = ParseMypyMessage(line);
        if (!msg.file.empty()) {
            result.messages.push_back(msg);
            
            if (msg.type == CompilerMessageType::Error) {
                result.errorCount++;
            } else if (msg.type == CompilerMessageType::Warning) {
                result.warningCount++;
            }
        }
    }
    
    result.passed = (exitCode == 0);
    
    if (onLintComplete) {
        onLintComplete(result);
    }
    
    return result;
}

PythonLintResult UCPythonPlugin::RunFlake8(const std::vector<std::string>& files,
                                           std::function<void(const std::string&)> onOutput) {
    PythonLintResult result;
    result.tool = "flake8";
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "flake8"
    };
    
    for (const auto& file : files) {
        args.push_back(file);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
    }
    
    // Parse output
    std::istringstream iss(output);
    std::string line;
    
    while (std::getline(iss, line)) {
        CompilerMessage msg = ParseFlake8Message(line);
        if (!msg.file.empty()) {
            result.messages.push_back(msg);
            
            if (msg.type == CompilerMessageType::Error) {
                result.errorCount++;
            } else if (msg.type == CompilerMessageType::Warning) {
                result.warningCount++;
            }
        }
    }
    
    result.passed = (exitCode == 0);
    
    if (onLintComplete) {
        onLintComplete(result);
    }
    
    return result;
}

PythonLintResult UCPythonPlugin::CheckBlackFormatting(const std::vector<std::string>& files,
                                                      std::function<void(const std::string&)> onOutput) {
    PythonLintResult result;
    result.tool = "black";
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "black",
        "--check",
        "--diff"
    };
    
    for (const auto& file : files) {
        args.push_back(file);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    result.passed = (exitCode == 0);
    
    // Parse "would reformat" messages
    std::regex reformatRegex(R"(would reformat (.+)$)");
    std::istringstream iss(error);
    std::string line;
    
    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, reformatRegex)) {
            CompilerMessage msg;
            msg.type = CompilerMessageType::Warning;
            msg.file = match[1].str();
            msg.message = "File would be reformatted by black";
            result.messages.push_back(msg);
            result.warningCount++;
        }
    }
    
    if (onLintComplete) {
        onLintComplete(result);
    }
    
    return result;
}

bool UCPythonPlugin::FormatWithBlack(const std::vector<std::string>& files,
                                     std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "black"
    };
    
    for (const auto& file : files) {
        args.push_back(file);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (onOutput) {
        onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    return exitCode == 0;
}

// ============================================================================
// TESTING
// ============================================================================

PythonTestSummary UCPythonPlugin::RunPytest(const std::string& testPath,
                                            const std::vector<std::string>& extraArgs,
                                            std::function<void(const std::string&)> onOutput) {
    PythonTestSummary summary;
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "pytest",
        "-v",
        "--tb=short"
    };
    
    if (!testPath.empty()) {
        args.push_back(testPath);
    }
    
    for (const auto& arg : extraArgs) {
        args.push_back(arg);
    }
    
    std::string output, error;
    int exitCode;
    
    auto startTime = std::chrono::steady_clock::now();
    ExecuteCommand(args, output, error, exitCode);
    auto endTime = std::chrono::steady_clock::now();
    
    summary.totalDuration = std::chrono::duration<float>(endTime - startTime).count();
    
    if (onOutput) {
        onOutput(output);
    }
    
    // Parse pytest output
    std::istringstream iss(output);
    std::string line;
    
    std::regex testResultRegex(R"(^(.+)::(.+)::(.+) (PASSED|FAILED|ERROR|SKIPPED).*$)");
    std::regex summaryRegex(R"((\d+) passed.*?(\d+) failed.*?(\d+) error)");
    
    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, testResultRegex)) {
            PythonTestResult test;
            test.testFile = match[1].str();
            test.testClass = match[2].str();
            test.testMethod = match[3].str();
            test.testName = test.testClass + "::" + test.testMethod;
            
            std::string result = match[4].str();
            if (result == "PASSED") {
                test.status = PythonTestResult::Status::Passed;
                summary.passed++;
            } else if (result == "FAILED") {
                test.status = PythonTestResult::Status::Failed;
                summary.failed++;
            } else if (result == "ERROR") {
                test.status = PythonTestResult::Status::Error;
                summary.errors++;
            } else if (result == "SKIPPED") {
                test.status = PythonTestResult::Status::Skipped;
                summary.skipped++;
            }
            
            summary.tests.push_back(test);
        }
    }
    
    summary.allPassed = (exitCode == 0);
    
    if (onTestComplete) {
        onTestComplete(summary);
    }
    
    return summary;
}

PythonTestSummary UCPythonPlugin::RunUnittest(const std::string& testPath,
                                               std::function<void(const std::string&)> onOutput) {
    PythonTestSummary summary;
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "unittest",
        "discover",
        "-v"
    };
    
    if (!testPath.empty()) {
        args.push_back("-s");
        args.push_back(testPath);
    }
    
    std::string output, error;
    int exitCode;
    
    auto startTime = std::chrono::steady_clock::now();
    ExecuteCommand(args, output, error, exitCode);
    auto endTime = std::chrono::steady_clock::now();
    
    summary.totalDuration = std::chrono::duration<float>(endTime - startTime).count();
    
    if (onOutput) {
        onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    // Parse unittest output
    std::string combined = output + error;
    std::istringstream iss(combined);
    std::string line;
    
    std::regex testResultRegex(R"(^(.+) \((.+)\) \.\.\. (ok|FAIL|ERROR|skipped)$)");
    
    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, testResultRegex)) {
            PythonTestResult test;
            test.testMethod = match[1].str();
            test.testClass = match[2].str();
            test.testName = test.testClass + "." + test.testMethod;
            
            std::string result = match[3].str();
            if (result == "ok") {
                test.status = PythonTestResult::Status::Passed;
                summary.passed++;
            } else if (result == "FAIL") {
                test.status = PythonTestResult::Status::Failed;
                summary.failed++;
            } else if (result == "ERROR") {
                test.status = PythonTestResult::Status::Error;
                summary.errors++;
            } else if (result == "skipped") {
                test.status = PythonTestResult::Status::Skipped;
                summary.skipped++;
            }
            
            summary.tests.push_back(test);
        }
    }
    
    summary.allPassed = (exitCode == 0);
    
    if (onTestComplete) {
        onTestComplete(summary);
    }
    
    return summary;
}

std::vector<std::string> UCPythonPlugin::DiscoverTests(const std::string& testPath) {
    std::vector<std::string> tests;
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "pytest",
        "--collect-only",
        "-q"
    };
    
    if (!testPath.empty()) {
        args.push_back(testPath);
    }
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode)) {
        std::istringstream iss(output);
        std::string line;
        
        while (std::getline(iss, line)) {
            if (line.find("::") != std::string::npos && line.find("<") == std::string::npos) {
                tests.push_back(line);
            }
        }
    }
    
    return tests;
}

// ============================================================================
// DEBUGGING
// ============================================================================

bool UCPythonPlugin::StartDebugServer(const std::string& scriptPath, int port, bool waitForClient) {
    if (debugServerRunning) {
        return false;
    }
    
    std::vector<std::string> args = {
        GetPythonExecutable(),
        "-m", "debugpy",
        "--listen", std::to_string(port)
    };
    
    if (waitForClient) {
        args.push_back("--wait-for-client");
    }
    
    args.push_back(scriptPath);
    
    // Start async
    debugServerRunning = true;
    
    debugThread = std::thread([this, args]() {
        std::string output, error;
        int exitCode;
        ExecuteCommand(args, output, error, exitCode);
        debugServerRunning = false;
    });
    
    return true;
}

void UCPythonPlugin::StopDebugServer() {
    if (!debugServerRunning) return;
    
    if (debugProcess) {
#ifdef _WIN32
        TerminateProcess(debugProcess, 0);
#else
        kill(reinterpret_cast<pid_t>(reinterpret_cast<intptr_t>(debugProcess)), SIGTERM);
#endif
        debugProcess = nullptr;
    }
    
    debugServerRunning = false;
}

// ============================================================================
// REPL
// ============================================================================

bool UCPythonPlugin::StartREPL(std::function<void(const std::string&)> onOutput,
                                std::function<void()> onReady) {
    if (replRunning) {
        return false;
    }
    
    // This would need proper interactive process handling
    // For now, just indicate it's not fully implemented
    replRunning = true;
    
    if (onReady) {
        onReady();
    }
    
    return true;
}

bool UCPythonPlugin::SendToREPL(const std::string& command) {
    if (!replRunning) {
        return false;
    }
    
    // Would write to REPL stdin
    return true;
}

void UCPythonPlugin::StopREPL() {
    if (!replRunning) return;
    
    if (replProcess) {
#ifdef _WIN32
        TerminateProcess(replProcess, 0);
#else
        kill(reinterpret_cast<pid_t>(reinterpret_cast<intptr_t>(replProcess)), SIGTERM);
#endif
        replProcess = nullptr;
    }
    
    replRunning = false;
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCPythonPlugin::ExecuteCommand(const std::vector<std::string>& args,
                                    std::string& output,
                                    std::string& error,
                                    int& exitCode) {
    if (args.empty()) return false;
    
    // Build command string
    std::stringstream cmdStream;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdStream << " ";
        
        // Quote arguments with spaces
        if (args[i].find(' ') != std::string::npos) {
            cmdStream << "\"" << args[i] << "\"";
        } else {
            cmdStream << args[i];
        }
    }
    
    std::string command = cmdStream.str();
    
#ifdef _WIN32
    // Windows implementation using CreateProcess
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;
    
    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    
    currentProcess = pi.hProcess;
    
    // Read output
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    
    while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        error += buffer;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD dwExitCode;
    GetExitCodeProcess(pi.hProcess, &dwExitCode);
    exitCode = static_cast<int>(dwExitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
    
    currentProcess = nullptr;
    return true;
    
#else
    // Unix implementation using fork/exec
    int stdoutPipe[2], stderrPipe[2];
    
    if (pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) {
        return false;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        return false;
    }
    
    if (pid == 0) {
        // Child process
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        
        close(stdoutPipe[1]);
        close(stderrPipe[1]);
        
        // Convert args to char**
        std::vector<char*> cargs;
        for (const auto& arg : args) {
            cargs.push_back(const_cast<char*>(arg.c_str()));
        }
        cargs.push_back(nullptr);
        
        execvp(cargs[0], cargs.data());
        _exit(127);
    }
    
    // Parent process
    close(stdoutPipe[1]);
    close(stderrPipe[1]);
    
    currentProcess = reinterpret_cast<void*>(static_cast<intptr_t>(pid));
    
    char buffer[4096];
    ssize_t bytesRead;
    
    while ((bytesRead = read(stdoutPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    
    while ((bytesRead = read(stderrPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        error += buffer;
    }
    
    close(stdoutPipe[0]);
    close(stderrPipe[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        exitCode = WEXITSTATUS(status);
    } else {
        exitCode = -1;
    }
    
    currentProcess = nullptr;
    return true;
#endif
}

void UCPythonPlugin::SetConfig(const PythonPluginConfig& config) {
    pluginConfig = config;
    
    // Reset cached data
    installationDetected = false;
    cachedVersion.clear();
    packagesCached = false;
    cachedPackages.clear();
    versionInfo = PythonVersionInfo();
    
    // Re-detect installation
    DetectInstallation();
}

} // namespace IDE
} // namespace UltraCanvas
