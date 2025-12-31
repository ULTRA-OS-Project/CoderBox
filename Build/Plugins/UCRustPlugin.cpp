// Apps/IDE/Build/Plugins/UCRustPlugin.cpp
// Rust/Cargo Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27

#include "UCRustPlugin.h"
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
// UTILITY IMPLEMENTATIONS
// ============================================================================

RustTarget RustTarget::Parse(const std::string& triple) {
    RustTarget target;
    target.triple = triple;
    std::vector<std::string> parts;
    std::istringstream iss(triple);
    std::string part;
    while (std::getline(iss, part, '-')) parts.push_back(part);
    if (parts.size() >= 1) target.arch = parts[0];
    if (parts.size() >= 2) target.vendor = parts[1];
    if (parts.size() >= 3) target.os = parts[2];
    if (parts.size() >= 4) target.env = parts[3];
    return target;
}

RustPluginConfig RustPluginConfig::Default() {
    RustPluginConfig config;
#ifdef _WIN32
    std::string home = std::getenv("USERPROFILE") ? std::getenv("USERPROFILE") : "";
    std::string bin = home + "\\.cargo\\bin\\";
    config.rustcPath = bin + "rustc.exe";
    config.cargoPath = bin + "cargo.exe";
    config.rustupPath = bin + "rustup.exe";
    config.rustfmtPath = bin + "rustfmt.exe";
    config.clippyDriverPath = bin + "clippy-driver.exe";
#else
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    std::string bin = home + "/.cargo/bin/";
    config.rustcPath = bin + "rustc";
    config.cargoPath = bin + "cargo";
    config.rustupPath = bin + "rustup";
    config.rustfmtPath = bin + "rustfmt";
    config.clippyDriverPath = bin + "clippy-driver";
#endif
    config.toolchain = RustToolchain::Stable;
    config.defaultEdition = RustEdition::Edition2021;
    config.defaultProfile = CargoBuildProfile::Dev;
    config.colorOutput = true;
    config.enableClippy = true;
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCRustPlugin::UCRustPlugin() : pluginConfig(RustPluginConfig::Default()) { DetectInstallation(); }
UCRustPlugin::UCRustPlugin(const RustPluginConfig& config) : pluginConfig(config) { DetectInstallation(); }
UCRustPlugin::~UCRustPlugin() { Cancel(); if (buildThread.joinable()) buildThread.join(); }

// ============================================================================
// PLUGIN IDENTIFICATION
// ============================================================================

std::string UCRustPlugin::GetPluginName() const { return "Rust/Cargo Compiler Plugin"; }
std::string UCRustPlugin::GetPluginVersion() const { return "1.0.0"; }
CompilerType UCRustPlugin::GetCompilerType() const { return CompilerType::Rust; }
std::string UCRustPlugin::GetCompilerName() const {
    return versionInfo.channel.empty() ? "Rust" : "rustc " + versionInfo.ToString() + " (" + versionInfo.channel + ")";
}

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCRustPlugin::IsAvailable() { return installationDetected ? !cachedVersion.empty() : DetectInstallation(); }
std::string UCRustPlugin::GetCompilerPath() { return GetCargoExecutable(); }
void UCRustPlugin::SetCompilerPath(const std::string& path) { pluginConfig.cargoPath = path; installationDetected = false; DetectInstallation(); }
std::vector<std::string> UCRustPlugin::GetSupportedExtensions() const { return {"rs"}; }

bool UCRustPlugin::CanCompile(const std::string& filePath) const {
    size_t dot = filePath.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "rs";
}

std::string UCRustPlugin::GetCompilerVersion() {
    if (!cachedVersion.empty()) return cachedVersion;
    std::string output, error; int exitCode;
    if (ExecuteCommand({GetRustcExecutable(), "--version"}, output, error, exitCode)) {
        std::regex re(R"(rustc (\d+\.\d+\.\d+))");
        std::smatch m;
        if (std::regex_search(output, m, re)) cachedVersion = m[1].str();
    }
    return cachedVersion;
}

bool UCRustPlugin::DetectInstallation() {
    installationDetected = true;
    std::string output, error; int exitCode;
    std::vector<std::string> paths = {pluginConfig.cargoPath, "cargo",
#ifdef _WIN32
        "C:\\Users\\" + std::string(std::getenv("USERNAME") ? std::getenv("USERNAME") : "") + "\\.cargo\\bin\\cargo.exe"
#else
        "/usr/bin/cargo", "/usr/local/bin/cargo", std::string(std::getenv("HOME") ? std::getenv("HOME") : "") + "/.cargo/bin/cargo"
#endif
    };
    for (const auto& p : paths) {
        if (p.empty()) continue;
        if (ExecuteCommand({p, "--version"}, output, error, exitCode) && exitCode == 0) {
            pluginConfig.cargoPath = p;
            size_t slash = p.rfind('/');
            if (slash == std::string::npos) slash = p.rfind('\\');
            if (slash != std::string::npos) {
                std::string bin = p.substr(0, slash + 1);
#ifdef _WIN32
                pluginConfig.rustcPath = bin + "rustc.exe";
                pluginConfig.rustupPath = bin + "rustup.exe";
                pluginConfig.rustfmtPath = bin + "rustfmt.exe";
#else
                pluginConfig.rustcPath = bin + "rustc";
                pluginConfig.rustupPath = bin + "rustup";
                pluginConfig.rustfmtPath = bin + "rustfmt";
#endif
            }
            GetCompilerVersion();
            GetVersionInfo();
            return true;
        }
    }
    return false;
}

UCRustPlugin::RustVersionInfo UCRustPlugin::GetVersionInfo() {
    if (versionInfo.major > 0) return versionInfo;
    std::string output, error; int exitCode;
    if (ExecuteCommand({GetRustcExecutable(), "--version", "--verbose"}, output, error, exitCode)) {
        std::istringstream iss(output); std::string line;
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, std::regex(R"(rustc (\d+)\.(\d+)\.(\d+))"))) {
                versionInfo.major = std::stoi(m[1]); versionInfo.minor = std::stoi(m[2]); versionInfo.patch = std::stoi(m[3]);
            }
            if (line.find("release:") != std::string::npos) {
                versionInfo.channel = line.find("nightly") != std::string::npos ? "nightly" : 
                                      line.find("beta") != std::string::npos ? "beta" : "stable";
            }
            if (std::regex_search(line, m, std::regex(R"(commit-hash: ([a-f0-9]+))"))) versionInfo.commitHash = m[1];
            if (std::regex_search(line, m, std::regex(R"(host: (.+))"))) versionInfo.hostTriple = m[1];
            if (std::regex_search(line, m, std::regex(R"(LLVM version: (.+))"))) versionInfo.llvmVersion = m[1];
        }
    }
    return versionInfo;
}

// ============================================================================
// COMPILATION
// ============================================================================

void UCRustPlugin::CompileAsync(const std::vector<std::string>& files, const BuildConfiguration& cfg,
    std::function<void(const BuildResult&)> onComplete, std::function<void(const std::string&)> onLine, std::function<void(float)> onProg) {
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

BuildResult UCRustPlugin::CompileSync(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    BuildResult result; result.success = false;
    if (files.empty()) { result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No files"}); result.errorCount = 1; return result; }
    
    std::string dir; size_t slash = files[0].rfind('/');
    if (slash == std::string::npos) slash = files[0].rfind('\\');
    dir = slash != std::string::npos ? files[0].substr(0, slash) : ".";
    
    std::ifstream test(dir + "/Cargo.toml");
    if (test.good()) { test.close(); return CargoBuild(dir, cfg); }
    test.close();
    
    auto cmd = GenerateCommandLine(files, cfg);
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    bool ok = ExecuteCommand(cmd, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (!ok) { result.messages.push_back({CompilerMessageType::FatalError, "", 0, 0, "Failed to run rustc"}); result.errorCount = 1; return result; }
    
    std::istringstream iss(out + err); std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        if (line[0] == '{') ParseJsonDiagnostic(line, result.messages);
        else { auto m = ParseOutputLine(line); if (!m.message.empty()) result.messages.push_back(m); }
    }
    for (const auto& m : result.messages) {
        if (m.type == CompilerMessageType::Error || m.type == CompilerMessageType::FatalError) result.errorCount++;
        else if (m.type == CompilerMessageType::Warning) result.warningCount++;
    }
    result.success = (code == 0);
    return result;
}

void UCRustPlugin::Cancel() {
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

bool UCRustPlugin::IsBuildInProgress() const { return buildInProgress; }

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCRustPlugin::ParseOutputLine(const std::string& line) {
    CompilerMessage msg; msg.type = CompilerMessageType::Info; msg.message = line; msg.line = 0; msg.column = 0;
    std::smatch m;
    if (std::regex_search(line, m, std::regex(R"(^(error|warning|note|help)\[([A-Z]\d+)\]: (.+)$)"))) {
        msg.message = "[" + m[2].str() + "] " + m[3].str();
        std::string lvl = m[1]; msg.type = lvl == "error" ? CompilerMessageType::Error : lvl == "warning" ? CompilerMessageType::Warning : CompilerMessageType::Note;
    } else if (std::regex_search(line, m, std::regex(R"(^(error|warning|note|help): (.+)$)"))) {
        msg.message = m[2]; std::string lvl = m[1];
        msg.type = lvl == "error" ? CompilerMessageType::Error : lvl == "warning" ? CompilerMessageType::Warning : CompilerMessageType::Note;
    } else if (std::regex_search(line, m, std::regex(R"(^\s*--> (.+):(\d+):(\d+)$)"))) {
        msg.file = m[1]; msg.line = std::stoi(m[2]); msg.column = std::stoi(m[3]); msg.message = "";
    }
    return msg;
}

void UCRustPlugin::ParseJsonDiagnostic(const std::string& json, std::vector<CompilerMessage>& msgs) {
    CompilerMessage msg; msg.type = CompilerMessageType::Info; std::smatch m;
    if (std::regex_search(json, m, std::regex(R"("level"\s*:\s*"(\w+)")"))) {
        std::string l = m[1]; msg.type = l == "error" ? CompilerMessageType::Error : l == "warning" ? CompilerMessageType::Warning : CompilerMessageType::Note;
    }
    if (std::regex_search(json, m, std::regex(R"("message"\s*:\s*"([^"]+)")"))) msg.message = m[1];
    if (std::regex_search(json, m, std::regex(R"("code"\s*:\s*\{\s*"code"\s*:\s*"([^"]+)")"))) msg.message = "[" + m[1].str() + "] " + msg.message;
    if (std::regex_search(json, m, std::regex(R"("file_name"\s*:\s*"([^"]+)")"))) msg.file = m[1];
    if (std::regex_search(json, m, std::regex(R"("line_start"\s*:\s*(\d+))"))) msg.line = std::stoi(m[1]);
    if (std::regex_search(json, m, std::regex(R"("column_start"\s*:\s*(\d+))"))) msg.column = std::stoi(m[1]);
    if (!msg.message.empty()) msgs.push_back(msg);
}

CargoBuildArtifact UCRustPlugin::ParseArtifact(const std::string& json) {
    CargoBuildArtifact a; std::smatch m;
    if (std::regex_search(json, m, std::regex(R"("package_id"\s*:\s*"([^"]+))"))) { a.packageName = m[1]; size_t sp = a.packageName.find(' '); if (sp != std::string::npos) a.packageName = a.packageName.substr(0, sp); }
    if (std::regex_search(json, m, std::regex(R"("target"\s*:\s*\{[^}]*"name"\s*:\s*"([^"]+)")"))) a.targetName = m[1];
    if (std::regex_search(json, m, std::regex(R"("kind"\s*:\s*\[\s*"([^"]+)"\s*\])"))) a.targetKind = m[1];
    if (std::regex_search(json, m, std::regex(R"("filenames"\s*:\s*\[\s*"([^"]+)"\s*\])"))) a.filePath = m[1];
    if (std::regex_search(json, m, std::regex(R"("fresh"\s*:\s*(true|false))"))) a.fresh = m[1] == "true";
    return a;
}

RustTestResult UCRustPlugin::ParseTestResult(const std::string& line) {
    RustTestResult r; std::smatch m;
    if (std::regex_search(line, m, std::regex(R"(test (\S+)\s+\.\.\.\s+(\w+))"))) {
        r.name = m[1]; size_t c = r.name.rfind("::"); if (c != std::string::npos) r.module = r.name.substr(0, c);
        std::string s = m[2]; r.status = s == "ok" ? RustTestResult::Status::Passed : s == "FAILED" ? RustTestResult::Status::Failed : s == "ignored" ? RustTestResult::Status::Ignored : RustTestResult::Status::Measured;
    }
    return r;
}

// ============================================================================
// COMMAND LINE
// ============================================================================

std::vector<std::string> UCRustPlugin::GenerateCommandLine(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    std::vector<std::string> args; args.push_back(GetRustcExecutable());
    args.push_back("--edition"); args.push_back(RustEditionToString(pluginConfig.defaultEdition));
    if (cfg.optimizationLevel > 0 || cfg.name == "Release") args.push_back("-O");
    if (cfg.debugSymbols || cfg.name == "Debug") args.push_back("-g");
    if (!cfg.outputPath.empty() && !cfg.outputName.empty()) { args.push_back("-o"); args.push_back(cfg.outputPath + "/" + cfg.outputName); }
    args.push_back("--error-format=json");
    if (pluginConfig.colorOutput) args.push_back("--color=always");
    if (pluginConfig.warningsAsErrors) { args.push_back("-D"); args.push_back("warnings"); }
    for (const auto& a : cfg.extraArgs) args.push_back(a);
    for (const auto& f : files) args.push_back(f);
    return args;
}

std::string UCRustPlugin::GetRustcExecutable() const { return pluginConfig.rustcPath; }
std::string UCRustPlugin::GetCargoExecutable() const { return pluginConfig.cargoPath; }

std::vector<std::string> UCRustPlugin::GetCargoBaseArgs() const {
    std::vector<std::string> a; a.push_back(GetCargoExecutable());
    if (pluginConfig.colorOutput) a.push_back("--color=always");
    if (pluginConfig.offline) a.push_back("--offline");
    if (pluginConfig.locked) a.push_back("--locked");
    if (pluginConfig.frozen) a.push_back("--frozen");
    return a;
}

// ============================================================================
// TOOLCHAIN MANAGEMENT
// ============================================================================

std::vector<std::string> UCRustPlugin::ListToolchains() {
    std::vector<std::string> tc; std::string out, err; int code;
    if (ExecuteCommand({pluginConfig.rustupPath, "toolchain", "list"}, out, err, code)) {
        std::istringstream iss(out); std::string line;
        while (std::getline(iss, line)) { if (!line.empty()) { size_t p = line.find(" (default)"); if (p != std::string::npos) line = line.substr(0, p); tc.push_back(line); } }
    }
    cachedToolchains = tc; return tc;
}

std::string UCRustPlugin::GetActiveToolchain() {
    std::string out, err; int code;
    if (ExecuteCommand({pluginConfig.rustupPath, "default"}, out, err, code)) { size_t sp = out.find(' '); return sp != std::string::npos ? out.substr(0, sp) : out; }
    return "";
}

bool UCRustPlugin::SetActiveToolchain(const std::string& tc) {
    std::string out, err; int code;
    if (ExecuteCommand({pluginConfig.rustupPath, "default", tc}, out, err, code) && code == 0) { cachedVersion.clear(); versionInfo = RustVersionInfo(); GetCompilerVersion(); GetVersionInfo(); return true; }
    return false;
}

bool UCRustPlugin::InstallToolchain(const std::string& tc, std::function<void(const std::string&)> cb) {
    std::string out, err; int code;
    bool ok = ExecuteCommand({pluginConfig.rustupPath, "toolchain", "install", tc}, out, err, code);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::UpdateToolchains(std::function<void(const std::string&)> cb) {
    std::string out, err; int code;
    bool ok = ExecuteCommand({pluginConfig.rustupPath, "update"}, out, err, code);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    if (ok && code == 0) { cachedVersion.clear(); versionInfo = RustVersionInfo(); return true; }
    return false;
}

// ============================================================================
// TARGET MANAGEMENT
// ============================================================================

std::vector<RustTarget> UCRustPlugin::ListInstalledTargets() {
    std::vector<RustTarget> tgts; std::string out, err; int code;
    if (ExecuteCommand({pluginConfig.rustupPath, "target", "list", "--installed"}, out, err, code)) {
        std::istringstream iss(out); std::string line;
        while (std::getline(iss, line)) { if (!line.empty()) { RustTarget t = RustTarget::Parse(line); t.isInstalled = true; tgts.push_back(t); } }
    }
    return tgts;
}

std::vector<RustTarget> UCRustPlugin::ListAvailableTargets() {
    std::vector<RustTarget> tgts; std::string out, err; int code;
    if (ExecuteCommand({pluginConfig.rustupPath, "target", "list"}, out, err, code)) {
        std::istringstream iss(out); std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) { bool inst = line.find("(installed)") != std::string::npos; size_t p = line.find(" (installed)"); if (p != std::string::npos) line = line.substr(0, p); RustTarget t = RustTarget::Parse(line); t.isInstalled = inst; tgts.push_back(t); }
        }
    }
    return tgts;
}

bool UCRustPlugin::AddTarget(const std::string& tgt, std::function<void(const std::string&)> cb) {
    std::string out, err; int code;
    bool ok = ExecuteCommand({pluginConfig.rustupPath, "target", "add", tgt}, out, err, code);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::RemoveTarget(const std::string& tgt) {
    std::string out, err; int code;
    return ExecuteCommand({pluginConfig.rustupPath, "target", "remove", tgt}, out, err, code) && code == 0;
}

void UCRustPlugin::SetConfig(const RustPluginConfig& cfg) {
    pluginConfig = cfg; installationDetected = false; cachedVersion.clear(); versionInfo = RustVersionInfo(); cachedToolchains.clear(); cachedTargets.clear(); DetectInstallation();
}

} // namespace IDE
} // namespace UltraCanvas
