// Apps/IDE/Build/Plugins/UCGoPlugin.cpp
// Go Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0

#include "UCGoPlugin.h"
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

GoPluginConfig GoPluginConfig::Default() {
    GoPluginConfig config;
#ifdef _WIN32
    config.goBin = "go";
#else
    config.goBin = "/usr/local/go/bin/go";
#endif
    config.moduleMode = GoModuleMode::On;
    config.buildMode = GoBuildMode::Default;
    config.targetOS = GoOS::Linux;
    config.targetArch = GoArch::AMD64;
    config.cgoEnabled = true;
    config.testTimeout = 600;
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCGoPlugin::UCGoPlugin() : pluginConfig(GoPluginConfig::Default()) { DetectInstallation(); }
UCGoPlugin::UCGoPlugin(const GoPluginConfig& config) : pluginConfig(config) { DetectInstallation(); }
UCGoPlugin::~UCGoPlugin() { Cancel(); if (buildThread.joinable()) buildThread.join(); }

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCGoPlugin::GetPluginName() const { return "Go Compiler Plugin"; }
std::string UCGoPlugin::GetPluginVersion() const { return "1.0.0"; }
CompilerType UCGoPlugin::GetCompilerType() const { return CompilerType::Custom; }
std::string UCGoPlugin::GetCompilerName() const { return "go " + detectedVersion; }

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCGoPlugin::IsAvailable() { return installationDetected ? !detectedVersion.empty() : DetectInstallation(); }
std::string UCGoPlugin::GetCompilerPath() { return GetGoExecutable(); }
void UCGoPlugin::SetCompilerPath(const std::string& path) { pluginConfig.goBin = path; installationDetected = false; DetectInstallation(); }
std::string UCGoPlugin::GetCompilerVersion() { return detectedVersion; }
std::vector<std::string> UCGoPlugin::GetSupportedExtensions() const { return {"go"}; }

bool UCGoPlugin::CanCompile(const std::string& filePath) const {
    size_t dot = filePath.rfind('.'); if (dot == std::string::npos) return false;
    std::string ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "go";
}

bool UCGoPlugin::DetectInstallation() {
    installationDetected = true;
    detectedVersion.clear(); detectedGoRoot.clear(); detectedGoPath.clear();
    
    std::vector<std::string> paths = {pluginConfig.goBin, "go",
#ifdef _WIN32
        "C:\\Go\\bin\\go.exe", "C:\\Program Files\\Go\\bin\\go.exe"
#else
        "/usr/local/go/bin/go", "/usr/bin/go", "/usr/local/bin/go", "/opt/go/bin/go"
#endif
    };
    
    for (const auto& path : paths) {
        if (path.empty()) continue;
        std::string out, err; int code;
        if (ExecuteCommand({path, "version"}, out, err, code) && code == 0) {
            pluginConfig.goBin = path;
            
            // Parse version: "go version go1.21.0 linux/amd64"
            std::regex verRe(R"(go(\d+\.\d+\.?\d*))"); std::smatch m;
            if (std::regex_search(out, m, verRe)) detectedVersion = m[1];
            
            // Get GOROOT and GOPATH
            std::string envOut, envErr; int envCode;
            if (ExecuteCommand({path, "env", "GOROOT"}, envOut, envErr, envCode) && envCode == 0) {
                envOut.erase(std::remove(envOut.begin(), envOut.end(), '\n'), envOut.end());
                envOut.erase(std::remove(envOut.begin(), envOut.end(), '\r'), envOut.end());
                detectedGoRoot = envOut;
            }
            if (ExecuteCommand({path, "env", "GOPATH"}, envOut, envErr, envCode) && envCode == 0) {
                envOut.erase(std::remove(envOut.begin(), envOut.end(), '\n'), envOut.end());
                envOut.erase(std::remove(envOut.begin(), envOut.end(), '\r'), envOut.end());
                detectedGoPath = envOut;
            }
            return true;
        }
    }
    return false;
}

std::string UCGoPlugin::GetGoExecutable() const { return pluginConfig.goBin; }

// ============================================================================
// COMPILATION
// ============================================================================

void UCGoPlugin::CompileAsync(const std::vector<std::string>& files, const BuildConfiguration& cfg,
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

BuildResult UCGoPlugin::CompileSync(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    BuildResult result; result.success = false;
    if (files.empty()) { result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No source files"}); result.errorCount = 1; return result; }
    
    auto cmd = GenerateCommandLine(files, cfg);
    std::string out, err; int code;
    auto env = GetBuildEnv();
    
    auto t1 = std::chrono::steady_clock::now();
    bool ok = ExecuteCommand(cmd, out, err, code, "", env);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (!ok) { result.messages.push_back({CompilerMessageType::FatalError, "", 0, 0, "Failed to execute go"}); result.errorCount = 1; return result; }
    
    std::string combined = out + "\n" + err;
    std::istringstream iss(combined); std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        auto msg = ParseOutputLine(line);
        if (!msg.message.empty() && msg.type != CompilerMessageType::Info) {
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error || msg.type == CompilerMessageType::FatalError) result.errorCount++;
            else if (msg.type == CompilerMessageType::Warning) result.warningCount++;
        }
    }
    result.success = (code == 0);
    return result;
}

void UCGoPlugin::Cancel() {
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

bool UCGoPlugin::IsBuildInProgress() const { return buildInProgress; }

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCGoPlugin::ParseOutputLine(const std::string& line) {
    CompilerMessage msg; msg.type = CompilerMessageType::Info; msg.line = 0; msg.column = 0;
    
    // Go error format: file.go:10:5: error message
    std::regex errRe(R"(^([^:]+\.go):(\d+):(\d+):\s*(.+)$)"); std::smatch m;
    if (std::regex_search(line, m, errRe)) {
        msg.file = m[1]; msg.line = std::stoi(m[2]); msg.column = std::stoi(m[3]);
        msg.message = m[4]; msg.type = CompilerMessageType::Error;
        return msg;
    }
    
    // Without column: file.go:10: error message
    std::regex errRe2(R"(^([^:]+\.go):(\d+):\s*(.+)$)");
    if (std::regex_search(line, m, errRe2)) {
        msg.file = m[1]; msg.line = std::stoi(m[2]);
        msg.message = m[3]; msg.type = CompilerMessageType::Error;
        return msg;
    }
    
    // Package line: # package/path
    if (line.size() > 2 && line[0] == '#' && line[1] == ' ') {
        msg.message = line.substr(2); msg.type = CompilerMessageType::Note;
        return msg;
    }
    
    // go vet output
    std::regex vetRe(R"(^([^:]+\.go):(\d+):(\d+):\s*(.*)\s*$)");
    if (std::regex_search(line, m, vetRe)) {
        msg.file = m[1]; msg.line = std::stoi(m[2]); msg.column = std::stoi(m[3]);
        msg.message = m[4]; msg.type = CompilerMessageType::Warning;
        return msg;
    }
    
    // Test failures
    if (line.find("FAIL") == 0) { msg.type = CompilerMessageType::Error; msg.message = line; return msg; }
    if (line.find("PASS") == 0) { msg.type = CompilerMessageType::Info; msg.message = line; return msg; }
    
    msg.message = line;
    return msg;
}

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCGoPlugin::GenerateCommandLine(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    std::vector<std::string> args;
    args.push_back(GetGoExecutable());
    args.push_back("build");
    
    // Build mode
    if (pluginConfig.buildMode != GoBuildMode::Default) {
        args.push_back("-buildmode=" + BuildModeToString(pluginConfig.buildMode));
    }
    
    // Output
    std::string outPath = cfg.outputPath.empty() ? pluginConfig.outputPath : cfg.outputPath;
    if (!outPath.empty()) { args.push_back("-o"); args.push_back(outPath); }
    
    // Race detector
    if (pluginConfig.race) args.push_back("-race");
    if (pluginConfig.msan) args.push_back("-msan");
    if (pluginConfig.asan) args.push_back("-asan");
    
    // Coverage
    if (pluginConfig.cover) args.push_back("-cover");
    
    // Trimpath
    if (pluginConfig.trimpath) args.push_back("-trimpath");
    
    // Verbose
    if (pluginConfig.verbose) args.push_back("-v");
    if (pluginConfig.printCommands) args.push_back("-x");
    if (pluginConfig.work) args.push_back("-work");
    
    // Tags
    if (!pluginConfig.tags.empty()) {
        std::string tags;
        for (size_t i = 0; i < pluginConfig.tags.size(); ++i) {
            if (i > 0) tags += ",";
            tags += pluginConfig.tags[i];
        }
        args.push_back("-tags=" + tags);
    }
    
    // GC flags
    if (!pluginConfig.gcflags.empty() || pluginConfig.disableInlining || pluginConfig.disableOptimizations) {
        std::string gcflags;
        if (pluginConfig.disableOptimizations) gcflags += "-N ";
        if (pluginConfig.disableInlining) gcflags += "-l ";
        for (const auto& f : pluginConfig.gcflags) gcflags += f + " ";
        if (!gcflags.empty()) args.push_back("-gcflags=" + gcflags);
    }
    
    // LD flags
    if (!pluginConfig.ldFlags.empty()) {
        std::string ldflags;
        for (const auto& f : pluginConfig.ldFlags) ldflags += f + " ";
        args.push_back("-ldflags=" + ldflags);
    }
    
    // Source files/packages
    for (const auto& f : files) args.push_back(f);
    
    return args;
}

std::vector<std::string> UCGoPlugin::GetBuildEnv() const {
    std::vector<std::string> env;
    
    // Module mode
    switch (pluginConfig.moduleMode) {
        case GoModuleMode::On: env.push_back("GO111MODULE=on"); break;
        case GoModuleMode::Off: env.push_back("GO111MODULE=off"); break;
        default: env.push_back("GO111MODULE=auto"); break;
    }
    
    // Cross compilation
    if (pluginConfig.crossCompile) {
        env.push_back("GOOS=" + GoOSToString(pluginConfig.targetOS));
        env.push_back("GOARCH=" + GoArchToString(pluginConfig.targetArch));
    }
    
    // CGO
    env.push_back(std::string("CGO_ENABLED=") + (pluginConfig.cgoEnabled ? "1" : "0"));
    if (!pluginConfig.cc.empty()) env.push_back("CC=" + pluginConfig.cc);
    if (!pluginConfig.cxx.empty()) env.push_back("CXX=" + pluginConfig.cxx);
    
    // Proxy settings
    if (!pluginConfig.goProxy.empty()) env.push_back("GOPROXY=" + pluginConfig.goProxy);
    if (!pluginConfig.goPrivate.empty()) env.push_back("GOPRIVATE=" + pluginConfig.goPrivate);
    
    return env;
}

std::string UCGoPlugin::BuildModeToString(GoBuildMode mode) const {
    switch (mode) {
        case GoBuildMode::Archive: return "archive";
        case GoBuildMode::CArchive: return "c-archive";
        case GoBuildMode::CShared: return "c-shared";
        case GoBuildMode::Shared: return "shared";
        case GoBuildMode::Exe: return "exe";
        case GoBuildMode::Pie: return "pie";
        case GoBuildMode::Plugin: return "plugin";
        default: return "default";
    }
}

std::string UCGoPlugin::GoOSToString(GoOS os) const { return UltraCanvas::IDE::GoOSToString(os); }
std::string UCGoPlugin::GoArchToString(GoArch arch) const { return UltraCanvas::IDE::GoArchToString(arch); }

// ============================================================================
// GO BUILD COMMANDS
// ============================================================================

BuildResult UCGoPlugin::Build(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "build"};
    if (pluginConfig.verbose) args.push_back("-v");
    if (pluginConfig.race) args.push_back("-race");
    if (!pluginConfig.outputPath.empty()) { args.push_back("-o"); args.push_back(pluginConfig.outputPath); }
    args.push_back(pkg);
    
    std::string out, err; int code;
    auto env = GetBuildEnv();
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, "", env);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    if (onBuildOutput) { if (!out.empty()) onBuildOutput(out); if (!err.empty()) onBuildOutput(err); }
    
    std::string combined = out + err;
    std::istringstream iss(combined); std::string line;
    while (std::getline(iss, line)) {
        auto msg = ParseOutputLine(line);
        if (msg.type == CompilerMessageType::Error) { result.messages.push_back(msg); result.errorCount++; }
    }
    result.success = (code == 0);
    return result;
}

BuildResult UCGoPlugin::Install(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "install"};
    if (pluginConfig.verbose) args.push_back("-v");
    args.push_back(pkg);
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

BuildResult UCGoPlugin::Run(const std::string& pkg, const std::vector<std::string>& progArgs,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "run", pkg};
    for (const auto& a : progArgs) args.push_back(a);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

bool UCGoPlugin::Clean(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "clean"};
    if (!pkg.empty()) args.push_back(pkg);
    else args.push_back("-cache");
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

BuildResult UCGoPlugin::Generate(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "generate"};
    if (pluginConfig.verbose) args.push_back("-v");
    args.push_back(pkg);
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

// ============================================================================
// MODULE COMMANDS
// ============================================================================

bool UCGoPlugin::ModInit(const std::string& modulePath, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "mod", "init", modulePath};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

bool UCGoPlugin::ModTidy(std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "mod", "tidy"};
    if (pluginConfig.verbose) args.push_back("-v");
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

bool UCGoPlugin::ModDownload(const std::string& module, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "mod", "download"};
    if (!module.empty()) args.push_back(module);
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

bool UCGoPlugin::ModVerify(std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "mod", "verify"};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

bool UCGoPlugin::Get(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "get"};
    if (pluginConfig.verbose) args.push_back("-v");
    args.push_back(pkg);
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

bool UCGoPlugin::GetUpdate(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "get", "-u", pkg};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

GoModuleInfo UCGoPlugin::ParseGoMod(const std::string& goModPath) {
    GoModuleInfo info;
    std::ifstream f(goModPath); if (!f) return info;
    
    std::string line;
    bool inRequire = false;
    
    while (std::getline(f, line)) {
        // Module path
        std::regex modRe(R"(^module\s+(\S+))"); std::smatch m;
        if (std::regex_search(line, m, modRe)) { info.modulePath = m[1]; continue; }
        
        // Go version
        std::regex goRe(R"(^go\s+(\S+))");
        if (std::regex_search(line, m, goRe)) { info.goVersion = m[1]; continue; }
        
        // Require block
        if (line.find("require (") != std::string::npos) { inRequire = true; continue; }
        if (line.find(")") != std::string::npos && inRequire) { inRequire = false; continue; }
        
        // Single require or inside block
        std::regex reqRe(R"(^\s*(?:require\s+)?(\S+)\s+(\S+))");
        if (std::regex_search(line, m, reqRe)) {
            if (inRequire || line.find("require") != std::string::npos) {
                info.require.push_back({m[1], m[2]});
            }
        }
        
        // Replace
        std::regex replRe(R"(^replace\s+(.+)$)");
        if (std::regex_search(line, m, replRe)) info.replace.push_back(m[1]);
    }
    return info;
}

std::vector<GoDependencyInfo> UCGoPlugin::ListDependencies(bool direct) {
    std::vector<GoDependencyInfo> deps;
    std::vector<std::string> args = {GetGoExecutable(), "list", "-m"};
    if (!direct) args.push_back("-m"); args.push_back("all");
    
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line;
        while (std::getline(iss, line)) {
            if (line.empty()) continue;
            GoDependencyInfo dep;
            std::istringstream lss(line);
            lss >> dep.path >> dep.version;
            deps.push_back(dep);
        }
    }
    return deps;
}

std::vector<GoDependencyInfo> UCGoPlugin::CheckUpdates() {
    std::vector<GoDependencyInfo> updates;
    std::vector<std::string> args = {GetGoExecutable(), "list", "-m", "-u", "all"};
    
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code, "", GetBuildEnv()) && code == 0) {
        std::istringstream iss(out); std::string line;
        std::regex updateRe(R"((\S+)\s+(\S+)\s+\[(\S+)\])");
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, updateRe)) {
                GoDependencyInfo dep;
                dep.path = m[1]; dep.version = m[2]; dep.latestVersion = m[3];
                dep.isUpdate = true;
                updates.push_back(dep);
            }
        }
    }
    return updates;
}

// ============================================================================
// TESTING
// ============================================================================

BuildResult UCGoPlugin::Test(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "test"};
    if (pluginConfig.testVerbose) args.push_back("-v");
    if (pluginConfig.testShort) args.push_back("-short");
    if (pluginConfig.race) args.push_back("-race");
    args.push_back("-timeout=" + std::to_string(pluginConfig.testTimeout) + "s");
    if (pluginConfig.testCount > 1) args.push_back("-count=" + std::to_string(pluginConfig.testCount));
    if (!pluginConfig.testRun.empty()) { args.push_back("-run"); args.push_back(pluginConfig.testRun); }
    args.push_back(pkg);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    
    // Parse test results
    auto tests = ParseTestResults(out + err);
    for (const auto& t : tests) {
        if (!t.passed) {
            result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "FAIL: " + t.name});
            result.errorCount++;
        }
        if (onTestComplete) onTestComplete(t);
    }
    result.success = (code == 0);
    return result;
}

BuildResult UCGoPlugin::RunTest(const std::string& testName, const std::string& pkg,
    std::function<void(const std::string&)> onOutput) {
    pluginConfig.testRun = testName;
    auto result = Test(pkg, onOutput);
    pluginConfig.testRun.clear();
    return result;
}

BuildResult UCGoPlugin::Benchmark(const std::string& benchPattern, const std::string& pkg,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "test", "-bench=" + benchPattern, "-run=^$"};
    if (pluginConfig.testVerbose) args.push_back("-v");
    args.push_back(pkg);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    
    auto benches = ParseBenchmarkResults(out);
    for (const auto& b : benches) { if (onBenchmarkComplete) onBenchmarkComplete(b); }
    result.success = (code == 0);
    return result;
}

BuildResult UCGoPlugin::TestWithCoverage(const std::string& pkg, const std::string& coverProfile,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "test", "-cover", "-coverprofile=" + coverProfile};
    if (pluginConfig.testVerbose) args.push_back("-v");
    args.push_back(pkg);
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

std::vector<GoTestResult> UCGoPlugin::ParseTestResults(const std::string& output) {
    std::vector<GoTestResult> results;
    std::istringstream iss(output); std::string line;
    
    std::regex passRe(R"(^---\s+PASS:\s+(\S+)\s+\((\d+\.?\d*)s\))");
    std::regex failRe(R"(^---\s+FAIL:\s+(\S+)\s+\((\d+\.?\d*)s\))");
    std::regex skipRe(R"(^---\s+SKIP:\s+(\S+))");
    
    while (std::getline(iss, line)) {
        std::smatch m;
        if (std::regex_search(line, m, passRe)) {
            GoTestResult r; r.name = m[1]; r.duration = std::stof(m[2]); r.passed = true;
            results.push_back(r);
        } else if (std::regex_search(line, m, failRe)) {
            GoTestResult r; r.name = m[1]; r.duration = std::stof(m[2]); r.passed = false;
            results.push_back(r);
        }
    }
    return results;
}

std::vector<GoBenchmarkResult> UCGoPlugin::ParseBenchmarkResults(const std::string& output) {
    std::vector<GoBenchmarkResult> results;
    std::istringstream iss(output); std::string line;
    
    std::regex benchRe(R"(^(Benchmark\S+)\s+(\d+)\s+(\d+\.?\d*)\s+ns/op)");
    
    while (std::getline(iss, line)) {
        std::smatch m;
        if (std::regex_search(line, m, benchRe)) {
            GoBenchmarkResult r;
            r.name = m[1]; r.iterations = std::stoi(m[2]); r.nsPerOp = std::stof(m[3]);
            results.push_back(r);
        }
    }
    return results;
}

GoCoverageInfo UCGoPlugin::ParseCoverage(const std::string& coverProfile) {
    GoCoverageInfo info;
    // Parse coverage profile file
    std::ifstream f(coverProfile); if (!f) return info;
    // Implementation would parse coverage.out format
    return info;
}

bool UCGoPlugin::GenerateCoverageHTML(const std::string& coverProfile, const std::string& outputHTML,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "tool", "cover", "-html=" + coverProfile, "-o", outputHTML};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

// ============================================================================
// CODE QUALITY
// ============================================================================

bool UCGoPlugin::Format(const std::string& path, bool write, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "fmt"};
    if (!write) args[1] = "gofmt"; // gofmt doesn't write by default
    args.push_back(path);
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

bool UCGoPlugin::FormatImports(const std::string& path, bool write, std::function<void(const std::string&)> onOutput) {
    // goimports must be installed
    std::vector<std::string> args = {"goimports"};
    if (write) args.push_back("-w");
    args.push_back(path);
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

BuildResult UCGoPlugin::Vet(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "vet", pkg};
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    
    std::string combined = out + err;
    std::istringstream iss(combined); std::string line;
    while (std::getline(iss, line)) {
        auto msg = ParseOutputLine(line);
        if (msg.type == CompilerMessageType::Warning || msg.type == CompilerMessageType::Error) {
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error) result.errorCount++;
            else result.warningCount++;
        }
    }
    result.success = (code == 0);
    return result;
}

BuildResult UCGoPlugin::Staticcheck(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {"staticcheck", pkg};
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

BuildResult UCGoPlugin::Lint(const std::string& pkg, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {"golint", pkg};
    
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

// ============================================================================
// PACKAGE ANALYSIS
// ============================================================================

std::vector<GoPackageInfo> UCGoPlugin::ListPackages(const std::string& pattern) {
    std::vector<GoPackageInfo> packages;
    std::vector<std::string> args = {GetGoExecutable(), "list", "-json", pattern};
    
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        // Parse JSON output - simplified
        std::regex nameRe(R"("Name":\s*"([^"]+)")");
        std::regex importRe(R"("ImportPath":\s*"([^"]+)")");
        std::regex dirRe(R"("Dir":\s*"([^"]+)")");
        
        std::smatch m;
        std::string::const_iterator searchStart(out.cbegin());
        while (std::regex_search(searchStart, out.cend(), m, nameRe)) {
            GoPackageInfo pkg; pkg.name = m[1];
            searchStart = m.suffix().first;
            packages.push_back(pkg);
        }
    }
    return packages;
}

GoPackageInfo UCGoPlugin::GetPackageInfo(const std::string& pkg) {
    GoPackageInfo info;
    std::vector<std::string> args = {GetGoExecutable(), "list", "-json", pkg};
    
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        // Simple JSON parsing
        std::regex nameRe(R"("Name":\s*"([^"]+)")"); std::smatch m;
        if (std::regex_search(out, m, nameRe)) info.name = m[1];
        std::regex importRe(R"("ImportPath":\s*"([^"]+)")");
        if (std::regex_search(out, m, importRe)) info.importPath = m[1];
        std::regex dirRe(R"("Dir":\s*"([^"]+)")");
        if (std::regex_search(out, m, dirRe)) info.dir = m[1];
    }
    return info;
}

std::vector<std::string> UCGoPlugin::ListImports(const std::string& pkg) {
    std::vector<std::string> imports;
    std::vector<std::string> args = {GetGoExecutable(), "list", "-f", "{{.Imports}}", pkg};
    
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        // Parse [pkg1 pkg2 pkg3] format
        out.erase(std::remove(out.begin(), out.end(), '['), out.end());
        out.erase(std::remove(out.begin(), out.end(), ']'), out.end());
        std::istringstream iss(out); std::string imp;
        while (iss >> imp) imports.push_back(imp);
    }
    return imports;
}

// ============================================================================
// SOURCE ANALYSIS
// ============================================================================

std::vector<GoFunctionInfo> UCGoPlugin::ParseFunctions(const std::string& file) {
    std::vector<GoFunctionInfo> funcs;
    std::ifstream f(file); if (!f) return funcs;
    
    std::string line; int lineNum = 0;
    std::regex funcRe(R"(^func\s+(?:\((\w+)\s+\*?(\w+)\)\s+)?(\w+)\s*\(([^)]*)\)(?:\s*\(([^)]*)\)|\s+(\w+))?)");
    
    while (std::getline(f, line)) {
        lineNum++;
        std::smatch m;
        if (std::regex_search(line, m, funcRe)) {
            GoFunctionInfo func; func.file = file; func.startLine = lineNum;
            if (m[1].matched && m[2].matched) {
                func.receiver = m[1].str() + " " + m[2].str();
                func.isMethod = true;
            }
            func.name = m[3];
            func.isExported = !func.name.empty() && std::isupper(func.name[0]);
            func.isTest = func.name.find("Test") == 0;
            func.isBenchmark = func.name.find("Benchmark") == 0;
            func.isExample = func.name.find("Example") == 0;
            
            // Parse params
            if (m[4].matched && !m[4].str().empty()) {
                std::istringstream pss(m[4].str()); std::string p;
                while (std::getline(pss, p, ',')) {
                    p.erase(0, p.find_first_not_of(" \t"));
                    p.erase(p.find_last_not_of(" \t") + 1);
                    if (!p.empty()) func.params.push_back(p);
                }
            }
            funcs.push_back(func);
        }
    }
    return funcs;
}

std::vector<GoTypeInfo> UCGoPlugin::ParseTypes(const std::string& file) {
    std::vector<GoTypeInfo> types;
    std::ifstream f(file); if (!f) return types;
    
    std::string line; int lineNum = 0;
    std::regex typeRe(R"(^type\s+(\w+)\s+(struct|interface|\w+))");
    
    while (std::getline(f, line)) {
        lineNum++;
        std::smatch m;
        if (std::regex_search(line, m, typeRe)) {
            GoTypeInfo ti; ti.file = file; ti.line = lineNum;
            ti.name = m[1]; ti.kind = m[2];
            ti.isExported = !ti.name.empty() && std::isupper(ti.name[0]);
            types.push_back(ti);
        }
    }
    return types;
}

std::string UCGoPlugin::GetPackageName(const std::string& file) {
    std::ifstream f(file); if (!f) return "";
    std::string line;
    std::regex pkgRe(R"(^package\s+(\w+))");
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, pkgRe)) return m[1];
    }
    return "";
}

std::vector<std::string> UCGoPlugin::GetFileImports(const std::string& file) {
    std::vector<std::string> imports;
    std::ifstream f(file); if (!f) return imports;
    
    std::string line;
    bool inImport = false;
    std::regex importRe(R"(^\s*"([^"]+)")");
    std::regex singleImportRe(R"(^import\s+"([^"]+)")");
    
    while (std::getline(f, line)) {
        if (line.find("import (") != std::string::npos) { inImport = true; continue; }
        if (inImport && line.find(")") != std::string::npos) { inImport = false; continue; }
        
        std::smatch m;
        if (inImport && std::regex_search(line, m, importRe)) { imports.push_back(m[1]); }
        else if (std::regex_search(line, m, singleImportRe)) { imports.push_back(m[1]); }
        
        if (line.find("func ") != std::string::npos || line.find("type ") != std::string::npos ||
            line.find("var ") != std::string::npos || line.find("const ") != std::string::npos) break;
    }
    return imports;
}

// ============================================================================
// DOCUMENTATION
// ============================================================================

bool UCGoPlugin::StartDocServer(int port) {
    // godoc is deprecated, use pkgsite or go doc
    return false;
}

std::string UCGoPlugin::GetDoc(const std::string& pkg) {
    std::vector<std::string> args = {GetGoExecutable(), "doc", pkg};
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) return out;
    return "";
}

// ============================================================================
// TOOL MANAGEMENT
// ============================================================================

bool UCGoPlugin::InstallTool(const std::string& toolPath, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetGoExecutable(), "install", toolPath + "@latest"};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code, "", GetBuildEnv());
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return code == 0;
}

std::map<std::string, std::string> UCGoPlugin::GetEnv() {
    std::map<std::string, std::string> env;
    std::vector<std::string> args = {GetGoExecutable(), "env"};
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line;
        std::regex envRe(R"((\w+)="?([^"]*)"?)");
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, envRe)) env[m[1]] = m[2];
        }
    }
    return env;
}

std::string UCGoPlugin::GetEnvVar(const std::string& name) {
    std::vector<std::string> args = {GetGoExecutable(), "env", name};
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code) && code == 0) {
        out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
        out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
        return out;
    }
    return "";
}

// ============================================================================
// CROSS COMPILATION
// ============================================================================

BuildResult UCGoPlugin::BuildForTarget(const std::string& pkg, GoOS targetOS, GoArch targetArch,
    const std::string& outputPath, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetGoExecutable(), "build"};
    if (!outputPath.empty()) { args.push_back("-o"); args.push_back(outputPath); }
    args.push_back(pkg);
    
    std::vector<std::string> env = {"GOOS=" + GoOSToString(targetOS), "GOARCH=" + GoArchToString(targetArch)};
    env.push_back("CGO_ENABLED=0"); // Usually disable CGO for cross-compile
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, "", env);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0);
    return result;
}

std::vector<std::pair<GoOS, GoArch>> UCGoPlugin::GetSupportedTargets() {
    return {
        {GoOS::Linux, GoArch::AMD64}, {GoOS::Linux, GoArch::ARM64}, {GoOS::Linux, GoArch::ARM},
        {GoOS::Windows, GoArch::AMD64}, {GoOS::Windows, GoArch::ARM64}, {GoOS::Windows, GoArch::I386},
        {GoOS::Darwin, GoArch::AMD64}, {GoOS::Darwin, GoArch::ARM64},
        {GoOS::FreeBSD, GoArch::AMD64}, {GoOS::FreeBSD, GoArch::ARM64},
        {GoOS::JS, GoArch::WASM}, {GoOS::WASI, GoArch::WASM}
    };
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UCGoPlugin::SetConfig(const GoPluginConfig& cfg) {
    pluginConfig = cfg; installationDetected = false; DetectInstallation();
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCGoPlugin::ExecuteCommand(const std::vector<std::string>& args, std::string& output, std::string& error,
    int& exitCode, const std::string& cwd, const std::vector<std::string>& env) {
    if (args.empty()) return false;
    
    std::stringstream ss; for (size_t i = 0; i < args.size(); ++i) { if (i > 0) ss << " "; if (args[i].find(' ') != std::string::npos) ss << "\"" << args[i] << "\""; else ss << args[i]; }
    std::string cmd = ss.str();
    
#ifdef _WIN32
    // Set environment variables
    for (const auto& e : env) {
        size_t eq = e.find('=');
        if (eq != std::string::npos) _putenv(e.c_str());
    }
    
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
        for (const auto& e : env) putenv(const_cast<char*>(e.c_str()));
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
