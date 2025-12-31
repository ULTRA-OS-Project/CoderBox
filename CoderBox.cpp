// Apps/CoderBox/CoderBox.cpp
// CoderBox - Main Application Business Logic Implementation
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#include "CoderBox.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <algorithm>

// Platform-specific
#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <pwd.h>
#endif

namespace UltraCanvas {
namespace CoderBox {

// ============================================================================
// CODERBOXCONFIG IMPLEMENTATION
// ============================================================================

CoderBoxConfig CoderBoxConfig::Default() {
    CoderBoxConfig config;
    
    // Set default project path to user's home directory
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path))) {
        config.defaultProjectPath = std::string(path) + "\\CoderBox\\Projects";
    } else {
        config.defaultProjectPath = "C:\\Users\\Public\\CoderBox\\Projects";
    }
#else
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw ? pw->pw_dir : "/tmp";
    }
    config.defaultProjectPath = std::string(homeDir) + "/CoderBox/Projects";
#endif
    
    return config;
}

bool CoderBoxConfig::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    // Simple key=value parsing
    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Parse known keys
        if (key == "autoBuildOnSave") autoBuildOnSave = (value == "true" || value == "1");
        else if (key == "showBuildNotifications") showBuildNotifications = (value == "true" || value == "1");
        else if (key == "clearOutputBeforeBuild") clearOutputBeforeBuild = (value == "true" || value == "1");
        else if (key == "maxParallelBuilds") maxParallelBuilds = std::stoi(value);
        else if (key == "defaultProjectPath") defaultProjectPath = value;
        else if (key == "maxRecentProjects") maxRecentProjects = std::stoi(value);
        else if (key == "autoDetectCompilers") autoDetectCompilers = (value == "true" || value == "1");
    }
    
    return true;
}

bool CoderBoxConfig::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# CoderBox Configuration\n";
    file << "# Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
    
    file << "[Build]\n";
    file << "autoBuildOnSave=" << (autoBuildOnSave ? "true" : "false") << "\n";
    file << "showBuildNotifications=" << (showBuildNotifications ? "true" : "false") << "\n";
    file << "clearOutputBeforeBuild=" << (clearOutputBeforeBuild ? "true" : "false") << "\n";
    file << "maxParallelBuilds=" << maxParallelBuilds << "\n\n";
    
    file << "[Project]\n";
    file << "defaultProjectPath=" << defaultProjectPath << "\n";
    file << "maxRecentProjects=" << maxRecentProjects << "\n\n";
    
    file << "[Plugins]\n";
    file << "autoDetectCompilers=" << (autoDetectCompilers ? "true" : "false") << "\n";
    
    return true;
}

// ============================================================================
// CODERBOX SINGLETON
// ============================================================================

CoderBox& CoderBox::Instance() {
    static CoderBox instance;
    return instance;
}

CoderBox::CoderBox() {
    // Default constructor
}

CoderBox::~CoderBox() {
    if (initialized) {
        Shutdown();
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool CoderBox::Initialize(const CoderBoxConfig& cfg) {
    if (initialized) {
        std::cerr << "[CoderBox] Already initialized" << std::endl;
        return true;
    }
    
    std::cout << "\n";
    PrintWelcome();
    
    config = cfg;
    SetState(CoderBoxState::Initializing);
    
    std::cout << "[CoderBox] Initializing business logic layer..." << std::endl;
    
    // Register all compiler plugins
    std::cout << "[CoderBox] Registering compiler plugins..." << std::endl;
    RegisterBuiltInPlugins();
    
    // Detect available compilers
    std::cout << "[CoderBox] Detecting compilers..." << std::endl;
    DetectCompilers();
    
    // Print plugin status
    PrintPluginStatus();
    
    // Initialize build system
    std::cout << "[CoderBox] Initializing build system..." << std::endl;
    if (!InitializeBuildSystem()) {
        std::cerr << "[CoderBox] ERROR: Failed to initialize build system" << std::endl;
        return false;
    }
    
    // Initialize project system
    std::cout << "[CoderBox] Initializing project system..." << std::endl;
    if (!InitializeProjectSystem()) {
        std::cerr << "[CoderBox] ERROR: Failed to initialize project system" << std::endl;
        return false;
    }
    
    // Load configuration
    LoadConfig();
    
    initialized = true;
    SetState(CoderBoxState::Ready);
    
    std::cout << "[CoderBox] Business logic initialization complete" << std::endl;
    std::cout << "\n";
    
    return true;
}

void CoderBox::Shutdown() {
    if (!initialized) {
        return;
    }
    
    std::cout << "[CoderBox] Shutting down business logic layer..." << std::endl;
    SetState(CoderBoxState::ShuttingDown);
    
    // Cancel any ongoing builds
    if (IsBuildInProgress()) {
        CancelBuild();
    }
    
    // Stop any running program
    Stop();
    
    // Close project
    CloseProject();
    
    // Save configuration
    SaveConfig();
    
    // Shutdown build manager
    UCBuildManager::Instance().Shutdown();
    
    initialized = false;
    std::cout << "[CoderBox] Business logic shutdown complete" << std::endl;
}

// ============================================================================
// PLUGIN MANAGEMENT
// ============================================================================

void CoderBox::RegisterBuiltInPlugins() {
    auto& registry = UCCompilerPluginRegistry::Instance();
    
    // Register all built-in compiler plugins
    registry.RegisterPlugin(std::make_shared<UCGCCPlugin>());
    registry.RegisterPlugin(std::make_shared<UCFreePascalPlugin>());
    registry.RegisterPlugin(std::make_shared<UCRustPlugin>());
    registry.RegisterPlugin(std::make_shared<UCPythonPlugin>());
    registry.RegisterPlugin(std::make_shared<UCJavaPlugin>());
    registry.RegisterPlugin(std::make_shared<UCGoPlugin>());
    registry.RegisterPlugin(std::make_shared<UCCSharpPlugin>());
    registry.RegisterPlugin(std::make_shared<UCFortranPlugin>());
    registry.RegisterPlugin(std::make_shared<UCLuaPlugin>());
    
    std::cout << "[CoderBox] Registered " << registry.GetAllPlugins().size() 
              << " compiler plugins" << std::endl;
}

void CoderBox::RegisterPlugin(std::shared_ptr<IUCCompilerPlugin> plugin) {
    if (plugin) {
        UCCompilerPluginRegistry::Instance().RegisterPlugin(plugin);
        std::cout << "[CoderBox] Registered plugin: " << plugin->GetName() << std::endl;
    }
}

void CoderBox::UnregisterPlugin(CompilerType type) {
    UCCompilerPluginRegistry::Instance().UnregisterPlugin(type);
}

std::vector<std::shared_ptr<IUCCompilerPlugin>> CoderBox::GetRegisteredPlugins() const {
    return UCCompilerPluginRegistry::Instance().GetAllPlugins();
}

std::vector<std::shared_ptr<IUCCompilerPlugin>> CoderBox::GetAvailablePlugins() const {
    return UCCompilerPluginRegistry::Instance().GetAvailablePlugins();
}

void CoderBox::DetectCompilers() {
    auto plugins = UCCompilerPluginRegistry::Instance().GetAllPlugins();
    for (auto& plugin : plugins) {
        plugin->DetectCompiler();
    }
}

void CoderBox::PrintPluginStatus() const {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                   Compiler Plugin Status                     ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════╣" << std::endl;
    
    auto plugins = UCCompilerPluginRegistry::Instance().GetAllPlugins();
    for (const auto& plugin : plugins) {
        std::string status = plugin->IsAvailable() ? "✓ Available" : "✗ Not Found";
        std::string name = plugin->GetName();
        std::string version = plugin->IsAvailable() ? plugin->GetVersion() : "-";
        
        std::cout << "║ " << std::left << std::setw(20) << name 
                  << std::setw(15) << status
                  << std::setw(25) << version << " ║" << std::endl;
    }
    
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "\n";
}

// ============================================================================
// PROJECT MANAGEMENT
// ============================================================================

std::shared_ptr<UCCoderBoxProject> CoderBox::NewProject(
    const std::string& name,
    const std::string& path,
    CompilerType compiler)
{
    auto project = UCCoderBoxProject::Create(name, path, compiler);
    if (project) {
        SetActiveProject(project);
        AddToRecentProjects(project->projectFilePath);
        
        if (onProjectOpen) {
            onProjectOpen(project);
        }
    }
    return project;
}

std::shared_ptr<UCCoderBoxProject> CoderBox::OpenProject(const std::string& projectPath) {
    auto project = UCCoderBoxProjectManager::Instance().OpenProject(projectPath);
    if (project) {
        SetActiveProject(project);
        AddToRecentProjects(projectPath);
        
        if (onProjectOpen) {
            onProjectOpen(project);
        }
    }
    return project;
}

std::shared_ptr<UCCoderBoxProject> CoderBox::ImportCMakeProject(const std::string& cmakeListsPath) {
    auto project = UCCoderBoxProject::CreateFromCMake(cmakeListsPath);
    if (project) {
        SetActiveProject(project);
        AddToRecentProjects(project->projectFilePath);
        
        if (onProjectOpen) {
            onProjectOpen(project);
        }
    }
    return project;
}

bool CoderBox::SaveProject() {
    if (!activeProject) {
        return false;
    }
    return UCCoderBoxProjectManager::Instance().SaveProject(activeProject);
}

bool CoderBox::CloseProject() {
    if (!activeProject) {
        return true;
    }
    
    auto project = activeProject;
    activeProject = nullptr;
    
    if (onProjectClose) {
        onProjectClose(project);
    }
    
    return UCCoderBoxProjectManager::Instance().CloseProject(project);
}

std::shared_ptr<UCCoderBoxProject> CoderBox::GetActiveProject() const {
    return activeProject;
}

void CoderBox::SetActiveProject(std::shared_ptr<UCCoderBoxProject> project) {
    activeProject = project;
    UCCoderBoxProjectManager::Instance().SetActiveProject(project);
}

std::vector<std::string> CoderBox::GetRecentProjects() const {
    return config.recentProjects;
}

void CoderBox::AddToRecentProjects(const std::string& projectPath) {
    // Remove if already exists
    auto it = std::find(config.recentProjects.begin(), config.recentProjects.end(), projectPath);
    if (it != config.recentProjects.end()) {
        config.recentProjects.erase(it);
    }
    
    // Add to front
    config.recentProjects.insert(config.recentProjects.begin(), projectPath);
    
    // Trim to max size
    if (static_cast<int>(config.recentProjects.size()) > config.maxRecentProjects) {
        config.recentProjects.resize(config.maxRecentProjects);
    }
}

// ============================================================================
// BUILD OPERATIONS
// ============================================================================

void CoderBox::Build() {
    if (!activeProject) {
        std::cerr << "[CoderBox] No active project to build" << std::endl;
        if (onError) {
            onError("No active project to build");
        }
        return;
    }
    
    SetState(CoderBoxState::Building);
    UCBuildManager::Instance().BuildProject(activeProject);
}

void CoderBox::Rebuild() {
    Clean();
    Build();
}

void CoderBox::Clean() {
    if (!activeProject) {
        std::cerr << "[CoderBox] No active project to clean" << std::endl;
        return;
    }
    
    UCBuildManager::Instance().CleanProject(activeProject);
}

void CoderBox::BuildAndRun() {
    if (!activeProject) {
        std::cerr << "[CoderBox] No active project to build and run" << std::endl;
        return;
    }
    
    // Set callback to run after build completes
    auto previousCallback = onBuildComplete;
    onBuildComplete = [this, previousCallback](const BuildResult& result) {
        if (result.success) {
            Run();
        }
        // Restore previous callback
        onBuildComplete = previousCallback;
        if (previousCallback) {
            previousCallback(result);
        }
    };
    
    Build();
}

void CoderBox::Run() {
    if (!activeProject) {
        std::cerr << "[CoderBox] No active project to run" << std::endl;
        return;
    }
    
    SetState(CoderBoxState::Running);
    
    // Get output path from project
    auto& buildConfig = activeProject->GetActiveConfiguration();
    std::string outputPath = activeProject->rootDirectory + "/" + 
                            buildConfig.outputDirectory + "/" + 
                            buildConfig.outputName;
    
    std::cout << "[CoderBox] Running: " << outputPath << std::endl;
    
    // Execute (platform-specific)
#ifdef _WIN32
    std::string cmd = "\"" + outputPath + "\"";
#else
    std::string cmd = "./" + outputPath;
#endif
    
    int result = system(cmd.c_str());
    (void)result;
    
    SetState(CoderBoxState::Ready);
}

void CoderBox::Stop() {
    if (runningProcess) {
        // Kill running process (platform-specific)
        runningProcess = nullptr;
    }
    
    if (currentState == CoderBoxState::Running) {
        SetState(CoderBoxState::Ready);
    }
}

void CoderBox::CancelBuild() {
    UCBuildManager::Instance().CancelBuild();
    SetState(CoderBoxState::Ready);
}

bool CoderBox::IsBuildInProgress() const {
    return UCBuildManager::Instance().IsBuildInProgress();
}

const BuildResult& CoderBox::GetLastBuildResult() const {
    return UCBuildManager::Instance().GetLastBuildResult();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool CoderBox::SaveConfig() {
    return config.SaveToFile(GetConfigFilePath());
}

bool CoderBox::LoadConfig() {
    return config.LoadFromFile(GetConfigFilePath());
}

std::string CoderBox::GetConfigFilePath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\CoderBox\\config.ini";
    }
    return "coderbox.ini";
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        return std::string(homeDir) + "/.config/coderbox/config.ini";
    }
    return "coderbox.ini";
#endif
}

// ============================================================================
// UTILITIES
// ============================================================================

std::string CoderBox::GetVersionString() const {
    return std::string(CODERBOX_VERSION);
}

std::string CoderBox::GetBuildInfo() const {
    std::stringstream ss;
    ss << CODERBOX_NAME << " v" << CODERBOX_VERSION;
    ss << " (Built: " << CODERBOX_BUILD_DATE << ")";
    return ss.str();
}

void CoderBox::PrintWelcome() const {
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "║    ██████╗ ██████╗ ██████╗ ███████╗██████╗ ██████╗  ██████╗ ██╗  ██╗    ║" << std::endl;
    std::cout << "║   ██╔════╝██╔═══██╗██╔══██╗██╔════╝██╔══██╗██╔══██╗██╔═══██╗╚██╗██╔╝    ║" << std::endl;
    std::cout << "║   ██║     ██║   ██║██║  ██║█████╗  ██████╔╝██████╔╝██║   ██║ ╚███╔╝     ║" << std::endl;
    std::cout << "║   ██║     ██║   ██║██║  ██║██╔══╝  ██╔══██╗██╔══██╗██║   ██║ ██╔██╗     ║" << std::endl;
    std::cout << "║   ╚██████╗╚██████╔╝██████╔╝███████╗██║  ██║██████╔╝╚██████╔╝██╔╝ ██╗    ║" << std::endl;
    std::cout << "║    ╚═════╝ ╚═════╝ ╚═════╝ ╚══════╝╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚═╝  ╚═╝    ║" << std::endl;
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "║              Integrated Development Environment              ║" << std::endl;
    std::cout << "║                     for ULTRA OS                             ║" << std::endl;
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "║  Version: " << std::left << std::setw(50) << GetBuildInfo() << "║" << std::endl;
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void CoderBox::SetState(CoderBoxState state) {
    if (currentState != state) {
        currentState = state;
        if (onStateChange) {
            onStateChange(state);
        }
    }
}

void CoderBox::SetupBuildCallbacks() {
    auto& buildMgr = UCBuildManager::Instance();
    
    buildMgr.onBuildStart = [this](const std::string& projectName) {
        std::cout << "[Build] Starting: " << projectName << std::endl;
    };
    
    buildMgr.onBuildComplete = [this](const BuildResult& result) {
        lastBuildResult = result;
        SetState(CoderBoxState::Ready);
        
        std::cout << "[Build] " << result.GetSummary() << std::endl;
        
        if (onBuildComplete) {
            onBuildComplete(result);
        }
    };
    
    buildMgr.onOutputLine = [this](const std::string& line) {
        if (onBuildOutput) {
            onBuildOutput(line);
        }
    };
    
    buildMgr.onCompilerMessage = [this](const CompilerMessage& msg) {
        if (onCompilerMessage) {
            onCompilerMessage(msg);
        }
    };
    
    buildMgr.onBuildCancel = [this]() {
        SetState(CoderBoxState::Ready);
        std::cout << "[Build] Cancelled" << std::endl;
    };
}

void CoderBox::SetupProjectCallbacks() {
    auto& projectMgr = UCCoderBoxProjectManager::Instance();
    
    projectMgr.onProjectOpen = [this](std::shared_ptr<UCCoderBoxProject> project) {
        std::cout << "[Project] Opened: " << project->name << std::endl;
    };
    
    projectMgr.onProjectClose = [this](std::shared_ptr<UCCoderBoxProject> project) {
        std::cout << "[Project] Closed: " << project->name << std::endl;
    };
    
    projectMgr.onProjectSave = [this](std::shared_ptr<UCCoderBoxProject> project) {
        std::cout << "[Project] Saved: " << project->name << std::endl;
    };
    
    projectMgr.onProjectError = [this](const std::string& error) {
        std::cerr << "[Project] Error: " << error << std::endl;
        if (onError) {
            onError(error);
        }
    };
}

bool CoderBox::InitializeBuildSystem() {
    if (!UCBuildManager::Instance().Initialize()) {
        return false;
    }
    
    SetupBuildCallbacks();
    return true;
}

bool CoderBox::InitializeProjectSystem() {
    SetupProjectCallbacks();
    UCCoderBoxProjectManager::Instance().LoadRecentProjects();
    return true;
}

// ============================================================================
// GLOBAL FUNCTIONS
// ============================================================================

void RegisterAllCompilerPlugins() {
    CoderBox::Instance().RegisterBuiltInPlugins();
}

void DetectAndPrintCompilers() {
    CoderBox::Instance().DetectCompilers();
    CoderBox::Instance().PrintPluginStatus();
}

} // namespace CoderBox
} // namespace UltraCanvas
