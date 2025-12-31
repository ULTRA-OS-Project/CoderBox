// Apps/IDE/Build/Plugins/UCRustPlugin_Cargo.cpp
// Rust/Cargo Plugin - Cargo Operations, Testing, Linting, Documentation
// Part of UCRustPlugin implementation

#include "UCRustPlugin.h"
#include <sstream>
#include <fstream>
#include <regex>
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
// CARGO PROJECT OPERATIONS
// ============================================================================

bool UCRustPlugin::CargoNew(const std::string& path, bool lib, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("new"); if (lib) args.push_back("--lib"); args.push_back(path);
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::CargoInit(const std::string& path, bool lib, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("init"); if (lib) args.push_back("--lib"); args.push_back(path);
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

BuildResult UCRustPlugin::CargoBuild(const std::string& proj, const BuildConfiguration& cfg, std::function<void(const std::string&)> cb) {
    BuildResult result;
    auto args = GetCargoBaseArgs(); args.push_back("build"); args.push_back("--message-format=json");
    if (cfg.name == "Release" || cfg.optimizationLevel > 1) args.push_back("--release");
    if (!pluginConfig.targetTriple.empty()) { args.push_back("--target"); args.push_back(pluginConfig.targetTriple); }
    if (pluginConfig.jobs > 0) { args.push_back("-j"); args.push_back(std::to_string(pluginConfig.jobs)); }
    if (pluginConfig.allFeatures) args.push_back("--all-features");
    else if (pluginConfig.noDefaultFeatures) args.push_back("--no-default-features");
    if (!pluginConfig.features.empty()) { args.push_back("--features"); std::string f; for (size_t i = 0; i < pluginConfig.features.size(); ++i) { if (i > 0) f += ","; f += pluginConfig.features[i]; } args.push_back(f); }
    if (pluginConfig.verboseOutput) args.push_back("-v");
    for (const auto& a : cfg.extraArgs) args.push_back(a);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, proj);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    std::istringstream iss(out); std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] != '{') continue;
        if (line.find("\"reason\":\"compiler-message\"") != std::string::npos) ParseJsonDiagnostic(line, result.messages);
        else if (line.find("\"reason\":\"compiler-artifact\"") != std::string::npos) { auto a = ParseArtifact(line); if (onArtifactBuilt) onArtifactBuilt(a); if (!a.filePath.empty()) result.outputPath = a.filePath; }
    }
    if (cb && !err.empty()) cb(err);
    for (const auto& m : result.messages) { if (m.type == CompilerMessageType::Error || m.type == CompilerMessageType::FatalError) result.errorCount++; else if (m.type == CompilerMessageType::Warning) result.warningCount++; }
    result.success = (code == 0);
    return result;
}

BuildResult UCRustPlugin::CargoRun(const std::string& proj, const std::vector<std::string>& pargs, std::function<void(const std::string&)> cb) {
    BuildResult result;
    auto args = GetCargoBaseArgs(); args.push_back("run");
    if (!pargs.empty()) { args.push_back("--"); for (const auto& a : pargs) args.push_back(a); }
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, proj);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    result.success = (code == 0);
    return result;
}

BuildResult UCRustPlugin::CargoCheck(const std::string& proj, std::function<void(const std::string&)> cb) {
    BuildResult result;
    auto args = GetCargoBaseArgs(); args.push_back("check"); args.push_back("--message-format=json");
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, proj);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    std::istringstream iss(out); std::string line;
    while (std::getline(iss, line)) { if (!line.empty() && line[0] == '{' && line.find("\"reason\":\"compiler-message\"") != std::string::npos) ParseJsonDiagnostic(line, result.messages); }
    if (cb && !err.empty()) cb(err);
    for (const auto& m : result.messages) { if (m.type == CompilerMessageType::Error || m.type == CompilerMessageType::FatalError) result.errorCount++; else if (m.type == CompilerMessageType::Warning) result.warningCount++; }
    result.success = (code == 0);
    return result;
}

bool UCRustPlugin::CargoClean(const std::string& proj, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("clean");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::CargoUpdate(const std::string& proj, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("update");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::CargoFetch(const std::string& proj, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("fetch");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

CargoPackageInfo UCRustPlugin::CargoMetadata(const std::string& proj) {
    CargoPackageInfo info;
    auto args = GetCargoBaseArgs(); args.push_back("metadata"); args.push_back("--format-version=1"); args.push_back("--no-deps");
    std::string out, err; int code;
    if (!ExecuteCommand(args, out, err, code, proj) || code != 0) return info;
    std::smatch m;
    if (std::regex_search(out, m, std::regex(R"("name"\s*:\s*"([^"]+)")"))) info.name = m[1];
    if (std::regex_search(out, m, std::regex(R"("version"\s*:\s*"([^"]+)")"))) info.version = m[1];
    if (std::regex_search(out, m, std::regex(R"("edition"\s*:\s*"(\d+)")"))) info.edition = StringToRustEdition(m[1]);
    return info;
}

std::vector<CargoPackageInfo> UCRustPlugin::CargoSearch(const std::string& query, int limit) {
    std::vector<CargoPackageInfo> results;
    auto args = GetCargoBaseArgs(); args.push_back("search"); args.push_back("--limit"); args.push_back(std::to_string(limit)); args.push_back(query);
    std::string out, err; int code;
    if (!ExecuteCommand(args, out, err, code) || code != 0) return results;
    std::regex re(R"(^(\S+)\s*=\s*"([^"]+)"\s*#\s*(.*)$)");
    std::istringstream iss(out); std::string line;
    while (std::getline(iss, line)) { std::smatch m; if (std::regex_search(line, m, re)) { CargoPackageInfo i; i.name = m[1]; i.version = m[2]; i.description = m[3]; results.push_back(i); } }
    return results;
}

bool UCRustPlugin::CargoAdd(const std::string& proj, const std::string& dep, bool dev, bool build, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("add"); if (dev) args.push_back("--dev"); if (build) args.push_back("--build"); args.push_back(dep);
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::CargoRemove(const std::string& proj, const std::string& dep, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("remove"); args.push_back(dep);
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

// ============================================================================
// LINTING (CLIPPY)
// ============================================================================

RustLintResult UCRustPlugin::RunClippy(const std::string& proj, std::function<void(const std::string&)> cb) {
    RustLintResult result; result.tool = "clippy";
    auto args = GetCargoBaseArgs(); args.push_back("clippy"); args.push_back("--message-format=json");
    if (pluginConfig.warningsAsErrors || pluginConfig.denyAllWarnings) { args.push_back("--"); args.push_back("-D"); args.push_back("warnings"); }
    std::string out, err; int code; ExecuteCommand(args, out, err, code, proj);
    std::istringstream iss(out); std::string line;
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] != '{') continue;
        if (line.find("\"reason\":\"compiler-message\"") != std::string::npos) {
            std::vector<CompilerMessage> msgs; ParseJsonDiagnostic(line, msgs);
            for (auto& m : msgs) { result.messages.push_back(m); if (m.type == CompilerMessageType::Error) result.errorCount++; else if (m.type == CompilerMessageType::Warning) result.warningCount++; else if (m.type == CompilerMessageType::Note) result.noteCount++; else result.helpCount++; }
        }
    }
    if (cb && !err.empty()) cb(err);
    result.passed = (code == 0);
    if (onLintComplete) onLintComplete(result);
    return result;
}

bool UCRustPlugin::ClippyFix(const std::string& proj, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("clippy"); args.push_back("--fix"); args.push_back("--allow-dirty"); args.push_back("--allow-staged");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

// ============================================================================
// FORMATTING
// ============================================================================

RustLintResult UCRustPlugin::CheckFormat(const std::string& proj, std::function<void(const std::string&)> cb) {
    RustLintResult result; result.tool = "rustfmt";
    auto args = GetCargoBaseArgs(); args.push_back("fmt"); args.push_back("--check");
    std::string out, err; int code; ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    std::regex re(R"(Diff in (.+) at line (\d+):)");
    std::istringstream iss(err); std::string line;
    while (std::getline(iss, line)) { std::smatch m; if (std::regex_search(line, m, re)) { CompilerMessage msg; msg.type = CompilerMessageType::Warning; msg.file = m[1]; msg.line = std::stoi(m[2]); msg.message = "Not formatted"; result.messages.push_back(msg); result.warningCount++; } }
    result.passed = (code == 0);
    return result;
}

bool UCRustPlugin::Format(const std::string& proj, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("fmt");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::FormatFile(const std::string& file, std::function<void(const std::string&)> cb) {
    std::vector<std::string> args; args.push_back(pluginConfig.rustfmtPath); args.push_back(file);
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

// ============================================================================
// TESTING
// ============================================================================

RustTestSummary UCRustPlugin::CargoTest(const std::string& proj, const std::string& filter, std::function<void(const std::string&)> cb) {
    RustTestSummary sum;
    auto args = GetCargoBaseArgs(); args.push_back("test"); if (!filter.empty()) args.push_back(filter);
    args.push_back("--");
    if (!pluginConfig.testThreaded || pluginConfig.testThreads == 1) args.push_back("--test-threads=1");
    else if (pluginConfig.testThreads > 1) args.push_back("--test-threads=" + std::to_string(pluginConfig.testThreads));
    if (pluginConfig.nocapture) args.push_back("--nocapture");
    if (pluginConfig.showOutput) args.push_back("--show-output");
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, proj);
    sum.totalDuration = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    
    std::regex re(R"(test (\S+) \.\.\. (\w+))");
    std::istringstream iss(out + err); std::string line;
    while (std::getline(iss, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            RustTestResult t; t.name = m[1]; size_t c = t.name.rfind("::"); if (c != std::string::npos) t.module = t.name.substr(0, c);
            std::string s = m[2]; if (s == "ok") { t.status = RustTestResult::Status::Passed; sum.passed++; } else if (s == "FAILED") { t.status = RustTestResult::Status::Failed; sum.failed++; } else if (s == "ignored") { t.status = RustTestResult::Status::Ignored; sum.ignored++; }
            sum.tests.push_back(t);
        }
    }
    sum.allPassed = (code == 0);
    if (onTestComplete) onTestComplete(sum);
    return sum;
}

RustTestSummary UCRustPlugin::RunTest(const std::string& proj, const std::string& name, std::function<void(const std::string&)> cb) { return CargoTest(proj, name, cb); }

RustTestSummary UCRustPlugin::CargoBench(const std::string& proj, const std::string& filter, std::function<void(const std::string&)> cb) {
    RustTestSummary sum;
    auto args = GetCargoBaseArgs(); args.push_back("bench"); if (!filter.empty()) args.push_back(filter);
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, proj);
    sum.totalDuration = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    
    std::regex re(R"(test (\S+)\s+\.\.\. bench:\s+([\d,]+) ns/iter \(\+/- ([\d,]+)\))");
    std::istringstream iss(out + err); std::string line;
    while (std::getline(iss, line)) {
        std::smatch m;
        if (std::regex_search(line, m, re)) {
            RustTestResult t; t.name = m[1]; t.status = RustTestResult::Status::Measured;
            std::string ns = m[2]; ns.erase(std::remove(ns.begin(), ns.end(), ','), ns.end()); t.nsPerIter = std::stoll(ns);
            std::string dev = m[3]; dev.erase(std::remove(dev.begin(), dev.end(), ','), dev.end()); t.deviation = std::stoll(dev);
            sum.tests.push_back(t); sum.measured++;
        }
    }
    sum.allPassed = (code == 0);
    if (onTestComplete) onTestComplete(sum);
    return sum;
}

std::vector<std::string> UCRustPlugin::ListTests(const std::string& proj) {
    std::vector<std::string> tests;
    auto args = GetCargoBaseArgs(); args.push_back("test"); args.push_back("--"); args.push_back("--list");
    std::string out, err; int code;
    if (!ExecuteCommand(args, out, err, code, proj)) return tests;
    std::regex re(R"(^(\S+): test$)");
    std::istringstream iss(out); std::string line;
    while (std::getline(iss, line)) { std::smatch m; if (std::regex_search(line, m, re)) tests.push_back(m[1]); }
    return tests;
}

// ============================================================================
// DOCUMENTATION
// ============================================================================

bool UCRustPlugin::CargoDoc(const std::string& proj, bool open, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("doc"); if (open) args.push_back("--open"); if (pluginConfig.documentPrivate) args.push_back("--document-private-items");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

RustTestSummary UCRustPlugin::CargoDoctest(const std::string& proj, std::function<void(const std::string&)> cb) {
    RustTestSummary sum;
    auto args = GetCargoBaseArgs(); args.push_back("test"); args.push_back("--doc");
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code, proj);
    sum.totalDuration = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    sum.allPassed = (code == 0);
    return sum;
}

// ============================================================================
// PUBLISHING
// ============================================================================

bool UCRustPlugin::CargoPackage(const std::string& proj, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("package"); args.push_back("--allow-dirty");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

bool UCRustPlugin::CargoPublish(const std::string& proj, bool dry, std::function<void(const std::string&)> cb) {
    auto args = GetCargoBaseArgs(); args.push_back("publish"); if (dry) args.push_back("--dry-run");
    std::string out, err; int code; bool ok = ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    return ok && code == 0;
}

// ============================================================================
// ANALYSIS
// ============================================================================

std::string UCRustPlugin::CargoExpand(const std::string& proj, const std::string& item) {
    auto args = GetCargoBaseArgs(); args.push_back("expand"); if (!item.empty()) args.push_back(item);
    std::string out, err; int code;
    if (ExecuteCommand(args, out, err, code, proj) && code == 0) return out;
    return err;
}

std::string UCRustPlugin::CargoTree(const std::string& proj, bool dup, bool inv) {
    auto args = GetCargoBaseArgs(); args.push_back("tree"); if (dup) args.push_back("--duplicates"); if (inv) args.push_back("--invert");
    std::string out, err; int code; ExecuteCommand(args, out, err, code, proj);
    return out.empty() ? err : out;
}

UCRustPlugin::AuditResult UCRustPlugin::CargoAudit(const std::string& proj, std::function<void(const std::string&)> cb) {
    AuditResult result;
    auto args = GetCargoBaseArgs(); args.push_back("audit");
    std::string out, err; int code; ExecuteCommand(args, out, err, code, proj);
    if (cb) { cb(out); if (!err.empty()) cb(err); }
    std::string combined = out + err;
    if (combined.find("Vulnerability found!") != std::string::npos) result.vulnerabilities++;
    if (combined.find("warning:") != std::string::npos) result.warnings++;
    result.passed = (code == 0 && result.vulnerabilities == 0);
    return result;
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCRustPlugin::ExecuteCommand(const std::vector<std::string>& args, std::string& out, std::string& err, int& code, const std::string& cwd) {
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
    while (ReadFile(oR, buf, sizeof(buf)-1, &n, NULL) && n > 0) { if (cancelRequested) break; buf[n] = '\0'; out += buf; }
    while (ReadFile(eR, buf, sizeof(buf)-1, &n, NULL) && n > 0) { if (cancelRequested) break; buf[n] = '\0'; err += buf; }
    if (cancelRequested) TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec; GetExitCodeProcess(pi.hProcess, &ec); code = static_cast<int>(ec);
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
    while ((n = read(op[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; out += buf; }
    while ((n = read(ep[0], buf, sizeof(buf)-1)) > 0) { buf[n] = '\0'; err += buf; }
    close(op[0]); close(ep[0]);
    int st; waitpid(pid, &st, 0);
    if (WIFEXITED(st)) code = WEXITSTATUS(st); else if (WIFSIGNALED(st)) code = 128 + WTERMSIG(st); else code = -1;
    currentProcess = nullptr; return true;
#endif
}

bool UCRustPlugin::ExecuteCommandAsync(const std::vector<std::string>& args, std::function<void(const std::string&)> onLine, std::function<void(int)> onDone, const std::string& cwd) {
    if (buildInProgress) return false;
    buildInProgress = true; cancelRequested = false;
    buildThread = std::thread([this, args, onLine, onDone, cwd]() {
        std::string out, err; int code; ExecuteCommand(args, out, err, code, cwd);
        if (onLine) { std::istringstream iss(out); std::string line; while (std::getline(iss, line)) onLine(line); std::istringstream eis(err); while (std::getline(eis, line)) onLine(line); }
        buildInProgress = false; if (onDone) onDone(code);
    });
    return true;
}

} // namespace IDE
} // namespace UltraCanvas
