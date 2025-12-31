// Apps/IDE/Debug/Launch/UCLaunchConfigDialog.h
// Launch Configuration Dialog for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasDialog.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasTextBox.h"
#include "UltraCanvasButton.h"
#include "UltraCanvasDropDown.h"
#include "UltraCanvasCheckbox.h"
#include "UltraCanvasTreeView.h"
#include "UltraCanvasTabControl.h"
#include "UCLaunchConfiguration.h"
#include "UCLaunchConfigManager.h"
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Dialog result
 */
enum class LaunchConfigDialogResult {
    None,
    OK,
    Cancel,
    Apply
};

/**
 * @brief Launch configuration dialog
 * 
 * Provides UI for creating and editing launch configurations.
 * Uses UltraCanvas dialog and form components.
 */
class UCLaunchConfigDialog : public UltraCanvasDialog {
public:
    UCLaunchConfigDialog(const std::string& id, int width = 700, int height = 550);
    ~UCLaunchConfigDialog() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetConfigManager(UCLaunchConfigManager* manager);
    
    // =========================================================================
    // CONFIGURATION
    // =========================================================================
    
    /**
     * @brief Edit an existing configuration
     */
    void EditConfiguration(const std::string& configId);
    
    /**
     * @brief Create a new configuration from template
     */
    void CreateNewConfiguration(LaunchTemplateType templateType = LaunchTemplateType::Custom);
    
    /**
     * @brief Get the edited configuration
     */
    UCLaunchConfiguration* GetConfiguration() { return editingConfig_.get(); }
    
    // =========================================================================
    // DIALOG
    // =========================================================================
    
    LaunchConfigDialogResult ShowModal();
    LaunchConfigDialogResult GetResult() const { return result_; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(UCLaunchConfiguration*)> onConfigurationSaved;
    std::function<void()> onDialogClosed;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void CreateConfigListPanel();
    void CreateTabPanel();
    void CreateGeneralTab();
    void CreateEnvironmentTab();
    void CreateDebuggerTab();
    void CreateAdvancedTab();
    void CreateButtonPanel();
    
    void PopulateConfigList();
    void SelectConfiguration(const std::string& id);
    void LoadConfigToUI();
    void SaveUIToConfig();
    bool ValidateInput();
    
    void OnAddConfiguration();
    void OnRemoveConfiguration();
    void OnDuplicateConfiguration();
    void OnOK();
    void OnCancel();
    void OnApply();
    
    void UpdateUIForLaunchType(LaunchConfigType type);
    void UpdateDebuggerOptions(DebuggerType type);
    
    UCLaunchConfigManager* configManager_ = nullptr;
    std::unique_ptr<UCLaunchConfiguration> editingConfig_;
    std::string originalConfigId_;
    LaunchConfigDialogResult result_ = LaunchConfigDialogResult::None;
    bool isNewConfig_ = false;
    
    // Left panel - Configuration list
    std::shared_ptr<UltraCanvasContainer> leftPanel_;
    std::shared_ptr<UltraCanvasTreeView> configListTree_;
    std::shared_ptr<UltraCanvasButton> addButton_;
    std::shared_ptr<UltraCanvasButton> removeButton_;
    std::shared_ptr<UltraCanvasButton> duplicateButton_;
    
    // Right panel - Configuration editor
    std::shared_ptr<UltraCanvasContainer> rightPanel_;
    std::shared_ptr<UltraCanvasTabControl> tabControl_;
    
    // General tab
    std::shared_ptr<UltraCanvasContainer> generalTab_;
    std::shared_ptr<UltraCanvasTextBox> nameTextBox_;
    std::shared_ptr<UltraCanvasDropDown> typeDropDown_;
    std::shared_ptr<UltraCanvasTextBox> programTextBox_;
    std::shared_ptr<UltraCanvasButton> programBrowseButton_;
    std::shared_ptr<UltraCanvasTextBox> argsTextBox_;
    std::shared_ptr<UltraCanvasTextBox> workingDirTextBox_;
    std::shared_ptr<UltraCanvasButton> workingDirBrowseButton_;
    std::shared_ptr<UltraCanvasDropDown> consoleDropDown_;
    std::shared_ptr<UltraCanvasCheckbox> stopAtEntryCheckbox_;
    
    // Attach-specific
    std::shared_ptr<UltraCanvasContainer> attachPanel_;
    std::shared_ptr<UltraCanvasTextBox> processIdTextBox_;
    std::shared_ptr<UltraCanvasTextBox> processNameTextBox_;
    std::shared_ptr<UltraCanvasButton> selectProcessButton_;
    
    // Remote-specific
    std::shared_ptr<UltraCanvasContainer> remotePanel_;
    std::shared_ptr<UltraCanvasTextBox> hostTextBox_;
    std::shared_ptr<UltraCanvasTextBox> portTextBox_;
    
    // Core dump-specific
    std::shared_ptr<UltraCanvasContainer> coreDumpPanel_;
    std::shared_ptr<UltraCanvasTextBox> coreDumpPathTextBox_;
    std::shared_ptr<UltraCanvasButton> coreDumpBrowseButton_;
    
    // Environment tab
    std::shared_ptr<UltraCanvasContainer> envTab_;
    std::shared_ptr<UltraCanvasTreeView> envVarsTree_;
    std::shared_ptr<UltraCanvasButton> addEnvButton_;
    std::shared_ptr<UltraCanvasButton> removeEnvButton_;
    std::shared_ptr<UltraCanvasCheckbox> inheritEnvCheckbox_;
    
    // Debugger tab
    std::shared_ptr<UltraCanvasContainer> debuggerTab_;
    std::shared_ptr<UltraCanvasDropDown> debuggerTypeDropDown_;
    std::shared_ptr<UltraCanvasTextBox> debuggerPathTextBox_;
    std::shared_ptr<UltraCanvasButton> debuggerBrowseButton_;
    std::shared_ptr<UltraCanvasTextBox> debuggerArgsTextBox_;
    std::shared_ptr<UltraCanvasCheckbox> prettyPrintingCheckbox_;
    std::shared_ptr<UltraCanvasTextBox> initCommandsTextBox_;
    
    // Advanced tab
    std::shared_ptr<UltraCanvasContainer> advancedTab_;
    std::shared_ptr<UltraCanvasDropDown> preLaunchDropDown_;
    std::shared_ptr<UltraCanvasTextBox> preLaunchTaskTextBox_;
    std::shared_ptr<UltraCanvasTextBox> symbolFileTextBox_;
    std::shared_ptr<UltraCanvasTreeView> sourcePathsTree_;
    
    // Button panel
    std::shared_ptr<UltraCanvasContainer> buttonPanel_;
    std::shared_ptr<UltraCanvasButton> okButton_;
    std::shared_ptr<UltraCanvasButton> cancelButton_;
    std::shared_ptr<UltraCanvasButton> applyButton_;
    
    // State tracking
    std::map<TreeNodeId, std::string> nodeToConfigId_;
};

/**
 * @brief Quick launch configuration selector
 * 
 * Simple dropdown-style popup for selecting a launch configuration
 * from the toolbar or menu.
 */
class UCLaunchConfigSelector : public UltraCanvasContainer {
public:
    UCLaunchConfigSelector(const std::string& id, int x, int y, int width, int height);
    ~UCLaunchConfigSelector() override = default;
    
    void Initialize();
    void SetConfigManager(UCLaunchConfigManager* manager);
    
    void Refresh();
    void SelectConfiguration(const std::string& id);
    std::string GetSelectedConfigurationId() const;
    
    std::function<void(const std::string& configId)> onConfigurationSelected;
    std::function<void()> onEditConfigurations;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateDropdown();
    
    UCLaunchConfigManager* configManager_ = nullptr;
    
    std::shared_ptr<UltraCanvasDropDown> configDropDown_;
    std::shared_ptr<UltraCanvasButton> editButton_;
    
    std::vector<std::string> configIds_;
};

} // namespace IDE
} // namespace UltraCanvas
