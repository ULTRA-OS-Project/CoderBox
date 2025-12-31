// Apps/IDE/Build/Plugins/UCLuaPlugin.cpp
// Lua Interpreter/Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCLuaPlugin.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

LuaPluginConfig LuaPluginConfig::Default() {
    LuaPluginConfig config;
    
#ifdef _WIN32
    config.luaPath = "lua";
    config.luacPath = "luac";
    config.luajitPath = "luajit";
    config.luauPath = "luau";
    config.tealPath = "tl";
    config.moonscriptPath = "moonc";
#else
    config.luaPath = "/usr/bin/lua";
    config.luacPath = "/usr/bin/luac";
    config.luajitPath = "/usr/bin/luajit";
    config.luauPath = "/usr/bin/luau";
    config.tealPath = "/usr/bin/tl";
    config.moonscriptPath = "/usr/bin/moonc";
#endif
    
    config.activeImpl = LuaImplementation::StandardLua;
    config.showWarnings = true;
    config.jitEnabled = true;
    config.jitOptLevel = 3;
    
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCLuaPlugin::UCLuaPlugin() : pluginConfig(LuaPluginConfig::Default()) {
    DetectInstallation();
}

UCLuaPlugin::UCLuaPlugin(const LuaPluginConfig& config) : pluginConfig(config) {
    DetectInstallation();
}

UCLuaPlugin::~UCLuaPlugin() {
    Cancel();
    StopRepl();
    if (buildThread.joinable()) buildThread.join();
    if (replThread.joinable()) replThread.join();
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCLuaPlugin::GetPluginName() const {
    return "Lua Interpreter Plugin";
}

std::string UCLuaPlugin::GetPluginVersion() const {
    return "1.0.0";
}

CompilerType UCLuaPlugin::GetCompilerType() const {
    return CompilerType::Custom;
}

std::string UCLuaPlugin::GetCompilerName() const {
    auto it = implVersions.find(pluginConfig.activeImpl);
    if (it != implVersions.end() && !it->second.empty()) {
        return LuaImplementationToString(pluginConfig.activeImpl) + " " + it->second;
    }
    return LuaImplementationToString(pluginConfig.activeImpl);
}

// ============================================================================
// INTERPRETER DETECTION
// ============================================================================

bool UCLuaPlugin::IsAvailable() {
    if (installationDetected) return !detectedImpls.empty();
    return DetectInstallation();
}

std::string UCLuaPlugin::GetCompilerPath() {
    return GetInterpreterExecutable();
}

void UCLuaPlugin::SetCompilerPath(const std::string& path) {
    switch (pluginConfig.activeImpl) {
        case LuaImplementation::StandardLua: pluginConfig.luaPath = path; break;
        case LuaImplementation::LuaJIT: pluginConfig.luajitPath = path; break;
        case LuaImplementation::LuaU: pluginConfig.luauPath = path; break;
        case LuaImplementation::Teal: pluginConfig.tealPath = path; break;
        case LuaImplementation::MoonScript: pluginConfig.moonscriptPath = path; break;
        default: break;
    }
    installationDetected = false;
    DetectInstallation();
}

std::string UCLuaPlugin::GetCompilerVersion() {
    auto it = implVersions.find(pluginConfig.activeImpl);
    return (it != implVersions.end()) ? it->second : "";
}

std::vector<std::string> UCLuaPlugin::GetSupportedExtensions() const {
    return {"lua", "luac", "moon", "tl", "teal"};
}

bool UCLuaPlugin::CanCompile(const std::string& filePath) const {
    size_t dot = filePath.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == "lua" || ext == "luac" || ext == "moon" || ext == "tl" || ext == "teal");
}

bool UCLuaPlugin::DetectInstallation() {
    installationDetected = true;
    detectedImpls.clear();
    implVersions.clear();
    
    // Try standard Lua
    std::vector<std::string> luaPaths = {
        pluginConfig.luaPath, "lua", "lua5.4", "lua54", "lua5.3", "lua53",
#ifndef _WIN32
        "/usr/bin/lua", "/usr/local/bin/lua", "/usr/bin/lua5.4", "/usr/bin/lua5.3"
#endif
    };
    for (const auto& p : luaPaths) {
        if (DetectImplementation(LuaImplementation::StandardLua, p)) break;
    }
    
    // Try LuaJIT
    std::vector<std::string> jitPaths = {
        pluginConfig.luajitPath, "luajit", "luajit-2.1",
#ifndef _WIN32
        "/usr/bin/luajit", "/usr/local/bin/luajit"
#endif
    };
    for (const auto& p : jitPaths) {
        if (DetectImplementation(LuaImplementation::LuaJIT, p)) break;
    }
    
    // Try Luau
    std::vector<std::string> luauPaths = {pluginConfig.luauPath, "luau", "luau-analyze"};
    for (const auto& p : luauPaths) {
        if (DetectImplementation(LuaImplementation::LuaU, p)) break;
    }
    
    // Try Teal
    std::vector<std::string> tealPaths = {pluginConfig.tealPath, "tl"};
    for (const auto& p : tealPaths) {
        if (DetectImplementation(LuaImplementation::Teal, p)) break;
    }
    
    // Try MoonScript
    std::vector<std::string> moonPaths = {pluginConfig.moonscriptPath, "moonc", "moon"};
    for (const auto& p : moonPaths) {
        if (DetectImplementation(LuaImplementation::MoonScript, p)) break;
    }
    
    // Set active to first available
    if (detectedImpls.empty()) return false;
    if (detectedImpls.find(pluginConfig.activeImpl) == detectedImpls.end()) {
        pluginConfig.activeImpl = detectedImpls.begin()->first;
    }
    
    return true;
}

bool UCLuaPlugin::DetectImplementation(LuaImplementation impl, const std::string& path) {
    if (path.empty()) return false;
    
    std::string output, error;
    int exitCode;
    
    std::vector<std::string> args = {path};
    
    // Different version flags for different implementations
    if (impl == LuaImplementation::Teal) {
        args.push_back("--version");
    } else if (impl == LuaImplementation::MoonScript) {
        args.push_back("-v");
    } else {
        args.push_back("-v");
    }
    
    if (ExecuteCommand(args, output, error, exitCode)) {
        std::string combined = output + error;
        
        // Check for recognizable version string
        bool found = false;
        if (impl == LuaImplementation::StandardLua && combined.find("Lua") != std::string::npos) found = true;
        if (impl == LuaImplementation::LuaJIT && combined.find("LuaJIT") != std::string::npos) found = true;
        if (impl == LuaImplementation::LuaU && (combined.find("Luau") != std::string::npos || combined.find("luau") != std::string::npos)) found = true;
        if (impl == LuaImplementation::Teal && combined.find("tl") != std::string::npos) found = true;
        if (impl == LuaImplementation::MoonScript && combined.find("MoonScript") != std::string::npos) found = true;
        
        if (found || exitCode == 0) {
            detectedImpls[impl] = path;
            
            // Parse version
            std::regex versionRegex(R"((\d+\.\d+\.?\d*))");
            std::smatch match;
            if (std::regex_search(combined, match, versionRegex)) {
                implVersions[impl] = match[1].str();
            }
            
            // Detect Lua version
            if (impl == LuaImplementation::StandardLua) {
                detectedVersion = ParseVersion(combined);
            } else if (impl == LuaImplementation::LuaJIT) {
                detectedVersion = LuaVersion::LuaJIT21;
            } else if (impl == LuaImplementation::LuaU) {
                detectedVersion = LuaVersion::Luau;
            }
            
            return true;
        }
    }
    return false;
}

LuaVersion UCLuaPlugin::ParseVersion(const std::string& versionString) {
    if (versionString.find("5.4") != std::string::npos) return LuaVersion::Lua54;
    if (versionString.find("5.3") != std::string::npos) return LuaVersion::Lua53;
    if (versionString.find("5.2") != std::string::npos) return LuaVersion::Lua52;
    if (versionString.find("5.1") != std::string::npos) return LuaVersion::Lua51;
    if (versionString.find("LuaJIT") != std::string::npos) return LuaVersion::LuaJIT21;
    return LuaVersion::Unknown;
}

std::vector<LuaImplementation> UCLuaPlugin::GetAvailableImplementations() {
    std::vector<LuaImplementation> available;
    for (const auto& pair : detectedImpls) {
        available.push_back(pair.first);
    }
    return available;
}

bool UCLuaPlugin::SetActiveImplementation(LuaImplementation impl) {
    if (detectedImpls.find(impl) != detectedImpls.end()) {
        pluginConfig.activeImpl = impl;
        return true;
    }
    return false;
}

std::string UCLuaPlugin::GetInterpreterExecutable() const {
    auto it = detectedImpls.find(pluginConfig.activeImpl);
    if (it != detectedImpls.end()) return it->second;
    
    switch (pluginConfig.activeImpl) {
        case LuaImplementation::StandardLua: return pluginConfig.luaPath;
        case LuaImplementation::LuaJIT: return pluginConfig.luajitPath;
        case LuaImplementation::LuaU: return pluginConfig.luauPath;
        case LuaImplementation::Teal: return pluginConfig.tealPath;
        case LuaImplementation::MoonScript: return pluginConfig.moonscriptPath;
        default: return "lua";
    }
}

std::string UCLuaPlugin::GetCompilerExecutable() const {
    if (pluginConfig.activeImpl == LuaImplementation::LuaJIT) {
        return GetInterpreterExecutable();  // LuaJIT compiles with -b
    }
    
    auto it = detectedImpls.find(LuaImplementation::StandardLua);
    if (it != detectedImpls.end()) {
        // Try luac in same directory
        std::string luaPath = it->second;
        size_t lastSlash = luaPath.rfind('/');
        if (lastSlash == std::string::npos) lastSlash = luaPath.rfind('\\');
        if (lastSlash != std::string::npos) {
            std::string dir = luaPath.substr(0, lastSlash + 1);
            return dir + "luac";
        }
    }
    return pluginConfig.luacPath;
}

// ============================================================================
// EXECUTION
// ============================================================================

void UCLuaPlugin::CompileAsync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config,
    std::function<void(const BuildResult&)> onComplete,
    std::function<void(const std::string&)> onOutputLine,
    std::function<void(float)> onProgress) {
    
    if (buildInProgress) {
        BuildResult result;
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "Build already in progress"});
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

BuildResult UCLuaPlugin::CompileSync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config) {
    
    if (sourceFiles.empty()) {
        BuildResult result;
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No source files"});
        return result;
    }
    
    // For Lua, "compile" means run the script
    return RunScript(sourceFiles[0], config.extraArgs);
}

void UCLuaPlugin::Cancel() {
    cancelRequested = true;
    if (currentProcess) {
#ifdef _WIN32
        TerminateProcess(currentProcess, 1);
#else
        kill(reinterpret_cast<pid_t>(reinterpret_cast<intptr_t>(currentProcess)), SIGTERM);
#endif
        currentProcess = nullptr;
    }
}

bool UCLuaPlugin::IsBuildInProgress() const {
    return buildInProgress;
}

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCLuaPlugin::ParseOutputLine(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    msg.line = 0;
    msg.column = 0;
    msg.message = line;
    
    // Lua error format: lua: file.lua:10: message
    // Or: file.lua:10: message
    std::regex luaErrorRegex(R"(^(?:lua[^:]*:\s*)?([^:]+):(\d+):\s*(.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, luaErrorRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.message = match[3].str();
        
        // Determine error type
        std::string lower = msg.message;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower.find("syntax error") != std::string::npos ||
            lower.find("unexpected") != std::string::npos ||
            lower.find("expected") != std::string::npos) {
            msg.type = CompilerMessageType::Error;
        } else if (lower.find("warning") != std::string::npos) {
            msg.type = CompilerMessageType::Warning;
        } else if (lower.find("attempt to") != std::string::npos ||
                   lower.find("bad argument") != std::string::npos ||
                   lower.find("nil value") != std::string::npos ||
                   lower.find("stack traceback") != std::string::npos) {
            msg.type = CompilerMessageType::Error;
        }
        
        return msg;
    }
    
    // Teal error format: file.tl:10:5: error message
    std::regex tealErrorRegex(R"(^([^:]+):(\d+):(\d+):\s*(.+)$)");
    if (std::regex_search(line, match, tealErrorRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = std::stoi(match[3].str());
        msg.message = match[4].str();
        msg.type = CompilerMessageType::Error;
        return msg;
    }
    
    // Stack traceback line
    if (line.find("stack traceback:") != std::string::npos) {
        msg.type = CompilerMessageType::Note;
        msg.message = line;
        return msg;
    }
    
    // Traceback entry: [C]: in function 'xxx' or file.lua:10: in function 'xxx'
    std::regex traceRegex(R"(^\s*([^:]+):(\d+):\s*in\s+(.+)$)");
    if (std::regex_search(line, match, traceRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.message = "in " + match[3].str();
        msg.type = CompilerMessageType::Note;
        return msg;
    }
    
    return msg;
}

LuaSyntaxError UCLuaPlugin::ParseSyntaxError(const std::string& output) {
    LuaSyntaxError err;
    
    // lua: file.lua:10: syntax error near 'end'
    std::regex syntaxRegex(R"(([^:]+):(\d+):\s*(.+?)(?:\s+near\s+'([^']+)')?$)");
    std::smatch match;
    
    if (std::regex_search(output, match, syntaxRegex)) {
        err.file = match[1].str();
        err.line = std::stoi(match[2].str());
        err.message = match[3].str();
        if (match[4].matched) {
            err.nearToken = match[4].str();
        }
    }
    
    return err;
}

LuaRuntimeError UCLuaPlugin::ParseRuntimeError(const std::string& output) {
    LuaRuntimeError err;
    
    std::istringstream iss(output);
    std::string line;
    bool inTraceback = false;
    
    while (std::getline(iss, line)) {
        if (line.find("stack traceback:") != std::string::npos) {
            inTraceback = true;
            continue;
        }
        
        if (inTraceback) {
            if (!line.empty() && (line[0] == '\t' || line[0] == ' ')) {
                err.traceback.push_back(line);
            }
        } else {
            // First error line
            std::regex errRegex(R"(([^:]+):(\d+):\s*(.+)$)");
            std::smatch match;
            if (std::regex_search(line, match, errRegex)) {
                err.file = match[1].str();
                err.line = std::stoi(match[2].str());
                err.message = match[3].str();
            }
        }
    }
    
    return err;
}

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCLuaPlugin::GenerateCommandLine(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config) {
    
    std::vector<std::string> args;
    args.push_back(GetInterpreterExecutable());
    
    // Implementation-specific flags
    if (pluginConfig.activeImpl == LuaImplementation::StandardLua) {
        if (pluginConfig.showWarnings && detectedVersion == LuaVersion::Lua54) {
            args.push_back("-W");
        }
        if (pluginConfig.interactiveMode) {
            args.push_back("-i");
        }
    } else if (pluginConfig.activeImpl == LuaImplementation::LuaJIT) {
        if (!pluginConfig.jitEnabled) {
            args.push_back("-joff");
        }
        if (pluginConfig.jitOptLevel >= 0 && pluginConfig.jitOptLevel <= 3) {
            args.push_back("-O" + std::to_string(pluginConfig.jitOptLevel));
        }
    }
    
    // Add source files
    for (const auto& file : sourceFiles) {
        args.push_back(file);
    }
    
    // Add extra args (script arguments)
    for (const auto& arg : config.extraArgs) {
        args.push_back(arg);
    }
    
    return args;
}

// ============================================================================
// SCRIPT EXECUTION
// ============================================================================

BuildResult UCLuaPlugin::RunScript(const std::string& scriptFile,
                                   const std::vector<std::string>& args,
                                   std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    
    std::vector<std::string> cmdArgs;
    cmdArgs.push_back(GetInterpreterExecutable());
    
    // Add implementation flags
    if (pluginConfig.activeImpl == LuaImplementation::StandardLua) {
        if (pluginConfig.showWarnings && detectedVersion == LuaVersion::Lua54) {
            cmdArgs.push_back("-W");
        }
    } else if (pluginConfig.activeImpl == LuaImplementation::LuaJIT) {
        if (!pluginConfig.jitEnabled) cmdArgs.push_back("-joff");
    }
    
    cmdArgs.push_back(scriptFile);
    for (const auto& a : args) cmdArgs.push_back(a);
    
    std::string output, error;
    int exitCode;
    
    // Set up environment with package paths
    std::string oldPath, oldCPath;
    if (!pluginConfig.packagePaths.empty()) {
        const char* p = std::getenv("LUA_PATH");
        if (p) oldPath = p;
#ifdef _WIN32
        _putenv_s("LUA_PATH", BuildPackagePath().c_str());
#else
        setenv("LUA_PATH", BuildPackagePath().c_str(), 1);
#endif
    }
    if (!pluginConfig.cPaths.empty()) {
        const char* p = std::getenv("LUA_CPATH");
        if (p) oldCPath = p;
#ifdef _WIN32
        _putenv_s("LUA_CPATH", BuildCPath().c_str());
#else
        setenv("LUA_CPATH", BuildCPath().c_str(), 1);
#endif
    }
    
    auto startTime = std::chrono::steady_clock::now();
    bool executed = ExecuteCommand(cmdArgs, output, error, exitCode);
    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration<float>(endTime - startTime).count();
    
    // Restore environment
    if (!pluginConfig.packagePaths.empty()) {
#ifdef _WIN32
        _putenv_s("LUA_PATH", oldPath.c_str());
#else
        if (oldPath.empty()) unsetenv("LUA_PATH");
        else setenv("LUA_PATH", oldPath.c_str(), 1);
#endif
    }
    
    if (!executed) {
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::FatalError, "", 0, 0, "Failed to run Lua"});
        return result;
    }
    
    // Handle output
    if (onOutput) {
        if (!output.empty()) onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    if (onScriptOutput && !output.empty()) onScriptOutput(output);
    
    // Parse errors
    std::string combined = error;
    if (!combined.empty()) {
        std::istringstream iss(combined);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            CompilerMessage msg = ParseOutputLine(line);
            if (msg.type == CompilerMessageType::Error || msg.type == CompilerMessageType::FatalError) {
                result.messages.push_back(msg);
                result.errorCount++;
            } else if (msg.type == CompilerMessageType::Warning) {
                result.messages.push_back(msg);
                result.warningCount++;
            }
        }
        
        if (exitCode != 0 && onRuntimeError) {
            onRuntimeError(ParseRuntimeError(combined));
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

BuildResult UCLuaPlugin::RunCode(const std::string& code,
                                 std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    
    std::vector<std::string> args;
    args.push_back(GetInterpreterExecutable());
    args.push_back("-e");
    args.push_back(code);
    
    std::string output, error;
    int exitCode;
    
    auto startTime = std::chrono::steady_clock::now();
    ExecuteCommand(args, output, error, exitCode);
    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration<float>(endTime - startTime).count();
    
    if (onOutput) {
        if (!output.empty()) onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    if (!error.empty()) {
        std::istringstream iss(error);
        std::string line;
        while (std::getline(iss, line)) {
            CompilerMessage msg = ParseOutputLine(line);
            if (msg.type != CompilerMessageType::Info) {
                result.messages.push_back(msg);
                if (msg.type == CompilerMessageType::Error) result.errorCount++;
            }
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

BuildResult UCLuaPlugin::RunScriptWithTimeout(const std::string& scriptFile,
                                              int timeoutSeconds,
                                              std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    
    std::vector<std::string> args = {GetInterpreterExecutable(), scriptFile};
    std::string output, error; int exitCode;
    
#ifndef _WIN32
    // Fork and exec with timeout monitoring
    int op[2], ep[2];
    if (pipe(op) == -1 || pipe(ep) == -1) {
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "Failed to create pipes"});
        result.errorCount = 1;
        return result;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(op[0]); close(op[1]); close(ep[0]); close(ep[1]);
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "Failed to fork process"});
        result.errorCount = 1;
        return result;
    }
    
    if (pid == 0) {
        // Child process
        close(op[0]); close(ep[0]);
        dup2(op[1], STDOUT_FILENO); dup2(ep[1], STDERR_FILENO);
        close(op[1]); close(ep[1]);
        std::vector<char*> ca;
        for (const auto& a : args) ca.push_back(const_cast<char*>(a.c_str()));
        ca.push_back(nullptr);
        execvp(ca[0], ca.data());
        _exit(127);
    }
    
    close(op[1]); close(ep[1]);
    
    // Monitor with timeout
    auto startTime = std::chrono::steady_clock::now();
    bool timedOut = false;
    
    // Set non-blocking
    fcntl(op[0], F_SETFL, O_NONBLOCK);
    fcntl(ep[0], F_SETFL, O_NONBLOCK);
    
    while (true) {
        int status;
        pid_t w = waitpid(pid, &status, WNOHANG);
        
        if (w == pid) {
            // Process finished
            if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) exitCode = 128 + WTERMSIG(status);
            break;
        }
        
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= timeoutSeconds) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            timedOut = true;
            exitCode = -1;
            break;
        }
        
        // Read available output
        char buf[4096];
        ssize_t n;
        while ((n = read(op[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; output += buf; }
        while ((n = read(ep[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; error += buf; }
        
        usleep(10000); // 10ms
    }
    
    // Read remaining output
    char buf[4096]; ssize_t n;
    while ((n = read(op[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; output += buf; }
    while ((n = read(ep[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; error += buf; }
    close(op[0]); close(ep[0]);
    
    if (timedOut) {
        result.messages.push_back({CompilerMessageType::Error, scriptFile, 0, 0, 
            "Script execution timed out after " + std::to_string(timeoutSeconds) + " seconds"});
        result.errorCount = 1;
    }
#else
    // Windows: simpler approach using thread
    std::atomic<bool> finished{false};
    std::thread execThread([&]() {
        ExecuteCommand(args, output, error, exitCode);
        finished = true;
    });
    
    auto startTime = std::chrono::steady_clock::now();
    while (!finished) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= timeoutSeconds) {
            Cancel();
            result.messages.push_back({CompilerMessageType::Error, scriptFile, 0, 0,
                "Script execution timed out after " + std::to_string(timeoutSeconds) + " seconds"});
            result.errorCount = 1;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (execThread.joinable()) execThread.join();
#endif
    
    if (onOutput) {
        if (!output.empty()) onOutput(output);
        if (!error.empty()) onOutput(error);
    }
    
    result.success = (exitCode == 0 && result.errorCount == 0);
    return result;
}

// ============================================================================
// BYTECODE COMPILATION
// ============================================================================

LuaBytecodeInfo UCLuaPlugin::CompileToBytecode(const std::string& sourceFile,
                                               const std::string& outputFile) {
    LuaBytecodeInfo info;
    info.sourceFile = sourceFile;
    
    std::string outFile = outputFile.empty() ? 
        sourceFile.substr(0, sourceFile.rfind('.')) + ".luac" : outputFile;
    info.outputFile = outFile;
    
    std::vector<std::string> args;
    
    if (pluginConfig.activeImpl == LuaImplementation::LuaJIT) {
        args.push_back(GetInterpreterExecutable());
        args.push_back("-b");
        if (pluginConfig.stripDebugInfo) args.push_back("-s");
        args.push_back(sourceFile);
        args.push_back(outFile);
    } else {
        args.push_back(GetCompilerExecutable());
        if (pluginConfig.stripDebugInfo) args.push_back("-s");
        args.push_back("-o");
        args.push_back(outFile);
        args.push_back(sourceFile);
    }
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode) && exitCode == 0) {
        // Get file sizes
        std::ifstream srcFile(sourceFile, std::ios::binary | std::ios::ate);
        if (srcFile) info.sourceSize = srcFile.tellg();
        
        std::ifstream outBin(outFile, std::ios::binary | std::ios::ate);
        if (outBin) info.bytecodeSize = outBin.tellg();
        
        info.stripped = pluginConfig.stripDebugInfo;
        
        if (onBytecodeCompiled) onBytecodeCompiled(info);
    }
    
    return info;
}

std::string UCLuaPlugin::DumpBytecode(const std::string& sourceFile) {
    std::vector<std::string> args;
    
    if (pluginConfig.activeImpl == LuaImplementation::LuaJIT) {
        args.push_back(GetInterpreterExecutable());
        args.push_back("-bl");
        args.push_back(sourceFile);
    } else {
        args.push_back(GetCompilerExecutable());
        args.push_back("-l");
        args.push_back(sourceFile);
    }
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode)) {
        return output.empty() ? error : output;
    }
    return "";
}

bool UCLuaPlugin::IsBytecodeFile(const std::string& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    
    char header[4];
    f.read(header, 4);
    
    // Lua bytecode starts with ESC "Lua" (0x1B 0x4C 0x75 0x61)
    // LuaJIT bytecode starts with ESC "LJ" (0x1B 0x4C 0x4A)
    return (header[0] == 0x1B && header[1] == 'L');
}

// ============================================================================
// SYNTAX CHECKING
// ============================================================================

BuildResult UCLuaPlugin::CheckSyntax(const std::string& sourceFile) {
    BuildResult result;
    
    std::vector<std::string> args;
    
    if (pluginConfig.activeImpl == LuaImplementation::LuaJIT) {
        // LuaJIT: compile to /dev/null or NUL
        args.push_back(GetInterpreterExecutable());
        args.push_back("-bl");
        args.push_back(sourceFile);
#ifdef _WIN32
        args.push_back("NUL");
#else
        args.push_back("/dev/null");
#endif
    } else {
        // Standard Lua: luac -p
        args.push_back(GetCompilerExecutable());
        args.push_back("-p");
        args.push_back(sourceFile);
    }
    
    std::string output, error;
    int exitCode;
    
    auto startTime = std::chrono::steady_clock::now();
    ExecuteCommand(args, output, error, exitCode);
    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration<float>(endTime - startTime).count();
    
    if (!error.empty()) {
        std::istringstream iss(error);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            CompilerMessage msg = ParseOutputLine(line);
            if (msg.type != CompilerMessageType::Info) {
                result.messages.push_back(msg);
                result.errorCount++;
            }
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

BuildResult UCLuaPlugin::CheckSyntaxCode(const std::string& code) {
    // Write to temp file and check
    std::string tempFile = "/tmp/lua_syntax_check.lua";
#ifdef _WIN32
    tempFile = std::getenv("TEMP") ? std::string(std::getenv("TEMP")) + "\\lua_syntax_check.lua" : "lua_syntax_check.lua";
#endif
    
    std::ofstream out(tempFile);
    out << code;
    out.close();
    
    auto result = CheckSyntax(tempFile);
    
    std::remove(tempFile.c_str());
    return result;
}

// ============================================================================
// SOURCE ANALYSIS
// ============================================================================

std::vector<LuaFunctionInfo> UCLuaPlugin::ScanFunctions(const std::string& sourceFile) {
    std::vector<LuaFunctionInfo> functions;
    
    std::ifstream file(sourceFile);
    if (!file) return functions;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    ParseFunctionsFromSource(source, sourceFile, functions);
    return functions;
}

void UCLuaPlugin::ParseFunctionsFromSource(const std::string& source,
                                           const std::string& file,
                                           std::vector<LuaFunctionInfo>& functions) {
    std::istringstream iss(source);
    std::string line;
    int lineNum = 0;
    
    // Patterns for function definitions
    std::regex globalFuncRegex(R"(^\s*function\s+(\w+)\s*\(([^)]*)\))");
    std::regex localFuncRegex(R"(^\s*local\s+function\s+(\w+)\s*\(([^)]*)\))");
    std::regex methodRegex(R"(^\s*function\s+(\w+):(\w+)\s*\(([^)]*)\))");
    std::regex tableFuncRegex(R"(^\s*function\s+(\w+)\.(\w+)\s*\(([^)]*)\))");
    std::regex localAssignRegex(R"(^\s*local\s+(\w+)\s*=\s*function\s*\(([^)]*)\))");
    
    while (std::getline(iss, line)) {
        lineNum++;
        std::smatch match;
        
        if (std::regex_search(line, match, methodRegex)) {
            LuaFunctionInfo func;
            func.parentTable = match[1].str();
            func.name = match[2].str();
            func.file = file;
            func.startLine = lineNum;
            func.isMethod = true;
            
            std::string params = match[3].str();
            std::istringstream pss(params);
            std::string param;
            while (std::getline(pss, param, ',')) {
                param.erase(0, param.find_first_not_of(" \t"));
                param.erase(param.find_last_not_of(" \t") + 1);
                if (!param.empty()) func.parameters.push_back(param);
            }
            functions.push_back(func);
        }
        else if (std::regex_search(line, match, tableFuncRegex)) {
            LuaFunctionInfo func;
            func.parentTable = match[1].str();
            func.name = match[2].str();
            func.file = file;
            func.startLine = lineNum;
            
            std::string params = match[3].str();
            std::istringstream pss(params);
            std::string param;
            while (std::getline(pss, param, ',')) {
                param.erase(0, param.find_first_not_of(" \t"));
                param.erase(param.find_last_not_of(" \t") + 1);
                if (!param.empty()) func.parameters.push_back(param);
            }
            functions.push_back(func);
        }
        else if (std::regex_search(line, match, localFuncRegex)) {
            LuaFunctionInfo func;
            func.name = match[1].str();
            func.file = file;
            func.startLine = lineNum;
            func.isLocal = true;
            
            std::string params = match[2].str();
            std::istringstream pss(params);
            std::string param;
            while (std::getline(pss, param, ',')) {
                param.erase(0, param.find_first_not_of(" \t"));
                param.erase(param.find_last_not_of(" \t") + 1);
                if (!param.empty()) func.parameters.push_back(param);
            }
            functions.push_back(func);
        }
        else if (std::regex_search(line, match, globalFuncRegex)) {
            LuaFunctionInfo func;
            func.name = match[1].str();
            func.file = file;
            func.startLine = lineNum;
            
            std::string params = match[2].str();
            std::istringstream pss(params);
            std::string param;
            while (std::getline(pss, param, ',')) {
                param.erase(0, param.find_first_not_of(" \t"));
                param.erase(param.find_last_not_of(" \t") + 1);
                if (!param.empty()) func.parameters.push_back(param);
            }
            functions.push_back(func);
        }
        else if (std::regex_search(line, match, localAssignRegex)) {
            LuaFunctionInfo func;
            func.name = match[1].str();
            func.file = file;
            func.startLine = lineNum;
            func.isLocal = true;
            
            std::string params = match[2].str();
            std::istringstream pss(params);
            std::string param;
            while (std::getline(pss, param, ',')) {
                param.erase(0, param.find_first_not_of(" \t"));
                param.erase(param.find_last_not_of(" \t") + 1);
                if (!param.empty()) func.parameters.push_back(param);
            }
            functions.push_back(func);
        }
    }
}

std::vector<LuaGlobalInfo> UCLuaPlugin::ScanGlobals(const std::string& sourceFile) {
    std::vector<LuaGlobalInfo> globals;
    
    std::ifstream file(sourceFile);
    if (!file) return globals;
    
    std::string line;
    int lineNum = 0;
    
    // Simple pattern: global assignment at start of line (not local)
    std::regex globalRegex(R"(^(\w+)\s*=)");
    std::regex localRegex(R"(^\s*local\s)");
    
    while (std::getline(file, line)) {
        lineNum++;
        
        // Skip local declarations
        if (std::regex_search(line, localRegex)) continue;
        
        std::smatch match;
        if (std::regex_search(line, match, globalRegex)) {
            LuaGlobalInfo g;
            g.name = match[1].str();
            g.file = sourceFile;
            g.line = lineNum;
            globals.push_back(g);
        }
    }
    
    return globals;
}

std::vector<std::string> UCLuaPlugin::ScanRequires(const std::string& sourceFile) {
    std::vector<std::string> requires;
    
    std::ifstream file(sourceFile);
    if (!file) return requires;
    
    std::string line;
    std::regex requireRegex(R"(require\s*[\(\s]["']([^"']+)["']\s*\)?)");
    
    while (std::getline(file, line)) {
        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());
        while (std::regex_search(searchStart, line.cend(), match, requireRegex)) {
            requires.push_back(match[1].str());
            searchStart = match.suffix().first;
        }
    }
    
    return requires;
}

LuaModuleInfo UCLuaPlugin::AnalyzeModule(const std::string& sourceFile) {
    LuaModuleInfo info;
    info.file = sourceFile;
    
    // Extract module name from filename
    size_t lastSlash = sourceFile.rfind('/');
    if (lastSlash == std::string::npos) lastSlash = sourceFile.rfind('\\');
    std::string filename = (lastSlash != std::string::npos) ? sourceFile.substr(lastSlash + 1) : sourceFile;
    size_t dot = filename.rfind('.');
    info.name = (dot != std::string::npos) ? filename.substr(0, dot) : filename;
    
    info.requires = ScanRequires(sourceFile);
    
    // Scan for exports (return table at end)
    std::ifstream file(sourceFile);
    if (file) {
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        
        // Look for "return { ... }" or "return M" pattern
        std::regex returnTableRegex(R"(return\s*\{([^}]+)\})");
        std::smatch match;
        if (std::regex_search(content, match, returnTableRegex)) {
            std::string exports = match[1].str();
            std::regex keyRegex(R"((\w+)\s*=)");
            std::sregex_iterator iter(exports.begin(), exports.end(), keyRegex);
            std::sregex_iterator end;
            while (iter != end) {
                info.exports.push_back((*iter)[1].str());
                ++iter;
            }
        }
    }
    
    return info;
}

// ============================================================================
// REPL
// ============================================================================

bool UCLuaPlugin::StartRepl(std::function<void(const std::string&)> onOutput) {
    if (IsReplRunning()) return false;
    
    replSession = std::make_unique<LuaReplSession>();
    replSession->isRunning = true;
    replSession->onOutput = onOutput;
    
#ifndef _WIN32
    // Create pipes for communication
    int inPipe[2], outPipe[2];
    if (pipe(inPipe) == -1 || pipe(outPipe) == -1) {
        replSession.reset();
        return false;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(inPipe[0]); close(inPipe[1]);
        close(outPipe[0]); close(outPipe[1]);
        replSession.reset();
        return false;
    }
    
    if (pid == 0) {
        // Child: run Lua in interactive mode
        close(inPipe[1]); close(outPipe[0]);
        dup2(inPipe[0], STDIN_FILENO);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(outPipe[1], STDERR_FILENO);
        close(inPipe[0]); close(outPipe[1]);
        
        execlp(GetInterpreterExecutable().c_str(), "lua", "-i", nullptr);
        _exit(127);
    }
    
    // Parent
    close(inPipe[0]); close(outPipe[1]);
    replSession->inputFd = inPipe[1];
    replSession->outputFd = outPipe[0];
    replSession->pid = pid;
    
    // Set non-blocking on output
    fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
    
    // Start output reader thread
    replSession->readerThread = std::thread([this]() {
        char buf[4096];
        while (replSession && replSession->isRunning) {
            ssize_t n = read(replSession->outputFd, buf, sizeof(buf)-1);
            if (n > 0) {
                buf[n] = '\0';
                if (replSession->onOutput) replSession->onOutput(std::string(buf));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
#else
    // Windows: simplified approach using RunCode for each input
    replSession->inputFd = -1;
    replSession->outputFd = -1;
#endif
    
    return true;
}

bool UCLuaPlugin::SendReplInput(const std::string& input) {
    if (!IsReplRunning()) return false;
    
#ifndef _WIN32
    if (replSession->inputFd >= 0) {
        std::string line = input + "\n";
        ssize_t written = write(replSession->inputFd, line.c_str(), line.size());
        if (written > 0) {
            replSession->history.push_back(input);
            return true;
        }
        return false;
    }
#endif
    
    // Fallback: Execute input as code
    auto result = RunCode(input, replSession->onOutput);
    replSession->history.push_back(input);
    
    return result.success;
}

void UCLuaPlugin::StopRepl() {
    if (replSession) {
        replSession->isRunning = false;
        
#ifndef _WIN32
        if (replSession->inputFd >= 0) {
            close(replSession->inputFd);
            replSession->inputFd = -1;
        }
        if (replSession->outputFd >= 0) {
            close(replSession->outputFd);
            replSession->outputFd = -1;
        }
        if (replSession->pid > 0) {
            kill(replSession->pid, SIGTERM);
            waitpid(replSession->pid, nullptr, 0);
            replSession->pid = 0;
        }
        if (replSession->readerThread.joinable()) {
            replSession->readerThread.join();
        }
#endif
        
        if (replSession->onExit) replSession->onExit();
        replSession.reset();
    }
}

bool UCLuaPlugin::IsReplRunning() const {
    return replSession && replSession->isRunning;
}

// ============================================================================
// TEAL
// ============================================================================

BuildResult UCLuaPlugin::CompileTeal(const std::string& tealFile, const std::string& outputFile) {
    BuildResult result;
    
    auto it = detectedImpls.find(LuaImplementation::Teal);
    if (it == detectedImpls.end()) {
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "Teal not found"});
        return result;
    }
    
    std::vector<std::string> args;
    args.push_back(it->second);
    args.push_back("gen");
    if (pluginConfig.tealStrict) args.push_back("--strict");
    args.push_back(tealFile);
    if (!outputFile.empty()) {
        args.push_back("-o");
        args.push_back(outputFile);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (!error.empty()) {
        std::istringstream iss(error);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            CompilerMessage msg = ParseOutputLine(line);
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error) result.errorCount++;
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

BuildResult UCLuaPlugin::CheckTeal(const std::string& tealFile) {
    BuildResult result;
    
    auto it = detectedImpls.find(LuaImplementation::Teal);
    if (it == detectedImpls.end()) {
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "Teal not found"});
        return result;
    }
    
    std::vector<std::string> args;
    args.push_back(it->second);
    args.push_back("check");
    if (pluginConfig.tealStrict) args.push_back("--strict");
    args.push_back(tealFile);
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    std::string combined = output + error;
    if (!combined.empty()) {
        std::istringstream iss(combined);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            CompilerMessage msg = ParseOutputLine(line);
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error) result.errorCount++;
            else if (msg.type == CompilerMessageType::Warning) result.warningCount++;
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

bool UCLuaPlugin::GenerateTealDeclaration(const std::string& luaFile, const std::string& outputFile) {
    auto it = detectedImpls.find(LuaImplementation::Teal);
    if (it == detectedImpls.end()) return false;
    
    std::vector<std::string> args;
    args.push_back(it->second);
    args.push_back("gen");
    args.push_back("--gen-target=5.1");
    args.push_back(luaFile);
    
    std::string output, error;
    int exitCode;
    return ExecuteCommand(args, output, error, exitCode) && exitCode == 0;
}

// ============================================================================
// MOONSCRIPT
// ============================================================================

BuildResult UCLuaPlugin::CompileMoonScript(const std::string& moonFile, const std::string& outputFile) {
    BuildResult result;
    
    auto it = detectedImpls.find(LuaImplementation::MoonScript);
    if (it == detectedImpls.end()) {
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "MoonScript not found"});
        return result;
    }
    
    std::vector<std::string> args;
    args.push_back(it->second);
    args.push_back(moonFile);
    if (!outputFile.empty()) {
        args.push_back("-o");
        args.push_back(outputFile);
    }
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (!error.empty()) {
        std::istringstream iss(error);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            CompilerMessage msg = ParseOutputLine(line);
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error) result.errorCount++;
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

BuildResult UCLuaPlugin::CheckMoonScript(const std::string& moonFile) {
    BuildResult result;
    
    auto it = detectedImpls.find(LuaImplementation::MoonScript);
    if (it == detectedImpls.end()) {
        result.success = false;
        result.errorCount = 1;
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "MoonScript not found"});
        return result;
    }
    
    std::vector<std::string> args;
    args.push_back(it->second);
    args.push_back("-p");  // Print output (parse only)
    args.push_back(moonFile);
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    if (!error.empty()) {
        result.messages.push_back({CompilerMessageType::Error, moonFile, 0, 0, error});
        result.errorCount = 1;
    }
    
    result.success = (exitCode == 0);
    return result;
}

// ============================================================================
// LUAJIT SPECIFIC
// ============================================================================

BuildResult UCLuaPlugin::RunWithJIT(const std::string& scriptFile,
                                    std::function<void(const std::string&)> onOutput) {
    auto savedImpl = pluginConfig.activeImpl;
    pluginConfig.activeImpl = LuaImplementation::LuaJIT;
    auto result = RunScript(scriptFile, {}, onOutput);
    pluginConfig.activeImpl = savedImpl;
    return result;
}

std::string UCLuaPlugin::GetJITStatus() {
    auto it = detectedImpls.find(LuaImplementation::LuaJIT);
    if (it == detectedImpls.end()) return "LuaJIT not available";
    
    std::string code = "print(jit.version) print(jit.status())";
    std::vector<std::string> args = {it->second, "-e", code};
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode)) {
        return output;
    }
    return "";
}

std::string UCLuaPlugin::DumpJITTrace(const std::string& scriptFile) {
    auto it = detectedImpls.find(LuaImplementation::LuaJIT);
    if (it == detectedImpls.end()) return "";
    
    std::vector<std::string> args = {it->second, "-jdump", scriptFile};
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode)) {
        return output + error;
    }
    return "";
}

// ============================================================================
// MODULE MANAGEMENT
// ============================================================================

std::string UCLuaPlugin::FindModule(const std::string& moduleName) {
    // Convert module name to path (e.g., "socket.http" -> "socket/http.lua")
    std::string path = moduleName;
    std::replace(path.begin(), path.end(), '.', '/');
    
    std::vector<std::string> extensions = {".lua", "/init.lua"};
    std::vector<std::string> searchPaths = pluginConfig.packagePaths;
    
    // Add default paths
    searchPaths.push_back("./");
    searchPaths.push_back("./lua/");
    
    for (const auto& base : searchPaths) {
        for (const auto& ext : extensions) {
            std::string fullPath = base + path + ext;
            std::ifstream test(fullPath);
            if (test.good()) return fullPath;
        }
    }
    
    return "";
}

std::string UCLuaPlugin::GetPackagePath() {
    return BuildPackagePath();
}

std::string UCLuaPlugin::GetPackageCPath() {
    return BuildCPath();
}

std::string UCLuaPlugin::BuildPackagePath() const {
    std::string path;
    const char* envPath = std::getenv("LUA_PATH");
    if (envPath) path = envPath;
    
    for (const auto& p : pluginConfig.packagePaths) {
        if (!path.empty()) path += ";";
        path += p + "/?.lua;" + p + "/?/init.lua";
    }
    
    return path;
}

std::string UCLuaPlugin::BuildCPath() const {
    std::string path;
    const char* envPath = std::getenv("LUA_CPATH");
    if (envPath) path = envPath;
    
    for (const auto& p : pluginConfig.cPaths) {
        if (!path.empty()) path += ";";
#ifdef _WIN32
        path += p + "/?.dll";
#else
        path += p + "/?.so";
#endif
    }
    
    return path;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UCLuaPlugin::SetConfig(const LuaPluginConfig& config) {
    pluginConfig = config;
    installationDetected = false;
    detectedImpls.clear();
    implVersions.clear();
    moduleCache.clear();
    DetectInstallation();
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCLuaPlugin::ExecuteCommand(const std::vector<std::string>& args,
                                 std::string& output,
                                 std::string& error,
                                 int& exitCode,
                                 const std::string& input,
                                 const std::string& workingDir) {
    if (args.empty()) return false;
    
    std::stringstream cmdStream;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdStream << " ";
        if (args[i].find(' ') != std::string::npos) {
            cmdStream << "\"" << args[i] << "\"";
        } else {
            cmdStream << args[i];
        }
    }
    std::string command = cmdStream.str();
    
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hStdOutRead, hStdOutWrite, hStdErrRead, hStdErrWrite;
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
    
    const char* cwd = workingDir.empty() ? NULL : workingDir.c_str();
    
    if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, cwd, &si, &pi)) {
        CloseHandle(hStdOutRead); CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead); CloseHandle(hStdErrWrite);
        return false;
    }
    
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    
    currentProcess = pi.hProcess;
    
    char buffer[4096];
    DWORD bytesRead;
    
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        if (cancelRequested) break;
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    
    while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        if (cancelRequested) break;
        buffer[bytesRead] = '\0';
        error += buffer;
    }
    
    if (cancelRequested) TerminateProcess(pi.hProcess, 1);
    
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
    int stdoutPipe[2], stderrPipe[2];
    
    if (pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) return false;
    
    pid_t pid = fork();
    
    if (pid == -1) {
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stderrPipe[0]); close(stderrPipe[1]);
        return false;
    }
    
    if (pid == 0) {
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);
        
        if (!workingDir.empty()) {
            if (chdir(workingDir.c_str()) != 0) _exit(127);
        }
        
        std::vector<char*> cargs;
        for (const auto& arg : args) {
            cargs.push_back(const_cast<char*>(arg.c_str()));
        }
        cargs.push_back(nullptr);
        
        execvp(cargs[0], cargs.data());
        _exit(127);
    }
    
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
    
    if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) exitCode = 128 + WTERMSIG(status);
    else exitCode = -1;
    
    currentProcess = nullptr;
    return true;
#endif
}

} // namespace IDE
} // namespace UltraCanvas
