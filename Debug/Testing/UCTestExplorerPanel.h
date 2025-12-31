// Apps/IDE/Debug/Testing/UCTestExplorerPanel.h
// Test Explorer Panel for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasContainer.h"
#include "UltraCanvasTreeView.h"
#include "UltraCanvasToolbar.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasTextBox.h"
#include "UltraCanvasProgressBar.h"
#include "UCTestDebugger.h"
#include <functional>
#include <memory>
#include <map>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Test Explorer Panel using UltraCanvas components
 * 
 * Provides UI for discovering, running, and debugging unit tests
 * with support for Google Test and Catch2 frameworks.
 */
class UCTestExplorerPanel : public UltraCanvasContainer {
public:
    UCTestExplorerPanel(const std::string& id, int x, int y, int width, int height);
    ~UCTestExplorerPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetTestDebugger(UCTestDebugger* debugger);
    
    // =========================================================================
    // TEST MANAGEMENT
    // =========================================================================
    
    /**
     * @brief Discover tests from executable
     */
    void DiscoverTests(const std::string& executable);
    
    /**
     * @brief Refresh test list
     */
    void RefreshTests();
    
    /**
     * @brief Clear all tests
     */
    void ClearTests();
    
    /**
     * @brief Update test states from results
     */
    void UpdateTestStates(const TestRunResult& result);
    
    // =========================================================================
    // FILTERING
    // =========================================================================
    
    /**
     * @brief Set filter text
     */
    void SetFilterText(const std::string& filter);
    
    /**
     * @brief Show only failed tests
     */
    void ShowFailedOnly(bool failedOnly);
    
    /**
     * @brief Show/hide passed tests
     */
    void ShowPassed(bool show);
    
    /**
     * @brief Show/hide skipped tests
     */
    void ShowSkipped(bool show);
    
    // =========================================================================
    // SELECTION
    // =========================================================================
    
    /**
     * @brief Get selected tests
     */
    std::vector<std::string> GetSelectedTests() const;
    
    /**
     * @brief Select test by name
     */
    void SelectTest(const std::string& testName);
    
    /**
     * @brief Select all tests
     */
    void SelectAll();
    
    /**
     * @brief Deselect all tests
     */
    void DeselectAll();
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const std::string& testName)> onTestSelected;
    std::function<void(const std::string& testName)> onTestDoubleClicked;
    std::function<void(const std::vector<std::string>& tests)> onRunTests;
    std::function<void(const std::vector<std::string>& tests)> onDebugTests;
    std::function<void(const std::string& file, int line)> onGoToSource;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateTestTree();
    void AddSuiteNode(const TestSuite& suite);
    void AddTestNode(const TestCase& test, TreeNodeId parentId);
    void UpdateTestNode(const TestCase& test);
    void UpdateSuiteNode(const TestSuite& suite);
    void UpdateStatusBar();
    void ApplyFilter();
    
    std::string GetTestIcon(TestState state) const;
    Color GetTestColor(TestState state) const;
    std::string FormatDuration(std::chrono::milliseconds duration) const;
    
    // Context menu
    void ShowContextMenu(int x, int y);
    void OnRunSelected();
    void OnDebugSelected();
    void OnRunAll();
    void OnRunFailed();
    void OnGoToTest();
    
    UCTestDebugger* testDebugger_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
    std::shared_ptr<UltraCanvasTextBox> filterInput_;
    std::shared_ptr<UltraCanvasTreeView> testTree_;
    std::shared_ptr<UltraCanvasProgressBar> progressBar_;
    std::shared_ptr<UltraCanvasLabel> statusLabel_;
    
    // Test data
    std::string currentExecutable_;
    std::vector<TestSuite> suites_;
    
    // Node mapping
    std::map<TreeNodeId, std::string> nodeToTestName_;
    std::map<std::string, TreeNodeId> testNameToNode_;
    std::map<std::string, TreeNodeId> suiteNameToNode_;
    TreeNodeId nextNodeId_ = 1;
    
    // Filter state
    std::string filterText_;
    bool showFailedOnly_ = false;
    bool showPassed_ = true;
    bool showSkipped_ = true;
    
    // Test counts
    int totalTests_ = 0;
    int passedTests_ = 0;
    int failedTests_ = 0;
    int skippedTests_ = 0;
    int runningTests_ = 0;
};

/**
 * @brief Test Output Panel
 * 
 * Shows test output and failure details.
 */
class UCTestOutputPanel : public UltraCanvasContainer {
public:
    UCTestOutputPanel(const std::string& id, int x, int y, int width, int height);
    ~UCTestOutputPanel() override = default;
    
    void Initialize();
    
    void Clear();
    void AppendOutput(const std::string& text);
    void AppendError(const std::string& text);
    void ShowTestResult(const TestCase& test);
    void ShowRunSummary(const TestRunResult& result);
    
    std::function<void(const std::string& file, int line)> onSourceLinkClicked;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void ParseAndHighlightOutput(const std::string& text);
    
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasTextBox> outputBox_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
};

/**
 * @brief Test Run Configuration Dialog
 */
class UCTestRunConfigDialog : public UltraCanvasDialog {
public:
    UCTestRunConfigDialog(const std::string& id);
    ~UCTestRunConfigDialog() override = default;
    
    void Initialize();
    void SetConfig(const TestDebugConfig& config);
    TestDebugConfig GetConfig() const;
    
    std::function<void(const TestDebugConfig&)> onConfigAccepted;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void LoadConfigToUI();
    void SaveUIToConfig();
    
    TestDebugConfig config_;
    
    std::shared_ptr<UltraCanvasTextBox> executableInput_;
    std::shared_ptr<UltraCanvasTextBox> workingDirInput_;
    std::shared_ptr<UltraCanvasTextBox> filterInput_;
    std::shared_ptr<UltraCanvasCheckbox> breakOnFailureCheckbox_;
    std::shared_ptr<UltraCanvasCheckbox> stopOnFirstCheckbox_;
    std::shared_ptr<UltraCanvasCheckbox> shuffleCheckbox_;
    std::shared_ptr<UltraCanvasTextBox> repeatInput_;
    std::shared_ptr<UltraCanvasTextBox> timeoutInput_;
    std::shared_ptr<UltraCanvasDropDown> frameworkDropdown_;
    
    std::shared_ptr<UltraCanvasButton> okButton_;
    std::shared_ptr<UltraCanvasButton> cancelButton_;
};

} // namespace IDE
} // namespace UltraCanvas
