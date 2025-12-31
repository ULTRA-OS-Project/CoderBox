// Apps/IDE/Build/Plugins/UCFreePascalPlugin.cpp
// FreePascal Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCFreePascalPlugin.h"
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
// FREEPASCAL PLUGIN CONFIG
// ============================================================================

FreePascalPluginConfig FreePascalPluginConfig::Default() {
    FreePascalPluginConfig config;
    
#ifdef _WIN32
    config.fpcPath = "fpc.exe";
#else
    config.fpcPath = "fpc";
#endif
    
    config.syntaxMode = SyntaxMode::ObjFPC;
    config.smartLinking = true;
    config.stackChecking = false;
    config.rangeChecking = false;
    config.overflowChecking = false;
    config.ioChecking = true;
    config.targetOS = "";
    config.targetCPU = "";
    config.generateAssembly = false;
    config.stripSymbols = false;
    config.verbosity = Verbosity::All;
    
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCFreePascalPlugin::UCFreePascalPlugin()
    : pluginConfig(FreePascalPluginConfig::Default())
    , outputParser(std::make_unique<FreePascalOutputParser>())
{
    DetectInstallation();
}

UCFreePascalPlugin::UCFreePascalPlugin(const FreePascalPluginConfig& config)
    : pluginConfig(config)
    , outputParser(std::make_unique<FreePascalOutputParser>())
{
    DetectInstallation();
}

UCFreePascalPlugin::~UCFreePascalPlugin() {
    Cancel();
    if (buildThread.joinable()) {
        buildThread.join();
    }
}

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCFreePascalPlugin::GetPluginName() const {
    return "FreePascal Plugin";
}

std::string UCFreePascalPlugin::GetPluginVersion() const {
    return "1.0.0";
}

CompilerType UCFreePascalPlugin::GetCompilerType() const {
    return CompilerType::FreePascal;
}

std::string UCFreePascalPlugin::GetCompilerName() const {
    return "Free Pascal Compiler (fpc)";
}

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCFreePascalPlugin::IsAvailable() {
    std::string path = GetCompilerPath();
    return CommandExists(path);
}

std::string UCFreePascalPlugin::GetCompilerPath() {
    if (!compilerPath.empty()) {
        return compilerPath;
    }
    
    // Try to find in PATH
    std::string foundPath = FindCommand(pluginConfig.fpcPath);
    if (!foundPath.empty()) {
        return foundPath;
    }
    
    return pluginConfig.fpcPath;
}

void UCFreePascalPlugin::SetCompilerPath(const std::string& path) {
    compilerPath = path;
    versionCached = false;
}

std::string UCFreePascalPlugin::GetCompilerVersion() {
    if (versionCached) {
        return cachedVersion;
    }
    
    std::string version;
    std::string compilerExe = GetCompilerPath();
    
    // Execute fpc -v to get version
    ExecuteProcess(
        compilerExe,
        {"-v"},
        "",
        [&version](const std::string& line) {
            // First line contains version info
            // Example: "Free Pascal Compiler version 3.2.2"
            if (version.empty() && line.find("Free Pascal") != std::string::npos) {
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

std::vector<std::string> UCFreePascalPlugin::GetSupportedExtensions() const {
    return {".pas", ".pp", ".p", ".lpr", ".dpr", ".inc"};
}

bool UCFreePascalPlugin::CanCompile(const std::string& filePath) const {
    auto extensions = GetSupportedExtensions();
    std::string ext = GetFileExtension(filePath);
    
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

bool UCFreePascalPlugin::DetectInstallation() {
    // Try to find fpc
    std::string foundPath = FindCommand(pluginConfig.fpcPath);
    if (!foundPath.empty()) {
        pluginConfig.fpcPath = foundPath;
        return true;
    }
    
    // Try common locations
    std::vector<std::string> commonPaths = {
#ifdef _WIN32
        "C:\\FPC\\bin\\",
        "C:\\lazarus\\fpc\\bin\\",
        "C:\\Program Files\\FreePascal\\bin\\",
        "C:\\Program Files (x86)\\FreePascal\\bin\\",
#else
        "/usr/bin/",
        "/usr/local/bin/",
        "/opt/fpc/bin/",
        "/usr/lib/fpc/",
#endif
    };
    
#ifdef _WIN32
    std::string exeName = "fpc.exe";
#else
    std::string exeName = "fpc";
#endif
    
    for (const auto& basePath : commonPaths) {
        std::string fullPath = basePath + exeName;
        if (CommandExists(fullPath)) {
            pluginConfig.fpcPath = fullPath;
            return true;
        }
    }
    
    // Try version-specific paths on Linux
#ifndef _WIN32
    // Check /usr/lib/fpc/<version>/fpc
    std::string versionedPath = "/usr/lib/fpc/";
    // Would need to scan directory for version folders
#endif
    
    return false;
}

std::string UCFreePascalPlugin::GetInstallationDirectory() const {
    std::string path = pluginConfig.fpcPath;
    
    // Remove executable name to get directory
    size_t lastSlash = path.rfind('/');
    if (lastSlash == std::string::npos) {
        lastSlash = path.rfind('\\');
    }
    
    if (lastSlash != std::string::npos) {
        return path.substr(0, lastSlash);
    }
    
    return "";
}

// ============================================================================
// COMPILATION
// ============================================================================

void UCFreePascalPlugin::CompileAsync(
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
    
    asyncOnComplete = onComplete;
    asyncOnOutputLine = onOutputLine;
    asyncOnProgress = onProgress;
    
    buildInProgress = true;
    cancelRequested = false;
    
    buildThread = std::thread([this, sourceFiles, config]() {
        BuildResult result = CompileSync(sourceFiles, config);
        
        buildInProgress = false;
        
        if (asyncOnComplete) {
            asyncOnComplete(result);
        }
    });
    
    buildThread.detach();
}

BuildResult UCFreePascalPlugin::CompileSync(
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
    
    // FreePascal typically compiles the main program file
    // which pulls in units automatically
    std::string mainFile = sourceFiles[0];
    
    // Build command line
    std::vector<std::string> args = BuildCompileArgs(sourceFiles, config);
    
    // Collect output
    std::vector<std::string> outputLines;
    
    std::string compilerExe = GetCompilerPath();
    
    int exitCode = ExecuteProcess(
        compilerExe,
        args,
        "",
        [this, &outputLines, &result](const std::string& line) {
            outputLines.push_back(line);
            result.rawOutput += line + "\n";
            
            // Parse output line
            CompilerMessage msg = ParseOutputLine(line);
            if (!msg.message.empty()) {
                result.messages.push_back(msg);
                if (msg.IsError()) result.errorCount++;
                if (msg.IsWarning()) result.warningCount++;
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

void UCFreePascalPlugin::Cancel() {
    cancelRequested = true;
    
#ifndef _WIN32
    if (currentProcessId > 0) {
        kill(currentProcessId, SIGTERM);
    }
#endif
}

bool UCFreePascalPlugin::IsBuildInProgress() const {
    return buildInProgress;
}

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCFreePascalPlugin::ParseOutputLine(const std::string& line) {
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

std::vector<std::string> UCFreePascalPlugin::GenerateCommandLine(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config
) {
    return BuildCompileArgs(sourceFiles, config);
}

std::vector<std::string> UCFreePascalPlugin::BuildCompileArgs(
    const std::vector<std::string>& sourceFiles,
    const BuildConfiguration& config
) {
    std::vector<std::string> args;
    
    // Syntax mode
    args.push_back(GetSyntaxModeFlag());
    
    // Verbosity
    args.push_back(GetVerbosityFlag());
    
    // Optimization
    args.push_back(GetOptimizationFlag(config.optimizationLevel));
    
    // Debug symbols
    if (config.debugSymbols) {
        args.push_back("-g");       // Generate debug info
        args.push_back("-gl");      // Line info for debugger
        args.push_back("-gw");      // DWARF debug info
    }
    
    // Code checking options
    if (pluginConfig.stackChecking) {
        args.push_back("-Ct");
    }
    if (pluginConfig.rangeChecking) {
        args.push_back("-Cr");
    }
    if (pluginConfig.overflowChecking) {
        args.push_back("-Co");
    }
    if (pluginConfig.ioChecking) {
        args.push_back("-Ci");
    }
    
    // Smart linking
    if (pluginConfig.smartLinking) {
        args.push_back("-CX");      // Create smart-linkable units
        args.push_back("-XX");      // Link smart
    }
    
    // Strip symbols (release)
    if (pluginConfig.stripSymbols || !config.debugSymbols) {
        args.push_back("-Xs");
    }
    
    // Target OS
    if (!pluginConfig.targetOS.empty()) {
        args.push_back("-T" + pluginConfig.targetOS);
    }
    
    // Target CPU
    if (!pluginConfig.targetCPU.empty()) {
        args.push_back("-P" + pluginConfig.targetCPU);
    }
    
    // Preprocessor defines
    for (const auto& define : config.defines) {
        args.push_back("-d" + define);
    }
    
    // Unit paths
    for (const auto& unitPath : pluginConfig.unitPaths) {
        args.push_back("-Fu" + unitPath);
    }
    
    // Include paths
    for (const auto& includePath : config.includePaths) {
        args.push_back("-Fi" + includePath);
    }
    for (const auto& includePath : pluginConfig.includePaths) {
        args.push_back("-Fi" + includePath);
    }
    
    // Library paths
    for (const auto& libPath : config.libraryPaths) {
        args.push_back("-Fl" + libPath);
    }
    for (const auto& libPath : pluginConfig.libraryPaths) {
        args.push_back("-Fl" + libPath);
    }
    
    // Output directory
    if (!config.outputDirectory.empty()) {
        args.push_back("-FE" + config.outputDirectory);
        args.push_back("-FU" + config.outputDirectory);  // Units output dir
    }
    
    // Output filename
    if (!config.outputName.empty()) {
        args.push_back("-o" + config.outputName);
    }
    
    // Additional compiler flags
    for (const auto& flag : config.compilerFlags) {
        args.push_back(flag);
    }
    
    // Libraries to link
    for (const auto& lib : config.libraries) {
        args.push_back("-k-l" + lib);
    }
    
    // Linker flags
    for (const auto& flag : config.linkerFlags) {
        args.push_back("-k" + flag);
    }
    
    // Main source file (FreePascal compiles main program which uses units)
    if (!sourceFiles.empty()) {
        args.push_back(sourceFiles[0]);
    }
    
    return args;
}

std::string UCFreePascalPlugin::GetSyntaxModeFlag() const {
    switch (pluginConfig.syntaxMode) {
        case FreePascalPluginConfig::SyntaxMode::FPC:
            return "-Mfpc";
        case FreePascalPluginConfig::SyntaxMode::ObjFPC:
            return "-Mobjfpc";
        case FreePascalPluginConfig::SyntaxMode::Delphi:
            return "-Mdelphi";
        case FreePascalPluginConfig::SyntaxMode::DelphiUnicode:
            return "-Mdelphiunicode";
        case FreePascalPluginConfig::SyntaxMode::TP:
            return "-Mtp";
        case FreePascalPluginConfig::SyntaxMode::MacPas:
            return "-Mmacpas";
        default:
            return "-Mobjfpc";
    }
}

std::string UCFreePascalPlugin::GetVerbosityFlag() const {
    switch (pluginConfig.verbosity) {
        case FreePascalPluginConfig::Verbosity::Quiet:
            return "-v0";
        case FreePascalPluginConfig::Verbosity::Errors:
            return "-ve";
        case FreePascalPluginConfig::Verbosity::ErrorsWarnings:
            return "-vew";
        case FreePascalPluginConfig::Verbosity::All:
            return "-vewnh";
        case FreePascalPluginConfig::Verbosity::Debug:
            return "-va";
        default:
            return "-vewnh";
    }
}

std::string UCFreePascalPlugin::GetOptimizationFlag(int level) const {
    switch (level) {
        case 0:
            return "-O-";       // No optimization
        case 1:
            return "-O1";       // Level 1
        case 2:
            return "-O2";       // Level 2
        case 3:
            return "-O3";       // Level 3 (aggressive)
        default:
            return "-O-";
    }
}

int UCFreePascalPlugin::ExecuteCompiler(
    const std::vector<std::string>& args,
    const std::string& workDir,
    std::function<void(const std::string&)> outputCallback
) {
    std::string compilerExe = GetCompilerPath();
    return ExecuteProcess(compilerExe, args, workDir, outputCallback);
}

bool UCFreePascalPlugin::EnsureDirectoryExists(const std::string& path) {
    if (path.empty()) return true;
    
#ifdef _WIN32
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

std::vector<std::pair<FreePascalPluginConfig::SyntaxMode, std::string>> 
UCFreePascalPlugin::GetSyntaxModes() const {
    return {
        {FreePascalPluginConfig::SyntaxMode::FPC, "FPC (Default)"},
        {FreePascalPluginConfig::SyntaxMode::ObjFPC, "Object Pascal"},
        {FreePascalPluginConfig::SyntaxMode::Delphi, "Delphi"},
        {FreePascalPluginConfig::SyntaxMode::DelphiUnicode, "Delphi Unicode"},
        {FreePascalPluginConfig::SyntaxMode::TP, "Turbo Pascal"},
        {FreePascalPluginConfig::SyntaxMode::MacPas, "Mac Pascal"}
    };
}

std::vector<std::string> UCFreePascalPlugin::GetTargetOperatingSystems() const {
    return {
        "linux",
        "win32",
        "win64",
        "darwin",
        "freebsd",
        "openbsd",
        "netbsd",
        "android",
        "wince",
        "embedded"
    };
}

std::vector<std::string> UCFreePascalPlugin::GetTargetCPUs() const {
    return {
        "i386",
        "x86_64",
        "arm",
        "aarch64",
        "powerpc",
        "powerpc64",
        "sparc",
        "mips",
        "mipsel",
        "riscv32",
        "riscv64"
    };
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

std::shared_ptr<UCFreePascalPlugin> CreateFreePascalPlugin() {
    return std::make_shared<UCFreePascalPlugin>();
}

void RegisterFreePascalPlugin() {
    auto& registry = UCCompilerPluginRegistry::Instance();
    registry.RegisterPlugin(CreateFreePascalPlugin());
}

} // namespace IDE
} // namespace UltraCanvas
