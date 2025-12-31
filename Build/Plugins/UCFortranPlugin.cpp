// Apps/IDE/Build/Plugins/UCFortranPlugin.cpp
// Fortran Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCFortranPlugin.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstdlib>
#include <chrono>
#include <set>

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

FortranPluginConfig FortranPluginConfig::Default() {
    FortranPluginConfig config;
    
#ifdef _WIN32
    config.gfortranPath = "gfortran";
    config.ifortPath = "ifort";
    config.flangPath = "flang";
#else
    config.gfortranPath = "/usr/bin/gfortran";
    config.ifortPath = "/opt/intel/oneapi/compiler/latest/bin/ifort";
    config.flangPath = "/usr/bin/flang";
#endif
    
    config.activeCompiler = FortranCompilerType::GFortran;
    config.standard = FortranStandard::Fortran2018;
    config.freeForm = true;
    config.wallWarnings = true;
    config.implicitNone = true;
    config.backtraceOnError = true;
    
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCFortranPlugin::UCFortranPlugin() : pluginConfig(FortranPluginConfig::Default()) {
    DetectInstallation();
}

UCFortranPlugin::UCFortranPlugin(const FortranPluginConfig& config) : pluginConfig(config) {
    DetectInstallation();
}

UCFortranPlugin::~UCFortranPlugin() {
    Cancel();
    if (buildThread.joinable()) buildThread.join();
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCFortranPlugin::GetPluginName() const {
    return "Fortran Compiler Plugin";
}

std::string UCFortranPlugin::GetPluginVersion() const {
    return "1.0.0";
}

CompilerType UCFortranPlugin::GetCompilerType() const {
    return CompilerType::Custom;  // No specific Fortran enum, use Custom
}

std::string UCFortranPlugin::GetCompilerName() const {
    auto it = compilerVersions.find(pluginConfig.activeCompiler);
    if (it != compilerVersions.end() && !it->second.empty()) {
        return FortranCompilerTypeToString(pluginConfig.activeCompiler) + " " + it->second;
    }
    return FortranCompilerTypeToString(pluginConfig.activeCompiler);
}

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCFortranPlugin::IsAvailable() {
    if (installationDetected) return !detectedCompilers.empty();
    return DetectInstallation();
}

std::string UCFortranPlugin::GetCompilerPath() {
    return GetCompilerExecutable();
}

void UCFortranPlugin::SetCompilerPath(const std::string& path) {
    switch (pluginConfig.activeCompiler) {
        case FortranCompilerType::GFortran: pluginConfig.gfortranPath = path; break;
        case FortranCompilerType::IntelFortran: pluginConfig.ifortPath = path; break;
        case FortranCompilerType::Flang: pluginConfig.flangPath = path; break;
        default: break;
    }
    installationDetected = false;
    DetectInstallation();
}

std::string UCFortranPlugin::GetCompilerVersion() {
    auto it = compilerVersions.find(pluginConfig.activeCompiler);
    if (it != compilerVersions.end()) return it->second;
    return "";
}

std::vector<std::string> UCFortranPlugin::GetSupportedExtensions() const {
    return {"f90", "f95", "f03", "f08", "f18", "f", "for", "fpp", "ftn", "F90", "F95", "F"};
}

bool UCFortranPlugin::CanCompile(const std::string& filePath) const {
    size_t dot = filePath.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = filePath.substr(dot + 1);
    
    auto supported = GetSupportedExtensions();
    // Case-insensitive comparison
    for (const auto& s : supported) {
        std::string lower1 = ext, lower2 = s;
        std::transform(lower1.begin(), lower1.end(), lower1.begin(), ::tolower);
        std::transform(lower2.begin(), lower2.end(), lower2.begin(), ::tolower);
        if (lower1 == lower2) return true;
    }
    return false;
}

bool UCFortranPlugin::DetectInstallation() {
    installationDetected = true;
    detectedCompilers.clear();
    compilerVersions.clear();
    
    // Try gfortran
    if (DetectCompiler(FortranCompilerType::GFortran, pluginConfig.gfortranPath) ||
        DetectCompiler(FortranCompilerType::GFortran, "gfortran")) {
        // Found
    }
    
    // Try Intel Fortran
    std::vector<std::string> intelPaths = {
        pluginConfig.ifortPath,
        "ifort",
        "ifx",  // Intel's new LLVM-based compiler
#ifndef _WIN32
        "/opt/intel/oneapi/compiler/latest/bin/ifort",
        "/opt/intel/oneapi/compiler/latest/bin/ifx",
#endif
    };
    for (const auto& p : intelPaths) {
        if (DetectCompiler(FortranCompilerType::IntelFortran, p)) break;
    }
    
    // Try Flang
    std::vector<std::string> flangPaths = {
        pluginConfig.flangPath,
        "flang",
        "flang-new",
#ifndef _WIN32
        "/usr/bin/flang",
        "/usr/local/bin/flang",
#endif
    };
    for (const auto& p : flangPaths) {
        if (DetectCompiler(FortranCompilerType::Flang, p)) break;
    }
    
    // Set active compiler to first available
    if (detectedCompilers.empty()) return false;
    
    if (detectedCompilers.find(pluginConfig.activeCompiler) == detectedCompilers.end()) {
        pluginConfig.activeCompiler = detectedCompilers.begin()->first;
    }
    
    return true;
}

bool UCFortranPlugin::DetectCompiler(FortranCompilerType type, const std::string& path) {
    if (path.empty()) return false;
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand({path, "--version"}, output, error, exitCode) && exitCode == 0) {
        detectedCompilers[type] = path;
        
        // Parse version
        std::string combined = output + error;
        std::regex versionRegex(R"((\d+\.\d+\.?\d*))");
        std::smatch match;
        if (std::regex_search(combined, match, versionRegex)) {
            compilerVersions[type] = match[1].str();
        }
        
        return true;
    }
    
    return false;
}

std::vector<FortranCompilerType> UCFortranPlugin::GetAvailableCompilers() {
    std::vector<FortranCompilerType> available;
    for (const auto& pair : detectedCompilers) {
        available.push_back(pair.first);
    }
    return available;
}

bool UCFortranPlugin::SetActiveCompiler(FortranCompilerType compiler) {
    if (detectedCompilers.find(compiler) != detectedCompilers.end()) {
        pluginConfig.activeCompiler = compiler;
        return true;
    }
    return false;
}

std::string UCFortranPlugin::GetCompilerExecutable() const {
    auto it = detectedCompilers.find(pluginConfig.activeCompiler);
    if (it != detectedCompilers.end()) return it->second;
    
    switch (pluginConfig.activeCompiler) {
        case FortranCompilerType::GFortran: return pluginConfig.gfortranPath;
        case FortranCompilerType::IntelFortran: return pluginConfig.ifortPath;
        case FortranCompilerType::Flang: return pluginConfig.flangPath;
        default: return "gfortran";
    }
}

// ============================================================================
// COMPILATION
// ============================================================================

void UCFortranPlugin::CompileAsync(
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

BuildResult UCFortranPlugin::CompileSync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config) {
    
    BuildResult result;
    result.success = false;
    
    if (sourceFiles.empty()) {
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No source files specified"});
        result.errorCount = 1;
        return result;
    }
    
    // Get compilation order based on module dependencies
    auto orderedFiles = GetCompilationOrder(sourceFiles);
    
    // Generate command line
    auto commandLine = GenerateCommandLine(orderedFiles, config);
    
    std::string output, error;
    int exitCode;
    
    auto startTime = std::chrono::steady_clock::now();
    bool executed = ExecuteCommand(commandLine, output, error, exitCode);
    auto endTime = std::chrono::steady_clock::now();
    result.buildTime = std::chrono::duration<float>(endTime - startTime).count();
    
    if (!executed) {
        result.messages.push_back({CompilerMessageType::FatalError, "", 0, 0, 
            "Failed to execute " + FortranCompilerTypeToString(pluginConfig.activeCompiler)});
        result.errorCount = 1;
        return result;
    }
    
    // Parse output
    std::string combined = output + "\n" + error;
    std::istringstream iss(combined);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        CompilerMessage msg = ParseOutputLine(line);
        if (!msg.message.empty() || !msg.file.empty()) {
            result.messages.push_back(msg);
            
            if (msg.type == CompilerMessageType::Error || msg.type == CompilerMessageType::FatalError) {
                result.errorCount++;
            } else if (msg.type == CompilerMessageType::Warning) {
                result.warningCount++;
            }
        }
    }
    
    result.success = (exitCode == 0);
    result.outputPath = config.outputPath + "/" + config.outputName;
    
    return result;
}

void UCFortranPlugin::Cancel() {
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

bool UCFortranPlugin::IsBuildInProgress() const {
    return buildInProgress;
}

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCFortranPlugin::ParseOutputLine(const std::string& line) {
    switch (pluginConfig.activeCompiler) {
        case FortranCompilerType::GFortran:
            return ParseGFortranMessage(line);
        case FortranCompilerType::IntelFortran:
            return ParseIntelMessage(line);
        case FortranCompilerType::Flang:
            return ParseFlangMessage(line);
        default:
            return ParseGFortranMessage(line);
    }
}

CompilerMessage UCFortranPlugin::ParseGFortranMessage(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    msg.line = 0;
    msg.column = 0;
    
    // gfortran format:
    // file.f90:10:5:
    // 
    //    10 |   call subroutine(x)
    //       |     1
    // Error: Procedure 'subroutine' called with...
    
    // Location line: file.f90:line:column:
    std::regex locRegex(R"(^(.+):(\d+):(\d+):?\s*$)");
    std::smatch match;
    
    if (std::regex_search(line, match, locRegex)) {
        pendingFile = match[1].str();
        pendingLine = std::stoi(match[2].str());
        pendingColumn = std::stoi(match[3].str());
        inErrorBlock = true;
        return msg;  // Return empty, wait for actual message
    }
    
    // Error/Warning line
    std::regex msgRegex(R"(^(Error|Warning|Fatal Error|Note|Info):\s*(.+)$)");
    if (std::regex_search(line, match, msgRegex)) {
        std::string level = match[1].str();
        msg.message = match[2].str();
        msg.file = pendingFile;
        msg.line = pendingLine;
        msg.column = pendingColumn;
        
        if (level == "Error" || level == "Fatal Error") {
            msg.type = CompilerMessageType::Error;
        } else if (level == "Warning") {
            msg.type = CompilerMessageType::Warning;
        } else if (level == "Note") {
            msg.type = CompilerMessageType::Note;
        }
        
        inErrorBlock = false;
        pendingFile.clear();
        pendingLine = 0;
        pendingColumn = 0;
        return msg;
    }
    
    // Simple error format: file.f90:10: Error: message
    std::regex simpleRegex(R"(^(.+):(\d+):?\s*(Error|Warning|Fatal Error):\s*(.+)$)");
    if (std::regex_search(line, match, simpleRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        std::string level = match[3].str();
        msg.message = match[4].str();
        
        if (level == "Error" || level == "Fatal Error") {
            msg.type = CompilerMessageType::Error;
        } else if (level == "Warning") {
            msg.type = CompilerMessageType::Warning;
        }
        return msg;
    }
    
    // Linker errors
    if (line.find("undefined reference") != std::string::npos ||
        line.find("cannot find") != std::string::npos) {
        msg.type = CompilerMessageType::Error;
        msg.message = line;
        return msg;
    }
    
    return msg;
}

CompilerMessage UCFortranPlugin::ParseIntelMessage(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    msg.line = 0;
    msg.column = 0;
    
    // Intel format:
    // file.f90(10): error #5082: Syntax error...
    // file.f90(10): warning #6178: ...
    
    std::regex intelRegex(R"(^(.+)\((\d+)\):\s*(error|warning|remark)\s*#(\d+):\s*(.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, intelRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        std::string level = match[3].str();
        std::string code = match[4].str();
        msg.message = "[#" + code + "] " + match[5].str();
        
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

CompilerMessage UCFortranPlugin::ParseFlangMessage(const std::string& line) {
    CompilerMessage msg;
    msg.type = CompilerMessageType::Info;
    msg.line = 0;
    msg.column = 0;
    
    // Flang format (similar to clang):
    // file.f90:10:5: error: message
    
    std::regex flangRegex(R"(^(.+):(\d+):(\d+):\s*(error|warning|note):\s*(.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, flangRegex)) {
        msg.file = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = std::stoi(match[3].str());
        std::string level = match[4].str();
        msg.message = match[5].str();
        
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

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCFortranPlugin::GenerateCommandLine(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config) {
    
    std::vector<std::string> args;
    args.push_back(GetCompilerExecutable());
    
    // Standard flags
    auto stdFlags = GetStandardFlags();
    args.insert(args.end(), stdFlags.begin(), stdFlags.end());
    
    // Warning flags
    auto warnFlags = GetWarningFlags();
    args.insert(args.end(), warnFlags.begin(), warnFlags.end());
    
    // Optimization
    if (config.optimizationLevel > 0 || config.name == "Release") {
        auto optFlags = GetOptimizationFlags(config.optimizationLevel);
        args.insert(args.end(), optFlags.begin(), optFlags.end());
    }
    
    // Debug
    if (config.debugSymbols || config.name == "Debug") {
        auto dbgFlags = GetDebugFlags();
        args.insert(args.end(), dbgFlags.begin(), dbgFlags.end());
    }
    
    // Source form
    if (pluginConfig.fixedForm) {
        if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
            args.push_back("-ffixed-form");
            if (pluginConfig.fixedLineLength == 132) {
                args.push_back("-ffixed-line-length-132");
            }
        } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
            args.push_back("-fixed");
            if (pluginConfig.fixedLineLength == 132) args.push_back("-extend-source");
        }
    } else if (pluginConfig.freeForm) {
        if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
            args.push_back("-ffree-form");
        } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
            args.push_back("-free");
        }
    }
    
    // Preprocessor
    if (pluginConfig.enablePreprocessor) {
        args.push_back("-cpp");
        for (const auto& def : pluginConfig.defines) {
            args.push_back("-D" + def);
        }
        for (const auto& undef : pluginConfig.undefines) {
            args.push_back("-U" + undef);
        }
    }
    
    // Module paths
    for (const auto& path : pluginConfig.modulePaths) {
        args.push_back("-I" + path);
    }
    if (!pluginConfig.moduleOutputDir.empty()) {
        if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
            args.push_back("-J" + pluginConfig.moduleOutputDir);
        } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
            args.push_back("-module");
            args.push_back(pluginConfig.moduleOutputDir);
        }
    }
    
    // OpenMP
    if (pluginConfig.openmp) {
        if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
            args.push_back("-fopenmp");
        } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
            args.push_back("-qopenmp");
        } else if (pluginConfig.activeCompiler == FortranCompilerType::Flang) {
            args.push_back("-fopenmp");
        }
    }
    
    // Coarray
    if (pluginConfig.coarray) {
        if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
            args.push_back("-fcoarray=" + pluginConfig.coarrayMode);
        }
    }
    
    // Output
    args.push_back("-o");
    args.push_back(config.outputPath + "/" + config.outputName);
    
    // Library paths
    for (const auto& path : pluginConfig.libraryPaths) {
        args.push_back("-L" + path);
    }
    
    // Libraries
    for (const auto& lib : pluginConfig.libraries) {
        args.push_back("-l" + lib);
    }
    
    // Static linking
    if (pluginConfig.staticLinking) {
        args.push_back("-static");
    }
    
    // Extra args from config
    for (const auto& arg : config.extraArgs) {
        args.push_back(arg);
    }
    
    // Source files
    for (const auto& file : sourceFiles) {
        args.push_back(file);
    }
    
    return args;
}

std::vector<std::string> UCFortranPlugin::GetStandardFlags() const {
    std::vector<std::string> flags;
    
    if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
        flags.push_back("-std=" + FortranStandardToString(pluginConfig.standard));
    } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
        switch (pluginConfig.standard) {
            case FortranStandard::Fortran77:   flags.push_back("-stand:f77"); break;
            case FortranStandard::Fortran90:   flags.push_back("-stand:f90"); break;
            case FortranStandard::Fortran95:   flags.push_back("-stand:f95"); break;
            case FortranStandard::Fortran2003: flags.push_back("-stand:f03"); break;
            case FortranStandard::Fortran2008: flags.push_back("-stand:f08"); break;
            case FortranStandard::Fortran2018: flags.push_back("-stand:f18"); break;
            default: break;
        }
    }
    
    return flags;
}

std::vector<std::string> UCFortranPlugin::GetWarningFlags() const {
    std::vector<std::string> flags;
    
    if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
        if (pluginConfig.wallWarnings) flags.push_back("-Wall");
        if (pluginConfig.wextraWarnings) flags.push_back("-Wextra");
        if (pluginConfig.pedantic) flags.push_back("-pedantic");
        if (pluginConfig.warningsAsErrors) flags.push_back("-Werror");
        if (pluginConfig.implicitNone) flags.push_back("-fimplicit-none");
        if (pluginConfig.arrayTempsWarning) flags.push_back("-Warray-temporaries");
    } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
        if (pluginConfig.wallWarnings) flags.push_back("-warn:all");
        if (pluginConfig.warningsAsErrors) flags.push_back("-warn:errors");
        if (pluginConfig.implicitNone) flags.push_back("-implicitnone");
    } else if (pluginConfig.activeCompiler == FortranCompilerType::Flang) {
        if (pluginConfig.wallWarnings) flags.push_back("-Wall");
        if (pluginConfig.pedantic) flags.push_back("-pedantic");
    }
    
    return flags;
}

std::vector<std::string> UCFortranPlugin::GetDebugFlags() const {
    std::vector<std::string> flags;
    
    flags.push_back("-g");
    
    if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
        if (pluginConfig.boundsCheck) flags.push_back("-fbounds-check");
        if (pluginConfig.nullPointerCheck) flags.push_back("-fcheck=pointer");
        if (pluginConfig.backtraceOnError) flags.push_back("-fbacktrace");
        if (pluginConfig.initLocal) flags.push_back("-finit-local-zero");
    } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
        if (pluginConfig.boundsCheck) flags.push_back("-check:bounds");
        if (pluginConfig.nullPointerCheck) flags.push_back("-check:pointers");
        flags.push_back("-traceback");
    }
    
    return flags;
}

std::vector<std::string> UCFortranPlugin::GetOptimizationFlags(int level) const {
    std::vector<std::string> flags;
    
    if (level <= 0) {
        flags.push_back("-O0");
    } else if (level == 1) {
        flags.push_back("-O1");
    } else if (level == 2) {
        flags.push_back("-O2");
    } else {
        flags.push_back("-O3");
    }
    
    if (pluginConfig.fastMath) {
        if (pluginConfig.activeCompiler == FortranCompilerType::GFortran) {
            flags.push_back("-ffast-math");
        } else if (pluginConfig.activeCompiler == FortranCompilerType::IntelFortran) {
            flags.push_back("-fp-model=fast");
        }
    }
    
    return flags;
}

// ============================================================================
// MODULE MANAGEMENT
// ============================================================================

std::vector<FortranModuleInfo> UCFortranPlugin::ScanModules(const std::vector<std::string>& sourceFiles) {
    std::vector<FortranModuleInfo> modules;
    
    for (const auto& file : sourceFiles) {
        ParseModuleFromSource(file, modules);
    }
    
    // Build dependency graph
    for (auto& mod : modules) {
        for (auto& other : modules) {
            if (&mod != &other) {
                for (const auto& use : other.uses) {
                    if (use == mod.name) {
                        mod.usedBy.push_back(other.name);
                    }
                }
            }
        }
    }
    
    return modules;
}

void UCFortranPlugin::ParseModuleFromSource(const std::string& sourceFile, 
                                            std::vector<FortranModuleInfo>& modules) {
    std::ifstream file(sourceFile);
    if (!file.is_open()) return;
    
    std::string line;
    FortranModuleInfo currentModule;
    bool inModule = false;
    
    std::regex moduleRegex(R"(^\s*module\s+(\w+)\s*$)", std::regex::icase);
    std::regex endModuleRegex(R"(^\s*end\s*module)", std::regex::icase);
    std::regex useRegex(R"(^\s*use\s+(\w+))", std::regex::icase);
    std::regex submoduleRegex(R"(^\s*submodule\s*\(\s*(\w+)\s*\)\s*(\w+))", std::regex::icase);
    
    while (std::getline(file, line)) {
        std::smatch match;
        
        // Check for module declaration
        if (std::regex_search(line, match, moduleRegex)) {
            if (inModule && !currentModule.name.empty()) {
                modules.push_back(currentModule);
            }
            currentModule = FortranModuleInfo();
            currentModule.name = match[1].str();
            currentModule.sourceFile = sourceFile;
            inModule = true;
        }
        // Check for submodule
        else if (std::regex_search(line, match, submoduleRegex)) {
            if (inModule && !currentModule.name.empty()) {
                modules.push_back(currentModule);
            }
            currentModule = FortranModuleInfo();
            currentModule.parentModule = match[1].str();
            currentModule.name = match[2].str();
            currentModule.sourceFile = sourceFile;
            currentModule.isSubmodule = true;
            currentModule.uses.push_back(currentModule.parentModule);
            inModule = true;
        }
        // Check for USE statement
        else if (std::regex_search(line, match, useRegex)) {
            std::string usedMod = match[1].str();
            // Convert to lowercase for comparison
            std::transform(usedMod.begin(), usedMod.end(), usedMod.begin(), ::tolower);
            if (inModule) {
                currentModule.uses.push_back(usedMod);
            }
        }
        // Check for end module
        else if (std::regex_search(line, match, endModuleRegex)) {
            if (inModule && !currentModule.name.empty()) {
                modules.push_back(currentModule);
                currentModule = FortranModuleInfo();
                inModule = false;
            }
        }
    }
    
    // Don't forget last module if file ended without END MODULE
    if (inModule && !currentModule.name.empty()) {
        modules.push_back(currentModule);
    }
}

std::vector<std::string> UCFortranPlugin::GetModuleDependencies(const std::string& sourceFile) {
    std::vector<FortranModuleInfo> modules;
    ParseModuleFromSource(sourceFile, modules);
    
    std::set<std::string> deps;
    for (const auto& mod : modules) {
        for (const auto& use : mod.uses) {
            deps.insert(use);
        }
    }
    
    return std::vector<std::string>(deps.begin(), deps.end());
}

std::vector<std::string> UCFortranPlugin::GetCompilationOrder(const std::vector<std::string>& sourceFiles) {
    // Scan all modules
    auto modules = ScanModules(sourceFiles);
    
    // Build file -> modules map
    std::map<std::string, std::vector<std::string>> fileModules;
    std::map<std::string, std::string> moduleToFile;
    
    for (const auto& mod : modules) {
        fileModules[mod.sourceFile].push_back(mod.name);
        moduleToFile[mod.name] = mod.sourceFile;
    }
    
    // Topological sort
    std::vector<std::string> ordered;
    std::set<std::string> visited;
    std::set<std::string> inProgress;
    
    std::function<bool(const std::string&)> visit = [&](const std::string& file) -> bool {
        if (inProgress.count(file)) return false;  // Cycle detected
        if (visited.count(file)) return true;
        
        inProgress.insert(file);
        
        // Get dependencies
        auto deps = GetModuleDependencies(file);
        for (const auto& dep : deps) {
            auto it = moduleToFile.find(dep);
            if (it != moduleToFile.end() && it->second != file) {
                if (!visit(it->second)) return false;
            }
        }
        
        inProgress.erase(file);
        visited.insert(file);
        ordered.push_back(file);
        return true;
    };
    
    for (const auto& file : sourceFiles) {
        visit(file);
    }
    
    return ordered;
}

std::string UCFortranPlugin::FindModuleFile(const std::string& moduleName) {
    std::string modFile = moduleName + ".mod";
    
    // Check module output directory first
    if (!pluginConfig.moduleOutputDir.empty()) {
        std::string path = pluginConfig.moduleOutputDir + "/" + modFile;
        std::ifstream test(path);
        if (test.good()) return path;
    }
    
    // Check module search paths
    for (const auto& dir : pluginConfig.modulePaths) {
        std::string path = dir + "/" + modFile;
        std::ifstream test(path);
        if (test.good()) return path;
    }
    
    return "";
}

// ============================================================================
// SOURCE ANALYSIS
// ============================================================================

bool UCFortranPlugin::IsFixedForm(const std::string& filePath) const {
    size_t dot = filePath.rfind('.');
    if (dot == std::string::npos) return false;
    
    std::string ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Fixed form extensions
    return (ext == "f" || ext == "for" || ext == "ftn" || ext == "f77");
}

std::vector<FortranProgramUnit> UCFortranPlugin::ScanProgramUnits(const std::string& sourceFile) {
    std::vector<FortranProgramUnit> units;
    
    std::ifstream file(sourceFile);
    if (!file.is_open()) return units;
    
    std::string line;
    int lineNum = 0;
    
    std::regex programRegex(R"(^\s*program\s+(\w+))", std::regex::icase);
    std::regex moduleRegex(R"(^\s*module\s+(\w+))", std::regex::icase);
    std::regex subroutineRegex(R"(^\s*subroutine\s+(\w+)\s*\(([^)]*)\))", std::regex::icase);
    std::regex functionRegex(R"(^\s*(\w+\s+)?function\s+(\w+)\s*\(([^)]*)\))", std::regex::icase);
    std::regex endRegex(R"(^\s*end\s*(program|module|subroutine|function))", std::regex::icase);
    
    FortranProgramUnit currentUnit;
    bool inUnit = false;
    
    while (std::getline(file, line)) {
        lineNum++;
        std::smatch match;
        
        if (std::regex_search(line, match, programRegex)) {
            currentUnit = FortranProgramUnit();
            currentUnit.type = FortranUnitType::Program;
            currentUnit.name = match[1].str();
            currentUnit.sourceFile = sourceFile;
            currentUnit.startLine = lineNum;
            inUnit = true;
        } else if (std::regex_search(line, match, moduleRegex)) {
            currentUnit = FortranProgramUnit();
            currentUnit.type = FortranUnitType::Module;
            currentUnit.name = match[1].str();
            currentUnit.sourceFile = sourceFile;
            currentUnit.startLine = lineNum;
            inUnit = true;
        } else if (std::regex_search(line, match, subroutineRegex)) {
            currentUnit = FortranProgramUnit();
            currentUnit.type = FortranUnitType::Subroutine;
            currentUnit.name = match[1].str();
            currentUnit.sourceFile = sourceFile;
            currentUnit.startLine = lineNum;
            // Parse arguments
            std::string args = match[2].str();
            std::istringstream argStream(args);
            std::string arg;
            while (std::getline(argStream, arg, ',')) {
                arg.erase(0, arg.find_first_not_of(" \t"));
                arg.erase(arg.find_last_not_of(" \t") + 1);
                if (!arg.empty()) currentUnit.arguments.push_back(arg);
            }
            inUnit = true;
        } else if (std::regex_search(line, match, functionRegex)) {
            currentUnit = FortranProgramUnit();
            currentUnit.type = FortranUnitType::Function;
            currentUnit.returnType = match[1].str();
            currentUnit.name = match[2].str();
            currentUnit.sourceFile = sourceFile;
            currentUnit.startLine = lineNum;
            inUnit = true;
        } else if (std::regex_search(line, match, endRegex)) {
            if (inUnit) {
                currentUnit.endLine = lineNum;
                units.push_back(currentUnit);
                inUnit = false;
            }
        }
    }
    
    return units;
}

BuildResult UCFortranPlugin::CheckSyntax(const std::string& sourceFile) {
    BuildResult result;
    
    std::vector<std::string> args;
    args.push_back(GetCompilerExecutable());
    args.push_back("-fsyntax-only");
    args.push_back(sourceFile);
    
    std::string output, error;
    int exitCode;
    
    ExecuteCommand(args, output, error, exitCode);
    
    std::string combined = output + "\n" + error;
    std::istringstream iss(combined);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        CompilerMessage msg = ParseOutputLine(line);
        if (!msg.message.empty()) {
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error) result.errorCount++;
            else if (msg.type == CompilerMessageType::Warning) result.warningCount++;
        }
    }
    
    result.success = (exitCode == 0);
    return result;
}

std::string UCFortranPlugin::Preprocess(const std::string& sourceFile) {
    std::vector<std::string> args;
    args.push_back(GetCompilerExecutable());
    args.push_back("-E");
    args.push_back("-cpp");
    args.push_back(sourceFile);
    
    std::string output, error;
    int exitCode;
    
    if (ExecuteCommand(args, output, error, exitCode) && exitCode == 0) {
        return output;
    }
    return error;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UCFortranPlugin::SetConfig(const FortranPluginConfig& config) {
    pluginConfig = config;
    installationDetected = false;
    detectedCompilers.clear();
    compilerVersions.clear();
    moduleCache.clear();
    DetectInstallation();
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCFortranPlugin::ExecuteCommand(const std::vector<std::string>& args,
                                     std::string& output,
                                     std::string& error,
                                     int& exitCode) {
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
    
    if (!CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
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
