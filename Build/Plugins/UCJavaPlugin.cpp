// Apps/IDE/Build/Plugins/UCJavaPlugin.cpp
// Java Compiler Plugin Implementation for ULTRA IDE
// Version: 1.0.0

#include "UCJavaPlugin.h"
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
#include <dirent.h>
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

JavaPluginConfig JavaPluginConfig::Default() {
    JavaPluginConfig config;
    const char* javaHome = std::getenv("JAVA_HOME");
    if (javaHome) {
        config.javaHome = javaHome;
#ifdef _WIN32
        std::string bin = std::string(javaHome) + "\\bin\\";
        config.javacPath = bin + "javac.exe"; config.javaPath = bin + "java.exe";
        config.jarPath = bin + "jar.exe"; config.javadocPath = bin + "javadoc.exe";
        config.jdepsPath = bin + "jdeps.exe";
#else
        std::string bin = std::string(javaHome) + "/bin/";
        config.javacPath = bin + "javac"; config.javaPath = bin + "java";
        config.jarPath = bin + "jar"; config.javadocPath = bin + "javadoc";
        config.jdepsPath = bin + "jdeps";
#endif
    } else {
        config.javacPath = "javac"; config.javaPath = "java"; config.jarPath = "jar";
        config.javadocPath = "javadoc"; config.jdepsPath = "jdeps";
    }
    config.sourceVersion = JavaVersion::Java21; config.targetVersion = JavaVersion::Java21;
    config.encoding = "UTF-8"; config.debugInfo = true; config.showAllWarnings = true;
    return config;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR / IDENTIFICATION
// ============================================================================

UCJavaPlugin::UCJavaPlugin() : pluginConfig(JavaPluginConfig::Default()) { DetectInstallation(); }
UCJavaPlugin::UCJavaPlugin(const JavaPluginConfig& config) : pluginConfig(config) { DetectInstallation(); }
UCJavaPlugin::~UCJavaPlugin() { Cancel(); if (buildThread.joinable()) buildThread.join(); }

std::string UCJavaPlugin::GetPluginName() const { return "Java Compiler Plugin"; }
std::string UCJavaPlugin::GetPluginVersion() const { return "1.0.0"; }
CompilerType UCJavaPlugin::GetCompilerType() const { return CompilerType::Custom; }
std::string UCJavaPlugin::GetCompilerName() const { return JDKTypeToString(detectedJDKType) + " " + detectedVersionString; }

// ============================================================================
// COMPILER DETECTION
// ============================================================================

bool UCJavaPlugin::IsAvailable() { return installationDetected ? detectedVersion != JavaVersion::Unknown : DetectInstallation(); }
std::string UCJavaPlugin::GetCompilerPath() { return GetJavacExecutable(); }
void UCJavaPlugin::SetCompilerPath(const std::string& path) { pluginConfig.javacPath = path; installationDetected = false; DetectInstallation(); }
std::string UCJavaPlugin::GetCompilerVersion() { return detectedVersionString; }
std::vector<std::string> UCJavaPlugin::GetSupportedExtensions() const { return {"java"}; }
bool UCJavaPlugin::CanCompile(const std::string& filePath) const {
    size_t dot = filePath.rfind('.'); if (dot == std::string::npos) return false;
    std::string ext = filePath.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "java";
}

bool UCJavaPlugin::DetectInstallation() {
    installationDetected = true; detectedJDKType = JDKType::Unknown;
    detectedVersion = JavaVersion::Unknown; detectedVersionString.clear();
    
    std::vector<std::string> paths = {pluginConfig.javacPath, "javac",
#ifdef _WIN32
        std::string(std::getenv("ProgramFiles") ? std::getenv("ProgramFiles") : "") + "\\Java\\jdk-21\\bin\\javac.exe"
#else
        "/usr/bin/javac", "/usr/lib/jvm/java-21-openjdk/bin/javac", "/usr/lib/jvm/default-java/bin/javac"
#endif
    };
    
    for (const auto& path : paths) {
        if (path.empty()) continue;
        std::string out, err; int code;
        if (ExecuteCommand({path, "-version"}, out, err, code)) {
            std::string combined = out + err;
            if (combined.find("javac") != std::string::npos) {
                pluginConfig.javacPath = path;
                size_t binPos = path.rfind("javac");
                if (binPos != std::string::npos) {
                    std::string bin = path.substr(0, binPos);
#ifdef _WIN32
                    pluginConfig.javaPath = bin + "java.exe"; pluginConfig.jarPath = bin + "jar.exe";
                    pluginConfig.javadocPath = bin + "javadoc.exe"; pluginConfig.jdepsPath = bin + "jdeps.exe";
#else
                    pluginConfig.javaPath = bin + "java"; pluginConfig.jarPath = bin + "jar";
                    pluginConfig.javadocPath = bin + "javadoc"; pluginConfig.jdepsPath = bin + "jdeps";
#endif
                }
                detectedVersion = ParseJavaVersion(combined); DetectJDKType(combined);
                std::regex vr(R"(javac\s+(\d+(?:\.\d+)*))"); std::smatch m;
                if (std::regex_search(combined, m, vr)) detectedVersionString = m[1].str();
                return true;
            }
        }
    }
    return false;
}

bool UCJavaPlugin::DetectJDKType(const std::string& v) {
    std::string l = v; std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    if (l.find("graalvm") != std::string::npos) detectedJDKType = JDKType::GraalVM;
    else if (l.find("corretto") != std::string::npos) detectedJDKType = JDKType::AmazonCorretto;
    else if (l.find("zulu") != std::string::npos) detectedJDKType = JDKType::Azul;
    else if (l.find("temurin") != std::string::npos || l.find("adoptium") != std::string::npos) detectedJDKType = JDKType::AdoptOpenJDK;
    else if (l.find("oracle") != std::string::npos) detectedJDKType = JDKType::OracleJDK;
    else detectedJDKType = JDKType::OpenJDK;
    return true;
}

JavaVersion UCJavaPlugin::ParseJavaVersion(const std::string& v) {
    std::regex vr(R"((\d+)(?:\.(\d+))?)"); std::smatch m;
    if (std::regex_search(v, m, vr)) {
        int major = std::stoi(m[1].str());
        if (major == 1 && m[2].matched) major = std::stoi(m[2].str());
        switch (major) {
            case 8: return JavaVersion::Java8; case 11: return JavaVersion::Java11;
            case 17: return JavaVersion::Java17; case 21: return JavaVersion::Java21;
            case 22: return JavaVersion::Java22; case 23: return JavaVersion::Java23;
            default: return major >= 21 ? JavaVersion::Java21 : major >= 17 ? JavaVersion::Java17 : JavaVersion::Java11;
        }
    }
    return JavaVersion::Unknown;
}

std::vector<std::string> UCJavaPlugin::ListInstalledJDKs() {
    std::vector<std::string> jdks;
#ifndef _WIN32
    std::vector<std::string> dirs = {"/usr/lib/jvm", "/opt/java"};
    for (const auto& dir : dirs) {
        DIR* d = opendir(dir.c_str());
        if (d) {
            struct dirent* e;
            while ((e = readdir(d))) {
                if (e->d_name[0] != '.') {
                    std::string jh = dir + "/" + e->d_name;
                    std::ifstream t(jh + "/bin/javac"); if (t.good()) jdks.push_back(jh);
                }
            }
            closedir(d);
        }
    }
#endif
    return jdks;
}

bool UCJavaPlugin::SetJavaHome(const std::string& jh) {
    pluginConfig.javaHome = jh;
#ifdef _WIN32
    std::string bin = jh + "\\bin\\";
    pluginConfig.javacPath = bin + "javac.exe"; pluginConfig.javaPath = bin + "java.exe";
    pluginConfig.jarPath = bin + "jar.exe";
#else
    std::string bin = jh + "/bin/";
    pluginConfig.javacPath = bin + "javac"; pluginConfig.javaPath = bin + "java";
    pluginConfig.jarPath = bin + "jar";
#endif
    installationDetected = false; return DetectInstallation();
}

std::string UCJavaPlugin::GetJavacExecutable() const { return pluginConfig.javacPath; }
std::string UCJavaPlugin::GetJavaExecutable() const { return pluginConfig.javaPath; }
std::string UCJavaPlugin::GetJarExecutable() const { return pluginConfig.jarPath; }
std::string UCJavaPlugin::GetJavadocExecutable() const { return pluginConfig.javadocPath; }
std::string UCJavaPlugin::GetJdepsExecutable() const { return pluginConfig.jdepsPath; }

// ============================================================================
// COMPILATION
// ============================================================================

void UCJavaPlugin::CompileAsync(const std::vector<std::string>& files, const BuildConfiguration& cfg,
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

BuildResult UCJavaPlugin::CompileSync(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    BuildResult result; result.success = false;
    if (files.empty()) { result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No source files"}); result.errorCount = 1; return result; }
    
    auto cmd = GenerateCommandLine(files, cfg);
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    bool ok = ExecuteCommand(cmd, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (!ok) { result.messages.push_back({CompilerMessageType::FatalError, "", 0, 0, "Failed to execute javac"}); result.errorCount = 1; return result; }
    
    std::istringstream iss(out + "\n" + err); std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        auto msg = ParseOutputLine(line);
        if (!msg.message.empty() || !msg.file.empty()) {
            result.messages.push_back(msg);
            if (msg.type == CompilerMessageType::Error || msg.type == CompilerMessageType::FatalError) result.errorCount++;
            else if (msg.type == CompilerMessageType::Warning) result.warningCount++;
        }
    }
    result.success = (code == 0);
    return result;
}

void UCJavaPlugin::Cancel() {
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

bool UCJavaPlugin::IsBuildInProgress() const { return buildInProgress; }

// ============================================================================
// OUTPUT PARSING
// ============================================================================

CompilerMessage UCJavaPlugin::ParseOutputLine(const std::string& line) {
    CompilerMessage msg; msg.type = CompilerMessageType::Info; msg.line = 0; msg.column = 0;
    
    std::regex errRe(R"(^(.+\.java):(\d+):\s*(error|warning):\s*(.+)$)"); std::smatch m;
    if (std::regex_search(line, m, errRe)) {
        msg.file = m[1]; msg.line = std::stoi(m[2]); msg.message = m[4];
        msg.type = m[3].str() == "error" ? CompilerMessageType::Error : CompilerMessageType::Warning;
        pendingFile = msg.file; pendingLine = msg.line; inErrorBlock = true;
        return msg;
    }
    
    std::regex noteRe(R"(^(.+\.java):(\d+):\s*note:\s*(.+)$)");
    if (std::regex_search(line, m, noteRe)) {
        msg.file = m[1]; msg.line = std::stoi(m[2]); msg.message = m[3];
        msg.type = CompilerMessageType::Note; return msg;
    }
    
    if (line.find('^') != std::string::npos && inErrorBlock) {
        size_t col = line.find('^');
        msg.file = pendingFile; msg.line = pendingLine; msg.column = static_cast<int>(col + 1);
        msg.type = CompilerMessageType::Note; return msg;
    }
    
    msg.message = line; return msg;
}

// ============================================================================
// COMMAND LINE GENERATION
// ============================================================================

std::vector<std::string> UCJavaPlugin::GenerateCommandLine(const std::vector<std::string>& files, const BuildConfiguration& cfg) {
    std::vector<std::string> args; args.push_back(GetJavacExecutable());
    
    args.push_back("-source"); args.push_back(JavaVersionToSourceString(pluginConfig.sourceVersion));
    args.push_back("-target"); args.push_back(JavaVersionToSourceString(pluginConfig.targetVersion));
    
    if (pluginConfig.enablePreview) args.push_back("--enable-preview");
    if (!pluginConfig.encoding.empty()) { args.push_back("-encoding"); args.push_back(pluginConfig.encoding); }
    
    if (pluginConfig.warningsAsErrors) args.push_back("-Werror");
    if (pluginConfig.showAllWarnings) args.push_back("-Xlint:all");
    else { if (pluginConfig.showDeprecation) args.push_back("-Xlint:deprecation"); if (pluginConfig.showUnchecked) args.push_back("-Xlint:unchecked"); }
    
    if (pluginConfig.debugInfo) args.push_back("-g"); else args.push_back("-g:none");
    
    std::string outDir = cfg.outputPath.empty() ? pluginConfig.outputDirectory : cfg.outputPath;
    if (!outDir.empty()) { args.push_back("-d"); args.push_back(outDir); }
    
    std::string cp = BuildClasspath();
    if (!cp.empty()) { args.push_back("-classpath"); args.push_back(cp); }
    
    if (static_cast<int>(detectedVersion) >= 9) {
        std::string mp = BuildModulePath();
        if (!mp.empty()) { args.push_back("--module-path"); args.push_back(mp); }
        for (const auto& mod : pluginConfig.addModules) { args.push_back("--add-modules"); args.push_back(mod); }
    }
    
    if (!pluginConfig.processAnnotations) args.push_back("-proc:none");
    if (pluginConfig.verbose) args.push_back("-verbose");
    
    for (const auto& a : cfg.extraArgs) args.push_back(a);
    for (const auto& f : files) args.push_back(f);
    
    return args;
}

std::vector<std::string> UCJavaPlugin::GetClasspathArgs() const {
    std::vector<std::string> args; std::string cp = BuildClasspath();
    if (!cp.empty()) { args.push_back("-classpath"); args.push_back(cp); }
    return args;
}

std::vector<std::string> UCJavaPlugin::GetModuleArgs() const {
    std::vector<std::string> args; std::string mp = BuildModulePath();
    if (!mp.empty()) { args.push_back("--module-path"); args.push_back(mp); }
    for (const auto& m : pluginConfig.addModules) { args.push_back("--add-modules"); args.push_back(m); }
    return args;
}

std::vector<std::string> UCJavaPlugin::GetWarningArgs() const {
    std::vector<std::string> args;
    if (pluginConfig.warningsAsErrors) args.push_back("-Werror");
    if (pluginConfig.showAllWarnings) args.push_back("-Xlint:all");
    return args;
}

std::vector<std::string> UCJavaPlugin::GetDebugArgs() const {
    return pluginConfig.debugInfo ? std::vector<std::string>{"-g"} : std::vector<std::string>{"-g:none"};
}

std::vector<std::string> UCJavaPlugin::GetJVMArgs() const {
    std::vector<std::string> args;
    if (pluginConfig.heapSizeInitialMB > 0) args.push_back("-Xms" + std::to_string(pluginConfig.heapSizeInitialMB) + "m");
    if (pluginConfig.heapSizeMaxMB > 0) args.push_back("-Xmx" + std::to_string(pluginConfig.heapSizeMaxMB) + "m");
    if (pluginConfig.enableAssertions) args.push_back("-ea");
    for (const auto& p : pluginConfig.systemProperties) args.push_back("-D" + p);
    for (const auto& a : pluginConfig.jvmArgs) args.push_back(a);
    return args;
}

std::string UCJavaPlugin::BuildClasspath() const {
    std::string cp;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    for (const auto& p : pluginConfig.classpath) { if (!cp.empty()) cp += sep; cp += p; }
    return cp;
}

std::string UCJavaPlugin::BuildModulePath() const {
    std::string mp;
#ifdef _WIN32
    const char sep = ';';
#else
    const char sep = ':';
#endif
    for (const auto& p : pluginConfig.modulePath) { if (!mp.empty()) mp += sep; mp += p; }
    return mp;
}

// ============================================================================
// EXECUTION
// ============================================================================

BuildResult UCJavaPlugin::RunClass(const std::string& className, const std::vector<std::string>& args,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> cmd; cmd.push_back(GetJavaExecutable());
    auto jvm = GetJVMArgs(); cmd.insert(cmd.end(), jvm.begin(), jvm.end());
    
    std::string cp = BuildClasspath();
    if (!cp.empty()) { cmd.push_back("-classpath"); cmd.push_back(cp); }
    else if (!pluginConfig.outputDirectory.empty()) { cmd.push_back("-classpath"); cmd.push_back(pluginConfig.outputDirectory); }
    if (pluginConfig.enablePreview) cmd.push_back("--enable-preview");
    
    cmd.push_back(className);
    for (const auto& a : args) cmd.push_back(a);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(cmd, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    if (onRuntimeOutput && !out.empty()) onRuntimeOutput(out);
    if (onRuntimeError && !err.empty()) onRuntimeError(err);
    
    if (!err.empty() && code != 0) {
        std::istringstream iss(err); std::string line;
        while (std::getline(iss, line)) {
            if (line.find("Exception") != std::string::npos || line.find("Error") != std::string::npos) {
                result.messages.push_back({CompilerMessageType::Error, "", 0, 0, line}); result.errorCount++;
            }
        }
    }
    result.success = (code == 0); return result;
}

BuildResult UCJavaPlugin::RunJar(const std::string& jarFile, const std::vector<std::string>& args,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> cmd; cmd.push_back(GetJavaExecutable());
    auto jvm = GetJVMArgs(); cmd.insert(cmd.end(), jvm.begin(), jvm.end());
    cmd.push_back("-jar"); cmd.push_back(jarFile);
    for (const auto& a : args) cmd.push_back(a);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(cmd, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0); return result;
}

BuildResult UCJavaPlugin::RunModule(const std::string& modName, const std::string& mainClass,
    const std::vector<std::string>& args, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> cmd; cmd.push_back(GetJavaExecutable());
    auto jvm = GetJVMArgs(); cmd.insert(cmd.end(), jvm.begin(), jvm.end());
    std::string mp = BuildModulePath();
    if (!mp.empty()) { cmd.push_back("--module-path"); cmd.push_back(mp); }
    cmd.push_back("-m"); cmd.push_back(modName + "/" + mainClass);
    for (const auto& a : args) cmd.push_back(a);
    
    std::string out, err; int code;
    ExecuteCommand(cmd, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0); return result;
}

BuildResult UCJavaPlugin::RunWithOptions(const std::string& className, const std::vector<std::string>& jvmArgs,
    const std::vector<std::string>& progArgs, std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> cmd; cmd.push_back(GetJavaExecutable());
    for (const auto& a : jvmArgs) cmd.push_back(a);
    cmd.push_back(className);
    for (const auto& a : progArgs) cmd.push_back(a);
    
    std::string out, err; int code;
    ExecuteCommand(cmd, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0); return result;
}

// ============================================================================
// JAR OPERATIONS
// ============================================================================

bool UCJavaPlugin::CreateJar(const std::string& jarFile, const std::string& classesDir, const std::string& mainClass,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args; args.push_back(GetJarExecutable());
    args.push_back(mainClass.empty() ? "cf" : "cfe"); args.push_back(jarFile);
    if (!mainClass.empty()) args.push_back(mainClass);
    args.push_back("-C"); args.push_back(classesDir); args.push_back(".");
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCJavaPlugin::CreateJarWithManifest(const std::string& jarFile, const std::string& classesDir, const std::string& manifest,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetJarExecutable(), "cfm", jarFile, manifest, "-C", classesDir, "."};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCJavaPlugin::UpdateJar(const std::string& jarFile, const std::vector<std::string>& files,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetJarExecutable(), "uf", jarFile};
    for (const auto& f : files) args.push_back(f);
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

bool UCJavaPlugin::ExtractJar(const std::string& jarFile, const std::string& outputDir,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetJarExecutable(), "xf", jarFile};
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code, outputDir);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

std::vector<std::string> UCJavaPlugin::ListJarContents(const std::string& jarFile) {
    std::vector<std::string> contents;
    std::string out, err; int code;
    if (ExecuteCommand({GetJarExecutable(), "tf", jarFile}, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line;
        while (std::getline(iss, line)) if (!line.empty()) contents.push_back(line);
    }
    return contents;
}

JarInfo UCJavaPlugin::GetJarInfo(const std::string& jarFile) {
    JarInfo info; info.path = jarFile;
    std::ifstream f(jarFile, std::ios::binary | std::ios::ate); if (f) info.fileSize = f.tellg();
    auto contents = ListJarContents(jarFile);
    for (const auto& e : contents) {
        if (e.find(".class") != std::string::npos) { info.classEntries.push_back(e); if (e == "module-info.class") info.isModular = true; }
        else if (e.find("META-INF/versions/") != std::string::npos) info.isMultiRelease = true;
        else info.resourceEntries.push_back(e);
    }
    return info;
}

// ============================================================================
// JAVADOC
// ============================================================================

BuildResult UCJavaPlugin::GenerateJavadoc(const std::vector<std::string>& files, const std::string& outputDir,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    std::vector<std::string> args = {GetJavadocExecutable(), "-d", outputDir};
    if (!pluginConfig.encoding.empty()) { args.push_back("-encoding"); args.push_back(pluginConfig.encoding); }
    std::string cp = BuildClasspath(); if (!cp.empty()) { args.push_back("-classpath"); args.push_back(cp); }
    for (const auto& f : files) args.push_back(f);
    
    std::string out, err; int code;
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    result.success = (code == 0); result.outputPath = outputDir;
    return result;
}

BuildResult UCJavaPlugin::GenerateJavadocForPackage(const std::string& pkg, const std::string& srcDir, const std::string& outDir,
    std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetJavadocExecutable(), "-d", outDir, "-sourcepath", srcDir, pkg};
    std::string out, err; int code;
    ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    BuildResult r; r.success = (code == 0); r.outputPath = outDir; return r;
}

// ============================================================================
// MODULE SYSTEM
// ============================================================================

BuildResult UCJavaPlugin::CompileModule(const std::string& modSrcDir, const std::string& outDir,
    std::function<void(const std::string&)> onOutput) {
    BuildResult result;
    
    // Find all .java files in module source directory
    std::vector<std::string> sourceFiles;
#ifndef _WIN32
    std::string out, err; int code;
    if (ExecuteCommand({"find", modSrcDir, "-name", "*.java", "-type", "f"}, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string file;
        while (std::getline(iss, file)) {
            if (!file.empty()) sourceFiles.push_back(file);
        }
    }
#endif
    
    if (sourceFiles.empty()) {
        result.messages.push_back({CompilerMessageType::Error, "", 0, 0, "No source files found in module directory"});
        result.errorCount = 1;
        return result;
    }
    
    // Build with module path
    std::vector<std::string> args = {GetJavacExecutable()};
    args.push_back("-d"); args.push_back(outDir);
    
    std::string mp = BuildModulePath();
    if (!mp.empty()) { args.push_back("--module-path"); args.push_back(mp); }
    
    if (!pluginConfig.encoding.empty()) { args.push_back("-encoding"); args.push_back(pluginConfig.encoding); }
    
    for (const auto& f : sourceFiles) args.push_back(f);
    
    auto t1 = std::chrono::steady_clock::now();
    ExecuteCommand(args, out, err, code);
    result.buildTime = std::chrono::duration<float>(std::chrono::steady_clock::now() - t1).count();
    
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    
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

JavaModuleInfo UCJavaPlugin::GetModuleInfo(const std::string& path) {
    JavaModuleInfo info;
    std::ifstream f(path); if (!f) return info;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    std::regex modRe(R"((?:open\s+)?module\s+(\S+)\s*\{)"); std::smatch m;
    if (std::regex_search(content, m, modRe)) { info.name = m[1]; info.isOpen = content.find("open module") != std::string::npos; }
    
    std::regex reqRe(R"(requires\s+(transitive\s+)?(\S+)\s*;)");
    std::sregex_iterator it(content.begin(), content.end(), reqRe), end;
    while (it != end) { if ((*it)[1].matched) info.requiresTransitive.push_back((*it)[2]); else info.requires.push_back((*it)[2]); ++it; }
    
    std::regex expRe(R"(exports\s+(\S+)\s*;)");
    it = std::sregex_iterator(content.begin(), content.end(), expRe);
    while (it != end) { info.exports.push_back((*it)[1]); ++it; }
    
    return info;
}

bool UCJavaPlugin::CreateModularJar(const std::string& jarFile, const std::string& classesDir, const std::string& mainClass,
    const std::string& modVer, std::function<void(const std::string&)> onOutput) {
    std::vector<std::string> args = {GetJarExecutable(), "--create", "--file", jarFile};
    if (!mainClass.empty()) { args.push_back("--main-class"); args.push_back(mainClass); }
    if (!modVer.empty()) { args.push_back("--module-version"); args.push_back(modVer); }
    args.push_back("-C"); args.push_back(classesDir); args.push_back(".");
    
    std::string out, err; int code;
    bool ok = ExecuteCommand(args, out, err, code);
    if (onOutput) { if (!out.empty()) onOutput(out); if (!err.empty()) onOutput(err); }
    return ok && code == 0;
}

// ============================================================================
// SOURCE ANALYSIS
// ============================================================================

JavaClassInfo UCJavaPlugin::ParseClassInfo(const std::string& srcFile) {
    JavaClassInfo info; info.sourceFile = srcFile;
    std::ifstream f(srcFile); if (!f) return info;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ParseClassFromSource(content, srcFile, info);
    return info;
}

void UCJavaPlugin::ParseClassFromSource(const std::string& src, const std::string&, JavaClassInfo& info) {
    std::regex pkgRe(R"(package\s+([\w.]+)\s*;)"); std::smatch m;
    if (std::regex_search(src, m, pkgRe)) info.packageName = m[1];
    
    std::regex clsRe(R"((public\s+)?(abstract\s+)?(final\s+)?(sealed\s+)?(class|interface|enum|record)\s+(\w+))");
    if (std::regex_search(src, m, clsRe)) {
        info.isPublic = m[1].matched; info.isAbstract = m[2].matched;
        info.isFinal = m[3].matched; info.isSealed = m[4].matched;
        std::string t = m[5]; info.isInterface = (t == "interface"); info.isEnum = (t == "enum"); info.isRecord = (t == "record");
        info.name = m[6]; info.fullName = info.packageName.empty() ? info.name : info.packageName + "." + info.name;
    }
    
    std::regex extRe(R"(extends\s+([\w.]+))");
    if (std::regex_search(src, m, extRe)) info.superClass = m[1];
}

std::vector<JavaClassInfo> UCJavaPlugin::ScanClasses(const std::string& directory) {
    std::vector<JavaClassInfo> classes;
#ifndef _WIN32
    // Recursively find .java files
    std::string out, err; int code;
    if (ExecuteCommand({"find", directory, "-name", "*.java", "-type", "f"}, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string file;
        while (std::getline(iss, file)) {
            if (!file.empty()) {
                auto info = ParseClassInfo(file);
                if (!info.name.empty()) classes.push_back(info);
            }
        }
    }
#endif
    return classes;
}

std::vector<JavaMethodInfo> UCJavaPlugin::GetMethods(const std::string& srcFile) {
    std::vector<JavaMethodInfo> methods;
    std::ifstream f(srcFile); if (!f) return methods;
    
    std::string line; int lineNum = 0;
    std::regex methodRe(R"((public|private|protected)?\s*(static)?\s*(final)?\s*(synchronized)?\s*([\w<>\[\],\s]+)\s+(\w+)\s*\(([^)]*)\))");
    
    while (std::getline(f, line)) {
        lineNum++;
        // Skip comments and imports
        if (line.find("//") == 0 || line.find("import ") != std::string::npos || 
            line.find("package ") != std::string::npos) continue;
        
        std::smatch m;
        if (std::regex_search(line, m, methodRe)) {
            JavaMethodInfo mi; mi.line = lineNum;
            if (m[1].matched) mi.accessModifier = m[1];
            mi.isStatic = m[2].matched;
            mi.isFinal = m[3].matched;
            mi.isSynchronized = m[4].matched;
            mi.returnType = m[5];
            mi.name = m[6];
            
            // Parse parameters
            std::string params = m[7];
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

std::vector<JavaFieldInfo> UCJavaPlugin::GetFields(const std::string& srcFile) {
    std::vector<JavaFieldInfo> fields;
    std::ifstream f(srcFile); if (!f) return fields;
    
    std::string line; int lineNum = 0;
    std::regex fieldRe(R"((public|private|protected)?\s*(static)?\s*(final)?\s*(volatile)?\s*([\w<>\[\]]+)\s+(\w+)\s*[;=])");
    
    while (std::getline(f, line)) {
        lineNum++;
        if (line.find("//") == 0 || line.find("import ") != std::string::npos ||
            line.find("(") != std::string::npos) continue; // Skip methods
        
        std::smatch m;
        if (std::regex_search(line, m, fieldRe)) {
            JavaFieldInfo fi; fi.line = lineNum;
            if (m[1].matched) fi.accessModifier = m[1];
            fi.isStatic = m[2].matched;
            fi.isFinal = m[3].matched;
            fi.isVolatile = m[4].matched;
            fi.type = m[5];
            fi.name = m[6];
            fields.push_back(fi);
        }
    }
    return fields;
}

std::vector<std::string> UCJavaPlugin::GetImports(const std::string& srcFile) {
    std::vector<std::string> imports;
    std::ifstream f(srcFile); if (!f) return imports;
    std::string line; std::regex impRe(R"(import\s+(static\s+)?([\w.*]+)\s*;)");
    while (std::getline(f, line)) {
        std::smatch m;
        if (std::regex_search(line, m, impRe)) imports.push_back(m[1].matched ? "static " + m[2].str() : m[2].str());
    }
    return imports;
}

std::string UCJavaPlugin::GetPackage(const std::string& srcFile) {
    std::ifstream f(srcFile); if (!f) return "";
    std::string line; std::regex pkgRe(R"(package\s+([\w.]+)\s*;)");
    while (std::getline(f, line)) { std::smatch m; if (std::regex_search(line, m, pkgRe)) return m[1]; }
    return "";
}

// ============================================================================
// DEPENDENCY ANALYSIS
// ============================================================================

std::vector<JavaDependency> UCJavaPlugin::AnalyzeDependencies(const std::string& target) {
    std::vector<JavaDependency> deps;
    std::string out, err; int code;
    if (ExecuteCommand({GetJdepsExecutable(), "-verbose:class", target}, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line;
        std::regex depRe(R"(^\s+(\S+)\s+->\s+(\S+)\s+(\S+)$)");
        while (std::getline(iss, line)) {
            std::smatch m;
            if (std::regex_search(line, m, depRe)) { JavaDependency d; d.className = m[1]; d.dependsOn = m[2]; d.type = m[3]; deps.push_back(d); }
        }
    }
    return deps;
}

std::vector<std::string> UCJavaPlugin::GetModuleDependencies(const std::string& target) {
    std::vector<std::string> mods;
    std::string out, err; int code;
    if (ExecuteCommand({GetJdepsExecutable(), "-s", target}, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string line; std::regex modRe(R"(->\s+(\S+)$)");
        while (std::getline(iss, line)) { std::smatch m; if (std::regex_search(line, m, modRe)) mods.push_back(m[1]); }
    }
    return mods;
}

std::string UCJavaPlugin::GenerateModuleInfo(const std::string& jar) {
    std::string out, err; int code;
    ExecuteCommand({GetJdepsExecutable(), "--generate-module-info", ".", jar}, out, err, code);
    return out;
}

// ============================================================================
// TESTING
// ============================================================================

BuildResult UCJavaPlugin::RunJUnitTests(const std::string& testClass, std::function<void(const std::string&)> onOutput) {
    return RunClass(testClass, {}, onOutput);
}

BuildResult UCJavaPlugin::RunAllTests(const std::string& testDir, std::function<void(const std::string&)> onOutput) {
    BuildResult result; auto tests = FindTestClasses(testDir);
    for (const auto& t : tests) {
        auto r = RunJUnitTests(t, onOutput);
        result.errorCount += r.errorCount;
        for (const auto& m : r.messages) result.messages.push_back(m);
        if (!r.success) result.success = false;
    }
    return result;
}

std::vector<std::string> UCJavaPlugin::FindTestClasses(const std::string& directory) {
    std::vector<std::string> testClasses;
#ifndef _WIN32
    std::string out, err; int code;
    if (ExecuteCommand({"find", directory, "-name", "*Test.java", "-o", "-name", "*Tests.java", "-type", "f"}, out, err, code) && code == 0) {
        std::istringstream iss(out); std::string file;
        while (std::getline(iss, file)) {
            if (!file.empty()) {
                // Extract class name from file
                auto info = ParseClassInfo(file);
                if (!info.fullName.empty()) testClasses.push_back(info.fullName);
            }
        }
    }
#endif
    return testClasses;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UCJavaPlugin::SetConfig(const JavaPluginConfig& cfg) {
    pluginConfig = cfg; installationDetected = false; classCache.clear(); DetectInstallation();
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

bool UCJavaPlugin::ExecuteCommand(const std::vector<std::string>& args, std::string& output, std::string& error, int& exitCode, const std::string& cwd) {
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
