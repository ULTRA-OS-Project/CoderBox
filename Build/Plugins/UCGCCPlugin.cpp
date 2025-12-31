// Apps/IDE/Build/Plugins/UCGCCPlugin.cpp
// GCC/G++ Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGCCPlugin.h"
#include "../UCBuildManager.h"
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <regex>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <signal.h>
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// GCC PLUGIN CONFIG
// ============================================================================

GCCPluginConfig GCCPluginConfig::Default() {
    GCCPluginConfig config;
    
#ifdef _WIN32
    // Windows - look for MinGW or MSYS2
    config.gccPath = "gcc.exe";
    config.gppPath = "g++.exe";
    config.arPath = "ar.exe";
    config.ldPath = "ld.exe";
#else
    // Unix-like
    config.gccPath = "gcc";
    config.gppPath = "g++";
    config.arPath = "ar";
    config.ldPath = "ld";
#endif
    
    config.cStandard = "c11";
    config.cppStandard = "c++20";
    config.colorDiagnostics = true;
    config.showColumn = true;
    config.caretDiagnostics = true;
    config.parallelJobs = 0;
    config.verboseOutput = false;
    config.saveTemps = false;
    
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCGCCPlugin::UCGCCPlugin(bool useCpp)
    : useCpp(useCpp)
    , pluginConfig(GCCPluginConfig::Default())
    , outputParser(std::make_unique<GCCOutputParser>())
{
    DetectInstallation();
}

UCGCCPlugin::UCGCCPlugin(const GCCPluginConfig& config, bool useCpp)
    : useCpp(useCpp)
    , pluginConfig(config)
    , outputParser(std::make_unique<GCCOutputParser>())
{
    DetectInstallation();
}

UCGCCPlugin::~UCGCCPlugin() {
    Cancel();
    if (buildThread.joinable()) {
        buildThread.join();
    }
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCGCCPlugin::GetPluginName() const {
    return useCpp ? "GCC C++ Plugin" : "GCC C Plugin";
}

std::string UCGCCPlugin::GetPluginVersion() const {
    return "1.0.0";
}

CompilerType UCGCCPlugin::GetCompilerType() const {
    return useCpp ? CompilerType::GPlusPlus : CompilerType::GCC;
}

std::string UCGCCPlugin::GetCompilerName() const {
    return useCpp ? "GNU C++ Compiler (g++)" : "GNU C Compiler (gcc)";
}

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCGCCPlugin::IsAvailable() {
    std::string path = GetCompilerPath();
    return CommandExists(path);
}

std::string UCGCCPlugin::GetCompilerPath() {
    if (!compilerPath.empty()) {
        return compilerPath;
    }
    
    // Use configured path
    std::string configPath = useCpp ? pluginConfig.gppPath : pluginConfig.gccPath;
    
    // Try to find in PATH
    std::string foundPath = FindCommand(configPath);
    if (!foundPath.empty()) {
        return foundPath;
    }
    
    // Return configured path (might be relative or in PATH)
    return configPath;
}

void UCGCCPlugin::SetCompilerPath(const std::string& path) {
    compilerPath = path;
    versionCached = false;
}

std::string UCGCCPlugin::GetCompilerVersion() {
    if (versionCached) {
        return cachedVersion;
    }
    
    std::string version;
    std::string compilerExe = GetCompilerPath();
    
    // Execute compiler --version
    ExecuteProcess(
        compilerExe,
        {"--version"},
        "",
        [&version](const std::string& line) {
            if (version.empty()) {
                // First line usually contains version info
                // Example: "g++ (GCC) 13.2.0"
                version = line;
            }
        }
    );
    
    // Extract just the version number
    std::regex versionRegex(R"((\d+\.\d+\.\d+))");
    std::smatch match;
    if (std::regex_search(version, match, versionRegex)) {
        cachedVersion = match[1].str();
    } else {
        cachedVersion = version;
    }
    
    versionCached = true;
    return cachedVersion;
}

std::vector<std::string> UCGCCPlugin::GetSupportedExtensions() const {
    if (useCpp) {
        return {".cpp", ".cc", ".cxx", ".c++", ".C", ".hpp", ".hxx", ".h"};
    } else {
        return {".c", ".h"};
    }
}

bool UCGCCPlugin::CanCompile(const std::string& filePath) const {
    auto extensions = GetSupportedExtensions();
    std::string ext = GetFileExtension(filePath);
    
    // Convert to lowercase for comparison
    std::string lowerExt = "." + ext;
    for (char& c : lowerExt) {
        c = std::tolower(c);
    }
    
    for (const auto& supported : extensions) {
        std::string lowerSupported = supported;
        for (char& c : lowerSupported) {
            c = std::tolower(c);
        }
        if (lowerExt == lowerSupported) {
            return true;
        }
    }
    
    return false;
}

bool UCGCCPlugin::DetectInstallation() {
    // Try to find gcc/g++
    std::string path = useCpp ? pluginConfig.gppPath : pluginConfig.gccPath;
    std::string foundPath = FindCommand(path);
    
    if (!foundPath.empty()) {
        if (useCpp) {
            pluginConfig.gppPath = foundPath;
        } else {
            pluginConfig.gccPath = foundPath;
        }
        return true;
    }
    
    // Try common locations
    std::vector<std::string> commonPaths = {
#ifdef _WIN32
        "C:\\MinGW\\bin\\",
        "C:\\msys64\\mingw64\\bin\\",
        "C:\\msys64\\ucrt64\\bin\\",
        "C:\\Program Files\\mingw-w64\\bin\\",
#else
        "/usr/bin/",
        "/usr/local/bin/",
        "/opt/local/bin/",
#endif
    };
    
    std::string exeName = useCpp ? 
#ifdef _WIN32
        "g++.exe" : "gcc.exe";
#else
        "g++" : "gcc";
#endif
    
    for (const auto& basePath : commonPaths) {
        std::string fullPath = basePath + exeName;
        if (CommandExists(fullPath)) {
            if (useCpp) {
                pluginConfig.gppPath = fullPath;
            } else {
                pluginConfig.gccPath = fullPath;
            }
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// COMPILATION
// ============================================================================

void UCGCCPlugin::CompileAsync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config,
    std::function<void(const BuildResult&)> onComplete,
    std::function<void(const std::string&)> onOutputLine,
    std::function<void(float)> onProgress
) {
    if (buildInProgress) {
        BuildResult result;
        result.success = false;
        result.exitCode = -1;
        CompilerMessage msg;
        msg.type = CompilerMessageType::Error;
        msg.message = "Build already in progress";
        result.messages.push_back(msg);
        result.errorCount = 1;
        if (onComplete) onComplete(result);
        return;
    }
    
    // Store callbacks
    asyncOnComplete = onComplete;
    asyncOnOutputLine = onOutputLine;
    asyncOnProgress = onProgress;
    
    buildInProgress = true;
    cancelRequested = false;
    
    // Start build thread
    buildThread = std::thread([this, sourceFiles, config]() {
        BuildResult result = CompileSync(sourceFiles, config);
        
        buildInProgress = false;
        
        if (asyncOnComplete) {
            asyncOnComplete(result);
        }
    });
    
    buildThread.detach();
}

BuildResult UCGCCPlugin::CompileSync(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config
) {
    BuildResult result;
    result.success = false;
    result.exitCode = -1;
    
    auto startTime = std::chrono::steady_clock::now();
    
    if (sourceFiles.empty()) {
        CompilerMessage msg;
        msg.type = CompilerMessageType::Error;
        msg.message = "No source files specified";
        result.messages.push_back(msg);
        result.errorCount = 1;
        return result;
    }
    
    if (!IsAvailable()) {
        CompilerMessage msg;
        msg.type = CompilerMessageType::Error;
        msg.message = "Compiler not found: " + GetCompilerPath();
        result.messages.push_back(msg);
        result.errorCount = 1;
        return result;
    }
    
    // Ensure output directory exists
    if (!config.outputDirectory.empty()) {
        EnsureDirectoryExists(config.outputDirectory);
    }
    
    // Build command line
    std::vector<std::string> args = BuildCompileArgs(sourceFiles, config);
    
    // Collect output
    std::vector<std::string> outputLines;
    int totalFiles = static_cast<int>(sourceFiles.size());
    int processedFiles = 0;
    
    // Execute compiler
    std::string compilerExe = GetCompilerPath();
    
    int exitCode = ExecuteProcess(
        compilerExe,
        args,
        "",  // Working directory
        [this, &outputLines, &result, &processedFiles, totalFiles](const std::string& line) {
            outputLines.push_back(line);
            result.rawOutput += line + "\n";
            
            // Parse output line
            CompilerMessage msg = ParseOutputLine(line);
            if (!msg.message.empty()) {
                result.messages.push_back(msg);
                if (msg.IsError()) result.errorCount++;
                if (msg.IsWarning()) result.warningCount++;
            }
            
            // Check for file completion (heuristic)
            if (line.find("Compiling") != std::string::npos || 
                line.find(".o") != std::string::npos) {
                processedFiles++;
                if (asyncOnProgress && totalFiles > 0) {
                    asyncOnProgress(static_cast<float>(processedFiles) / totalFiles);
                }
            }
            
            if (asyncOnOutputLine) {
                asyncOnOutputLine(line);
            }
        }
    );
    
    auto endTime = std::chrono::steady_clock::now();
    result.buildTimeSeconds = std::chrono::duration<double>(endTime - startTime).count();
    
    result.exitCode = exitCode;
    result.success = (exitCode == 0) && (result.errorCount == 0);
    
    if (result.success && !config.outputName.empty()) {
        result.outputFile = config.GetOutputPath();
    }
    
    return result;
}

// ============================================================================
// BUILD CONTROL
// ============================================================================

void UCGCCPlugin::Cancel() {
    cancelRequested = true;
    
#ifndef _WIN32
    if (currentProcessId > 0) {
        kill(currentProcessId, SIGTERM);
    }
#endif
}

bool UCGCCPlugin::IsBuildInProgress() const {
    return buildInProgress;
}

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCGCCPlugin::ParseOutputLine(const std::string& line) {
    if (outputParser) {
        return outputParser->ParseLine(line);
    }
    
    CompilerMessage msg;
    msg.rawLine = line;
    msg.message = line;
    return msg;
}

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCGCCPlugin::GenerateCommandLine(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config
) {
    return BuildCompileArgs(sourceFiles, config);
}

std::vector<std::string> UCGCCPlugin::BuildCompileArgs(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config
) {
    std::vector<std::string> args;
    
    // Language standard
    if (useCpp) {
        args.push_back("-std=" + pluginConfig.cppStandard);
    } else {
        args.push_back("-std=" + pluginConfig.cStandard);
    }
    
    // Optimization
    args.push_back(GetOptimizationFlag(config.optimizationLevel));
    
    // Debug symbols
    if (config.debugSymbols) {
        args.push_back("-g");
    }
    
    // Warnings
    if (config.enableWarnings) {
        args.push_back("-Wall");
        args.push_back("-Wextra");
    }
    
    if (config.treatWarningsAsErrors) {
        args.push_back("-Werror");
    }
    
    // Diagnostics
    if (pluginConfig.colorDiagnostics) {
        args.push_back("-fdiagnostics-color=always");
    }
    
    // Preprocessor defines
    for (const auto& define : config.defines) {
        args.push_back("-D" + define);
    }
    
    // Include paths
    for (const auto& includePath : config.includePaths) {
        args.push_back("-I" + includePath);
    }
    
    // Additional compiler flags
    for (const auto& flag : config.compilerFlags) {
        args.push_back(flag);
    }
    
    // Output type specific flags
    switch (config.outputType) {
        case BuildOutputType::SharedLibrary:
            args.push_back("-shared");
            args.push_back("-fPIC");
            break;
        case BuildOutputType::StaticLibrary:
            // Will use ar for static library
            args.push_back("-c");  // Compile only, don't link
            break;
        case BuildOutputType::Executable:
        default:
            break;
    }
    
    // Output file
    if (!config.outputName.empty() && config.outputType != BuildOutputType::StaticLibrary) {
        args.push_back("-o");
        args.push_back(config.GetOutputPath());
    }
    
    // Source files
    for (const auto& sourceFile : sourceFiles) {
        args.push_back(sourceFile);
    }
    
    // Library paths (for linking)
    if (config.outputType != BuildOutputType::StaticLibrary) {
        for (const auto& libPath : config.libraryPaths) {
            args.push_back("-L" + libPath);
        }
        
        // Libraries
        for (const auto& lib : config.libraries) {
            args.push_back("-l" + lib);
        }
        
        // Linker flags
        for (const auto& flag : config.linkerFlags) {
            args.push_back("-Wl," + flag);
        }
    }
    
    return args;
}

std::vector<std::string> UCGCCPlugin::BuildLinkArgs(
    const std::vector<std::string>& objectFiles,
    const BuildConfiguration& config
) {
    std::vector<std::string> args;
    
    // Output file
    args.push_back("-o");
    args.push_back(config.GetOutputPath());
    
    // Object files
    for (const auto& objFile : objectFiles) {
        args.push_back(objFile);
    }
    
    // Library paths
    for (const auto& libPath : config.libraryPaths) {
        args.push_back("-L" + libPath);
    }
    
    // Libraries
    for (const auto& lib : config.libraries) {
        args.push_back("-l" + lib);
    }
    
    // Linker flags
    for (const auto& flag : config.linkerFlags) {
        args.push_back("-Wl," + flag);
    }
    
    return args;
}

std::string UCGCCPlugin::GetOptimizationFlag(int level) const {
    switch (level) {
        case 0: return "-O0";
        case 1: return "-O1";
        case 2: return "-O2";
        case 3: return "-O3";
        default: return "-O0";
    }
}

int UCGCCPlugin::ExecuteCompiler(
    const std::vector<std::string>& args,
    const std::string& workDir,
    std::function<void(const std::string&)> outputCallback
) {
    std::string compilerExe = GetCompilerPath();
    return ExecuteProcess(compilerExe, args, workDir, outputCallback);
}

std::string UCGCCPlugin::GetObjectFilePath(const std::string& sourceFile, 
                                           const std::string& outputDir) const {
    std::string fileName = GetFileName(sourceFile);
    
    // Replace extension with .o
    size_t dotPos = fileName.rfind('.');
    if (dotPos != std::string::npos) {
        fileName = fileName.substr(0, dotPos);
    }
    fileName += ".o";
    
    if (outputDir.empty()) {
        return fileName;
    }
    
    return JoinPath(outputDir, fileName);
}

bool UCGCCPlugin::EnsureDirectoryExists(const std::string& path) {
    if (path.empty()) return true;
    
#ifdef _WIN32
    // Windows - use _mkdir
    std::string currentPath;
    for (char c : path) {
        if (c == '/' || c == '\\') {
            if (!currentPath.empty()) {
                _mkdir(currentPath.c_str());
            }
        }
        currentPath += c;
    }
    if (!currentPath.empty()) {
        _mkdir(currentPath.c_str());
    }
    return true;
#else
    // POSIX - use mkdir with mode 0755
    std::string currentPath;
    for (char c : path) {
        if (c == '/') {
            if (!currentPath.empty()) {
                mkdir(currentPath.c_str(), 0755);
            }
        }
        currentPath += c;
    }
    if (!currentPath.empty()) {
        mkdir(currentPath.c_str(), 0755);
    }
    return true;
#endif
}

std::vector<std::string> UCGCCPlugin::GetAvailableWarningFlags() const {
    return {
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-Wno-unused-parameter",
        "-Wno-unused-variable",
        "-Wconversion",
        "-Wshadow",
        "-Wformat=2",
        "-Wcast-align",
        "-Wcast-qual",
        "-Wdouble-promotion",
        "-Wfloat-equal",
        "-Wlogical-op",
        "-Wmissing-declarations",
        "-Wredundant-decls",
        "-Wstrict-overflow=5",
        "-Wswitch-default",
        "-Wundef"
    };
}

std::vector<std::string> UCGCCPlugin::GetOptimizationLevels() const {
    return {"-O0", "-O1", "-O2", "-O3", "-Os", "-Ofast", "-Og"};
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

std::shared_ptr<UCGCCPlugin> CreateGCCPlugin() {
    return std::make_shared<UCGCCPlugin>(false);
}

std::shared_ptr<UCGCCPlugin> CreateGPlusPlusPlugin() {
    return std::make_shared<UCGCCPlugin>(true);
}

void RegisterGCCPlugins() {
    auto& registry = UCCompilerPluginRegistry::Instance();
    
    // Register GCC (C compiler)
    registry.RegisterPlugin(CreateGCCPlugin());
    
    // Register G++ (C++ compiler)
    registry.RegisterPlugin(CreateGPlusPlusPlugin());
}

} // namespace IDE
} // namespace UltraCanvas
