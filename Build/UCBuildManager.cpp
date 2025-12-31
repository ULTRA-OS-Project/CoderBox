// Apps/IDE/Build/UCBuildManager.cpp
// Build orchestration implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCBuildManager.h"
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <array>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/types.h>
    #include <fcntl.h>
    #include <signal.h>
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// UCBUILDMANAGER IMPLEMENTATION
// ============================================================================

UCBuildManager& UCBuildManager::Instance() {
    static UCBuildManager instance;
    return instance;
}

UCBuildManager::UCBuildManager() {
    // Setup output callbacks
    buildOutput.onRawLine = [this](const std::string& line) {
        if (onOutputLine) {
            onOutputLine(line);
        }
    };
    
    buildOutput.onMessage = [this](const CompilerMessage& msg) {
        if (onCompilerMessage) {
            onCompilerMessage(msg);
        }
    };
}

UCBuildManager::~UCBuildManager() {
    Shutdown();
}

bool UCBuildManager::Initialize() {
    if (initialized) {
        return true;
    }
    
    shutdownRequested = false;
    
    // Start worker thread
    workerThread = std::thread(&UCBuildManager::WorkerThread, this);
    
    initialized = true;
    return true;
}

void UCBuildManager::Shutdown() {
    if (!initialized) {
        return;
    }
    
    // Request shutdown
    shutdownRequested = true;
    cancelRequested = true;
    
    // Wake up worker thread
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queueCondition.notify_all();
    }
    
    // Wait for worker thread
    if (workerThread.joinable()) {
        workerThread.join();
    }
    
    initialized = false;
}

void UCBuildManager::WorkerThread() {
    while (!shutdownRequested) {
        BuildQueueItem item;
        
        // Wait for work
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait(lock, [this] {
                return shutdownRequested || !buildQueue.empty();
            });
            
            if (shutdownRequested) {
                break;
            }
            
            if (buildQueue.empty()) {
                continue;
            }
            
            item = buildQueue.top();
            buildQueue.pop();
        }
        
        // Execute build
        ExecuteBuild(item);
    }
}

void UCBuildManager::BuildProject(std::shared_ptr<UCIDEProject> project) {
    if (!project) return;
    
    BuildQueueItem item;
    item.project = project;
    item.job.projectPath = project->projectFilePath;
    item.job.configuration = project->GetActiveConfiguration();
    item.job.cleanBuild = false;
    item.priority = 10;
    item.queuedTime = std::chrono::steady_clock::now();
    
    // Get all source files
    auto sourceFiles = project->GetSourceFiles();
    for (const auto* file : sourceFiles) {
        item.job.sourceFiles.push_back(file->absolutePath);
    }
    
    QueueBuild(item);
}

void UCBuildManager::BuildFile(std::shared_ptr<UCIDEProject> project, const std::string& filePath) {
    if (!project) return;
    
    BuildQueueItem item;
    item.project = project;
    item.job.projectPath = project->projectFilePath;
    item.job.configuration = project->GetActiveConfiguration();
    item.job.sourceFiles = {filePath};
    item.job.cleanBuild = false;
    item.priority = 20;  // Higher priority for single file
    item.queuedTime = std::chrono::steady_clock::now();
    
    QueueBuild(item);
}

void UCBuildManager::RebuildProject(std::shared_ptr<UCIDEProject> project) {
    if (!project) return;
    
    BuildQueueItem item;
    item.project = project;
    item.job.projectPath = project->projectFilePath;
    item.job.configuration = project->GetActiveConfiguration();
    item.job.cleanBuild = true;
    item.priority = 5;
    item.queuedTime = std::chrono::steady_clock::now();
    
    auto sourceFiles = project->GetSourceFiles();
    for (const auto* file : sourceFiles) {
        item.job.sourceFiles.push_back(file->absolutePath);
    }
    
    QueueBuild(item);
}

void UCBuildManager::CleanProject(std::shared_ptr<UCIDEProject> project) {
    if (!project) return;
    
    // For CMake projects, use cmake --build . --target clean
    if (project->cmake.enabled) {
        CMakeClean(project);
        return;
    }
    
    // For non-CMake, delete build directory contents
    SetState(BuildState::Cleaning);
    
    const auto& config = project->GetActiveConfiguration();
    std::string buildDir = project->GetAbsolutePath(config.outputDirectory);
    
    // Remove build directory (platform-specific)
#ifdef _WIN32
    std::string cmd = "rmdir /s /q \"" + buildDir + "\" 2>nul";
#else
    std::string cmd = "rm -rf \"" + buildDir + "\" 2>/dev/null";
#endif
    
    int result = system(cmd.c_str());
    (void)result;  // Ignore result - directory might not exist
    
    SetState(BuildState::Finished);
    
    BuildEvent event(BuildEventType::BuildComplete);
    event.message = "Clean completed";
    EmitEvent(event);
}

void UCBuildManager::CancelBuild() {
    if (buildInProgress) {
        cancelRequested = true;
        SetState(BuildState::Cancelling);
    }
}

void UCBuildManager::CancelAllBuilds() {
    CancelBuild();
    
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!buildQueue.empty()) {
        buildQueue.pop();
    }
}

void UCBuildManager::CMakeConfigure(std::shared_ptr<UCIDEProject> project) {
    if (!project || !project->cmake.enabled) return;
    
    SetState(BuildState::Preparing);
    buildInProgress = true;
    currentProject = project;
    
    if (onBuildStart) {
        onBuildStart(project->name + " (CMake Configure)");
    }
    
    buildOutput.Clear();
    
    // Build cmake command
    std::vector<std::string> args;
    args.push_back("-B");
    args.push_back(project->cmake.buildDirectory);
    
    // Add generator if specified
    if (!project->cmake.generator.empty()) {
        args.push_back("-G");
        args.push_back(project->cmake.generator);
    }
    
    // Add variables
    for (const auto& var : project->cmake.variables) {
        args.push_back("-D" + var.first + "=" + var.second);
    }
    
    // Execute cmake
    int exitCode = ExecuteProcess(
        "cmake",
        args,
        project->rootDirectory,
        [this](const std::string& line) {
            buildOutput.ProcessLine(line);
        }
    );
    
    BuildResult result;
    result.exitCode = exitCode;
    result.success = (exitCode == 0);
    result.messages = buildOutput.GetMessages();
    result.errorCount = buildOutput.GetErrorCount();
    result.warningCount = buildOutput.GetWarningCount();
    result.rawOutput = buildOutput.GetRawOutputString();
    
    {
        std::lock_guard<std::mutex> lock(resultMutex);
        lastResult = result;
    }
    
    SetState(result.success ? BuildState::Finished : BuildState::Failed);
    buildInProgress = false;
    currentProject = nullptr;
    
    if (onBuildComplete) {
        onBuildComplete(result);
    }
}

void UCBuildManager::CMakeBuild(std::shared_ptr<UCIDEProject> project, const std::string& target) {
    if (!project || !project->cmake.enabled) return;
    
    SetState(BuildState::Compiling);
    buildInProgress = true;
    cancelRequested = false;
    currentProject = project;
    
    std::string targetName = target.empty() ? project->cmake.activeTarget : target;
    
    if (onBuildStart) {
        onBuildStart(project->name + (targetName.empty() ? "" : " (" + targetName + ")"));
    }
    
    buildOutput.Clear();
    auto startTime = std::chrono::steady_clock::now();
    
    // Build cmake command
    std::vector<std::string> args;
    args.push_back("--build");
    args.push_back(project->cmake.buildDirectory);
    
    if (!targetName.empty()) {
        args.push_back("--target");
        args.push_back(targetName);
    }
    
    // Add parallel jobs
    args.push_back("-j");
    
    // Execute cmake --build
    int exitCode = ExecuteProcess(
        "cmake",
        args,
        project->rootDirectory,
        [this](const std::string& line) {
            if (cancelRequested) return;
            buildOutput.ProcessLine(line);
        }
    );
    
    auto endTime = std::chrono::steady_clock::now();
    double buildTime = std::chrono::duration<double>(endTime - startTime).count();
    
    BuildResult result;
    result.exitCode = exitCode;
    result.success = (exitCode == 0) && !cancelRequested;
    result.messages = buildOutput.GetMessages();
    result.errorCount = buildOutput.GetErrorCount();
    result.warningCount = buildOutput.GetWarningCount();
    result.rawOutput = buildOutput.GetRawOutputString();
    result.buildTimeSeconds = buildTime;
    
    {
        std::lock_guard<std::mutex> lock(resultMutex);
        lastResult = result;
    }
    
    if (cancelRequested) {
        SetState(BuildState::Idle);
        if (onBuildCancel) {
            onBuildCancel();
        }
    } else {
        SetState(result.success ? BuildState::Finished : BuildState::Failed);
        if (onBuildComplete) {
            onBuildComplete(result);
        }
    }
    
    buildInProgress = false;
    currentProject = nullptr;
}

void UCBuildManager::CMakeClean(std::shared_ptr<UCIDEProject> project) {
    if (!project || !project->cmake.enabled) return;
    
    SetState(BuildState::Cleaning);
    
    std::vector<std::string> args;
    args.push_back("--build");
    args.push_back(project->cmake.buildDirectory);
    args.push_back("--target");
    args.push_back("clean");
    
    ExecuteProcess(
        "cmake",
        args,
        project->rootDirectory,
        [this](const std::string& line) {
            buildOutput.ProcessLine(line);
        }
    );
    
    SetState(BuildState::Finished);
}

void UCBuildManager::ExecuteBuild(const BuildQueueItem& item) {
    if (!item.project) return;
    
    buildInProgress = true;
    cancelRequested = false;
    currentProject = item.project;
    
    // Use CMake build if enabled
    if (item.project->cmake.enabled && !item.job.IsSingleFileBuild()) {
        CMakeBuild(item.project, item.project->cmake.activeTarget);
        return;
    }
    
    SetState(BuildState::Preparing);
    
    if (onBuildStart) {
        onBuildStart(item.project->name);
    }
    
    buildOutput.Clear();
    auto startTime = std::chrono::steady_clock::now();
    
    // Get compiler plugin
    auto& registry = UCCompilerPluginRegistry::Instance();
    auto plugin = registry.GetPlugin(item.project->primaryCompiler);
    
    if (!plugin) {
        BuildResult result;
        result.success = false;
        result.exitCode = -1;
        
        CompilerMessage msg;
        msg.type = CompilerMessageType::Error;
        msg.message = "No compiler plugin found for " + 
                      CompilerTypeToString(item.project->primaryCompiler);
        result.messages.push_back(msg);
        result.errorCount = 1;
        
        {
            std::lock_guard<std::mutex> lock(resultMutex);
            lastResult = result;
        }
        
        SetState(BuildState::Failed);
        buildInProgress = false;
        currentProject = nullptr;
        
        if (onBuildComplete) {
            onBuildComplete(result);
        }
        return;
    }
    
    if (!plugin->IsAvailable()) {
        BuildResult result;
        result.success = false;
        result.exitCode = -1;
        
        CompilerMessage msg;
        msg.type = CompilerMessageType::Error;
        msg.message = "Compiler not available: " + plugin->GetCompilerName();
        result.messages.push_back(msg);
        result.errorCount = 1;
        
        {
            std::lock_guard<std::mutex> lock(resultMutex);
            lastResult = result;
        }
        
        SetState(BuildState::Failed);
        buildInProgress = false;
        currentProject = nullptr;
        
        if (onBuildComplete) {
            onBuildComplete(result);
        }
        return;
    }
    
    // Set parser for this compiler
    buildOutput.SetParser(UCBuildOutput::GetParserForCompiler(plugin->GetCompilerType()));
    
    SetState(BuildState::Compiling);
    
    // Compile
    int totalFiles = static_cast<int>(item.job.sourceFiles.size());
    int completedFiles = 0;
    
    BuildResult result = plugin->CompileSync(item.job.sourceFiles, item.job.configuration);
    
    // Process output
    if (!result.rawOutput.empty()) {
        buildOutput.ProcessOutput(result.rawOutput);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.buildTimeSeconds = std::chrono::duration<double>(endTime - startTime).count();
    result.errorCount = buildOutput.GetErrorCount();
    result.warningCount = buildOutput.GetWarningCount();
    
    // Update messages from parser
    if (result.messages.empty()) {
        result.messages = buildOutput.GetMessages();
    }
    
    {
        std::lock_guard<std::mutex> lock(resultMutex);
        lastResult = result;
    }
    
    if (cancelRequested) {
        SetState(BuildState::Idle);
        if (onBuildCancel) {
            onBuildCancel();
        }
    } else {
        SetState(result.success ? BuildState::Finished : BuildState::Failed);
        if (onBuildComplete) {
            onBuildComplete(result);
        }
    }
    
    buildInProgress = false;
    currentProject = nullptr;
}

size_t UCBuildManager::GetQueuedBuildCount() const {
    std::lock_guard<std::mutex> lock(queueMutex);
    return buildQueue.size();
}

void UCBuildManager::SetState(BuildState state) {
    BuildState oldState = currentState.exchange(state);
    
    if (oldState != state) {
        if (onStateChange) {
            onStateChange(state);
        }
        
        BuildEvent event(BuildEventType::StateChanged);
        event.state = state;
        event.message = BuildStateToString(state);
        EmitEvent(event);
    }
}

void UCBuildManager::SetProgress(float progress) {
    buildProgress = progress;
    
    if (onProgressChange) {
        onProgressChange(progress);
    }
    
    BuildEvent event(BuildEventType::BuildProgress);
    event.progress = progress;
    EmitEvent(event);
}

void UCBuildManager::EmitEvent(const BuildEvent& event) {
    if (onBuildEvent) {
        onBuildEvent(event);
    }
}

void UCBuildManager::QueueBuild(const BuildQueueItem& item) {
    if (!initialized) {
        Initialize();
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        buildQueue.push(item);
    }
    
    queueCondition.notify_one();
}

// ============================================================================
// PROCESS UTILITIES
// ============================================================================

#ifdef _WIN32

int ExecuteProcess(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& workDir,
    std::function<void(const std::string&)> outputCallback,
    std::function<void(const std::string&)> errorCallback
) {
    // Build command line
    std::string cmdLine = command;
    for (const auto& arg : args) {
        cmdLine += " ";
        if (arg.find(' ') != std::string::npos) {
            cmdLine += "\"" + arg + "\"";
        } else {
            cmdLine += arg;
        }
    }
    
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;
    
    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;
    
    CreatePipe(&hStdOutRead, &hStdOutWrite, &saAttr, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    
    CreatePipe(&hStdErrRead, &hStdErrWrite, &saAttr, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hStdErrWrite;
    si.hStdOutput = hStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');
    
    BOOL success = CreateProcessA(
        NULL,
        cmdBuf.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        workDir.empty() ? NULL : workDir.c_str(),
        &si,
        &pi
    );
    
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);
    
    if (!success) {
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        return -1;
    }
    
    // Read output
    char buffer[4096];
    DWORD bytesRead;
    std::string lineBuffer;
    
    while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        lineBuffer += buffer;
        
        size_t pos;
        while ((pos = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (outputCallback) {
                outputCallback(line);
            }
            lineBuffer = lineBuffer.substr(pos + 1);
        }
    }
    
    if (!lineBuffer.empty() && outputCallback) {
        outputCallback(lineBuffer);
    }
    
    // Read error output
    lineBuffer.clear();
    while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        lineBuffer += buffer;
        
        size_t pos;
        while ((pos = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (errorCallback) {
                errorCallback(line);
            } else if (outputCallback) {
                outputCallback(line);
            }
            lineBuffer = lineBuffer.substr(pos + 1);
        }
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
    
    return static_cast<int>(exitCode);
}

bool CommandExists(const std::string& command) {
    std::string fullPath = FindCommand(command);
    return !fullPath.empty();
}

std::string FindCommand(const std::string& command) {
    // Check if command has extension
    std::string cmd = command;
    if (cmd.find('.') == std::string::npos) {
        cmd += ".exe";
    }
    
    // Check current directory
    if (GetFileAttributesA(cmd.c_str()) != INVALID_FILE_ATTRIBUTES) {
        char fullPath[MAX_PATH];
        GetFullPathNameA(cmd.c_str(), MAX_PATH, fullPath, NULL);
        return fullPath;
    }
    
    // Search PATH
    char pathEnv[32768];
    if (GetEnvironmentVariableA("PATH", pathEnv, sizeof(pathEnv)) > 0) {
        std::string path = pathEnv;
        size_t start = 0;
        size_t end;
        
        while ((end = path.find(';', start)) != std::string::npos) {
            std::string dir = path.substr(start, end - start);
            std::string fullPath = dir + "\\" + cmd;
            if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                return fullPath;
            }
            start = end + 1;
        }
        
        // Check last path component
        std::string dir = path.substr(start);
        std::string fullPath = dir + "\\" + cmd;
        if (GetFileAttributesA(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return fullPath;
        }
    }
    
    return "";
}

std::string GetEnvVar(const std::string& name) {
    char buffer[32768];
    DWORD len = GetEnvironmentVariableA(name.c_str(), buffer, sizeof(buffer));
    if (len > 0 && len < sizeof(buffer)) {
        return std::string(buffer, len);
    }
    return "";
}

void SetEnvVar(const std::string& name, const std::string& value) {
    SetEnvironmentVariableA(name.c_str(), value.c_str());
}

#else  // POSIX

int ExecuteProcess(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& workDir,
    std::function<void(const std::string&)> outputCallback,
    std::function<void(const std::string&)> errorCallback
) {
    int pipeOut[2];
    int pipeErr[2];
    
    if (pipe(pipeOut) == -1 || pipe(pipeErr) == -1) {
        return -1;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        close(pipeOut[0]);
        close(pipeOut[1]);
        close(pipeErr[0]);
        close(pipeErr[1]);
        return -1;
    }
    
    if (pid == 0) {
        // Child process
        close(pipeOut[0]);
        close(pipeErr[0]);
        
        dup2(pipeOut[1], STDOUT_FILENO);
        dup2(pipeErr[1], STDERR_FILENO);
        
        close(pipeOut[1]);
        close(pipeErr[1]);
        
        if (!workDir.empty()) {
            if (chdir(workDir.c_str()) != 0) {
                _exit(127);
            }
        }
        
        // Build argument array
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        execvp(command.c_str(), argv.data());
        _exit(127);
    }
    
    // Parent process
    close(pipeOut[1]);
    close(pipeErr[1]);
    
    // Read output
    char buffer[4096];
    std::string lineBuffer;
    ssize_t bytesRead;
    
    // Set non-blocking
    fcntl(pipeOut[0], F_SETFL, O_NONBLOCK);
    fcntl(pipeErr[0], F_SETFL, O_NONBLOCK);
    
    fd_set readfds;
    int maxfd = std::max(pipeOut[0], pipeErr[0]) + 1;
    
    bool outOpen = true;
    bool errOpen = true;
    
    while (outOpen || errOpen) {
        FD_ZERO(&readfds);
        if (outOpen) FD_SET(pipeOut[0], &readfds);
        if (errOpen) FD_SET(pipeErr[0], &readfds);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms timeout
        
        int ret = select(maxfd, &readfds, NULL, NULL, &tv);
        
        if (ret > 0) {
            if (outOpen && FD_ISSET(pipeOut[0], &readfds)) {
                bytesRead = read(pipeOut[0], buffer, sizeof(buffer) - 1);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    lineBuffer += buffer;
                    
                    size_t pos;
                    while ((pos = lineBuffer.find('\n')) != std::string::npos) {
                        std::string line = lineBuffer.substr(0, pos);
                        if (outputCallback) {
                            outputCallback(line);
                        }
                        lineBuffer = lineBuffer.substr(pos + 1);
                    }
                } else if (bytesRead == 0) {
                    outOpen = false;
                }
            }
            
            if (errOpen && FD_ISSET(pipeErr[0], &readfds)) {
                bytesRead = read(pipeErr[0], buffer, sizeof(buffer) - 1);
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::string errBuffer = buffer;
                    
                    size_t pos;
                    while ((pos = errBuffer.find('\n')) != std::string::npos) {
                        std::string line = errBuffer.substr(0, pos);
                        if (errorCallback) {
                            errorCallback(line);
                        } else if (outputCallback) {
                            outputCallback(line);
                        }
                        errBuffer = errBuffer.substr(pos + 1);
                    }
                } else if (bytesRead == 0) {
                    errOpen = false;
                }
            }
        }
    }
    
    // Output remaining buffer
    if (!lineBuffer.empty() && outputCallback) {
        outputCallback(lineBuffer);
    }
    
    close(pipeOut[0]);
    close(pipeErr[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    
    return -1;
}

bool CommandExists(const std::string& command) {
    std::string fullPath = FindCommand(command);
    return !fullPath.empty();
}

std::string FindCommand(const std::string& command) {
    // If command contains path separator, check directly
    if (command.find('/') != std::string::npos) {
        if (access(command.c_str(), X_OK) == 0) {
            return command;
        }
        return "";
    }
    
    // Search PATH
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        pathEnv = "/usr/bin:/bin";
    }
    
    std::string path = pathEnv;
    size_t start = 0;
    size_t end;
    
    while ((end = path.find(':', start)) != std::string::npos) {
        std::string dir = path.substr(start, end - start);
        std::string fullPath = dir + "/" + command;
        if (access(fullPath.c_str(), X_OK) == 0) {
            return fullPath;
        }
        start = end + 1;
    }
    
    // Check last path component
    std::string dir = path.substr(start);
    std::string fullPath = dir + "/" + command;
    if (access(fullPath.c_str(), X_OK) == 0) {
        return fullPath;
    }
    
    return "";
}

std::string GetEnvVar(const std::string& name) {
    const char* value = getenv(name.c_str());
    return value ? value : "";
}

void SetEnvVar(const std::string& name, const std::string& value) {
    setenv(name.c_str(), value.c_str(), 1);
}

#endif

} // namespace IDE
} // namespace UltraCanvas
