// Apps/IDE/Debug/Launch/UCLaunchConfigDialog.cpp
// Launch Configuration Dialog Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCLaunchConfigDialog.h"
#include "UltraCanvasFileDialog.h"
#include <sstream>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// UCLaunchConfigDialog IMPLEMENTATION
// ============================================================================

UCLaunchConfigDialog::UCLaunchConfigDialog(const std::string& id, int width, int height)
    : UltraCanvasDialog(id, 0, 0, 0, width, height) {
    
    SetTitle("Debug Configurations");
    SetResizable(true);
    SetMinSize(600, 450);
}

void UCLaunchConfigDialog::Initialize() {
    CreateUI();
    PopulateConfigList();
}

void UCLaunchConfigDialog::SetConfigManager(UCLaunchConfigManager* manager) {
    configManager_ = manager;
    if (configManager_) PopulateConfigList();
}

void UCLaunchConfigDialog::CreateUI() {
    CreateConfigListPanel();
    CreateTabPanel();
    CreateButtonPanel();
}

void UCLaunchConfigDialog::CreateConfigListPanel() {
    leftPanel_ = std::make_shared<UltraCanvasContainer>(
        "left_panel", 0, 8, 40, 200, GetHeight() - 100);
    leftPanel_->SetBackgroundColor(Color(245, 245, 245));
    
    configListTree_ = std::make_shared<UltraCanvasTreeView>(
        "config_list", 0, 4, 4, 192, leftPanel_->GetHeight() - 40);
    configListTree_->SetShowExpandButtons(false);
    configListTree_->SetShowIcons(true);
    configListTree_->SetRowHeight(24);
    configListTree_->SetSelectionMode(TreeSelectionMode::Single);
    
    configListTree_->onSelectionChange = [this](const std::vector<TreeNodeId>& selected) {
        if (!selected.empty()) {
            auto it = nodeToConfigId_.find(selected[0]);
            if (it != nodeToConfigId_.end()) SelectConfiguration(it->second);
        }
    };
    leftPanel_->AddChild(configListTree_);
    
    int btnY = leftPanel_->GetHeight() - 32;
    addButton_ = std::make_shared<UltraCanvasButton>("add_config", 0, 4, btnY, 60, 24);
    addButton_->SetText("Add");
    addButton_->SetOnClick([this]() { OnAddConfiguration(); });
    leftPanel_->AddChild(addButton_);
    
    removeButton_ = std::make_shared<UltraCanvasButton>("remove_config", 0, 68, btnY, 60, 24);
    removeButton_->SetText("Remove");
    removeButton_->SetOnClick([this]() { OnRemoveConfiguration(); });
    leftPanel_->AddChild(removeButton_);
    
    duplicateButton_ = std::make_shared<UltraCanvasButton>("dup_config", 0, 132, btnY, 60, 24);
    duplicateButton_->SetText("Copy");
    duplicateButton_->SetOnClick([this]() { OnDuplicateConfiguration(); });
    leftPanel_->AddChild(duplicateButton_);
    
    AddChild(leftPanel_);
}

void UCLaunchConfigDialog::CreateTabPanel() {
    rightPanel_ = std::make_shared<UltraCanvasContainer>(
        "right_panel", 0, 216, 40, GetWidth() - 224, GetHeight() - 100);
    
    tabControl_ = std::make_shared<UltraCanvasTabControl>(
        "config_tabs", 0, 0, 0, rightPanel_->GetWidth(), rightPanel_->GetHeight());
    
    CreateGeneralTab();
    CreateEnvironmentTab();
    CreateDebuggerTab();
    CreateAdvancedTab();
    
    rightPanel_->AddChild(tabControl_);
    AddChild(rightPanel_);
}

void UCLaunchConfigDialog::CreateGeneralTab() {
    generalTab_ = std::make_shared<UltraCanvasContainer>(
        "general_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    int y = 8, labelWidth = 100, inputWidth = generalTab_->GetWidth() - labelWidth - 50;
    
    auto nameLabel = std::make_shared<UltraCanvasLabel>("name_label", 0, 8, y + 4, labelWidth, 20);
    nameLabel->SetText("Name:");
    generalTab_->AddChild(nameLabel);
    nameTextBox_ = std::make_shared<UltraCanvasTextBox>("name_input", 0, labelWidth + 16, y, inputWidth, 24);
    generalTab_->AddChild(nameTextBox_);
    y += 32;
    
    auto typeLabel = std::make_shared<UltraCanvasLabel>("type_label", 0, 8, y + 4, labelWidth, 20);
    typeLabel->SetText("Type:");
    generalTab_->AddChild(typeLabel);
    typeDropDown_ = std::make_shared<UltraCanvasDropDown>("type_dropdown", 0, labelWidth + 16, y, 180, 24);
    typeDropDown_->AddItem("Launch");
    typeDropDown_->AddItem("Attach to Process");
    typeDropDown_->AddItem("Attach by Name");
    typeDropDown_->AddItem("Remote");
    typeDropDown_->AddItem("Core Dump");
    typeDropDown_->SetOnSelectionChange([this](int index, const std::string&) {
        UpdateUIForLaunchType(static_cast<LaunchConfigType>(index));
    });
    generalTab_->AddChild(typeDropDown_);
    y += 32;
    
    auto programLabel = std::make_shared<UltraCanvasLabel>("program_label", 0, 8, y + 4, labelWidth, 20);
    programLabel->SetText("Program:");
    generalTab_->AddChild(programLabel);
    programTextBox_ = std::make_shared<UltraCanvasTextBox>("program_input", 0, labelWidth + 16, y, inputWidth - 32, 24);
    programTextBox_->SetPlaceholder("Path to executable");
    generalTab_->AddChild(programTextBox_);
    programBrowseButton_ = std::make_shared<UltraCanvasButton>("program_browse", 0, labelWidth + inputWidth - 8, y, 28, 24);
    programBrowseButton_->SetText("...");
    generalTab_->AddChild(programBrowseButton_);
    y += 32;
    
    auto argsLabel = std::make_shared<UltraCanvasLabel>("args_label", 0, 8, y + 4, labelWidth, 20);
    argsLabel->SetText("Arguments:");
    generalTab_->AddChild(argsLabel);
    argsTextBox_ = std::make_shared<UltraCanvasTextBox>("args_input", 0, labelWidth + 16, y, inputWidth, 24);
    argsTextBox_->SetPlaceholder("Command line arguments");
    generalTab_->AddChild(argsTextBox_);
    y += 32;
    
    auto workDirLabel = std::make_shared<UltraCanvasLabel>("workdir_label", 0, 8, y + 4, labelWidth, 20);
    workDirLabel->SetText("Working Dir:");
    generalTab_->AddChild(workDirLabel);
    workingDirTextBox_ = std::make_shared<UltraCanvasTextBox>("workdir_input", 0, labelWidth + 16, y, inputWidth - 32, 24);
    generalTab_->AddChild(workingDirTextBox_);
    workingDirBrowseButton_ = std::make_shared<UltraCanvasButton>("workdir_browse", 0, labelWidth + inputWidth - 8, y, 28, 24);
    workingDirBrowseButton_->SetText("...");
    generalTab_->AddChild(workingDirBrowseButton_);
    y += 32;
    
    auto consoleLabel = std::make_shared<UltraCanvasLabel>("console_label", 0, 8, y + 4, labelWidth, 20);
    consoleLabel->SetText("Console:");
    generalTab_->AddChild(consoleLabel);
    consoleDropDown_ = std::make_shared<UltraCanvasDropDown>("console_dropdown", 0, labelWidth + 16, y, 180, 24);
    consoleDropDown_->AddItem("Integrated Terminal");
    consoleDropDown_->AddItem("External Terminal");
    consoleDropDown_->AddItem("None");
    generalTab_->AddChild(consoleDropDown_);
    y += 32;
    
    stopAtEntryCheckbox_ = std::make_shared<UltraCanvasCheckbox>("stop_entry_check", 0, labelWidth + 16, y, 200, 24);
    stopAtEntryCheckbox_->SetText("Stop at program entry (main)");
    generalTab_->AddChild(stopAtEntryCheckbox_);
    y += 32;
    
    // Attach panel
    attachPanel_ = std::make_shared<UltraCanvasContainer>("attach_panel", 0, 0, y, generalTab_->GetWidth(), 80);
    attachPanel_->SetVisible(false);
    auto pidLabel = std::make_shared<UltraCanvasLabel>("pid_label", 0, 8, 4, labelWidth, 20);
    pidLabel->SetText("Process ID:");
    attachPanel_->AddChild(pidLabel);
    processIdTextBox_ = std::make_shared<UltraCanvasTextBox>("pid_input", 0, labelWidth + 16, 0, 100, 24);
    attachPanel_->AddChild(processIdTextBox_);
    selectProcessButton_ = std::make_shared<UltraCanvasButton>("select_process", 0, labelWidth + 124, 0, 100, 24);
    selectProcessButton_->SetText("Select...");
    attachPanel_->AddChild(selectProcessButton_);
    auto pnameLabel = std::make_shared<UltraCanvasLabel>("pname_label", 0, 8, 36, labelWidth, 20);
    pnameLabel->SetText("Process Name:");
    attachPanel_->AddChild(pnameLabel);
    processNameTextBox_ = std::make_shared<UltraCanvasTextBox>("pname_input", 0, labelWidth + 16, 32, inputWidth, 24);
    attachPanel_->AddChild(processNameTextBox_);
    generalTab_->AddChild(attachPanel_);
    
    // Remote panel
    remotePanel_ = std::make_shared<UltraCanvasContainer>("remote_panel", 0, 0, y, generalTab_->GetWidth(), 50);
    remotePanel_->SetVisible(false);
    auto hostLabel = std::make_shared<UltraCanvasLabel>("host_label", 0, 8, 4, labelWidth, 20);
    hostLabel->SetText("Host:");
    remotePanel_->AddChild(hostLabel);
    hostTextBox_ = std::make_shared<UltraCanvasTextBox>("host_input", 0, labelWidth + 16, 0, 200, 24);
    hostTextBox_->SetText("localhost");
    remotePanel_->AddChild(hostTextBox_);
    auto portLabel = std::make_shared<UltraCanvasLabel>("port_label", 0, 240, 4, 40, 20);
    portLabel->SetText("Port:");
    remotePanel_->AddChild(portLabel);
    portTextBox_ = std::make_shared<UltraCanvasTextBox>("port_input", 0, 290, 0, 80, 24);
    portTextBox_->SetText("3333");
    remotePanel_->AddChild(portTextBox_);
    generalTab_->AddChild(remotePanel_);
    
    // Core dump panel
    coreDumpPanel_ = std::make_shared<UltraCanvasContainer>("coredump_panel", 0, 0, y, generalTab_->GetWidth(), 50);
    coreDumpPanel_->SetVisible(false);
    auto coreLabel = std::make_shared<UltraCanvasLabel>("core_label", 0, 8, 4, labelWidth, 20);
    coreLabel->SetText("Core File:");
    coreDumpPanel_->AddChild(coreLabel);
    coreDumpPathTextBox_ = std::make_shared<UltraCanvasTextBox>("core_input", 0, labelWidth + 16, 0, inputWidth - 32, 24);
    coreDumpPanel_->AddChild(coreDumpPathTextBox_);
    coreDumpBrowseButton_ = std::make_shared<UltraCanvasButton>("core_browse", 0, labelWidth + inputWidth - 8, 0, 28, 24);
    coreDumpBrowseButton_->SetText("...");
    coreDumpPanel_->AddChild(coreDumpBrowseButton_);
    generalTab_->AddChild(coreDumpPanel_);
    
    tabControl_->AddTab("General", generalTab_);
}

void UCLaunchConfigDialog::CreateEnvironmentTab() {
    envTab_ = std::make_shared<UltraCanvasContainer>("env_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    inheritEnvCheckbox_ = std::make_shared<UltraCanvasCheckbox>("inherit_env", 0, 8, 8, 250, 24);
    inheritEnvCheckbox_->SetText("Inherit system environment");
    inheritEnvCheckbox_->SetChecked(true);
    envTab_->AddChild(inheritEnvCheckbox_);
    
    auto envLabel = std::make_shared<UltraCanvasLabel>("env_label", 0, 8, 40, 200, 20);
    envLabel->SetText("Environment Variables:");
    envTab_->AddChild(envLabel);
    
    envVarsTree_ = std::make_shared<UltraCanvasTreeView>("env_tree", 0, 8, 64, envTab_->GetWidth() - 80, envTab_->GetHeight() - 80);
    envVarsTree_->SetShowExpandButtons(false);
    envVarsTree_->SetRowHeight(22);
    envTab_->AddChild(envVarsTree_);
    
    addEnvButton_ = std::make_shared<UltraCanvasButton>("add_env", 0, envTab_->GetWidth() - 64, 64, 56, 24);
    addEnvButton_->SetText("Add");
    envTab_->AddChild(addEnvButton_);
    
    removeEnvButton_ = std::make_shared<UltraCanvasButton>("remove_env", 0, envTab_->GetWidth() - 64, 92, 56, 24);
    removeEnvButton_->SetText("Remove");
    envTab_->AddChild(removeEnvButton_);
    
    tabControl_->AddTab("Environment", envTab_);
}

void UCLaunchConfigDialog::CreateDebuggerTab() {
    debuggerTab_ = std::make_shared<UltraCanvasContainer>("debugger_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    int y = 8, labelWidth = 120, inputWidth = debuggerTab_->GetWidth() - labelWidth - 40;
    
    auto typeLabel = std::make_shared<UltraCanvasLabel>("dbg_type_label", 0, 8, y + 4, labelWidth, 20);
    typeLabel->SetText("Debugger:");
    debuggerTab_->AddChild(typeLabel);
    debuggerTypeDropDown_ = std::make_shared<UltraCanvasDropDown>("dbg_type_dropdown", 0, labelWidth + 16, y, 150, 24);
    debuggerTypeDropDown_->AddItem("Auto Detect");
    debuggerTypeDropDown_->AddItem("GDB");
    debuggerTypeDropDown_->AddItem("LLDB");
    debuggerTypeDropDown_->AddItem("Custom");
    debuggerTypeDropDown_->SetOnSelectionChange([this](int index, const std::string&) {
        UpdateDebuggerOptions(static_cast<DebuggerType>(index));
    });
    debuggerTab_->AddChild(debuggerTypeDropDown_);
    y += 32;
    
    auto pathLabel = std::make_shared<UltraCanvasLabel>("dbg_path_label", 0, 8, y + 4, labelWidth, 20);
    pathLabel->SetText("Debugger Path:");
    debuggerTab_->AddChild(pathLabel);
    debuggerPathTextBox_ = std::make_shared<UltraCanvasTextBox>("dbg_path_input", 0, labelWidth + 16, y, inputWidth - 32, 24);
    debuggerPathTextBox_->SetPlaceholder("Leave empty for auto-detect");
    debuggerTab_->AddChild(debuggerPathTextBox_);
    debuggerBrowseButton_ = std::make_shared<UltraCanvasButton>("dbg_browse", 0, labelWidth + inputWidth - 8, y, 28, 24);
    debuggerBrowseButton_->SetText("...");
    debuggerTab_->AddChild(debuggerBrowseButton_);
    y += 32;
    
    auto argsLabel = std::make_shared<UltraCanvasLabel>("dbg_args_label", 0, 8, y + 4, labelWidth, 20);
    argsLabel->SetText("Extra Arguments:");
    debuggerTab_->AddChild(argsLabel);
    debuggerArgsTextBox_ = std::make_shared<UltraCanvasTextBox>("dbg_args_input", 0, labelWidth + 16, y, inputWidth, 24);
    debuggerTab_->AddChild(debuggerArgsTextBox_);
    y += 32;
    
    prettyPrintingCheckbox_ = std::make_shared<UltraCanvasCheckbox>("pretty_print", 0, labelWidth + 16, y, 200, 24);
    prettyPrintingCheckbox_->SetText("Enable pretty printing");
    prettyPrintingCheckbox_->SetChecked(true);
    debuggerTab_->AddChild(prettyPrintingCheckbox_);
    y += 32;
    
    auto initLabel = std::make_shared<UltraCanvasLabel>("init_label", 0, 8, y + 4, labelWidth, 20);
    initLabel->SetText("Init Commands:");
    debuggerTab_->AddChild(initLabel);
    initCommandsTextBox_ = std::make_shared<UltraCanvasTextBox>("init_commands", 0, labelWidth + 16, y, inputWidth, 80);
    initCommandsTextBox_->SetPlaceholder("Commands to run at debugger startup");
    debuggerTab_->AddChild(initCommandsTextBox_);
    
    tabControl_->AddTab("Debugger", debuggerTab_);
}

void UCLaunchConfigDialog::CreateAdvancedTab() {
    advancedTab_ = std::make_shared<UltraCanvasContainer>("advanced_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    int y = 8, labelWidth = 120, inputWidth = advancedTab_->GetWidth() - labelWidth - 40;
    
    auto preLaunchLabel = std::make_shared<UltraCanvasLabel>("prelaunch_label", 0, 8, y + 4, labelWidth, 20);
    preLaunchLabel->SetText("Pre-launch:");
    advancedTab_->AddChild(preLaunchLabel);
    preLaunchDropDown_ = std::make_shared<UltraCanvasDropDown>("prelaunch_dropdown", 0, labelWidth + 16, y, 180, 24);
    preLaunchDropDown_->AddItem("None");
    preLaunchDropDown_->AddItem("Build");
    preLaunchDropDown_->AddItem("Build if modified");
    preLaunchDropDown_->AddItem("Custom task");
    preLaunchDropDown_->SetSelectedIndex(2);
    advancedTab_->AddChild(preLaunchDropDown_);
    y += 32;
    
    auto taskLabel = std::make_shared<UltraCanvasLabel>("task_label", 0, 8, y + 4, labelWidth, 20);
    taskLabel->SetText("Task Name:");
    advancedTab_->AddChild(taskLabel);
    preLaunchTaskTextBox_ = std::make_shared<UltraCanvasTextBox>("task_input", 0, labelWidth + 16, y, inputWidth, 24);
    preLaunchTaskTextBox_->SetPlaceholder("Build task name");
    advancedTab_->AddChild(preLaunchTaskTextBox_);
    y += 32;
    
    auto symbolLabel = std::make_shared<UltraCanvasLabel>("symbol_label", 0, 8, y + 4, labelWidth, 20);
    symbolLabel->SetText("Symbol File:");
    advancedTab_->AddChild(symbolLabel);
    symbolFileTextBox_ = std::make_shared<UltraCanvasTextBox>("symbol_input", 0, labelWidth + 16, y, inputWidth, 24);
    symbolFileTextBox_->SetPlaceholder("Separate symbol file (optional)");
    advancedTab_->AddChild(symbolFileTextBox_);
    y += 32;
    
    auto sourceLabel = std::make_shared<UltraCanvasLabel>("source_label", 0, 8, y + 4, labelWidth, 20);
    sourceLabel->SetText("Source Paths:");
    advancedTab_->AddChild(sourceLabel);
    sourcePathsTree_ = std::make_shared<UltraCanvasTreeView>("source_paths", 0, labelWidth + 16, y, inputWidth, 100);
    sourcePathsTree_->SetShowExpandButtons(false);
    sourcePathsTree_->SetRowHeight(20);
    advancedTab_->AddChild(sourcePathsTree_);
    
    tabControl_->AddTab("Advanced", advancedTab_);
}

void UCLaunchConfigDialog::CreateButtonPanel() {
    buttonPanel_ = std::make_shared<UltraCanvasContainer>("button_panel", 0, 0, GetHeight() - 48, GetWidth(), 48);
    buttonPanel_->SetBackgroundColor(Color(240, 240, 240));
    
    int btnWidth = 80, btnHeight = 28, btnY = 10;
    int rightX = GetWidth() - 16;
    
    cancelButton_ = std::make_shared<UltraCanvasButton>("cancel_btn", 0, rightX - btnWidth, btnY, btnWidth, btnHeight);
    cancelButton_->SetText("Cancel");
    cancelButton_->SetOnClick([this]() { OnCancel(); });
    buttonPanel_->AddChild(cancelButton_);
    rightX -= btnWidth + 8;
    
    applyButton_ = std::make_shared<UltraCanvasButton>("apply_btn", 0, rightX - btnWidth, btnY, btnWidth, btnHeight);
    applyButton_->SetText("Apply");
    applyButton_->SetOnClick([this]() { OnApply(); });
    buttonPanel_->AddChild(applyButton_);
    rightX -= btnWidth + 8;
    
    okButton_ = std::make_shared<UltraCanvasButton>("ok_btn", 0, rightX - btnWidth, btnY, btnWidth, btnHeight);
    okButton_->SetText("OK");
    okButton_->SetOnClick([this]() { OnOK(); });
    buttonPanel_->AddChild(okButton_);
    
    AddChild(buttonPanel_);
}

void UCLaunchConfigDialog::PopulateConfigList() {
    if (!configListTree_ || !configManager_) return;
    configListTree_->Clear();
    nodeToConfigId_.clear();
    
    TreeNodeId nodeId = 1;
    for (auto* config : configManager_->GetAllConfigurations()) {
        TreeNodeData nodeData;
        nodeData.text = config->GetName();
        nodeData.leftIcon.iconPath = "icons/debug_launch.png";
        nodeData.leftIcon.width = 16;
        nodeData.leftIcon.height = 16;
        nodeData.leftIcon.visible = true;
        if (config->IsDefault()) nodeData.text += " (default)";
        configListTree_->AddNode(nodeId, nodeData);
        nodeToConfigId_[nodeId] = config->GetId();
        nodeId++;
    }
}

void UCLaunchConfigDialog::SelectConfiguration(const std::string& id) {
    if (!configManager_) return;
    SaveUIToConfig();
    auto* config = configManager_->GetConfiguration(id);
    if (!config) return;
    editingConfig_ = config->Clone();
    originalConfigId_ = id;
    isNewConfig_ = false;
    LoadConfigToUI();
}

void UCLaunchConfigDialog::EditConfiguration(const std::string& configId) {
    if (!configManager_) return;
    auto* config = configManager_->GetConfiguration(configId);
    if (!config) return;
    editingConfig_ = config->Clone();
    originalConfigId_ = configId;
    isNewConfig_ = false;
    LoadConfigToUI();
    for (const auto& pair : nodeToConfigId_) {
        if (pair.second == configId) {
            configListTree_->SetSelectedNode(pair.first);
            break;
        }
    }
}

void UCLaunchConfigDialog::CreateNewConfiguration(LaunchTemplateType templateType) {
    if (!configManager_) return;
    editingConfig_ = configManager_->CreateFromTemplate(templateType);
    originalConfigId_.clear();
    isNewConfig_ = true;
    LoadConfigToUI();
}

void UCLaunchConfigDialog::LoadConfigToUI() {
    if (!editingConfig_) return;
    if (nameTextBox_) nameTextBox_->SetText(editingConfig_->GetName());
    if (typeDropDown_) typeDropDown_->SetSelectedIndex(static_cast<int>(editingConfig_->GetType()));
    if (programTextBox_) programTextBox_->SetText(editingConfig_->GetProgram());
    if (argsTextBox_) argsTextBox_->SetText(editingConfig_->GetArgs());
    if (workingDirTextBox_) workingDirTextBox_->SetText(editingConfig_->GetWorkingDirectory());
    if (consoleDropDown_) consoleDropDown_->SetSelectedIndex(static_cast<int>(editingConfig_->GetConsoleType()));
    if (stopAtEntryCheckbox_) stopAtEntryCheckbox_->SetChecked(editingConfig_->GetStopAtEntry() != StopAtEntry::No);
    if (processIdTextBox_) processIdTextBox_->SetText(std::to_string(editingConfig_->GetProcessId()));
    if (processNameTextBox_) processNameTextBox_->SetText(editingConfig_->GetProcessName());
    if (hostTextBox_) hostTextBox_->SetText(editingConfig_->GetRemoteSettings().host);
    if (portTextBox_) portTextBox_->SetText(std::to_string(editingConfig_->GetRemoteSettings().port));
    if (coreDumpPathTextBox_) coreDumpPathTextBox_->SetText(editingConfig_->GetCoreDumpPath());
    if (inheritEnvCheckbox_) inheritEnvCheckbox_->SetChecked(editingConfig_->GetInheritEnvironment());
    if (debuggerTypeDropDown_) debuggerTypeDropDown_->SetSelectedIndex(static_cast<int>(editingConfig_->GetDebuggerType()));
    if (debuggerPathTextBox_) debuggerPathTextBox_->SetText(editingConfig_->GetGDBSettings().gdbPath);
    if (debuggerArgsTextBox_) debuggerArgsTextBox_->SetText(editingConfig_->GetGDBSettings().gdbArgs);
    if (prettyPrintingCheckbox_) prettyPrintingCheckbox_->SetChecked(editingConfig_->GetGDBSettings().prettyPrinting);
    if (initCommandsTextBox_) initCommandsTextBox_->SetText(editingConfig_->GetGDBSettings().initCommands);
    if (preLaunchDropDown_) preLaunchDropDown_->SetSelectedIndex(static_cast<int>(editingConfig_->GetPreLaunchAction()));
    if (preLaunchTaskTextBox_) preLaunchTaskTextBox_->SetText(editingConfig_->GetPreLaunchTask());
    if (symbolFileTextBox_) symbolFileTextBox_->SetText(editingConfig_->GetSymbolFile());
    UpdateUIForLaunchType(editingConfig_->GetType());
}

void UCLaunchConfigDialog::SaveUIToConfig() {
    if (!editingConfig_) return;
    if (nameTextBox_) editingConfig_->SetName(nameTextBox_->GetText());
    if (typeDropDown_) editingConfig_->SetType(static_cast<LaunchConfigType>(typeDropDown_->GetSelectedIndex()));
    if (programTextBox_) editingConfig_->SetProgram(programTextBox_->GetText());
    if (argsTextBox_) editingConfig_->SetArgs(argsTextBox_->GetText());
    if (workingDirTextBox_) editingConfig_->SetWorkingDirectory(workingDirTextBox_->GetText());
    if (consoleDropDown_) editingConfig_->SetConsoleType(static_cast<ConsoleType>(consoleDropDown_->GetSelectedIndex()));
    if (stopAtEntryCheckbox_) editingConfig_->SetStopAtEntry(stopAtEntryCheckbox_->IsChecked() ? StopAtEntry::Main : StopAtEntry::No);
    if (processIdTextBox_) { try { editingConfig_->SetProcessId(std::stoi(processIdTextBox_->GetText())); } catch (...) {} }
    if (processNameTextBox_) editingConfig_->SetProcessName(processNameTextBox_->GetText());
    if (hostTextBox_) editingConfig_->GetRemoteSettings().host = hostTextBox_->GetText();
    if (portTextBox_) { try { editingConfig_->GetRemoteSettings().port = std::stoi(portTextBox_->GetText()); } catch (...) {} }
    if (coreDumpPathTextBox_) editingConfig_->SetCoreDumpPath(coreDumpPathTextBox_->GetText());
    if (inheritEnvCheckbox_) editingConfig_->SetInheritEnvironment(inheritEnvCheckbox_->IsChecked());
    if (debuggerTypeDropDown_) editingConfig_->SetDebuggerType(static_cast<DebuggerType>(debuggerTypeDropDown_->GetSelectedIndex()));
    if (debuggerPathTextBox_) editingConfig_->GetGDBSettings().gdbPath = debuggerPathTextBox_->GetText();
    if (debuggerArgsTextBox_) editingConfig_->GetGDBSettings().gdbArgs = debuggerArgsTextBox_->GetText();
    if (prettyPrintingCheckbox_) editingConfig_->GetGDBSettings().prettyPrinting = prettyPrintingCheckbox_->IsChecked();
    if (initCommandsTextBox_) editingConfig_->GetGDBSettings().initCommands = initCommandsTextBox_->GetText();
    if (preLaunchDropDown_) editingConfig_->SetPreLaunchAction(static_cast<PreLaunchAction>(preLaunchDropDown_->GetSelectedIndex()));
    if (preLaunchTaskTextBox_) editingConfig_->SetPreLaunchTask(preLaunchTaskTextBox_->GetText());
    if (symbolFileTextBox_) editingConfig_->SetSymbolFile(symbolFileTextBox_->GetText());
}

bool UCLaunchConfigDialog::ValidateInput() {
    if (!editingConfig_) return false;
    SaveUIToConfig();
    return editingConfig_->IsValid();
}

void UCLaunchConfigDialog::OnAddConfiguration() { CreateNewConfiguration(LaunchTemplateType::CppLaunch); }
void UCLaunchConfigDialog::OnRemoveConfiguration() {
    if (originalConfigId_.empty() || !configManager_) return;
    configManager_->RemoveConfiguration(originalConfigId_);
    editingConfig_.reset();
    originalConfigId_.clear();
    PopulateConfigList();
}
void UCLaunchConfigDialog::OnDuplicateConfiguration() {
    if (originalConfigId_.empty() || !configManager_) return;
    std::string newId = configManager_->DuplicateConfiguration(originalConfigId_);
    PopulateConfigList();
    if (!newId.empty()) EditConfiguration(newId);
}
void UCLaunchConfigDialog::OnOK() { if (!ValidateInput()) return; OnApply(); result_ = LaunchConfigDialogResult::OK; Close(); }
void UCLaunchConfigDialog::OnCancel() { result_ = LaunchConfigDialogResult::Cancel; Close(); }
void UCLaunchConfigDialog::OnApply() {
    if (!configManager_ || !editingConfig_) return;
    SaveUIToConfig();
    if (isNewConfig_) {
        std::string newId = configManager_->AddConfiguration(std::move(editingConfig_));
        editingConfig_ = configManager_->GetConfiguration(newId)->Clone();
        originalConfigId_ = newId;
        isNewConfig_ = false;
        PopulateConfigList();
    } else {
        auto* existing = configManager_->GetConfiguration(originalConfigId_);
        if (existing) *existing = *editingConfig_;
    }
    if (onConfigurationSaved) onConfigurationSaved(editingConfig_.get());
    result_ = LaunchConfigDialogResult::Apply;
}

void UCLaunchConfigDialog::UpdateUIForLaunchType(LaunchConfigType type) {
    bool showProgram = (type == LaunchConfigType::Launch || type == LaunchConfigType::CoreDump);
    bool showAttach = (type == LaunchConfigType::Attach || type == LaunchConfigType::AttachByName);
    bool showRemote = (type == LaunchConfigType::Remote);
    bool showCoreDump = (type == LaunchConfigType::CoreDump);
    if (programTextBox_) programTextBox_->SetVisible(showProgram || showRemote);
    if (argsTextBox_) argsTextBox_->SetVisible(type == LaunchConfigType::Launch);
    if (workingDirTextBox_) workingDirTextBox_->SetVisible(type == LaunchConfigType::Launch);
    if (attachPanel_) attachPanel_->SetVisible(showAttach);
    if (remotePanel_) remotePanel_->SetVisible(showRemote);
    if (coreDumpPanel_) coreDumpPanel_->SetVisible(showCoreDump);
}

void UCLaunchConfigDialog::UpdateDebuggerOptions(DebuggerType type) {
    bool showPath = (type != DebuggerType::Auto);
    if (debuggerPathTextBox_) {
        debuggerPathTextBox_->SetVisible(showPath);
        debuggerPathTextBox_->SetPlaceholder(type == DebuggerType::Auto ? "Auto-detected" : "Path to debugger");
    }
}

LaunchConfigDialogResult UCLaunchConfigDialog::ShowModal() {
    result_ = LaunchConfigDialogResult::None;
    UltraCanvasDialog::ShowModal();
    return result_;
}

void UCLaunchConfigDialog::HandleRender(IRenderContext* ctx) { UltraCanvasDialog::HandleRender(ctx); }

// ============================================================================
// UCLaunchConfigSelector IMPLEMENTATION
// ============================================================================

UCLaunchConfigSelector::UCLaunchConfigSelector(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {}

void UCLaunchConfigSelector::Initialize() { CreateUI(); }

void UCLaunchConfigSelector::SetConfigManager(UCLaunchConfigManager* manager) {
    configManager_ = manager;
    if (configManager_) {
        configManager_->onConfigurationAdded = [this](const std::string&) { Refresh(); };
        configManager_->onConfigurationRemoved = [this](const std::string&) { Refresh(); };
        configManager_->onConfigurationChanged = [this](const std::string&) { Refresh(); };
        Refresh();
    }
}

void UCLaunchConfigSelector::CreateUI() {
    configDropDown_ = std::make_shared<UltraCanvasDropDown>("config_select", 0, 0, 0, GetWidth() - 32, GetHeight());
    configDropDown_->SetOnSelectionChange([this](int index, const std::string&) {
        if (index >= 0 && index < static_cast<int>(configIds_.size()) && onConfigurationSelected)
            onConfigurationSelected(configIds_[index]);
    });
    AddChild(configDropDown_);
    
    editButton_ = std::make_shared<UltraCanvasButton>("edit_configs", 0, GetWidth() - 28, 0, 28, GetHeight());
    editButton_->SetText("...");
    editButton_->SetTooltip("Edit Configurations");
    editButton_->SetOnClick([this]() { if (onEditConfigurations) onEditConfigurations(); });
    AddChild(editButton_);
}

void UCLaunchConfigSelector::Refresh() { PopulateDropdown(); }

void UCLaunchConfigSelector::PopulateDropdown() {
    if (!configDropDown_ || !configManager_) return;
    configDropDown_->ClearItems();
    configIds_.clear();
    std::string activeId = configManager_->GetActiveConfigurationId();
    int activeIndex = 0;
    int i = 0;
    for (auto* config : configManager_->GetAllConfigurations()) {
        configDropDown_->AddItem(config->GetName());
        configIds_.push_back(config->GetId());
        if (config->GetId() == activeId) activeIndex = i;
        i++;
    }
    if (!configIds_.empty()) configDropDown_->SetSelectedIndex(activeIndex);
}

void UCLaunchConfigSelector::SelectConfiguration(const std::string& id) {
    for (size_t i = 0; i < configIds_.size(); i++) {
        if (configIds_[i] == id) { configDropDown_->SetSelectedIndex(static_cast<int>(i)); break; }
    }
}

std::string UCLaunchConfigSelector::GetSelectedConfigurationId() const {
    int index = configDropDown_->GetSelectedIndex();
    return (index >= 0 && index < static_cast<int>(configIds_.size())) ? configIds_[index] : "";
}

void UCLaunchConfigSelector::HandleRender(IRenderContext* ctx) { UltraCanvasContainer::HandleRender(ctx); }

} // namespace IDE
} // namespace UltraCanvas
