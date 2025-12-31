// Apps/IDE/Build/UCBuildManager.h
// Build orchestration with background execution for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "IUCCompilerPlugin.h"
#include "UCBuildOutput.h"
#include "../Project/UCIDEProject.h"
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <functional>
#include <atomic>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// BUILD STATE
// ============================================================================

/**
 * @brief Current state of the build system
 */
enum class BuildState {
    Idle,               // No build in progress
    Preparing,          // Preparing build (gathering files, etc.)
    Compiling,          // Compilation in progress
    Linking,            // Linking in progress
    Cleaning,           // Clean operation in progress
    Cancelling,         // Build is being cancelled
    Finished,           // Build completed (check result for success)
    Failed              // Build failed with error
};

/**
 * @brief Convert BuildState to string
 */
inline std::string BuildStateToString(BuildState state) {
    switch (state) {
        case BuildState::Idle:          return "Idle";
        case BuildState::Preparing:     return "Preparing";
        case BuildState::Compiling:     return "Compiling";
        case BuildState::Linking:       return "Linking";
        case BuildState::Cleaning:      return "Cleaning";
        case BuildState::Cancelling:    return "Cancelling";
        case BuildState::Finished:      return "Finished";
        case BuildState::Failed:        return "Failed";
        default:                        return "Unknown";
    }
}

// ============================================================================
// BUILD EVENT
// ============================================================================

/**
 * @brief Types of build events
 */
enum class BuildEventType {
    BuildStarted,
    BuildProgress,
    BuildOutput,
    CompilerMessage,
    BuildComplete,
    BuildCancelled,
    BuildFailed,
    StateChanged
};

/**
 * @brief Build event data
 */
struct BuildEvent {
    BuildEventType type;
    std::string message;
    float progress = 0.0f;
    CompilerMessage compilerMessage;
    BuildResult result;
    BuildState state = BuildState::Idle;
    std::chrono::steady_clock::time_point timestamp;
    
    BuildEvent(BuildEventType t) : type(t), timestamp(std::chrono::steady_clock::now()) {}
};

// ============================================================================
// BUILD QUEUE ITEM
// ============================================================================

/**
 * @brief Extended build job with additional metadata
 */
struct BuildQueueItem {
    BuildJob job;
    std::shared_ptr<UCIDEProject> project;
    int priority = 0;                       // Higher = more priority
    std::chrono::steady_clock::time_point queuedTime;
    
    bool operator<(const BuildQueueItem& other) const {
        return priority < other.priority;
    }
};

// ============================================================================
// BUILD MANAGER
// ============================================================================

/**
 * @brief Singleton class managing all build operations
 * 
 * The UCBuildManager handles:
 * - Build queue management
 * - Background build execution
 * - CMake integration
 * - Compiler plugin coordination
 * - Build output collection and distribution
 */
class UCBuildManager {
public:
    /**
     * @brief Get singleton instance
     */
    static UCBuildManager& Instance();
    
    // ===== BUILD OPERATIONS =====
    
    /**
     * @brief Build entire project
     * @param project Project to build
     */
    void BuildProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Build a single file
     * @param project Project containing the file
     * @param filePath Path to file to compile
     */
    void BuildFile(std::shared_ptr<UCIDEProject> project, const std::string& filePath);
    
    /**
     * @brief Rebuild project (clean + build)
     * @param project Project to rebuild
     */
    void RebuildProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Clean project (remove build artifacts)
     * @param project Project to clean
     */
    void CleanProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Cancel current build
     */
    void CancelBuild();
    
    /**
     * @brief Cancel all queued builds
     */
    void CancelAllBuilds();
    
    // ===== CMAKE OPERATIONS =====
    
    /**
     * @brief Run CMake configure step
     * @param project Project with CMake configuration
     */
    void CMakeConfigure(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Build CMake target
     * @param project Project with CMake configuration
     * @param target Target name (empty = all)
     */
    void CMakeBuild(std::shared_ptr<UCIDEProject> project, const std::string& target = "");
    
    /**
     * @brief Clean CMake build
     * @param project Project with CMake configuration
     */
    void CMakeClean(std::shared_ptr<UCIDEProject> project);
    
    // ===== STATE QUERIES =====
    
    /**
     * @brief Check if any build is in progress
     */
    bool IsBuildInProgress() const { return buildInProgress.load(); }
    
    /**
     * @brief Get current build state
     */
    BuildState GetBuildState() const { return currentState.load(); }
    
    /**
     * @brief Get current build progress (0.0 to 1.0)
     */
    float GetBuildProgress() const { return buildProgress.load(); }
    
    /**
     * @brief Get last build result
     */
    const BuildResult& GetLastBuildResult() const { return lastResult; }
    
    /**
     * @brief Get number of queued builds
     */
    size_t GetQueuedBuildCount() const;
    
    /**
     * @brief Get current project being built
     */
    std::shared_ptr<UCIDEProject> GetCurrentProject() const { return currentProject; }
    
    // ===== OUTPUT ACCESS =====
    
    /**
     * @brief Get build output manager
     */
    UCBuildOutput& GetBuildOutput() { return buildOutput; }
    const UCBuildOutput& GetBuildOutput() const { return buildOutput; }
    
    // ===== CALLBACKS =====
    
    /**
     * @brief Called when build starts
     */
    std::function<void(const std::string& projectName)> onBuildStart;
    
    /**
     * @brief Called when build completes
     */
    std::function<void(const BuildResult&)> onBuildComplete;
    
    /**
     * @brief Called for each raw output line
     */
    std::function<void(const std::string&)> onOutputLine;
    
    /**
     * @brief Called for each parsed compiler message
     */
    std::function<void(const CompilerMessage&)> onCompilerMessage;
    
    /**
     * @brief Called when build progress changes
     */
    std::function<void(float)> onProgressChange;
    
    /**
     * @brief Called when build state changes
     */
    std::function<void(BuildState)> onStateChange;
    
    /**
     * @brief Called when build is cancelled
     */
    std::function<void()> onBuildCancel;
    
    /**
     * @brief Called for any build event
     */
    std::function<void(const BuildEvent&)> onBuildEvent;
    
    // ===== CONFIGURATION =====
    
    /**
     * @brief Set maximum parallel builds (for future use)
     */
    void SetMaxParallelBuilds(int max) { maxParallelBuilds = max; }
    
    /**
     * @brief Get maximum parallel builds
     */
    int GetMaxParallelBuilds() const { return maxParallelBuilds; }
    
    /**
     * @brief Set working directory for builds
     */
    void SetWorkingDirectory(const std::string& dir) { workingDirectory = dir; }
    
    /**
     * @brief Get working directory
     */
    const std::string& GetWorkingDirectory() const { return workingDirectory; }
    
    // ===== LIFECYCLE =====
    
    /**
     * @brief Initialize build manager
     */
    bool Initialize();
    
    /**
     * @brief Shutdown build manager (waits for current build)
     */
    void Shutdown();

private:
    UCBuildManager();
    ~UCBuildManager();
    
    UCBuildManager(const UCBuildManager&) = delete;
    UCBuildManager& operator=(const UCBuildManager&) = delete;
    
    // ===== INTERNAL METHODS =====
    
    /**
     * @brief Worker thread function
     */
    void WorkerThread();
    
    /**
     * @brief Execute a build job
     */
    void ExecuteBuild(const BuildQueueItem& item);
    
    /**
     * @brief Execute CMake command
     */
    int ExecuteCMakeCommand(const std::vector<std::string>& args);
    
    /**
     * @brief Execute shell command and capture output
     */
    int ExecuteCommand(const std::string& command, 
                       const std::string& workDir,
                       std::function<void(const std::string&)> outputCallback);
    
    /**
     * @brief Change build state and notify
     */
    void SetState(BuildState state);
    
    /**
     * @brief Update progress and notify
     */
    void SetProgress(float progress);
    
    /**
     * @brief Emit build event
     */
    void EmitEvent(const BuildEvent& event);
    
    /**
     * @brief Add job to queue
     */
    void QueueBuild(const BuildQueueItem& item);
    
    // ===== STATE =====
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> shutdownRequested{false};
    std::atomic<BuildState> currentState{BuildState::Idle};
    std::atomic<float> buildProgress{0.0f};
    
    BuildResult lastResult;
    std::shared_ptr<UCIDEProject> currentProject;
    
    // ===== THREADING =====
    
    std::thread workerThread;
    std::mutex queueMutex;
    std::condition_variable queueCondition;
    std::priority_queue<BuildQueueItem> buildQueue;
    
    mutable std::mutex resultMutex;
    
    // ===== OUTPUT =====
    
    UCBuildOutput buildOutput;
    
    // ===== CONFIGURATION =====
    
    int maxParallelBuilds = 1;
    std::string workingDirectory;
    bool initialized = false;
};

// ============================================================================
// PROCESS UTILITIES
// ============================================================================

/**
 * @brief Execute a process and capture output
 * @param command Command to execute
 * @param args Command arguments
 * @param workDir Working directory
 * @param outputCallback Callback for each line of output
 * @param errorCallback Callback for each line of error output
 * @return Process exit code
 */
int ExecuteProcess(
    const std::string& command,
    const std::vector<std::string>& args,
    const std::string& workDir,
    std::function<void(const std::string&)> outputCallback,
    std::function<void(const std::string&)> errorCallback = nullptr
);

/**
 * @brief Check if a command exists in PATH
 */
bool CommandExists(const std::string& command);

/**
 * @brief Find full path to command
 */
std::string FindCommand(const std::string& command);

/**
 * @brief Get environment variable
 */
std::string GetEnvVar(const std::string& name);

/**
 * @brief Set environment variable for child processes
 */
void SetEnvVar(const std::string& name, const std::string& value);

} // namespace IDE
} // namespace UltraCanvas
