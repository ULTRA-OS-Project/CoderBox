// Apps/IDE/Debug/Launch/UCLaunchConfiguration.h
// Launch Configuration for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Type of launch configuration
 */
enum class LaunchConfigType {
    Launch,         // Launch new process
    Attach,         // Attach to running process
    AttachByName,   // Attach by process name
    Remote,         // Remote debugging (gdbserver)
    CoreDump        // Debug core dump
};

/**
 * @brief Debugger type to use
 */
enum class DebuggerType {
    Auto,           // Auto-detect based on platform/project
    GDB,            // GNU Debugger
    LLDB,           // LLVM Debugger
    Custom          // Custom debugger path
};

/**
 * @brief Console type for program I/O
 */
enum class ConsoleType {
    Integrated,     // IDE's integrated console
    External,       // External terminal
    None            // No console (for GUI apps)
};

/**
 * @brief Pre-launch action to perform
 */
enum class PreLaunchAction {
    None,           // No pre-launch action
    Build,          // Build project before launch
    BuildIfModified,// Build only if sources modified
    CustomTask      // Run custom task
};

/**
 * @brief Stop at entry behavior
 */
enum class StopAtEntry {
    No,             // Don't stop at entry
    Main,           // Stop at main()
    Entry,          // Stop at entry point (_start)
    Custom          // Stop at custom function
};

/**
 * @brief Environment variable definition
 */
struct EnvironmentVariable {
    std::string name;
    std::string value;
    bool append = false;    // Append to existing value
    bool prepend = false;   // Prepend to existing value
    
    EnvironmentVariable() = default;
    EnvironmentVariable(const std::string& n, const std::string& v)
        : name(n), value(v) {}
};

/**
 * @brief Source file mapping for remote debugging
 */
struct SourceMapping {
    std::string remotePath;
    std::string localPath;
    
    SourceMapping() = default;
    SourceMapping(const std::string& remote, const std::string& local)
        : remotePath(remote), localPath(local) {}
};

/**
 * @brief GDB-specific settings
 */
struct GDBSettings {
    std::string gdbPath;                    // Path to GDB executable
    std::string gdbArgs;                    // Additional GDB arguments
    bool useMI = true;                      // Use MI interface
    bool prettyPrinting = true;             // Enable pretty printing
    bool enableNonStop = false;             // Non-stop mode for multi-threaded
    std::string initCommands;               // Commands to run at startup
    std::string postRemoteConnectCommands;  // Commands after remote connect
    std::vector<std::string> setupCommands; // Setup commands before run
};

/**
 * @brief LLDB-specific settings  
 */
struct LLDBSettings {
    std::string lldbPath;                   // Path to LLDB executable
    std::string lldbArgs;                   // Additional LLDB arguments
    std::string initCommands;               // Commands to run at startup
    std::vector<std::string> setupCommands; // Setup commands before run
};

/**
 * @brief Remote debugging settings
 */
struct RemoteSettings {
    std::string host = "localhost";
    int port = 3333;
    std::string serverAddress;              // Full address (host:port)
    bool autoStartServer = false;           // Auto-start gdbserver
    std::string serverPath;                 // Path to gdbserver on remote
    std::string serverArgs;                 // Additional server arguments
    std::vector<SourceMapping> sourceMappings;
};

/**
 * @brief Launch configuration for debug sessions
 */
class UCLaunchConfiguration {
public:
    UCLaunchConfiguration();
    UCLaunchConfiguration(const std::string& name);
    ~UCLaunchConfiguration() = default;
    
    // =========================================================================
    // IDENTIFICATION
    // =========================================================================
    
    void SetName(const std::string& name) { name_ = name; }
    std::string GetName() const { return name_; }
    
    void SetId(const std::string& id) { id_ = id; }
    std::string GetId() const { return id_; }
    
    void SetDescription(const std::string& desc) { description_ = desc; }
    std::string GetDescription() const { return description_; }
    
    void SetIsDefault(bool isDefault) { isDefault_ = isDefault; }
    bool IsDefault() const { return isDefault_; }
    
    // =========================================================================
    // LAUNCH TYPE
    // =========================================================================
    
    void SetType(LaunchConfigType type) { type_ = type; }
    LaunchConfigType GetType() const { return type_; }
    
    void SetDebuggerType(DebuggerType type) { debuggerType_ = type; }
    DebuggerType GetDebuggerType() const { return debuggerType_; }
    
    // =========================================================================
    // EXECUTABLE & ARGUMENTS
    // =========================================================================
    
    void SetProgram(const std::string& program) { program_ = program; }
    std::string GetProgram() const { return program_; }
    
    void SetArgs(const std::string& args) { args_ = args; }
    std::string GetArgs() const { return args_; }
    
    void SetArgsList(const std::vector<std::string>& args) { argsList_ = args; }
    std::vector<std::string> GetArgsList() const { return argsList_; }
    
    void SetWorkingDirectory(const std::string& dir) { workingDirectory_ = dir; }
    std::string GetWorkingDirectory() const { return workingDirectory_; }
    
    // =========================================================================
    // ENVIRONMENT
    // =========================================================================
    
    void SetEnvironment(const std::vector<EnvironmentVariable>& env) { environment_ = env; }
    std::vector<EnvironmentVariable>& GetEnvironment() { return environment_; }
    const std::vector<EnvironmentVariable>& GetEnvironment() const { return environment_; }
    
    void AddEnvironmentVariable(const std::string& name, const std::string& value);
    void RemoveEnvironmentVariable(const std::string& name);
    void ClearEnvironment() { environment_.clear(); }
    
    void SetInheritEnvironment(bool inherit) { inheritEnvironment_ = inherit; }
    bool GetInheritEnvironment() const { return inheritEnvironment_; }
    
    // =========================================================================
    // CONSOLE & I/O
    // =========================================================================
    
    void SetConsoleType(ConsoleType type) { consoleType_ = type; }
    ConsoleType GetConsoleType() const { return consoleType_; }
    
    void SetExternalTerminal(const std::string& terminal) { externalTerminal_ = terminal; }
    std::string GetExternalTerminal() const { return externalTerminal_; }
    
    void SetStdinFile(const std::string& file) { stdinFile_ = file; }
    std::string GetStdinFile() const { return stdinFile_; }
    
    void SetStdoutFile(const std::string& file) { stdoutFile_ = file; }
    std::string GetStdoutFile() const { return stdoutFile_; }
    
    // =========================================================================
    // PRE-LAUNCH
    // =========================================================================
    
    void SetPreLaunchAction(PreLaunchAction action) { preLaunchAction_ = action; }
    PreLaunchAction GetPreLaunchAction() const { return preLaunchAction_; }
    
    void SetPreLaunchTask(const std::string& task) { preLaunchTask_ = task; }
    std::string GetPreLaunchTask() const { return preLaunchTask_; }
    
    // =========================================================================
    // STOP AT ENTRY
    // =========================================================================
    
    void SetStopAtEntry(StopAtEntry stop) { stopAtEntry_ = stop; }
    StopAtEntry GetStopAtEntry() const { return stopAtEntry_; }
    
    void SetStopAtFunction(const std::string& func) { stopAtFunction_ = func; }
    std::string GetStopAtFunction() const { return stopAtFunction_; }
    
    // =========================================================================
    // ATTACH SETTINGS
    // =========================================================================
    
    void SetProcessId(int pid) { processId_ = pid; }
    int GetProcessId() const { return processId_; }
    
    void SetProcessName(const std::string& name) { processName_ = name; }
    std::string GetProcessName() const { return processName_; }
    
    void SetWaitForProcess(bool wait) { waitForProcess_ = wait; }
    bool GetWaitForProcess() const { return waitForProcess_; }
    
    // =========================================================================
    // CORE DUMP
    // =========================================================================
    
    void SetCoreDumpPath(const std::string& path) { coreDumpPath_ = path; }
    std::string GetCoreDumpPath() const { return coreDumpPath_; }
    
    // =========================================================================
    // DEBUGGER-SPECIFIC SETTINGS
    // =========================================================================
    
    GDBSettings& GetGDBSettings() { return gdbSettings_; }
    const GDBSettings& GetGDBSettings() const { return gdbSettings_; }
    
    LLDBSettings& GetLLDBSettings() { return lldbSettings_; }
    const LLDBSettings& GetLLDBSettings() const { return lldbSettings_; }
    
    RemoteSettings& GetRemoteSettings() { return remoteSettings_; }
    const RemoteSettings& GetRemoteSettings() const { return remoteSettings_; }
    
    // =========================================================================
    // SYMBOL & SOURCE SETTINGS
    // =========================================================================
    
    void SetSymbolFile(const std::string& file) { symbolFile_ = file; }
    std::string GetSymbolFile() const { return symbolFile_; }
    
    void AddSymbolSearchPath(const std::string& path);
    std::vector<std::string>& GetSymbolSearchPaths() { return symbolSearchPaths_; }
    
    void AddSourceSearchPath(const std::string& path);
    std::vector<std::string>& GetSourceSearchPaths() { return sourceSearchPaths_; }
    
    // =========================================================================
    // VALIDATION
    // =========================================================================
    
    bool IsValid() const;
    std::string GetValidationError() const;
    
    // =========================================================================
    // SERIALIZATION
    // =========================================================================
    
    std::string ToJson() const;
    static std::unique_ptr<UCLaunchConfiguration> FromJson(const std::string& json);
    
    // =========================================================================
    // CLONE
    // =========================================================================
    
    std::unique_ptr<UCLaunchConfiguration> Clone() const;
    
private:
    // Identification
    std::string id_;
    std::string name_;
    std::string description_;
    bool isDefault_ = false;
    
    // Type
    LaunchConfigType type_ = LaunchConfigType::Launch;
    DebuggerType debuggerType_ = DebuggerType::Auto;
    
    // Executable
    std::string program_;
    std::string args_;
    std::vector<std::string> argsList_;
    std::string workingDirectory_;
    
    // Environment
    std::vector<EnvironmentVariable> environment_;
    bool inheritEnvironment_ = true;
    
    // Console
    ConsoleType consoleType_ = ConsoleType::Integrated;
    std::string externalTerminal_;
    std::string stdinFile_;
    std::string stdoutFile_;
    
    // Pre-launch
    PreLaunchAction preLaunchAction_ = PreLaunchAction::BuildIfModified;
    std::string preLaunchTask_;
    
    // Stop at entry
    StopAtEntry stopAtEntry_ = StopAtEntry::No;
    std::string stopAtFunction_;
    
    // Attach
    int processId_ = 0;
    std::string processName_;
    bool waitForProcess_ = false;
    
    // Core dump
    std::string coreDumpPath_;
    
    // Debugger settings
    GDBSettings gdbSettings_;
    LLDBSettings lldbSettings_;
    RemoteSettings remoteSettings_;
    
    // Symbols & sources
    std::string symbolFile_;
    std::vector<std::string> symbolSearchPaths_;
    std::vector<std::string> sourceSearchPaths_;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string LaunchConfigTypeToString(LaunchConfigType type);
LaunchConfigType StringToLaunchConfigType(const std::string& str);

std::string DebuggerTypeToString(DebuggerType type);
DebuggerType StringToDebuggerType(const std::string& str);

std::string ConsoleTypeToString(ConsoleType type);
ConsoleType StringToConsoleType(const std::string& str);

std::string PreLaunchActionToString(PreLaunchAction action);
PreLaunchAction StringToPreLaunchAction(const std::string& str);

std::string StopAtEntryToString(StopAtEntry stop);
StopAtEntry StringToStopAtEntry(const std::string& str);

} // namespace IDE
} // namespace UltraCanvas
