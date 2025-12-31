// Apps/IDE/Debug/Launch/UCLaunchConfiguration.cpp
// Launch Configuration Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCLaunchConfiguration.h"
#include <sstream>
#include <algorithm>
#include <random>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static std::string GenerateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";
    
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char& c : uuid) {
        if (c == 'x') {
            c = hex[dis(gen)];
        } else if (c == 'y') {
            c = hex[(dis(gen) & 0x3) | 0x8];
        }
    }
    return uuid;
}

static std::string EscapeJsonString(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (c < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

// ============================================================================
// UCLaunchConfiguration IMPLEMENTATION
// ============================================================================

UCLaunchConfiguration::UCLaunchConfiguration() {
    id_ = GenerateUUID();
    name_ = "New Configuration";
}

UCLaunchConfiguration::UCLaunchConfiguration(const std::string& name)
    : name_(name) {
    id_ = GenerateUUID();
}

void UCLaunchConfiguration::AddEnvironmentVariable(const std::string& name, 
                                                    const std::string& value) {
    // Check if already exists
    for (auto& env : environment_) {
        if (env.name == name) {
            env.value = value;
            return;
        }
    }
    environment_.emplace_back(name, value);
}

void UCLaunchConfiguration::RemoveEnvironmentVariable(const std::string& name) {
    environment_.erase(
        std::remove_if(environment_.begin(), environment_.end(),
                       [&name](const EnvironmentVariable& env) {
                           return env.name == name;
                       }),
        environment_.end());
}

void UCLaunchConfiguration::AddSymbolSearchPath(const std::string& path) {
    if (std::find(symbolSearchPaths_.begin(), symbolSearchPaths_.end(), path) 
        == symbolSearchPaths_.end()) {
        symbolSearchPaths_.push_back(path);
    }
}

void UCLaunchConfiguration::AddSourceSearchPath(const std::string& path) {
    if (std::find(sourceSearchPaths_.begin(), sourceSearchPaths_.end(), path)
        == sourceSearchPaths_.end()) {
        sourceSearchPaths_.push_back(path);
    }
}

bool UCLaunchConfiguration::IsValid() const {
    switch (type_) {
        case LaunchConfigType::Launch:
            return !program_.empty();
            
        case LaunchConfigType::Attach:
            return processId_ > 0;
            
        case LaunchConfigType::AttachByName:
            return !processName_.empty();
            
        case LaunchConfigType::Remote:
            return !remoteSettings_.host.empty() && remoteSettings_.port > 0;
            
        case LaunchConfigType::CoreDump:
            return !coreDumpPath_.empty() && !program_.empty();
    }
    return false;
}

std::string UCLaunchConfiguration::GetValidationError() const {
    switch (type_) {
        case LaunchConfigType::Launch:
            if (program_.empty()) return "Program path is required";
            break;
            
        case LaunchConfigType::Attach:
            if (processId_ <= 0) return "Process ID is required";
            break;
            
        case LaunchConfigType::AttachByName:
            if (processName_.empty()) return "Process name is required";
            break;
            
        case LaunchConfigType::Remote:
            if (remoteSettings_.host.empty()) return "Remote host is required";
            if (remoteSettings_.port <= 0) return "Remote port is required";
            break;
            
        case LaunchConfigType::CoreDump:
            if (coreDumpPath_.empty()) return "Core dump path is required";
            if (program_.empty()) return "Executable path is required";
            break;
    }
    return "";
}

std::string UCLaunchConfiguration::ToJson() const {
    std::ostringstream json;
    json << "{\n";
    
    // Basic fields
    json << "  \"id\": \"" << EscapeJsonString(id_) << "\",\n";
    json << "  \"name\": \"" << EscapeJsonString(name_) << "\",\n";
    json << "  \"description\": \"" << EscapeJsonString(description_) << "\",\n";
    json << "  \"isDefault\": " << (isDefault_ ? "true" : "false") << ",\n";
    json << "  \"type\": \"" << LaunchConfigTypeToString(type_) << "\",\n";
    json << "  \"debuggerType\": \"" << DebuggerTypeToString(debuggerType_) << "\",\n";
    
    // Executable
    json << "  \"program\": \"" << EscapeJsonString(program_) << "\",\n";
    json << "  \"args\": \"" << EscapeJsonString(args_) << "\",\n";
    json << "  \"workingDirectory\": \"" << EscapeJsonString(workingDirectory_) << "\",\n";
    
    // Args list
    json << "  \"argsList\": [";
    for (size_t i = 0; i < argsList_.size(); i++) {
        if (i > 0) json << ", ";
        json << "\"" << EscapeJsonString(argsList_[i]) << "\"";
    }
    json << "],\n";
    
    // Environment
    json << "  \"environment\": [";
    for (size_t i = 0; i < environment_.size(); i++) {
        if (i > 0) json << ", ";
        json << "{\"name\": \"" << EscapeJsonString(environment_[i].name) 
             << "\", \"value\": \"" << EscapeJsonString(environment_[i].value) << "\"}";
    }
    json << "],\n";
    json << "  \"inheritEnvironment\": " << (inheritEnvironment_ ? "true" : "false") << ",\n";
    
    // Console
    json << "  \"consoleType\": \"" << ConsoleTypeToString(consoleType_) << "\",\n";
    json << "  \"externalTerminal\": \"" << EscapeJsonString(externalTerminal_) << "\",\n";
    
    // Pre-launch
    json << "  \"preLaunchAction\": \"" << PreLaunchActionToString(preLaunchAction_) << "\",\n";
    json << "  \"preLaunchTask\": \"" << EscapeJsonString(preLaunchTask_) << "\",\n";
    
    // Stop at entry
    json << "  \"stopAtEntry\": \"" << StopAtEntryToString(stopAtEntry_) << "\",\n";
    json << "  \"stopAtFunction\": \"" << EscapeJsonString(stopAtFunction_) << "\",\n";
    
    // Attach
    json << "  \"processId\": " << processId_ << ",\n";
    json << "  \"processName\": \"" << EscapeJsonString(processName_) << "\",\n";
    json << "  \"waitForProcess\": " << (waitForProcess_ ? "true" : "false") << ",\n";
    
    // Core dump
    json << "  \"coreDumpPath\": \"" << EscapeJsonString(coreDumpPath_) << "\",\n";
    
    // GDB settings
    json << "  \"gdbSettings\": {\n";
    json << "    \"gdbPath\": \"" << EscapeJsonString(gdbSettings_.gdbPath) << "\",\n";
    json << "    \"gdbArgs\": \"" << EscapeJsonString(gdbSettings_.gdbArgs) << "\",\n";
    json << "    \"useMI\": " << (gdbSettings_.useMI ? "true" : "false") << ",\n";
    json << "    \"prettyPrinting\": " << (gdbSettings_.prettyPrinting ? "true" : "false") << ",\n";
    json << "    \"enableNonStop\": " << (gdbSettings_.enableNonStop ? "true" : "false") << ",\n";
    json << "    \"initCommands\": \"" << EscapeJsonString(gdbSettings_.initCommands) << "\"\n";
    json << "  },\n";
    
    // LLDB settings
    json << "  \"lldbSettings\": {\n";
    json << "    \"lldbPath\": \"" << EscapeJsonString(lldbSettings_.lldbPath) << "\",\n";
    json << "    \"lldbArgs\": \"" << EscapeJsonString(lldbSettings_.lldbArgs) << "\",\n";
    json << "    \"initCommands\": \"" << EscapeJsonString(lldbSettings_.initCommands) << "\"\n";
    json << "  },\n";
    
    // Remote settings
    json << "  \"remoteSettings\": {\n";
    json << "    \"host\": \"" << EscapeJsonString(remoteSettings_.host) << "\",\n";
    json << "    \"port\": " << remoteSettings_.port << ",\n";
    json << "    \"autoStartServer\": " << (remoteSettings_.autoStartServer ? "true" : "false") << ",\n";
    json << "    \"serverPath\": \"" << EscapeJsonString(remoteSettings_.serverPath) << "\"\n";
    json << "  },\n";
    
    // Symbol file
    json << "  \"symbolFile\": \"" << EscapeJsonString(symbolFile_) << "\",\n";
    
    // Symbol search paths
    json << "  \"symbolSearchPaths\": [";
    for (size_t i = 0; i < symbolSearchPaths_.size(); i++) {
        if (i > 0) json << ", ";
        json << "\"" << EscapeJsonString(symbolSearchPaths_[i]) << "\"";
    }
    json << "],\n";
    
    // Source search paths
    json << "  \"sourceSearchPaths\": [";
    for (size_t i = 0; i < sourceSearchPaths_.size(); i++) {
        if (i > 0) json << ", ";
        json << "\"" << EscapeJsonString(sourceSearchPaths_[i]) << "\"";
    }
    json << "]\n";
    
    json << "}";
    return json.str();
}

std::unique_ptr<UCLaunchConfiguration> UCLaunchConfiguration::FromJson(const std::string& json) {
    // Simple JSON parser - in production use a proper JSON library
    auto config = std::make_unique<UCLaunchConfiguration>();
    
    // TODO: Implement JSON parsing
    // This would use a JSON library like nlohmann/json or RapidJSON
    
    return config;
}

std::unique_ptr<UCLaunchConfiguration> UCLaunchConfiguration::Clone() const {
    auto clone = std::make_unique<UCLaunchConfiguration>();
    
    clone->id_ = GenerateUUID();  // New ID for clone
    clone->name_ = name_ + " (Copy)";
    clone->description_ = description_;
    clone->isDefault_ = false;  // Clone is not default
    
    clone->type_ = type_;
    clone->debuggerType_ = debuggerType_;
    clone->program_ = program_;
    clone->args_ = args_;
    clone->argsList_ = argsList_;
    clone->workingDirectory_ = workingDirectory_;
    clone->environment_ = environment_;
    clone->inheritEnvironment_ = inheritEnvironment_;
    clone->consoleType_ = consoleType_;
    clone->externalTerminal_ = externalTerminal_;
    clone->stdinFile_ = stdinFile_;
    clone->stdoutFile_ = stdoutFile_;
    clone->preLaunchAction_ = preLaunchAction_;
    clone->preLaunchTask_ = preLaunchTask_;
    clone->stopAtEntry_ = stopAtEntry_;
    clone->stopAtFunction_ = stopAtFunction_;
    clone->processId_ = processId_;
    clone->processName_ = processName_;
    clone->waitForProcess_ = waitForProcess_;
    clone->coreDumpPath_ = coreDumpPath_;
    clone->gdbSettings_ = gdbSettings_;
    clone->lldbSettings_ = lldbSettings_;
    clone->remoteSettings_ = remoteSettings_;
    clone->symbolFile_ = symbolFile_;
    clone->symbolSearchPaths_ = symbolSearchPaths_;
    clone->sourceSearchPaths_ = sourceSearchPaths_;
    
    return clone;
}

// ============================================================================
// ENUM TO STRING CONVERSIONS
// ============================================================================

std::string LaunchConfigTypeToString(LaunchConfigType type) {
    switch (type) {
        case LaunchConfigType::Launch: return "launch";
        case LaunchConfigType::Attach: return "attach";
        case LaunchConfigType::AttachByName: return "attachByName";
        case LaunchConfigType::Remote: return "remote";
        case LaunchConfigType::CoreDump: return "coreDump";
    }
    return "launch";
}

LaunchConfigType StringToLaunchConfigType(const std::string& str) {
    if (str == "attach") return LaunchConfigType::Attach;
    if (str == "attachByName") return LaunchConfigType::AttachByName;
    if (str == "remote") return LaunchConfigType::Remote;
    if (str == "coreDump") return LaunchConfigType::CoreDump;
    return LaunchConfigType::Launch;
}

std::string DebuggerTypeToString(DebuggerType type) {
    switch (type) {
        case DebuggerType::Auto: return "auto";
        case DebuggerType::GDB: return "gdb";
        case DebuggerType::LLDB: return "lldb";
        case DebuggerType::Custom: return "custom";
    }
    return "auto";
}

DebuggerType StringToDebuggerType(const std::string& str) {
    if (str == "gdb") return DebuggerType::GDB;
    if (str == "lldb") return DebuggerType::LLDB;
    if (str == "custom") return DebuggerType::Custom;
    return DebuggerType::Auto;
}

std::string ConsoleTypeToString(ConsoleType type) {
    switch (type) {
        case ConsoleType::Integrated: return "integrated";
        case ConsoleType::External: return "external";
        case ConsoleType::None: return "none";
    }
    return "integrated";
}

ConsoleType StringToConsoleType(const std::string& str) {
    if (str == "external") return ConsoleType::External;
    if (str == "none") return ConsoleType::None;
    return ConsoleType::Integrated;
}

std::string PreLaunchActionToString(PreLaunchAction action) {
    switch (action) {
        case PreLaunchAction::None: return "none";
        case PreLaunchAction::Build: return "build";
        case PreLaunchAction::BuildIfModified: return "buildIfModified";
        case PreLaunchAction::CustomTask: return "customTask";
    }
    return "buildIfModified";
}

PreLaunchAction StringToPreLaunchAction(const std::string& str) {
    if (str == "none") return PreLaunchAction::None;
    if (str == "build") return PreLaunchAction::Build;
    if (str == "customTask") return PreLaunchAction::CustomTask;
    return PreLaunchAction::BuildIfModified;
}

std::string StopAtEntryToString(StopAtEntry stop) {
    switch (stop) {
        case StopAtEntry::No: return "no";
        case StopAtEntry::Main: return "main";
        case StopAtEntry::Entry: return "entry";
        case StopAtEntry::Custom: return "custom";
    }
    return "no";
}

StopAtEntry StringToStopAtEntry(const std::string& str) {
    if (str == "main") return StopAtEntry::Main;
    if (str == "entry") return StopAtEntry::Entry;
    if (str == "custom") return StopAtEntry::Custom;
    return StopAtEntry::No;
}

} // namespace IDE
} // namespace UltraCanvas
