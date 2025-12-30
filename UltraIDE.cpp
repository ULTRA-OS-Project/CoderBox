// Apps/IDE/UltraIDE.cpp
// Main ULTRA IDE Application Implementation
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UltraIDE.h"
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
namespace IDE {

// ============================================================================
// ULTRAIDECONFIG IMPLEMENTATION
// ============================================================================

UltraIDEConfig UltraIDEConfig::Default() {
    UltraIDEConfig config;
    
    // Set default project path to user's home directory
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, path))) {
        config.defaultProjectPath = std::string(path) + "\\UltraIDE\\Projects";
    } else {
        config.defaultProjectPath = "C:\\Users\\Public\\UltraIDE\\Projects";
    }
#else
    const char* homeDir = getenv("HOME");
    if (!homeDir) {
        struct passwd* pw = getpwuid(getuid());
        homeDir = pw ? pw->pw_dir : "/tmp";
    }
    config.defaultProjectPath = std::string(homeDir) + "/UltraIDE/Projects";
#endif
    
    return config;
}

bool UltraIDEConfig::LoadFromFile(const std::string& path) {
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
        if (key == "windowTitle") windowTitle = value;
        else if (key == "windowWidth") windowWidth = std::stoi(value);
        else if (key == "windowHeight") windowHeight = std::stoi(value);
        else if (key == "maximized") maximized = (value == "true" || value == "1");
        else if (key == "themeName") themeName = value;
        else if (key == "fontFamily") fontFamily = value;
        else if (key == "fontSize") fontSize = std::stoi(value);
        else if (key == "tabSize") tabSize = std::stoi(value);
        else if (key == "insertSpaces") insertSpaces = (value == "true" || value == "1");
        else if (key == "showLineNumbers") showLineNumbers = (value == "true" || value == "1");
        else if (key == "defaultProjectPath") defaultProjectPath = value;
    }
    
    return true;
}

bool UltraIDEConfig::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# ULTRA IDE Configuration\n";
    file << "# Generated: " << __DATE__ << " " << __TIME__ << "\n\n";
    
    file << "[Window]\n";
    file << "windowTitle=" << windowTitle << "\n";
    file << "windowWidth=" << windowWidth << "\n";
    file << "windowHeight=" << windowHeight << "\n";
    file << "maximized=" << (maximized ? "true" : "false") << "\n\n";
    
    file << "[Theme]\n";
    file << "themeName=" << themeName << "\n";
    file << "fontFamily=" << fontFamily << "\n";
    file << "fontSize=" << fontSize << "\n\n";
    
    file << "[Editor]\n";
    file << "tabSize=" << tabSize << "\n";
    file << "insertSpaces=" << (insertSpaces ? "true" : "false") << "\n";
    file << "autoIndent=" << (autoIndent ? "true" : "false") << "\n";
    file << "showLineNumbers=" << (showLineNumbers ? "true" : "false") << "\n";
    file << "highlightCurrentLine=" << (highlightCurrentLine ? "true" : "false") << "\n";
    file << "wordWrap=" << (wordWrap ? "true" : "false") << "\n\n";
    
    file << "[Build]\n";
    file << "autoBuildOnSave=" << (autoBuildOnSave ? "true" : "false") << "\n";
    file << "showBuildNotifications=" << (showBuildNotifications ? "true" : "false") << "\n";
    file << "clearOutputBeforeBuild=" << (clearOutputBeforeBuild ? "true" : "false") << "\n\n";
    
    file << "[Project]\n";
    file << "defaultProjectPath=" << defaultProjectPath << "\n";
    
    return true;
}

// ============================================================================
// ULTRAIDE SINGLETON
// ============================================================================

UltraIDE& UltraIDE::Instance() {
    static UltraIDE instance;
    return instance;
}

UltraIDE::UltraIDE() {
    // Default constructor
}

UltraIDE::~UltraIDE() {
    if (running) {
        Shutdown();
    }
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool UltraIDE::Initialize(const UltraIDEConfig& cfg) {
    if (initialized) {
        std::cerr << "[ULTRA IDE] Already initialized" << std::endl;
        return true;
    }
    
    std::cout << "\n";
    PrintWelcome();
    
    config = cfg;
    SetState(IDEState::Initializing);
    
    std::cout << "[ULTRA IDE] Initializing..." << std::endl;
    
    // Register all compiler plugins
    std::cout << "[ULTRA IDE] Registering compiler plugins..." << std::endl;
    RegisterBuiltInPlugins();
    
    // Detect available compilers
    std::cout << "[ULTRA IDE] Detecting compilers..." << std::endl;
    DetectCompilers();
    
    // Print plugin status
    PrintPluginStatus();
    
    // Initialize build system
    std::cout << "[ULTRA IDE] Initializing build system..." << std::endl;
    if (!InitializeBuildSystem()) {
        std::cerr << "[ULTRA IDE] ERROR: Failed to initialize build system" << std::endl;
        return false;
    }
    
    // Initialize project system
    std::cout << "[ULTRA IDE] Initializing project system..." << std::endl;
    if (!InitializeProjectSystem()) {
        std::cerr << "[ULTRA IDE] ERROR: Failed to initialize project system" << std::endl;
        return false;
    }
    
    // Load configuration
    LoadConfig();
    
    initialized = true;
    SetState(IDEState::Ready);
    
    std::cout << "[ULTRA IDE] Initialization complete" << std::endl;
    std::cout << "\n";
    
    return true;
}

int UltraIDE::Run() {
    if (!initialized) {
        std::cerr << "[ULTRA IDE] ERROR: IDE not initialized" << std::endl;
        return -1;
    }
    
    running = true;
    std::cout << "[ULTRA IDE] Running..." << std::endl;
    
    // Main loop would integrate with UltraCanvas event system
    // For now, this is a placeholder
    while (running) {
        ProcessEvents();
        UpdateUI();
        
        // In actual implementation, this would be event-driven
        // For testing, we break immediately
        break;
    }
    
    return 0;
}

void UltraIDE::Shutdown() {
    if (!initialized) {
        return;
    }
    
    std::cout << "[ULTRA IDE] Shutting down..." << std::endl;
    SetState(IDEState::ShuttingDown);
    
    running = false;
    
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
    std::cout << "[ULTRA IDE] Shutdown complete" << std::endl;
}

// ============================================================================
// PLUGIN MANAGEMENT
// ============================================================================

void UltraIDE::RegisterBuiltInPlugins() {
    auto& registry = UCCompilerPluginRegistry::Instance();
    
    std::cout << "  Registering GCC/G++ plugins..." << std::endl;
    RegisterGCCPlugins();  // Registers both GCC and G++
    
    std::cout << "  Registering FreePascal plugin..." << std::endl;
    RegisterFreePascalPlugin();
    
    std::cout << "  Registering Rust plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCRustPlugin>());
    
    std::cout << "  Registering Python plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCPythonPlugin>());
    
    std::cout << "  Registering Java plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCJavaPlugin>());
    
    std::cout << "  Registering Go plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCGoPlugin>());
    
    std::cout << "  Registering C# plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCCSharpPlugin>());
    
    std::cout << "  Registering Fortran plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCFortranPlugin>());
    
    std::cout << "  Registering Lua plugin..." << std::endl;
    registry.RegisterPlugin(std::make_shared<UCLuaPlugin>());
    
    std::cout << "  Total plugins registered: " << registry.GetPluginCount() << std::endl;
}

void UltraIDE::RegisterPlugin(std::shared_ptr<IUCCompilerPlugin> plugin) {
    if (plugin) {
        UCCompilerPluginRegistry::Instance().RegisterPlugin(plugin);
    }
}

void UltraIDE::UnregisterPlugin(CompilerType type) {
    UCCompilerPluginRegistry::Instance().UnregisterPlugin(type);
}

std::vector<std::shared_ptr<IUCCompilerPlugin>> UltraIDE::GetRegisteredPlugins() const {
    return UCCompilerPluginRegistry::Instance().GetAllPlugins();
}

std::vector<std::shared_ptr<IUCCompilerPlugin>> UltraIDE::GetAvailablePlugins() const {
    return UCCompilerPluginRegistry::Instance().GetAvailablePlugins();
}

void UltraIDE::DetectCompilers() {
    auto plugins = GetRegisteredPlugins();
    for (auto& plugin : plugins) {
        if (plugin) {
            plugin->IsAvailable();  // This triggers detection
        }
    }
}

void UltraIDE::PrintPluginStatus() const {
    std::cout << "\n";
    std::cout << "========================================" << std::endl;
    std::cout << "  ULTRA IDE - Compiler Plugin Status   " << std::endl;
    std::cout << "========================================" << std::endl;
    
    auto plugins = UCCompilerPluginRegistry::Instance().GetAllPlugins();
    
    int available = 0;
    int unavailable = 0;
    
    for (const auto& plugin : plugins) {
        if (!plugin) continue;
        
        bool isAvailable = plugin->IsAvailable();
        std::string status = isAvailable ? "[✓]" : "[✗]";
        std::string version = isAvailable ? plugin->GetCompilerVersion() : "Not found";
        
        if (isAvailable) available++;
        else unavailable++;
        
        std::cout << "  " << status << " " 
                  << std::left << std::setw(20) << plugin->GetCompilerName()
                  << " v" << version << std::endl;
    }
    
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "  Available: " << available 
              << " | Unavailable: " << unavailable 
              << " | Total: " << plugins.size() << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\n";
}

// ============================================================================
// PROJECT MANAGEMENT
// ============================================================================

std::shared_ptr<UCIDEProject> UltraIDE::NewProject(
    const std::string& name,
    const std::string& path,
    CompilerType compiler
) {
    auto& projectManager = UCIDEProjectManager::Instance();
    activeProject = projectManager.CreateProject(name, path, compiler);
    
    if (activeProject && onProjectOpen) {
        onProjectOpen(activeProject);
    }
    
    return activeProject;
}

std::shared_ptr<UCIDEProject> UltraIDE::OpenProject(const std::string& projectPath) {
    auto& projectManager = UCIDEProjectManager::Instance();
    activeProject = projectManager.OpenProject(projectPath);
    
    if (activeProject && onProjectOpen) {
        onProjectOpen(activeProject);
    }
    
    return activeProject;
}

std::shared_ptr<UCIDEProject> UltraIDE::ImportCMakeProject(const std::string& cmakeListsPath) {
    auto& projectManager = UCIDEProjectManager::Instance();
    activeProject = projectManager.ImportCMakeProject(cmakeListsPath);
    
    if (activeProject && onProjectOpen) {
        onProjectOpen(activeProject);
    }
    
    return activeProject;
}

bool UltraIDE::SaveProject() {
    if (!activeProject) {
        return false;
    }
    return UCIDEProjectManager::Instance().SaveProject(activeProject);
}

bool UltraIDE::CloseProject() {
    if (!activeProject) {
        return true;
    }
    
    if (onProjectClose) {
        onProjectClose(activeProject);
    }
    
    bool result = UCIDEProjectManager::Instance().CloseProject(activeProject);
    activeProject = nullptr;
    return result;
}

std::shared_ptr<UCIDEProject> UltraIDE::GetActiveProject() const {
    return activeProject;
}

std::vector<std::string> UltraIDE::GetRecentProjects() const {
    return UCIDEProjectManager::Instance().GetRecentProjects();
}

// ============================================================================
// BUILD OPERATIONS
// ============================================================================

void UltraIDE::Build() {
    if (!activeProject) {
        if (onError) {
            onError("No project open");
        }
        return;
    }
    
    SetState(IDEState::Building);
    UCBuildManager::Instance().BuildProject(activeProject);
}

void UltraIDE::Rebuild() {
    if (!activeProject) {
        if (onError) {
            onError("No project open");
        }
        return;
    }
    
    SetState(IDEState::Building);
    UCBuildManager::Instance().RebuildProject(activeProject);
}

void UltraIDE::Clean() {
    if (!activeProject) {
        if (onError) {
            onError("No project open");
        }
        return;
    }
    
    UCBuildManager::Instance().CleanProject(activeProject);
}

void UltraIDE::BuildAndRun() {
    if (!activeProject) {
        if (onError) {
            onError("No project open");
        }
        return;
    }
    
    // Set up callback to run after successful build
    auto originalCallback = UCBuildManager::Instance().onBuildComplete;
    
    UCBuildManager::Instance().onBuildComplete = [this, originalCallback](const BuildResult& result) {
        if (originalCallback) {
            originalCallback(result);
        }
        
        if (result.success) {
            Run();
        }
    };
    
    Build();
    
    // Restore original callback
    UCBuildManager::Instance().onBuildComplete = originalCallback;
}

void UltraIDE::Run() {
    if (!activeProject) {
        if (onError) {
            onError("No project open");
        }
        return;
    }
    
    const auto& config = activeProject->GetActiveConfiguration();
    std::string outputPath = activeProject->GetAbsolutePath(config.GetOutputPath());
    
    if (outputPath.empty()) {
        if (onError) {
            onError("No executable to run");
        }
        return;
    }
    
    SetState(IDEState::Running);
    
    // Execute the program (platform-specific implementation needed)
    std::cout << "[ULTRA IDE] Running: " << outputPath << std::endl;
    
    // This would need platform-specific process spawning
    // For now, just using system()
#ifdef _WIN32
    std::string cmd = "\"" + outputPath + "\"";
#else
    std::string cmd = "./" + outputPath;
#endif
    
    int result = system(cmd.c_str());
    (void)result;
    
    SetState(IDEState::Ready);
}

void UltraIDE::Stop() {
    if (runningProcess) {
        // Kill running process (platform-specific)
        runningProcess = nullptr;
    }
    
    if (currentState == IDEState::Running) {
        SetState(IDEState::Ready);
    }
}

void UltraIDE::CancelBuild() {
    UCBuildManager::Instance().CancelBuild();
    SetState(IDEState::Ready);
}

bool UltraIDE::IsBuildInProgress() const {
    return UCBuildManager::Instance().IsBuildInProgress();
}

const BuildResult& UltraIDE::GetLastBuildResult() const {
    return UCBuildManager::Instance().GetLastBuildResult();
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

void UltraIDE::OpenFile(const std::string& filePath) {
    if (std::find(openFiles.begin(), openFiles.end(), filePath) == openFiles.end()) {
        openFiles.push_back(filePath);
    }
    activeFile = filePath;
    
    if (onFileOpen) {
        onFileOpen(filePath);
    }
}

void UltraIDE::SaveFile() {
    if (activeFile.empty()) {
        return;
    }
    
    if (onFileSave) {
        onFileSave(activeFile);
    }
}

void UltraIDE::SaveAllFiles() {
    for (const auto& file : openFiles) {
        if (onFileSave) {
            onFileSave(file);
        }
    }
}

void UltraIDE::CloseFile(const std::string& filePath) {
    auto it = std::find(openFiles.begin(), openFiles.end(), filePath);
    if (it != openFiles.end()) {
        openFiles.erase(it);
    }
    
    if (activeFile == filePath) {
        activeFile = openFiles.empty() ? "" : openFiles.back();
    }
}

std::vector<std::string> UltraIDE::GetOpenFiles() const {
    return openFiles;
}

std::string UltraIDE::GetActiveFile() const {
    return activeFile;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

bool UltraIDE::SaveConfig() {
    return config.SaveToFile(GetConfigFilePath());
}

bool UltraIDE::LoadConfig() {
    return config.LoadFromFile(GetConfigFilePath());
}

std::string UltraIDE::GetConfigFilePath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\UltraIDE\\config.ini";
    }
    return "ultraide.ini";
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        return std::string(homeDir) + "/.config/ultraide/config.ini";
    }
    return "ultraide.ini";
#endif
}

// ============================================================================
// UTILITIES
// ============================================================================

std::string UltraIDE::GetVersionString() const {
    return std::string(ULTRAIDE_VERSION);
}

std::string UltraIDE::GetBuildInfo() const {
    std::stringstream ss;
    ss << ULTRAIDE_NAME << " v" << ULTRAIDE_VERSION;
    ss << " (Built: " << ULTRAIDE_BUILD_DATE << ")";
    return ss.str();
}

void UltraIDE::PrintWelcome() const {
    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                              ║" << std::endl;
    std::cout << "║   ██╗   ██╗██╗  ████████╗██████╗  █████╗     ██╗██████╗ ███████╗   ║" << std::endl;
    std::cout << "║   ██║   ██║██║  ╚══██╔══╝██╔══██╗██╔══██╗    ██║██╔══██╗██╔════╝   ║" << std::endl;
    std::cout << "║   ██║   ██║██║     ██║   ██████╔╝███████║    ██║██║  ██║█████╗     ║" << std::endl;
    std::cout << "║   ██║   ██║██║     ██║   ██╔══██╗██╔══██║    ██║██║  ██║██╔══╝     ║" << std::endl;
    std::cout << "║   ╚██████╔╝███████╗██║   ██║  ██║██║  ██║    ██║██████╔╝███████╗   ║" << std::endl;
    std::cout << "║    ╚═════╝ ╚══════╝╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝    ╚═╝╚═════╝ ╚══════╝   ║" << std::endl;
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

void UltraIDE::SetState(IDEState state) {
    if (currentState != state) {
        currentState = state;
        if (onStateChange) {
            onStateChange(state);
        }
    }
}

void UltraIDE::SetupBuildCallbacks() {
    auto& buildMgr = UCBuildManager::Instance();
    
    buildMgr.onBuildStart = [this](const std::string& projectName) {
        std::cout << "[Build] Starting: " << projectName << std::endl;
    };
    
    buildMgr.onBuildComplete = [this](const BuildResult& result) {
        lastBuildResult = result;
        SetState(IDEState::Ready);
        
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
        SetState(IDEState::Ready);
        std::cout << "[Build] Cancelled" << std::endl;
    };
}

void UltraIDE::SetupProjectCallbacks() {
    auto& projectMgr = UCIDEProjectManager::Instance();
    
    projectMgr.onProjectOpen = [this](std::shared_ptr<UCIDEProject> project) {
        std::cout << "[Project] Opened: " << project->name << std::endl;
    };
    
    projectMgr.onProjectClose = [this](std::shared_ptr<UCIDEProject> project) {
        std::cout << "[Project] Closed: " << project->name << std::endl;
    };
    
    projectMgr.onProjectSave = [this](std::shared_ptr<UCIDEProject> project) {
        std::cout << "[Project] Saved: " << project->name << std::endl;
    };
    
    projectMgr.onProjectError = [this](const std::string& error) {
        std::cerr << "[Project] Error: " << error << std::endl;
        if (onError) {
            onError(error);
        }
    };
}

bool UltraIDE::InitializeUI() {
    // UI initialization would go here (using UltraCanvas widgets)
    // This is a placeholder for Phase 4
    return true;
}

bool UltraIDE::InitializeBuildSystem() {
    if (!UCBuildManager::Instance().Initialize()) {
        return false;
    }
    
    SetupBuildCallbacks();
    return true;
}

bool UltraIDE::InitializeProjectSystem() {
    SetupProjectCallbacks();
    UCIDEProjectManager::Instance().LoadRecentProjects();
    return true;
}

void UltraIDE::ProcessEvents() {
    // Event processing would integrate with UltraCanvas
}

void UltraIDE::UpdateUI() {
    // UI updates would go here
}

// ============================================================================
// GLOBAL REGISTRATION FUNCTIONS
// ============================================================================

void RegisterAllCompilerPlugins() {
    UltraIDE::Instance().RegisterBuiltInPlugins();
}

void DetectAndPrintCompilers() {
    UltraIDE::Instance().DetectCompilers();
    UltraIDE::Instance().PrintPluginStatus();
}

} // namespace IDE
} // namespace UltraCanvas
