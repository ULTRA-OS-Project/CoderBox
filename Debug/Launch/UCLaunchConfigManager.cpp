// Apps/IDE/Debug/Launch/UCLaunchConfigManager.cpp
// Launch Configuration Manager Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCLaunchConfigManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace UltraCanvas {
namespace IDE {

UCLaunchConfigManager::UCLaunchConfigManager() = default;
UCLaunchConfigManager::~UCLaunchConfigManager() = default;

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

std::string UCLaunchConfigManager::AddConfiguration(
    std::unique_ptr<UCLaunchConfiguration> config) {
    if (!config) return "";
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string id = config->GetId();
    
    // Ensure unique name
    std::string baseName = config->GetName();
    std::string name = baseName;
    int suffix = 1;
    
    while (GetConfigurationByName(name) != nullptr) {
        name = baseName + " (" + std::to_string(++suffix) + ")";
    }
    config->SetName(name);
    
    // Set as default if first configuration
    if (configurations_.empty()) {
        config->SetIsDefault(true);
        defaultConfigId_ = id;
        activeConfigId_ = id;
    }
    
    configurations_[id] = std::move(config);
    
    NotifyConfigurationAdded(id);
    return id;
}

bool UCLaunchConfigManager::RemoveConfiguration(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = configurations_.find(id);
    if (it == configurations_.end()) return false;
    
    bool wasDefault = it->second->IsDefault();
    bool wasActive = (id == activeConfigId_);
    
    configurations_.erase(it);
    
    // Remove from recent
    recentConfigIds_.erase(
        std::remove(recentConfigIds_.begin(), recentConfigIds_.end(), id),
        recentConfigIds_.end());
    
    // Handle default/active removal
    if (!configurations_.empty()) {
        if (wasDefault) {
            auto firstConfig = configurations_.begin()->second.get();
            firstConfig->SetIsDefault(true);
            defaultConfigId_ = firstConfig->GetId();
        }
        
        if (wasActive) {
            activeConfigId_ = configurations_.begin()->first;
            NotifyActiveChanged(activeConfigId_);
        }
    } else {
        defaultConfigId_.clear();
        activeConfigId_.clear();
    }
    
    NotifyConfigurationRemoved(id);
    return true;
}

UCLaunchConfiguration* UCLaunchConfigManager::GetConfiguration(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = configurations_.find(id);
    return (it != configurations_.end()) ? it->second.get() : nullptr;
}

const UCLaunchConfiguration* UCLaunchConfigManager::GetConfiguration(
    const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = configurations_.find(id);
    return (it != configurations_.end()) ? it->second.get() : nullptr;
}

UCLaunchConfiguration* UCLaunchConfigManager::GetConfigurationByName(
    const std::string& name) {
    for (auto& pair : configurations_) {
        if (pair.second->GetName() == name) {
            return pair.second.get();
        }
    }
    return nullptr;
}

std::vector<UCLaunchConfiguration*> UCLaunchConfigManager::GetAllConfigurations() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<UCLaunchConfiguration*> result;
    result.reserve(configurations_.size());
    
    for (auto& pair : configurations_) {
        result.push_back(pair.second.get());
    }
    
    return result;
}

std::vector<const UCLaunchConfiguration*> UCLaunchConfigManager::GetAllConfigurations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<const UCLaunchConfiguration*> result;
    result.reserve(configurations_.size());
    
    for (const auto& pair : configurations_) {
        result.push_back(pair.second.get());
    }
    
    return result;
}

bool UCLaunchConfigManager::HasConfiguration(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return configurations_.find(id) != configurations_.end();
}

void UCLaunchConfigManager::ClearConfigurations() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    configurations_.clear();
    defaultConfigId_.clear();
    activeConfigId_.clear();
    recentConfigIds_.clear();
}

// ============================================================================
// DEFAULT CONFIGURATION
// ============================================================================

void UCLaunchConfigManager::SetDefaultConfiguration(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = configurations_.find(id);
    if (it == configurations_.end()) return;
    
    // Clear previous default
    for (auto& pair : configurations_) {
        pair.second->SetIsDefault(false);
    }
    
    it->second->SetIsDefault(true);
    defaultConfigId_ = id;
}

UCLaunchConfiguration* UCLaunchConfigManager::GetDefaultConfiguration() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (defaultConfigId_.empty()) return nullptr;
    
    auto it = configurations_.find(defaultConfigId_);
    return (it != configurations_.end()) ? it->second.get() : nullptr;
}

const UCLaunchConfiguration* UCLaunchConfigManager::GetDefaultConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (defaultConfigId_.empty()) return nullptr;
    
    auto it = configurations_.find(defaultConfigId_);
    return (it != configurations_.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// ACTIVE CONFIGURATION
// ============================================================================

void UCLaunchConfigManager::SetActiveConfiguration(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!HasConfiguration(id)) return;
    
    if (activeConfigId_ != id) {
        activeConfigId_ = id;
        NotifyActiveChanged(id);
    }
}

UCLaunchConfiguration* UCLaunchConfigManager::GetActiveConfiguration() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeConfigId_.empty()) return nullptr;
    
    auto it = configurations_.find(activeConfigId_);
    return (it != configurations_.end()) ? it->second.get() : nullptr;
}

const UCLaunchConfiguration* UCLaunchConfigManager::GetActiveConfiguration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (activeConfigId_.empty()) return nullptr;
    
    auto it = configurations_.find(activeConfigId_);
    return (it != configurations_.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// TEMPLATES
// ============================================================================

std::unique_ptr<UCLaunchConfiguration> UCLaunchConfigManager::CreateFromTemplate(
    LaunchTemplateType type) {
    
    auto config = std::make_unique<UCLaunchConfiguration>();
    
    switch (type) {
        case LaunchTemplateType::CppLaunch:
            config->SetName("C++ Launch");
            config->SetType(LaunchConfigType::Launch);
            config->SetDebuggerType(DebuggerType::Auto);
            config->SetProgram("${workspaceFolder}/build/${projectName}");
            config->SetWorkingDirectory("${workspaceFolder}");
            config->SetStopAtEntry(StopAtEntry::Main);
            config->SetPreLaunchAction(PreLaunchAction::BuildIfModified);
            config->SetConsoleType(ConsoleType::Integrated);
            break;
            
        case LaunchTemplateType::CppAttach:
            config->SetName("C++ Attach");
            config->SetType(LaunchConfigType::Attach);
            config->SetDebuggerType(DebuggerType::Auto);
            config->SetDescription("Attach to a running process");
            break;
            
        case LaunchTemplateType::CppRemote:
            config->SetName("C++ Remote");
            config->SetType(LaunchConfigType::Remote);
            config->SetDebuggerType(DebuggerType::GDB);
            config->SetProgram("${workspaceFolder}/build/${projectName}");
            config->GetRemoteSettings().host = "localhost";
            config->GetRemoteSettings().port = 3333;
            config->SetDescription("Remote debugging via gdbserver");
            break;
            
        case LaunchTemplateType::CppCoreDump:
            config->SetName("C++ Core Dump");
            config->SetType(LaunchConfigType::CoreDump);
            config->SetDebuggerType(DebuggerType::Auto);
            config->SetProgram("${workspaceFolder}/build/${projectName}");
            config->SetCoreDumpPath("${workspaceFolder}/core");
            config->SetDescription("Debug a core dump file");
            break;
            
        case LaunchTemplateType::PythonLaunch:
            config->SetName("Python Launch");
            config->SetType(LaunchConfigType::Launch);
            config->SetProgram("python3");
            config->SetArgs("${file}");
            config->SetWorkingDirectory("${workspaceFolder}");
            config->SetConsoleType(ConsoleType::Integrated);
            break;
            
        case LaunchTemplateType::PythonAttach:
            config->SetName("Python Attach");
            config->SetType(LaunchConfigType::Attach);
            config->SetDescription("Attach to Python debugpy");
            break;
            
        case LaunchTemplateType::RustLaunch:
            config->SetName("Rust Launch");
            config->SetType(LaunchConfigType::Launch);
            config->SetDebuggerType(DebuggerType::Auto);
            config->SetProgram("${workspaceFolder}/target/debug/${projectName}");
            config->SetWorkingDirectory("${workspaceFolder}");
            config->SetPreLaunchAction(PreLaunchAction::BuildIfModified);
            break;
            
        case LaunchTemplateType::GoLaunch:
            config->SetName("Go Launch");
            config->SetType(LaunchConfigType::Launch);
            config->SetDebuggerType(DebuggerType::Auto);
            config->SetProgram("${workspaceFolder}/${projectName}");
            config->SetWorkingDirectory("${workspaceFolder}");
            break;
            
        case LaunchTemplateType::NodeLaunch:
            config->SetName("Node.js Launch");
            config->SetType(LaunchConfigType::Launch);
            config->SetProgram("node");
            config->SetArgs("${file}");
            config->SetWorkingDirectory("${workspaceFolder}");
            config->SetConsoleType(ConsoleType::Integrated);
            break;
            
        case LaunchTemplateType::Custom:
        default:
            config->SetName("Custom Configuration");
            config->SetType(LaunchConfigType::Launch);
            break;
    }
    
    return config;
}

std::vector<LaunchTemplateType> UCLaunchConfigManager::GetAvailableTemplates() const {
    return {
        LaunchTemplateType::CppLaunch,
        LaunchTemplateType::CppAttach,
        LaunchTemplateType::CppRemote,
        LaunchTemplateType::CppCoreDump,
        LaunchTemplateType::PythonLaunch,
        LaunchTemplateType::PythonAttach,
        LaunchTemplateType::RustLaunch,
        LaunchTemplateType::GoLaunch,
        LaunchTemplateType::NodeLaunch,
        LaunchTemplateType::Custom
    };
}

std::string UCLaunchConfigManager::GetTemplateName(LaunchTemplateType type) {
    switch (type) {
        case LaunchTemplateType::CppLaunch: return "C++ (Launch)";
        case LaunchTemplateType::CppAttach: return "C++ (Attach)";
        case LaunchTemplateType::CppRemote: return "C++ (Remote)";
        case LaunchTemplateType::CppCoreDump: return "C++ (Core Dump)";
        case LaunchTemplateType::PythonLaunch: return "Python (Launch)";
        case LaunchTemplateType::PythonAttach: return "Python (Attach)";
        case LaunchTemplateType::RustLaunch: return "Rust (Launch)";
        case LaunchTemplateType::GoLaunch: return "Go (Launch)";
        case LaunchTemplateType::NodeLaunch: return "Node.js (Launch)";
        case LaunchTemplateType::Custom: return "Custom Configuration";
    }
    return "Unknown";
}

std::string UCLaunchConfigManager::GetTemplateDescription(LaunchTemplateType type) {
    switch (type) {
        case LaunchTemplateType::CppLaunch:
            return "Launch and debug a C/C++ program";
        case LaunchTemplateType::CppAttach:
            return "Attach to a running C/C++ process";
        case LaunchTemplateType::CppRemote:
            return "Connect to a remote gdbserver";
        case LaunchTemplateType::CppCoreDump:
            return "Debug a core dump file";
        case LaunchTemplateType::PythonLaunch:
            return "Launch and debug a Python script";
        case LaunchTemplateType::PythonAttach:
            return "Attach to a running Python process";
        case LaunchTemplateType::RustLaunch:
            return "Launch and debug a Rust program";
        case LaunchTemplateType::GoLaunch:
            return "Launch and debug a Go program";
        case LaunchTemplateType::NodeLaunch:
            return "Launch and debug a Node.js application";
        case LaunchTemplateType::Custom:
            return "Create a custom debug configuration";
    }
    return "";
}

// ============================================================================
// PERSISTENCE
// ============================================================================

bool UCLaunchConfigManager::SaveToFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    
    // Write JSON format similar to VS Code launch.json
    file << "{\n";
    file << "  \"version\": \"0.2.0\",\n";
    file << "  \"configurations\": [\n";
    
    bool first = true;
    for (const auto& pair : configurations_) {
        if (!first) file << ",\n";
        first = false;
        
        // Indent each line of the config JSON
        std::string configJson = pair.second->ToJson();
        std::istringstream iss(configJson);
        std::string line;
        bool firstLine = true;
        while (std::getline(iss, line)) {
            if (!firstLine) file << "\n";
            file << "    " << line;
            firstLine = false;
        }
    }
    
    file << "\n  ]\n";
    file << "}\n";
    
    return true;
}

bool UCLaunchConfigManager::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // TODO: Parse JSON content and create configurations
    // This would use a JSON library in production
    
    if (onConfigurationsLoaded) {
        onConfigurationsLoaded();
    }
    
    return true;
}

std::string UCLaunchConfigManager::GetDefaultConfigPath() const {
    if (projectDirectory_.empty()) return "launch.json";
    
    return projectDirectory_ + "/.ultraide/launch.json";
}

// ============================================================================
// RECENT CONFIGURATIONS
// ============================================================================

void UCLaunchConfigManager::AddToRecent(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Remove if already exists
    recentConfigIds_.erase(
        std::remove(recentConfigIds_.begin(), recentConfigIds_.end(), id),
        recentConfigIds_.end());
    
    // Add to front
    recentConfigIds_.insert(recentConfigIds_.begin(), id);
    
    // Trim to max size
    if (recentConfigIds_.size() > MAX_RECENT) {
        recentConfigIds_.resize(MAX_RECENT);
    }
}

std::vector<std::string> UCLaunchConfigManager::GetRecentConfigurations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Filter out removed configurations
    std::vector<std::string> valid;
    for (const auto& id : recentConfigIds_) {
        if (configurations_.find(id) != configurations_.end()) {
            valid.push_back(id);
        }
    }
    return valid;
}

// ============================================================================
// VALIDATION
// ============================================================================

std::map<std::string, std::string> UCLaunchConfigManager::ValidateAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, std::string> errors;
    
    for (const auto& pair : configurations_) {
        if (!pair.second->IsValid()) {
            errors[pair.first] = pair.second->GetValidationError();
        }
    }
    
    return errors;
}

// ============================================================================
// DUPLICATE
// ============================================================================

std::string UCLaunchConfigManager::DuplicateConfiguration(const std::string& id) {
    auto original = GetConfiguration(id);
    if (!original) return "";
    
    auto clone = original->Clone();
    return AddConfiguration(std::move(clone));
}

// ============================================================================
// NOTIFICATIONS
// ============================================================================

void UCLaunchConfigManager::NotifyConfigurationAdded(const std::string& id) {
    if (onConfigurationAdded) {
        onConfigurationAdded(id);
    }
}

void UCLaunchConfigManager::NotifyConfigurationRemoved(const std::string& id) {
    if (onConfigurationRemoved) {
        onConfigurationRemoved(id);
    }
}

void UCLaunchConfigManager::NotifyConfigurationChanged(const std::string& id) {
    if (onConfigurationChanged) {
        onConfigurationChanged(id);
    }
}

void UCLaunchConfigManager::NotifyActiveChanged(const std::string& id) {
    if (onActiveConfigurationChanged) {
        onActiveConfigurationChanged(id);
    }
}

} // namespace IDE
} // namespace UltraCanvas
