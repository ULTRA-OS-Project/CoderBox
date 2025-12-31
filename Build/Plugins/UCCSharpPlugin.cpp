// Apps/IDE/Build/Plugins/UCCSharpPlugin.cpp
// C#/.NET Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0

#include "UCCSharpPlugin.h"
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
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

CSharpPluginConfig CSharpPluginConfig::Default() {
    CSharpPluginConfig config;
#ifdef _WIN32
    config.dotnetPath = "dotnet";
    config.cscPath = "csc";
    config.msbuildPath = "msbuild";
#else
    config.dotnetPath = "/usr/bin/dotnet";
    config.cscPath = "/usr/bin/csc";
    config.monoPath = "/usr/bin/mono";
#endif
    config.activeDotNet = DotNetType::DotNetSDK;
    config.langVersion = CSharpVersion::Latest;
    config.targetFramework = TargetFramework::Net80;
    config.outputType = DotNetOutputType::Exe;
    config.nullable = true;
    config.implicitUsings = true;
    config.optimize = true;
    config.debug = true;
    config.warningLevel = 4;
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCCSharpPlugin::UCCSharpPlugin() : pluginConfig(CSharpPluginConfig::Default()) { DetectInstallation(); }
UCCSharpPlugin::UCCSharpPlugin(const CSharpPluginConfig& config) : pluginConfig(config) { DetectInstallation(); }
UCCSharpPlugin::~UCCSharpPlugin() { Cancel(); if (buildThread.joinable()) buildThread.join(); }

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCCSharpPlugin::GetPluginName() const { return "C#/.NET Compiler Plugin"; }
std::string UCCSharpPlugin::GetPluginVersion() const { return "1.0.0"; }
CompilerType UCCSharpPlugin::GetCompilerType() const { return CompilerType::Custom; }
std::string UCCSharpPlugin::GetCompilerName() const { return DotNetTypeToString(detectedDotNetType) + " " + detectedVersionString; }

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCCSharpPlugin::IsAvailable() { return installationDetected ? detectedDotNetType != DotNetType::Unknown : DetectInstallation(); }
std::string UCCSharpPlugin::GetCompilerPath() { return GetDotNetExecutable(); }
void UCCSharpPlugin::SetCompilerPath(const std::string& path) { pluginConfig.dotnetPath = path; installationDetected = false; DetectInstallation(); }
std::string UCCSharpPlugin::GetCompilerVersion() { return detectedVersionString; }
std::vector<std::string> UCCSharpPlugin::GetSupportedExtensions() const { return {"cs", "csx", "csproj", "sln"}; }

bool UCCSharpPlugin::CanCompile(const std::string& filePath) const {
    size_t dot = filePath.rfind('.'); if (dot == std::string::npos) return false;
    std::string ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "cs" || ext == "csx" || ext == "csproj" || ext == "sln";
}

bool UCCSharpPlugin::DetectInstallation() {
    installationDetected = true;
    detectedDotNetType = DotNetType::Unknown;
    detectedVersion = DotNetVersion::Unknown;
    detectedVersionString.clear();
    installedSDKs.clear();
    installedRuntimes.clear();
    
    if (DetectDotNetSDK()) return true;
    if (DetectMono()) return true;
    return false;
}

bool UCCSharpPlugin::DetectDotNetSDK() {
    std::vector<std::string> paths = {pluginConfig.dotnetPath, "dotnet",
#ifdef _WIN32
        "C:\\Program Files\\dotnet\\dotnet.exe"
#else
        "/usr/bin/dotnet", "/usr/local/bin/dotnet", "/usr/share/dotnet/dotnet"
#endif
    };
    
    for (const auto& path : paths) {
        if (path.empty()) continue;
        std::string out, err; int code;
        if (ExecuteCommand({path, "--version"}, out, err, code) && code == 0) {
            out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
            out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
            
            pluginConfig.dotnetPath = path;
            detectedVersionString = out;
            detectedVersion = ParseDotNetVersion(out);
            detectedDotNetType = DotNetType::DotNetSDK;
            
            // Get installed SDKs
            std::string sdkOut, sdkErr; int sdkCode;
            if (ExecuteCommand({path, "--list-sdks"}, sdkOut, sdkErr, sdkCode) && sdkCode == 0) {
                std::istringstream iss(sdkOut); std::string line;
                while (std::getline(iss, line)) if (!line.empty()) installedSDKs.push_back(line);
            }
            
            // Get installed runtimes
            std::string rtOut, rtErr; int rtCode;
            if (ExecuteCommand({path, "--list-runtimes"}, rtOut, rtErr, rtCode) && rtCode == 0) {
                std::istringstream iss(rtOut); std::string line;
                while (std::getline(iss, line)) if (!line.empty()) installedRuntimes.push_back(line);
            }
            
            return true;
        }
    }
    return false;
}

bool UCCSharpPlugin::DetectMono() {
    std::vector<std::string> paths = {pluginConfig.monoPath, "mono", "mcs",
#ifndef _WIN32
        "/usr/bin/mono", "/usr/bin/mcs"
#endif
    };
    
    for (const auto& path : paths) {
        if (path.empty()) continue;
        std::string out, err; int code;
        if (ExecuteCommand({path, "--version"}, out, err, code)) {
            if (out.find("Mono") != std::string::npos || err.find("Mono") != std::string::npos) {
                pluginConfig.monoPath = path;
                detectedDotNetType = DotNetType::Mono;
                std::regex verRe(R"((\d+\.\d+\.\d+))"); std::smatch m;
                if (std::regex_search(out, m, verRe)) detectedVersionString = m[1];
                return true;
            }
        }
    }
    return false;
}

DotNetVersion UCCSharpPlugin::ParseDotNetVersion(const std::string& ver) {
    if (ver.find("9.") == 0) return DotNetVersion::Net90;
    if (ver.find("8.") == 0) return DotNetVersion::Net80;
    if (ver.find("7.") == 0) return DotNetVersion::Net70;
    if (ver.find("6.") == 0) return DotNetVersion::Net60;
    if (ver.find("5.") == 0) return DotNetVersion::Net50;
    if (ver.find("3.1") == 0) return DotNetVersion::NetCore31;
    return DotNetVersion::Unknown;
}

std::vector<std::string> UCCSharpPlugin::ListInstalledSDKs() { return installedSDKs; }
std::vector<std::string> UCCSharpPlugin::ListInstalledRuntimes() { return installedRuntimes; }

std::string UCCSharpPlugin::GetDotNetExecutable() const { return pluginConfig.dotnetPath; }
std::string UCCSharpPlugin::GetCscExecutable() const { return pluginConfig.cscPath; }
std::string UCCSharpPlugin::GetMonoExecutable() const { return pluginConfig.monoPath; }

// ============================================================================
// COMPILATION
// ============================================================================

void UCCSharpPlugin::CompileAsync(const std::vector<std::string>& files, const BuildConfiguration& cfg,
    std::function<void(const BuildResult&)> onComplete, std::function<void(const std::string&)>, std::function<void(float)>) {
    if (buildInProgress) {
        BuildResult r; r.success = false; r.errorCount = 1;
        r.messages.push_back({CompilerMessageType::Error, "", 0, 0, "Build already in progress"});
        if (onComplete) onComplete(r); return;
    }
    buildInProgress = true; cancelRequested = false;
    buildThread = std::thread([this, files, cfg, onComplete]() {
        BuildResult r = CompileSync(files, cfg); buildInProgress = false; if (onComplete) onComplete(r);
    });
}

BuildResult UCCSharpPlugin::CompileSync(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    BuildResult result; result.success = false;
    if (files.empty()) { result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No source files"}); result.errorCount = 1; return result; }
    
    // Check if it's a project/solution file
    const std::string& first = files[0];
    if (first.find(".csproj") != std::string::npos || first.find(".sln") != std::string::npos) {
        return Build(first, cfg.optimized ? "Release" : "Debug");
    }
    
    // Direct compilation with csc/Roslyn
    auto cmd = GenerateCommandLine(files, cfg);
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    bool ok = ExecuteCommand(cmd, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (!ok) { result.messages.push_back({CompilerMessageType::FatalError, "", 0, 0, "Failed to execute compiler"}); result.errorCount = 1; return result; }
    
    std::string combined = out + "\n" + err;
    std::istringstream iss(combined); std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        auto msg = ParseOutputLine(line);
        if (!msg.message.empty() || msg.line > 0) {
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error || msg.type == CompilerMessageType::FatalError) result.errorCount++;
            else if (msg.type == CompilerMessageType::Warning) result.warningCount++;
        }
    }
    result.success = (code == 0);
    return result;
}

void UCCSharpPlugin::Cancel() {
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

bool UCCSharpPlugin::IsBuildInProgress() const { return buildInProgress; }

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCCSharpPlugin::ParseOutputLine(const std::string& line) {
    CompilerMessage msg; msg.type = CompilerMessageType::Info; msg.line = 0; msg.column = 0;
    
    // C# error format: Program.cs(10,5): error CS1002: ; expected
    // Or: file.cs(10,5): warning CS0168: The variable 'x' is declared but never used
    std::regex errRe(R"(^(.+?)\((\d+),(\d+)\):\s*(error|warning)\s+(CS\d+):\s*(.+)$)");
    std::smatch m;
    
    if (std::regex_search(line, m, errRe)) {
        msg.file = m[1]; msg.line = std::stoi(m[2]); msg.column = std::stoi(m[3]);
        std::string level = m[4]; std::string code = m[5]; msg.message = code + ": " + m[6].str();
        msg.type = (level == "error") ? CompilerMessageType::Error : CompilerMessageType::Warning;
        return msg;
    }
    
    // MSBuild format: error CS1002: ; expected
    std::regex msbuildRe(R"(^\s*(error|warning)\s+(CS\d+):\s*(.+)$)");
    if (std::regex_search(line, m, msbuildRe)) {
        std::string level = m[1]; std::string code = m[2]; msg.message = code + ": " + m[3].str();
        msg.type = (level == "error") ? CompilerMessageType::Error : CompilerMessageType::Warning;
        return msg;
    }
    
    // dotnet build format
    if (line.find("Build FAILED") != std::string::npos) { msg.type = CompilerMessageType::Error; msg.message = line; return msg; }
    if (line.find("Build succeeded") != std::string::npos) { msg.type = CompilerMessageType::Info; msg.message = line; return msg; }
    
    // Restore errors
    std::regex restoreRe(R"(error\s*:\s*(.+)$)");
    if (std::regex_search(line, m, restoreRe)) { msg.type = CompilerMessageType::Error; msg.message = m[1]; return msg; }
    
    msg.message = line;
    return msg;
}

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCCSharpPlugin::GenerateCommandLine(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    std::vector<std::string> args;
    
    if (detectedDotNetType == DotNetType::DotNetSDK) {
        // Use dotnet build for project files, csc for direct compilation
        bool isProject = false;
        for (const auto& f : files) {
            if (f.find(".csproj") != std::string::npos || f.find(".sln") != std::string::npos) { isProject = true; break; }
        }
        
        if (isProject) {
            args.push_back(GetDotNetExecutable());
            args.push_back("build");
            args.push_back(files[0]);
            args.push_back("-c");
            args.push_back(cfg.optimized ? "Release" : "Debug");
            if (!cfg.outputPath.empty()) { args.push_back("-o"); args.push_back(cfg.outputPath); }
        } else {
            // Direct Roslyn compilation
            args.push_back(GetDotNetExecutable());
            args.push_back("exec");
            // Find csc.dll in SDK
            args.push_back(GetCscExecutable());
        }
    } else if (detectedDotNetType == DotNetType::Mono) {
        args.push_back("mcs");
    }
    
    // Output
    std::string outFile = cfg.outputPath;
    if (outFile.empty()) {
        size_t dot = files[0].rfind('.'); size_t slash = files[0].rfind('/');
        if (slash == std::string::npos) slash = files[0].rfind('\\');
        std::string base = (slash != std::string::npos) ? files[0].substr(slash + 1) : files[0];
        if (dot != std::string::npos && dot > slash) base = base.substr(0, base.rfind('.'));
        outFile = base + (pluginConfig.outputType == DotNetOutputType::Library ? ".dll" : ".exe");
    }
    args.push_back("-out:" + outFile);
    
    // Target type
    args.push_back("-target:" + OutputTypeToString(pluginConfig.outputType));
    
    // Language version
    if (pluginConfig.langVersion != CSharpVersion::Unknown) {
        args.push_back("-langversion:" + CSharpVersionToString(pluginConfig.langVersion));
    }
    
    // Optimization
    if (cfg.optimized || pluginConfig.optimize) args.push_back("-optimize+");
    else args.push_back("-optimize-");
    
    // Debug
    if (cfg.debugSymbols || pluginConfig.debug) {
        args.push_back("-debug+");
        args.push_back("-debug:" + pluginConfig.debugType);
    }
    
    // Warnings
    args.push_back("-warn:" + std::to_string(pluginConfig.warningLevel));
    if (pluginConfig.warningsAsErrors) args.push_back("-warnaserror+");
    for (const auto& w : pluginConfig.disabledWarnings) args.push_back("-nowarn:" + w);
    
    // Nullable
    if (pluginConfig.nullable) args.push_back("-nullable:enable");
    
    // References
    for (const auto& ref : pluginConfig.references) args.push_back("-reference:" + ref);
    
    // Defines
    for (const auto& def : pluginConfig.defines) args.push_back("-define:" + def);
    
    // Resources
    for (const auto& res : pluginConfig.embeddedResources) args.push_back("-resource:" + res);
    
    // Source files
    for (const auto& f : files) args.push_back(f);
    
    return args;
}

std::string UCCSharpPlugin::TargetFrameworkToString(TargetFramework tf) const {
    switch (tf) {
        case TargetFramework::Net48: return "net48";
        case TargetFramework::NetCoreApp31: return "netcoreapp3.1";
        case TargetFramework::Net50: return "net5.0";
        case TargetFramework::Net60: return "net6.0";
        case TargetFramework::Net70: return "net7.0";
        case TargetFramework::Net80: return "net8.0";
        case TargetFramework::Net90: return "net9.0";
        case TargetFramework::NetStandard20: return "netstandard2.0";
        case TargetFramework::NetStandard21: return "netstandard2.1";
        default: return "net8.0";
    }
}

std::string UCCSharpPlugin::CSharpVersionToString(CSharpVersion ver) const {
    switch (ver) {
        case CSharpVersion::CSharp7: return "7";
        case CSharpVersion::CSharp7_1: return "7.1";
        case CSharpVersion::CSharp7_2: return "7.2";
        case CSharpVersion::CSharp7_3: return "7.3";
        case CSharpVersion::CSharp8: return "8.0";
        case CSharpVersion::CSharp9: return "9.0";
        case CSharpVersion::CSharp10: return "10.0";
        case CSharpVersion::CSharp11: return "11.0";
        case CSharpVersion::CSharp12: return "12.0";
        case CSharpVersion::CSharp13: return "13.0";
        case CSharpVersion::Latest: return "latest";
        case CSharpVersion::Preview: return "preview";
        default: return "latest";
    }
}

std::string UCCSharpPlugin::OutputTypeToString(DotNetOutputType type) const {
    switch (type) {
        case DotNetOutputType::Exe: return "exe";
        case DotNetOutputType::WinExe: return "winexe";
        case DotNetOutputType::Library: return "library";
        case DotNetOutputType::Module: return "module";
        default: return "exe";
    }
}

// ============================================================================
// DOTNET CLI OPERATIONS
// ============================================================================

bool UCCSharpPlugin::CreateProject(const std::string& dir, const std::string& tmpl, const std::string& name,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "new", tmpl};
    if (!name.empty()) { args.push_back("-n"); args.push_back(name); }
    args.push_back("-o"); args.push_back(dir);
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCCSharpPlugin::CreateSolution(const std::string& dir, const std::string& name,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "new", "sln"};
    if (!name.empty()) { args.push_back("-n"); args.push_back(name); }
    args.push_back("-o"); args.push_back(dir);
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

BuildResult UCCSharpPlugin::Build(const std::string& proj, const std::string& config,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetDotNetExecutable(), "build", proj, "-c", config};
    if (!pluginConfig.outputDirectory.empty()) { args.push_back("-o"); args.push_back(pluginConfig.outputDirectory); }
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    if (onBuildOutput) { if (!out.empty()) onBuildOutput(out); }
    
    std::string combined = out + err;
    std::istringstream iss(combined); std::string line;
    while (std::getline(iss, line)) {
        auto msg = ParseOutputLine(line);
        if (msg.type == CompilerMessageType::Error || msg.type == CompilerMessageType::FatalError) {
            result.messages.push_back(msg); result.errorCount++;
        } else if (msg.type == CompilerMessageType::Warning) {
            result.messages.push_back(msg); result.warningCount++;
        }
    }
    result.success = (code == 0);
    return result;
}

bool UCCSharpPlugin::Clean(const std::string& proj, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "clean", proj};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCCSharpPlugin::Restore(const std::string& proj, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "restore", proj};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

BuildResult UCCSharpPlugin::Publish(const std::string& proj, const std::string& outDir, const std::string& config,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetDotNetExecutable(), "publish", proj, "-c", config};
    if (!outDir.empty()) { args.push_back("-o"); args.push_back(outDir); }
    if (pluginConfig.selfContained) { args.push_back("--self-contained"); args.push_back("true"); }
    if (pluginConfig.singleFile) args.push_back("-p:PublishSingleFile=true");
    if (pluginConfig.trimmed) args.push_back("-p:PublishTrimmed=true");
    if (pluginConfig.aot) args.push_back("-p:PublishAot=true");
    if (!pluginConfig.runtimeIdentifier.empty()) { args.push_back("-r"); args.push_back(pluginConfig.runtimeIdentifier); }
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    result.outputPath = outDir;
    return result;
}

BuildResult UCCSharpPlugin::Run(const std::string& proj, const std::vector<std::string>& progArgs,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetDotNetExecutable(), "run", "--project", proj};
    if (!progArgs.empty()) { args.push_back("--"); for (const auto& a : progArgs) args.push_back(a); }
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    if (onRuntimeOutput && !out.empty()) onRuntimeOutput(out);
    result.success = (code == 0);
    return result;
}

BuildResult UCCSharpPlugin::Test(const std::string& proj, const std::string& filter,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetDotNetExecutable(), "test", proj};
    if (!filter.empty()) { args.push_back("--filter"); args.push_back(filter); }
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    
    // Parse test results
    if (out.find("Failed:") != std::string::npos) {
        std::regex failRe(R"(Failed:\s*(\d+))"); std::smatch m;
        if (std::regex_search(out, m, failRe)) result.errorCount = std::stoi(m[1]);
    }
    result.success = (code == 0);
    return result;
}

bool UCCSharpPlugin::Pack(const std::string& proj, const std::string& outDir,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "pack", proj};
    if (!outDir.empty()) { args.push_back("-o"); args.push_back(outDir); }
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

// ============================================================================
// NUGET OPERATIONS
// ============================================================================

bool UCCSharpPlugin::AddPackage(const std::string& proj, const std::string& pkgId, const std::string& ver,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "add", proj, "package", pkgId};
    if (!ver.empty()) { args.push_back("-v"); args.push_back(ver); }
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCCSharpPlugin::RemovePackage(const std::string& proj, const std::string& pkgId,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "remove", proj, "package", pkgId};
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

std::vector<NuGetPackageInfo> UCCSharpPlugin::ListPackages(const std::string& proj) {
    std::vector<NuGetPackageInfo> packages;
    std::vector<std::string> args = {GetDotNetExecutable(), "list", proj, "package"};
    
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line;
        std::regex pkgRe(R"(>\s+(\S+)\s+(\S+)\s+(\S+))");
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, pkgRe)) {
                NuGetPackageInfo pkg; pkg.id = m[1]; pkg.version = m[3]; pkg.isInstalled = true;
                packages.push_back(pkg);
            }
        }
    }
    return packages;
}

std::vector<NuGetPackageInfo> UCCSharpPlugin::SearchPackages(const std::string& query, int take) {
    std::vector<NuGetPackageInfo> packages;
    // Would need NuGet API or dotnet search (if available)
    return packages;
}

bool UCCSharpPlugin::UpdatePackages(const std::string& proj, std::function<void(const std::string&)> onOutput) {
    // dotnet doesn't have direct update, need to remove and add
    auto packages = ListPackages(proj);
    for (const auto& pkg : packages) {
        RemovePackage(proj, pkg.id, nullptr);
        AddPackage(proj, pkg.id, "", onOutput);
    }
    return true;
}

// ============================================================================
// PROJECT/SOLUTION MANAGEMENT
// ============================================================================

bool UCCSharpPlugin::AddProjectToSolution(const std::string& sln, const std::string& proj,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "sln", sln, "add", proj};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCCSharpPlugin::RemoveProjectFromSolution(const std::string& sln, const std::string& proj,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "sln", sln, "remove", proj};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCCSharpPlugin::AddProjectReference(const std::string& proj, const std::string& refProj,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "add", proj, "reference", refProj};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

CSharpProjectInfo UCCSharpPlugin::ParseProjectFile(const std::string& csproj) {
    CSharpProjectInfo info; info.projectFile = csproj;
    
    std::ifstream f(csproj); if (!f) return info;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    // Parse target framework
    std::regex tfRe(R"(<TargetFramework>([^<]+)</TargetFramework>)"); std::smatch m;
    if (std::regex_search(content, m, tfRe)) {
        std::string tf = m[1];
        if (tf == "net8.0") info.targetFramework = TargetFramework::Net80;
        else if (tf == "net7.0") info.targetFramework = TargetFramework::Net70;
        else if (tf == "net6.0") info.targetFramework = TargetFramework::Net60;
        else if (tf == "net9.0") info.targetFramework = TargetFramework::Net90;
    }
    
    // Parse output type
    std::regex otRe(R"(<OutputType>([^<]+)</OutputType>)");
    if (std::regex_search(content, m, otRe)) {
        std::string ot = m[1];
        if (ot == "Exe") info.outputType = DotNetOutputType::Exe;
        else if (ot == "Library") info.outputType = DotNetOutputType::Library;
        else if (ot == "WinExe") info.outputType = DotNetOutputType::WinExe;
    }
    
    // Parse nullable
    std::regex nullRe(R"(<Nullable>enable</Nullable>)");
    info.nullable = std::regex_search(content, nullRe);
    
    // Parse implicit usings
    std::regex usingsRe(R"(<ImplicitUsings>enable</ImplicitUsings>)");
    info.implicitUsings = std::regex_search(content, usingsRe);
    
    // Parse assembly name
    std::regex asmRe(R"(<AssemblyName>([^<]+)</AssemblyName>)");
    if (std::regex_search(content, m, asmRe)) info.assemblyName = m[1];
    
    // Parse root namespace
    std::regex nsRe(R"(<RootNamespace>([^<]+)</RootNamespace>)");
    if (std::regex_search(content, m, nsRe)) info.rootNamespace = m[1];
    
    // Parse package references
    std::regex pkgRe(R"(<PackageReference\s+Include="([^"]+)"\s+Version="([^"]+)")");
    std::sregex_iterator it(content.begin(), content.end(), pkgRe), end;
    while (it != end) { info.packageReferences.push_back((*it)[1].str() + " " + (*it)[2].str()); ++it; }
    
    return info;
}

SolutionInfo UCCSharpPlugin::ParseSolutionFile(const std::string& sln) {
    SolutionInfo info; info.solutionFile = sln;
    
    std::ifstream f(sln); if (!f) return info;
    std::string line;
    std::regex projRe(R"(Project\("[^"]+"\)\s*=\s*"[^"]+",\s*"([^"]+\.csproj)")");
    
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, projRe)) {
            info.projects.push_back(m[1]);
        }
    }
    
    return info;
}

// ============================================================================
// SOURCE ANALYSIS
// ============================================================================

std::vector<CSharpTypeInfo> UCCSharpPlugin::ParseTypes(const std::string& srcFile) {
    std::vector<CSharpTypeInfo> types;
    std::ifstream f(srcFile); if (!f) return types;
    
    std::string line; int lineNum = 0;
    std::regex typeRe(R"((public|private|internal|protected)?\s*(abstract|sealed|static|partial)?\s*(class|struct|interface|enum|record)\s+(\w+))");
    
    while (std::getline(f, line)) {
        lineNum++;
        std::smatch m;
        if (std::regex_search(line, m, typeRe)) {
            CSharpTypeInfo ti; ti.sourceFile = srcFile; ti.startLine = lineNum;
            if (m[1].matched) {
                std::string acc = m[1];
                ti.isPublic = (acc == "public"); ti.isPrivate = (acc == "private");
                ti.isInternal = (acc == "internal"); ti.isProtected = (acc == "protected");
            }
            if (m[2].matched) {
                std::string mod = m[2];
                ti.isAbstract = (mod == "abstract"); ti.isSealed = (mod == "sealed");
                ti.isStatic = (mod == "static"); ti.isPartial = (mod == "partial");
            }
            std::string kind = m[3];
            ti.isClass = (kind == "class"); ti.isStruct = (kind == "struct");
            ti.isInterface = (kind == "interface"); ti.isEnum = (kind == "enum");
            ti.isRecord = (kind == "record");
            ti.name = m[4];
            types.push_back(ti);
        }
    }
    return types;
}

std::vector<CSharpMethodInfo> UCCSharpPlugin::GetMethods(const std::string& srcFile) {
    std::vector<CSharpMethodInfo> methods;
    std::ifstream f(srcFile); if (!f) return methods;
    
    std::string line; int lineNum = 0;
    std::regex methRe(R"((public|private|protected|internal)?\s*(static|virtual|override|abstract|async)?\s*(\w+(?:<[^>]+>)?)\s+(\w+)\s*\(([^)]*)\))");
    
    while (std::getline(f, line)) {
        lineNum++;
        std::smatch m;
        if (std::regex_search(line, m, methRe)) {
            CSharpMethodInfo mi; mi.startLine = lineNum;
            if (m[1].matched) {
                std::string acc = m[1];
                mi.isPublic = (acc == "public"); mi.isPrivate = (acc == "private");
                mi.isProtected = (acc == "protected"); mi.isInternal = (acc == "internal");
            }
            if (m[2].matched) {
                std::string mod = m[2];
                mi.isStatic = (mod == "static"); mi.isVirtual = (mod == "virtual");
                mi.isOverride = (mod == "override"); mi.isAbstract = (mod == "abstract");
                mi.isAsync = (mod == "async");
            }
            mi.returnType = m[3]; mi.name = m[4];
            std::string params = m[5];
            if (!params.empty()) {
                std::istringstream pss(params); std::string p;
                while (std::getline(pss, p, ',')) {
                    p.erase(0, p.find_first_not_of(" \t"));
                    p.erase(p.find_last_not_of(" \t") + 1);
                    if (!p.empty()) mi.parameters.push_back(p);
                }
            }
            methods.push_back(mi);
        }
    }
    return methods;
}

std::vector<CSharpPropertyInfo> UCCSharpPlugin::GetProperties(const std::string& srcFile) {
    std::vector<CSharpPropertyInfo> props;
    std::ifstream f(srcFile); if (!f) return props;
    
    std::string line; int lineNum = 0;
    std::regex propRe(R"((public|private|protected)?\s*(static|virtual)?\s*(\w+(?:<[^>]+>)?)\s+(\w+)\s*\{)");
    
    while (std::getline(f, line)) {
        lineNum++;
        std::smatch m;
        if (std::regex_search(line, m, propRe)) {
            CSharpPropertyInfo pi; pi.line = lineNum;
            if (m[1].matched) { std::string acc = m[1]; pi.isPublic = (acc == "public"); pi.isPrivate = (acc == "private"); }
            if (m[2].matched) { std::string mod = m[2]; pi.isStatic = (mod == "static"); pi.isVirtual = (mod == "virtual"); }
            pi.type = m[3]; pi.name = m[4];
            pi.hasGetter = line.find("get") != std::string::npos;
            pi.hasSetter = line.find("set") != std::string::npos;
            pi.hasInit = line.find("init") != std::string::npos;
            props.push_back(pi);
        }
    }
    return props;
}

std::vector<std::string> UCCSharpPlugin::GetUsings(const std::string& srcFile) {
    std::vector<std::string> usings;
    std::ifstream f(srcFile); if (!f) return usings;
    
    std::string line;
    std::regex usingRe(R"(using\s+(static\s+)?([\w.]+)\s*;)");
    
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, usingRe)) {
            usings.push_back(m[1].matched ? "static " + m[2].str() : m[2].str());
        }
        if (line.find("namespace") != std::string::npos || line.find("class") != std::string::npos) break;
    }
    return usings;
}

std::string UCCSharpPlugin::GetNamespace(const std::string& srcFile) {
    std::ifstream f(srcFile); if (!f) return "";
    
    std::string line;
    std::regex nsRe(R"(namespace\s+([\w.]+))");
    
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, nsRe)) return m[1];
    }
    return "";
}

// ============================================================================
// ROSLYN DIRECT COMPILATION
// ============================================================================

BuildResult UCCSharpPlugin::CompileWithRoslyn(const std::vector<std::string>& files, const std::string& outFile,
    std::function<void(const std::string&)> onOutput) {
    BuildConfiguration cfg; cfg.outputPath = outFile;
    return CompileSync(files, cfg);
}

BuildResult UCCSharpPlugin::CompileScript(const std::string& script, std::function<void(const std::string&)> onOutput) {
    // C# scripting with dotnet-script or similar
    BuildResult result;
    std::vector<std::string> args = {GetDotNetExecutable(), "script", script};
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

// ============================================================================
// TOOL MANAGEMENT
// ============================================================================

bool UCCSharpPlugin::InstallTool(const std::string& toolId, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "tool", "install", "-g", toolId};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

std::vector<std::string> UCCSharpPlugin::ListTools() {
    std::vector<std::string> tools;
    std::vector<std::string> args = {GetDotNetExecutable(), "tool", "list", "-g"};
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line;
        while (std::getline(iss, line)) if (!line.empty() && line[0] != '-' && line.find("Package") == std::string::npos) tools.push_back(line);
    }
    return tools;
}

BuildResult UCCSharpPlugin::RunTool(const std::string& toolName, const std::vector<std::string>& args,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> cmd = {GetDotNetExecutable(), "tool", "run", toolName};
    for (const auto& a : args) cmd.push_back(a);
    
    std::string out, err; int code;
    ExecuteCommand(cmd, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

// ============================================================================
// CODE FORMATTING
// ============================================================================

bool UCCSharpPlugin::FormatCode(const std::string& proj, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetDotNetExecutable(), "format", proj};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

BuildResult UCCSharpPlugin::VerifyFormat(const std::string& proj, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetDotNetExecutable(), "format", proj, "--verify-no-changes"};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UCCSharpPlugin::SetConfig(const CSharpPluginConfig& cfg) {
    pluginConfig = cfg; installationDetected = false; DetectInstallation();
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCCSharpPlugin::ExecuteCommand(const std::vector<std::string>& args, std::string& output, std::string& error, int& exitCode, const std::string& cwd) {
    if (args.empty()) return false;
    
    std::stringstream ss; for (size_t i = 0; i < args.size(); ++i) { if (i > 0) ss << " "; if (args[i].find(' ') != std::string::npos) ss << "\"" << args[i] << "\""; else ss << args[i]; }
    std::string cmd = ss.str();
    
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = NULL;
    HANDLE oR, oW, eR, eW;
    CreatePipe(&oR, &oW, &sa, 0); CreatePipe(&eR, &eW, &sa, 0);
    SetHandleInformation(oR, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(eR, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {}; si.cb = sizeof(si); si.hStdOutput = oW; si.hStdError = eW; si.dwFlags |= STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(NULL, const_cast<char*>(cmd.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, cwd.empty() ? NULL : cwd.c_str(), &si, &pi)) { CloseHandle(oR); CloseHandle(oW); CloseHandle(eR); CloseHandle(eW); return false; }
    CloseHandle(oW); CloseHandle(eW);
    currentProcess = pi.hProcess;
    char buf[4096]; DWORD n;
    while (ReadFile(oR, buf, sizeof(buf)-1, &n, NULL) && n > 0) { if (cancelRequested) break; buf[n] = '\0'; output += buf; }
    while (ReadFile(eR, buf, sizeof(buf)-1, &n, NULL) && n > 0) { if (cancelRequested) break; buf[n] = '\0'; error += buf; }
    if (cancelRequested) TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec; GetExitCodeProcess(pi.hProcess, &ec); exitCode = static_cast<int>(ec);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(oR); CloseHandle(eR);
    currentProcess = nullptr; return true;
#else
    int op[2], ep[2];
    if (pipe(op) == -1 || pipe(ep) == -1) return false;
    pid_t pid = fork();
    if (pid == -1) { close(op[0]); close(op[1]); close(ep[0]); close(ep[1]); return false; }
    if (pid == 0) {
        close(op[0]); close(ep[0]); dup2(op[1], STDOUT_FILENO); dup2(ep[1], STDERR_FILENO); close(op[1]); close(ep[1]);
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) _exit(127);
        std::vector<char*> ca; for (const auto& a : args) ca.push_back(const_cast<char*>(a.c_str())); ca.push_back(nullptr);
        execvp(ca[0], ca.data()); _exit(127);
    }
    close(op[1]); close(ep[1]);
    currentProcess = reinterpret_cast<void*>(static_cast<intptr_t>(pid));
    char buf[4096]; ssize_t n;
    while ((n = read(op[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; output += buf; }
    while ((n = read(ep[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; error += buf; }
    close(op[0]); close(ep[0]);
    int st; waitpid(pid, &st, 0);
    if (WIFEXITED(st)) exitCode = WEXITSTATUS(st); else if (WIFSIGNALED(st)) exitCode = 128 + WTERMSIG(st); else exitCode = -1;
    currentProcess = nullptr; return true;
#endif
}

} // namespace IDE
} // namespace UltraCanvas
