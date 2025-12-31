// Apps/IDE/Debug/Testing/UCTestExplorerPanel.cpp
// Test Explorer Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCTestExplorerPanel.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// UCTestExplorerPanel
// ============================================================================

UCTestExplorerPanel::UCTestExplorerPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    SetBackgroundColor(Color(250, 250, 250));
}

void UCTestExplorerPanel::Initialize() {
    CreateUI();
}

void UCTestExplorerPanel::SetTestDebugger(UCTestDebugger* debugger) {
    testDebugger_ = debugger;
    
    if (testDebugger_) {
        testDebugger_->onTestStarted = [this](const TestCase& test) {
            UpdateTestNode(test);
            runningTests_++;
            UpdateStatusBar();
        };
        
        testDebugger_->onTestFinished = [this](const TestCase& test) {
            UpdateTestNode(test);
            runningTests_--;
            if (test.state == TestState::Passed) passedTests_++;
            else if (test.state == TestState::Failed) failedTests_++;
            else if (test.state == TestState::Skipped) skippedTests_++;
            UpdateStatusBar();
        };
        
        testDebugger_->onRunFinished = [this](const TestRunResult& result) {
            UpdateTestStates(result);
            progressBar_->SetVisible(false);
            UpdateStatusBar();
        };
        
        testDebugger_->onRunStarted = [this](const TestRunResult&) {
            progressBar_->SetVisible(true);
            progressBar_->SetValue(0);
            passedTests_ = failedTests_ = skippedTests_ = runningTests_ = 0;
        };
    }
}

void UCTestExplorerPanel::CreateUI() {
    int yOffset = 0;
    
    // Title
    titleLabel_ = std::make_shared<UltraCanvasLabel>("test_explorer_title", 0, 8, yOffset + 4, 100, 20);
    titleLabel_->SetText("Test Explorer");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetFontSize(12);
    AddChild(titleLabel_);
    yOffset += 28;
    
    // Toolbar
    toolbar_ = std::make_shared<UltraCanvasToolbar>("test_toolbar", 0, 0, yOffset, GetWidth(), 28);
    toolbar_->SetBackgroundColor(Color(240, 240, 240));
    toolbar_->SetSpacing(2);
    
    auto runAllBtn = std::make_shared<UltraCanvasToolbarButton>("run_all");
    runAllBtn->SetIcon("icons/test_run_all.png");
    runAllBtn->SetTooltip("Run All Tests");
    runAllBtn->SetOnClick([this]() { OnRunAll(); });
    toolbar_->AddButton(runAllBtn);
    
    auto runSelectedBtn = std::make_shared<UltraCanvasToolbarButton>("run_selected");
    runSelectedBtn->SetIcon("icons/test_run.png");
    runSelectedBtn->SetTooltip("Run Selected Tests");
    runSelectedBtn->SetOnClick([this]() { OnRunSelected(); });
    toolbar_->AddButton(runSelectedBtn);
    
    auto debugSelectedBtn = std::make_shared<UltraCanvasToolbarButton>("debug_selected");
    debugSelectedBtn->SetIcon("icons/test_debug.png");
    debugSelectedBtn->SetTooltip("Debug Selected Tests");
    debugSelectedBtn->SetOnClick([this]() { OnDebugSelected(); });
    toolbar_->AddButton(debugSelectedBtn);
    
    toolbar_->AddSeparator();
    
    auto runFailedBtn = std::make_shared<UltraCanvasToolbarButton>("run_failed");
    runFailedBtn->SetIcon("icons/test_run_failed.png");
    runFailedBtn->SetTooltip("Run Failed Tests");
    runFailedBtn->SetOnClick([this]() { OnRunFailed(); });
    toolbar_->AddButton(runFailedBtn);
    
    toolbar_->AddSeparator();
    
    auto refreshBtn = std::make_shared<UltraCanvasToolbarButton>("refresh");
    refreshBtn->SetIcon("icons/refresh.png");
    refreshBtn->SetTooltip("Refresh Tests");
    refreshBtn->SetOnClick([this]() { RefreshTests(); });
    toolbar_->AddButton(refreshBtn);
    
    AddChild(toolbar_);
    yOffset += 32;
    
    // Filter input
    filterInput_ = std::make_shared<UltraCanvasTextBox>("test_filter", 0, 4, yOffset, GetWidth() - 8, 24);
    filterInput_->SetPlaceholder("Filter tests...");
    filterInput_->SetOnTextChange([this](const std::string& text) {
        SetFilterText(text);
    });
    AddChild(filterInput_);
    yOffset += 28;
    
    // Test tree
    testTree_ = std::make_shared<UltraCanvasTreeView>(
        "test_tree", 0, 0, yOffset, GetWidth(), GetHeight() - yOffset - 50);
    testTree_->SetShowExpandButtons(true);
    testTree_->SetShowIcons(true);
    testTree_->SetShowCheckboxes(true);
    testTree_->SetRowHeight(22);
    testTree_->SetMultiSelect(true);
    
    testTree_->onNodeSelect = [this](TreeNodeId nodeId) {
        auto it = nodeToTestName_.find(nodeId);
        if (it != nodeToTestName_.end() && onTestSelected) {
            onTestSelected(it->second);
        }
    };
    
    testTree_->onNodeDoubleClick = [this](TreeNodeId nodeId) {
        auto it = nodeToTestName_.find(nodeId);
        if (it != nodeToTestName_.end()) {
            if (onTestDoubleClicked) {
                onTestDoubleClicked(it->second);
            } else {
                OnGoToTest();
            }
        }
    };
    
    testTree_->onNodeRightClick = [this](TreeNodeId nodeId, int x, int y) {
        ShowContextMenu(x, y);
    };
    
    AddChild(testTree_);
    
    // Progress bar
    progressBar_ = std::make_shared<UltraCanvasProgressBar>(
        "test_progress", 0, 4, GetHeight() - 46, GetWidth() - 8, 16);
    progressBar_->SetVisible(false);
    AddChild(progressBar_);
    
    // Status label
    statusLabel_ = std::make_shared<UltraCanvasLabel>(
        "test_status", 0, 8, GetHeight() - 24, GetWidth() - 16, 20);
    statusLabel_->SetText("No tests loaded");
    statusLabel_->SetTextColor(Color(100, 100, 100));
    AddChild(statusLabel_);
}

void UCTestExplorerPanel::DiscoverTests(const std::string& executable) {
    currentExecutable_ = executable;
    
    if (!testDebugger_) return;
    
    // Discover tests
    testDebugger_->DiscoverTests(executable);
    
    // Get suites and populate tree
    suites_.clear();
    for (const auto& suite : testDebugger_->GetTestSuites()) {
        suites_.push_back(suite);
    }
    
    PopulateTestTree();
}

void UCTestExplorerPanel::RefreshTests() {
    if (!currentExecutable_.empty()) {
        DiscoverTests(currentExecutable_);
    }
}

void UCTestExplorerPanel::ClearTests() {
    testTree_->Clear();
    suites_.clear();
    nodeToTestName_.clear();
    testNameToNode_.clear();
    suiteNameToNode_.clear();
    nextNodeId_ = 1;
    totalTests_ = passedTests_ = failedTests_ = skippedTests_ = 0;
    UpdateStatusBar();
}

void UCTestExplorerPanel::PopulateTestTree() {
    testTree_->Clear();
    nodeToTestName_.clear();
    testNameToNode_.clear();
    suiteNameToNode_.clear();
    nextNodeId_ = 1;
    totalTests_ = 0;
    
    for (const auto& suite : suites_) {
        AddSuiteNode(suite);
    }
    
    ApplyFilter();
    UpdateStatusBar();
}

void UCTestExplorerPanel::AddSuiteNode(const TestSuite& suite) {
    TreeNodeId suiteNodeId = nextNodeId_++;
    
    TreeNodeData suiteData;
    suiteData.text = suite.name + " (" + std::to_string(suite.tests.size()) + " tests)";
    suiteData.leftIcon.iconPath = "icons/test_suite.png";
    suiteData.leftIcon.width = 16;
    suiteData.leftIcon.height = 16;
    suiteData.leftIcon.visible = true;
    suiteData.expanded = true;
    
    testTree_->AddNode(suiteNodeId, suiteData);
    suiteNameToNode_[suite.name] = suiteNodeId;
    
    for (const auto& test : suite.tests) {
        AddTestNode(test, suiteNodeId);
        totalTests_++;
    }
}

void UCTestExplorerPanel::AddTestNode(const TestCase& test, TreeNodeId parentId) {
    TreeNodeId testNodeId = nextNodeId_++;
    
    TreeNodeData testData;
    testData.text = test.name;
    
    if (test.isParameterized && !test.parameterInfo.empty()) {
        testData.text += " [" + test.parameterInfo + "]";
    }
    
    testData.leftIcon.iconPath = GetTestIcon(test.state);
    testData.leftIcon.width = 16;
    testData.leftIcon.height = 16;
    testData.leftIcon.visible = true;
    testData.textColor = GetTestColor(test.state);
    
    if (test.duration.count() > 0) {
        testData.text += " (" + FormatDuration(test.duration) + ")";
    }
    
    testTree_->AddNode(testNodeId, testData, parentId);
    nodeToTestName_[testNodeId] = test.fullName;
    testNameToNode_[test.fullName] = testNodeId;
}

void UCTestExplorerPanel::UpdateTestNode(const TestCase& test) {
    auto it = testNameToNode_.find(test.fullName);
    if (it == testNameToNode_.end()) return;
    
    TreeNodeId nodeId = it->second;
    
    TreeNodeData testData;
    testData.text = test.name;
    
    if (test.state == TestState::Running) {
        testData.text += " (running...)";
    } else if (test.duration.count() > 0) {
        testData.text += " (" + FormatDuration(test.duration) + ")";
    }
    
    testData.leftIcon.iconPath = GetTestIcon(test.state);
    testData.leftIcon.width = 16;
    testData.leftIcon.height = 16;
    testData.leftIcon.visible = true;
    testData.textColor = GetTestColor(test.state);
    
    testTree_->UpdateNode(nodeId, testData);
}

void UCTestExplorerPanel::UpdateSuiteNode(const TestSuite& suite) {
    auto it = suiteNameToNode_.find(suite.name);
    if (it == suiteNameToNode_.end()) return;
    
    TreeNodeData suiteData;
    std::ostringstream text;
    text << suite.name << " (";
    text << suite.passedCount << "/" << suite.tests.size();
    if (suite.failedCount > 0) text << ", " << suite.failedCount << " failed";
    text << ")";
    suiteData.text = text.str();
    
    if (suite.failedCount > 0) {
        suiteData.leftIcon.iconPath = "icons/test_suite_failed.png";
        suiteData.textColor = Color(200, 0, 0);
    } else if (suite.passedCount == static_cast<int>(suite.tests.size())) {
        suiteData.leftIcon.iconPath = "icons/test_suite_passed.png";
        suiteData.textColor = Color(0, 150, 0);
    } else {
        suiteData.leftIcon.iconPath = "icons/test_suite.png";
    }
    
    suiteData.leftIcon.width = 16;
    suiteData.leftIcon.height = 16;
    suiteData.leftIcon.visible = true;
    
    testTree_->UpdateNode(it->second, suiteData);
}

void UCTestExplorerPanel::UpdateTestStates(const TestRunResult& result) {
    passedTests_ = result.passedTests;
    failedTests_ = result.failedTests;
    skippedTests_ = result.skippedTests;
    
    for (const auto& suite : result.suites) {
        for (const auto& test : suite.tests) {
            UpdateTestNode(test);
        }
        UpdateSuiteNode(suite);
    }
    
    UpdateStatusBar();
}

void UCTestExplorerPanel::UpdateStatusBar() {
    std::ostringstream status;
    
    if (totalTests_ == 0) {
        status << "No tests loaded";
    } else if (runningTests_ > 0) {
        status << "Running... " << runningTests_ << " test(s)";
    } else {
        status << totalTests_ << " tests: ";
        status << passedTests_ << " passed";
        if (failedTests_ > 0) status << ", " << failedTests_ << " failed";
        if (skippedTests_ > 0) status << ", " << skippedTests_ << " skipped";
    }
    
    if (statusLabel_) {
        statusLabel_->SetText(status.str());
        
        if (failedTests_ > 0) {
            statusLabel_->SetTextColor(Color(200, 0, 0));
        } else if (passedTests_ == totalTests_ && totalTests_ > 0) {
            statusLabel_->SetTextColor(Color(0, 150, 0));
        } else {
            statusLabel_->SetTextColor(Color(100, 100, 100));
        }
    }
    
    if (progressBar_ && progressBar_->IsVisible() && totalTests_ > 0) {
        int completed = passedTests_ + failedTests_ + skippedTests_;
        progressBar_->SetValue(completed * 100 / totalTests_);
    }
}

void UCTestExplorerPanel::SetFilterText(const std::string& filter) {
    filterText_ = filter;
    ApplyFilter();
}

void UCTestExplorerPanel::ShowFailedOnly(bool failedOnly) {
    showFailedOnly_ = failedOnly;
    ApplyFilter();
}

void UCTestExplorerPanel::ShowPassed(bool show) {
    showPassed_ = show;
    ApplyFilter();
}

void UCTestExplorerPanel::ShowSkipped(bool show) {
    showSkipped_ = show;
    ApplyFilter();
}

void UCTestExplorerPanel::ApplyFilter() {
    // Show/hide nodes based on filter
    for (const auto& pair : testNameToNode_) {
        bool visible = true;
        
        // Text filter
        if (!filterText_.empty()) {
            if (pair.first.find(filterText_) == std::string::npos) {
                visible = false;
            }
        }
        
        // State filter
        if (visible && testDebugger_) {
            const TestCase* test = testDebugger_->FindTest(pair.first);
            if (test) {
                if (showFailedOnly_ && test->state != TestState::Failed) visible = false;
                if (!showPassed_ && test->state == TestState::Passed) visible = false;
                if (!showSkipped_ && test->state == TestState::Skipped) visible = false;
            }
        }
        
        testTree_->SetNodeVisible(pair.second, visible);
    }
}

std::vector<std::string> UCTestExplorerPanel::GetSelectedTests() const {
    std::vector<std::string> selected;
    
    auto selectedNodes = testTree_->GetSelectedNodes();
    for (TreeNodeId nodeId : selectedNodes) {
        auto it = nodeToTestName_.find(nodeId);
        if (it != nodeToTestName_.end()) {
            selected.push_back(it->second);
        }
    }
    
    return selected;
}

void UCTestExplorerPanel::SelectTest(const std::string& testName) {
    auto it = testNameToNode_.find(testName);
    if (it != testNameToNode_.end()) {
        testTree_->SelectNode(it->second);
        testTree_->ScrollToNode(it->second);
    }
}

void UCTestExplorerPanel::SelectAll() {
    testTree_->SelectAll();
}

void UCTestExplorerPanel::DeselectAll() {
    testTree_->ClearSelection();
}

std::string UCTestExplorerPanel::GetTestIcon(TestState state) const {
    switch (state) {
        case TestState::NotRun: return "icons/test_not_run.png";
        case TestState::Running: return "icons/test_running.png";
        case TestState::Passed: return "icons/test_passed.png";
        case TestState::Failed: return "icons/test_failed.png";
        case TestState::Skipped: return "icons/test_skipped.png";
        case TestState::Disabled: return "icons/test_disabled.png";
        case TestState::Error: return "icons/test_error.png";
        case TestState::Timeout: return "icons/test_timeout.png";
    }
    return "icons/test_not_run.png";
}

Color UCTestExplorerPanel::GetTestColor(TestState state) const {
    switch (state) {
        case TestState::Passed: return Color(0, 150, 0);
        case TestState::Failed: return Color(200, 0, 0);
        case TestState::Error: return Color(200, 100, 0);
        case TestState::Skipped:
        case TestState::Disabled: return Color(150, 150, 150);
        case TestState::Running: return Color(0, 100, 200);
        default: return Color(0, 0, 0);
    }
}

std::string UCTestExplorerPanel::FormatDuration(std::chrono::milliseconds duration) const {
    if (duration.count() < 1000) {
        return std::to_string(duration.count()) + "ms";
    }
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << (duration.count() / 1000.0) << "s";
    return ss.str();
}

void UCTestExplorerPanel::ShowContextMenu(int x, int y) {
    // Create context menu
    // Options: Run, Debug, Run Failed, Go to Source, etc.
}

void UCTestExplorerPanel::OnRunSelected() {
    auto selected = GetSelectedTests();
    if (selected.empty()) return;
    
    if (onRunTests) {
        onRunTests(selected);
    }
}

void UCTestExplorerPanel::OnDebugSelected() {
    auto selected = GetSelectedTests();
    if (selected.empty()) return;
    
    if (onDebugTests) {
        onDebugTests(selected);
    }
}

void UCTestExplorerPanel::OnRunAll() {
    std::vector<std::string> allTests;
    for (const auto& pair : testNameToNode_) {
        allTests.push_back(pair.first);
    }
    
    if (onRunTests) {
        onRunTests(allTests);
    }
}

void UCTestExplorerPanel::OnRunFailed() {
    std::vector<std::string> failedTests;
    
    if (testDebugger_) {
        for (const auto& pair : testNameToNode_) {
            const TestCase* test = testDebugger_->FindTest(pair.first);
            if (test && test->state == TestState::Failed) {
                failedTests.push_back(pair.first);
            }
        }
    }
    
    if (!failedTests.empty() && onRunTests) {
        onRunTests(failedTests);
    }
}

void UCTestExplorerPanel::OnGoToTest() {
    auto selected = GetSelectedTests();
    if (selected.empty() || !testDebugger_) return;
    
    const TestCase* test = testDebugger_->FindTest(selected[0]);
    if (test && !test->sourceFile.empty() && test->sourceLine > 0) {
        if (onGoToSource) {
            onGoToSource(test->sourceFile, test->sourceLine);
        }
    }
}

void UCTestExplorerPanel::HandleRender(IRenderContext* ctx) {
    UltraCanvasContainer::HandleRender(ctx);
}

// ============================================================================
// UCTestOutputPanel
// ============================================================================

UCTestOutputPanel::UCTestOutputPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    SetBackgroundColor(Color(255, 255, 255));
}

void UCTestOutputPanel::Initialize() {
    CreateUI();
}

void UCTestOutputPanel::CreateUI() {
    int yOffset = 0;
    
    titleLabel_ = std::make_shared<UltraCanvasLabel>("output_title", 0, 8, yOffset + 4, 100, 20);
    titleLabel_->SetText("Test Output");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    AddChild(titleLabel_);
    yOffset += 28;
    
    toolbar_ = std::make_shared<UltraCanvasToolbar>("output_toolbar", 0, 0, yOffset, GetWidth(), 24);
    
    auto clearBtn = std::make_shared<UltraCanvasToolbarButton>("clear");
    clearBtn->SetIcon("icons/clear.png");
    clearBtn->SetTooltip("Clear Output");
    clearBtn->SetOnClick([this]() { Clear(); });
    toolbar_->AddButton(clearBtn);
    
    AddChild(toolbar_);
    yOffset += 28;
    
    outputBox_ = std::make_shared<UltraCanvasTextBox>("output_box", 0, 4, yOffset, GetWidth() - 8, GetHeight() - yOffset - 4);
    outputBox_->SetReadOnly(true);
    outputBox_->SetMultiLine(true);
    outputBox_->SetFont("Consolas", 10);
    outputBox_->SetBackgroundColor(Color(30, 30, 30));
    outputBox_->SetTextColor(Color(220, 220, 220));
    AddChild(outputBox_);
}

void UCTestOutputPanel::Clear() {
    if (outputBox_) outputBox_->SetText("");
}

void UCTestOutputPanel::AppendOutput(const std::string& text) {
    if (outputBox_) {
        outputBox_->AppendText(text);
    }
}

void UCTestOutputPanel::AppendError(const std::string& text) {
    if (outputBox_) {
        outputBox_->AppendText("[ERROR] " + text + "\n");
    }
}

void UCTestOutputPanel::ShowTestResult(const TestCase& test) {
    std::ostringstream ss;
    ss << "\n--- " << test.fullName << " ---\n";
    ss << "State: " << TestStateToString(test.state) << "\n";
    
    if (test.duration.count() > 0) {
        ss << "Duration: " << test.duration.count() << "ms\n";
    }
    
    if (!test.errorMessage.empty()) {
        ss << "\nError:\n" << test.errorMessage << "\n";
    }
    
    if (!test.stackTrace.empty()) {
        ss << "\nStack Trace:\n" << test.stackTrace << "\n";
    }
    
    AppendOutput(ss.str());
}

void UCTestOutputPanel::ShowRunSummary(const TestRunResult& result) {
    std::ostringstream ss;
    ss << "\n========================================\n";
    ss << "Test Run Summary\n";
    ss << "========================================\n";
    ss << "Total:   " << result.totalTests << "\n";
    ss << "Passed:  " << result.passedTests << "\n";
    ss << "Failed:  " << result.failedTests << "\n";
    ss << "Skipped: " << result.skippedTests << "\n";
    ss << "Duration: " << result.totalDuration.count() << "ms\n";
    ss << "Result: " << (result.success ? "PASSED" : "FAILED") << "\n";
    ss << "========================================\n";
    
    AppendOutput(ss.str());
}

void UCTestOutputPanel::HandleRender(IRenderContext* ctx) {
    UltraCanvasContainer::HandleRender(ctx);
}

// ============================================================================
// UCTestRunConfigDialog
// ============================================================================

UCTestRunConfigDialog::UCTestRunConfigDialog(const std::string& id)
    : UltraCanvasDialog(id, 0, 0, 0, 500, 400) {
    SetTitle("Test Run Configuration");
}

void UCTestRunConfigDialog::Initialize() {
    CreateUI();
}

void UCTestRunConfigDialog::CreateUI() {
    int y = 50, labelWidth = 120, inputWidth = 350;
    
    auto execLabel = std::make_shared<UltraCanvasLabel>("exec_label", 0, 16, y + 4, labelWidth, 20);
    execLabel->SetText("Executable:");
    AddChild(execLabel);
    
    executableInput_ = std::make_shared<UltraCanvasTextBox>("exec_input", 0, 140, y, inputWidth, 24);
    AddChild(executableInput_);
    y += 32;
    
    auto wdLabel = std::make_shared<UltraCanvasLabel>("wd_label", 0, 16, y + 4, labelWidth, 20);
    wdLabel->SetText("Working Dir:");
    AddChild(wdLabel);
    
    workingDirInput_ = std::make_shared<UltraCanvasTextBox>("wd_input", 0, 140, y, inputWidth, 24);
    AddChild(workingDirInput_);
    y += 32;
    
    auto filterLabel = std::make_shared<UltraCanvasLabel>("filter_label", 0, 16, y + 4, labelWidth, 20);
    filterLabel->SetText("Filter:");
    AddChild(filterLabel);
    
    filterInput_ = std::make_shared<UltraCanvasTextBox>("filter_input", 0, 140, y, inputWidth, 24);
    filterInput_->SetPlaceholder("*");
    AddChild(filterInput_);
    y += 40;
    
    breakOnFailureCheckbox_ = std::make_shared<UltraCanvasCheckbox>("break_failure", 0, 16, y, 200, 20);
    breakOnFailureCheckbox_->SetText("Break on failure");
    breakOnFailureCheckbox_->SetChecked(true);
    AddChild(breakOnFailureCheckbox_);
    y += 24;
    
    stopOnFirstCheckbox_ = std::make_shared<UltraCanvasCheckbox>("stop_first", 0, 16, y, 200, 20);
    stopOnFirstCheckbox_->SetText("Stop on first failure");
    AddChild(stopOnFirstCheckbox_);
    y += 24;
    
    shuffleCheckbox_ = std::make_shared<UltraCanvasCheckbox>("shuffle", 0, 16, y, 200, 20);
    shuffleCheckbox_->SetText("Shuffle tests");
    AddChild(shuffleCheckbox_);
    y += 40;
    
    auto repeatLabel = std::make_shared<UltraCanvasLabel>("repeat_label", 0, 16, y + 4, labelWidth, 20);
    repeatLabel->SetText("Repeat count:");
    AddChild(repeatLabel);
    
    repeatInput_ = std::make_shared<UltraCanvasTextBox>("repeat_input", 0, 140, y, 80, 24);
    repeatInput_->SetText("1");
    AddChild(repeatInput_);
    y += 32;
    
    auto timeoutLabel = std::make_shared<UltraCanvasLabel>("timeout_label", 0, 16, y + 4, labelWidth, 20);
    timeoutLabel->SetText("Timeout (ms):");
    AddChild(timeoutLabel);
    
    timeoutInput_ = std::make_shared<UltraCanvasTextBox>("timeout_input", 0, 140, y, 80, 24);
    timeoutInput_->SetText("60000");
    AddChild(timeoutInput_);
    
    // Buttons
    okButton_ = std::make_shared<UltraCanvasButton>("ok", 0, GetWidth() - 180, GetHeight() - 48, 80, 28);
    okButton_->SetText("OK");
    okButton_->SetOnClick([this]() {
        SaveUIToConfig();
        if (onConfigAccepted) onConfigAccepted(config_);
        Close();
    });
    AddChild(okButton_);
    
    cancelButton_ = std::make_shared<UltraCanvasButton>("cancel", 0, GetWidth() - 90, GetHeight() - 48, 80, 28);
    cancelButton_->SetText("Cancel");
    cancelButton_->SetOnClick([this]() { Close(); });
    AddChild(cancelButton_);
}

void UCTestRunConfigDialog::SetConfig(const TestDebugConfig& config) {
    config_ = config;
    LoadConfigToUI();
}

TestDebugConfig UCTestRunConfigDialog::GetConfig() const {
    return config_;
}

void UCTestRunConfigDialog::LoadConfigToUI() {
    if (executableInput_) executableInput_->SetText(config_.executablePath);
    if (workingDirInput_) workingDirInput_->SetText(config_.workingDirectory);
    if (filterInput_) filterInput_->SetText(config_.filter.pattern);
    if (breakOnFailureCheckbox_) breakOnFailureCheckbox_->SetChecked(config_.breakOnFailure);
    if (stopOnFirstCheckbox_) stopOnFirstCheckbox_->SetChecked(config_.stopOnFirstFailure);
    if (shuffleCheckbox_) shuffleCheckbox_->SetChecked(config_.shuffleTests);
    if (repeatInput_) repeatInput_->SetText(std::to_string(config_.repeatCount));
    if (timeoutInput_) timeoutInput_->SetText(std::to_string(config_.timeout));
}

void UCTestRunConfigDialog::SaveUIToConfig() {
    if (executableInput_) config_.executablePath = executableInput_->GetText();
    if (workingDirInput_) config_.workingDirectory = workingDirInput_->GetText();
    if (filterInput_) config_.filter.pattern = filterInput_->GetText();
    if (breakOnFailureCheckbox_) config_.breakOnFailure = breakOnFailureCheckbox_->IsChecked();
    if (stopOnFirstCheckbox_) config_.stopOnFirstFailure = stopOnFirstCheckbox_->IsChecked();
    if (shuffleCheckbox_) config_.shuffleTests = shuffleCheckbox_->IsChecked();
    if (repeatInput_) config_.repeatCount = std::stoi(repeatInput_->GetText());
    if (timeoutInput_) config_.timeout = std::stoi(timeoutInput_->GetText());
}

void UCTestRunConfigDialog::HandleRender(IRenderContext* ctx) {
    UltraCanvasDialog::HandleRender(ctx);
}

} // namespace IDE
} // namespace UltraCanvas
