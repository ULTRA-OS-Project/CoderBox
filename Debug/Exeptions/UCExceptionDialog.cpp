// Apps/IDE/Debug/Exceptions/UCExceptionDialog.cpp
// Exception Dialog Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCExceptionDialog.h"
#include <sstream>
#include <ctime>
#include <iomanip>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// UCExceptionSettingsDialog
// ============================================================================

UCExceptionSettingsDialog::UCExceptionSettingsDialog(const std::string& id, int width, int height)
    : UltraCanvasDialog(id, 0, 0, 0, width, height) {
    SetTitle("Exception Settings");
    SetResizable(true);
}

void UCExceptionSettingsDialog::Initialize() {
    CreateUI();
}

void UCExceptionSettingsDialog::SetExceptionManager(UCExceptionManager* manager) {
    exceptionManager_ = manager;
    PopulateCppExceptions();
    PopulateFilters();
}

void UCExceptionSettingsDialog::SetSignalHandler(UCSignalHandler* handler) {
    signalHandler_ = handler;
    PopulateSignals();
}

void UCExceptionSettingsDialog::Refresh() {
    PopulateCppExceptions();
    PopulateSignals();
    PopulateFilters();
}

void UCExceptionSettingsDialog::CreateUI() {
    tabControl_ = std::make_shared<UltraCanvasTabControl>(
        "exception_tabs", 0, 8, 40, GetWidth() - 16, GetHeight() - 100);
    
    CreateCppExceptionsTab();
    CreateSignalsTab();
    CreateFiltersTab();
    
    AddChild(tabControl_);
    CreateButtonPanel();
}

void UCExceptionSettingsDialog::CreateCppExceptionsTab() {
    cppTab_ = std::make_shared<UltraCanvasContainer>(
        "cpp_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    int y = 8;
    
    breakOnAllCppCheckbox_ = std::make_shared<UltraCanvasCheckbox>(
        "break_all_cpp", 0, 8, y, 250, 20);
    breakOnAllCppCheckbox_->SetText("Break on all C++ exceptions");
    breakOnAllCppCheckbox_->SetOnChange([this](bool checked) {
        if (exceptionManager_) exceptionManager_->SetBreakOnAllCppExceptions(checked);
    });
    cppTab_->AddChild(breakOnAllCppCheckbox_);
    y += 24;
    
    breakOnUnhandledCppCheckbox_ = std::make_shared<UltraCanvasCheckbox>(
        "break_unhandled_cpp", 0, 8, y, 250, 20);
    breakOnUnhandledCppCheckbox_->SetText("Break on unhandled C++ exceptions");
    breakOnUnhandledCppCheckbox_->SetChecked(true);
    breakOnUnhandledCppCheckbox_->SetOnChange([this](bool checked) {
        if (exceptionManager_) exceptionManager_->SetBreakOnUnhandledCppExceptions(checked);
    });
    cppTab_->AddChild(breakOnUnhandledCppCheckbox_);
    y += 30;
    
    auto listLabel = std::make_shared<UltraCanvasLabel>("cpp_list_label", 0, 8, y, 200, 20);
    listLabel->SetText("Exception Types:");
    cppTab_->AddChild(listLabel);
    y += 24;
    
    cppExceptionsTree_ = std::make_shared<UltraCanvasTreeView>(
        "cpp_exceptions_tree", 0, 8, y, cppTab_->GetWidth() - 16, cppTab_->GetHeight() - y - 60);
    cppExceptionsTree_->SetShowExpandButtons(true);
    cppExceptionsTree_->SetShowIcons(true);
    cppExceptionsTree_->SetShowCheckboxes(true);
    cppExceptionsTree_->SetRowHeight(22);
    
    cppExceptionsTree_->onNodeCheckChange = [this](TreeNodeId nodeId, bool checked) {
        auto it = cppNodeToName_.find(nodeId);
        if (it != cppNodeToName_.end()) {
            OnExceptionCheckChanged(it->second, checked);
        }
    };
    cppTab_->AddChild(cppExceptionsTree_);
    
    int inputY = cppTab_->GetHeight() - 36;
    addExceptionInput_ = std::make_shared<UltraCanvasTextBox>(
        "add_exception_input", 0, 8, inputY, 300, 24);
    addExceptionInput_->SetPlaceholder("Enter exception type (e.g., std::runtime_error)");
    cppTab_->AddChild(addExceptionInput_);
    
    addExceptionButton_ = std::make_shared<UltraCanvasButton>(
        "add_exception_btn", 0, 316, inputY, 60, 24);
    addExceptionButton_->SetText("Add");
    addExceptionButton_->SetOnClick([this]() {
        std::string typeName = addExceptionInput_->GetText();
        if (!typeName.empty() && exceptionManager_) {
            exceptionManager_->AddCppExceptionType(typeName);
            addExceptionInput_->SetText("");
            PopulateCppExceptions();
        }
    });
    cppTab_->AddChild(addExceptionButton_);
    
    tabControl_->AddTab("C++ Exceptions", cppTab_);
}

void UCExceptionSettingsDialog::CreateSignalsTab() {
    signalsTab_ = std::make_shared<UltraCanvasContainer>(
        "signals_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    int y = 8;
    
    stopAllButton_ = std::make_shared<UltraCanvasButton>("stop_all", 0, 8, y, 80, 24);
    stopAllButton_->SetText("Stop All");
    stopAllButton_->SetOnClick([this]() {
        if (signalHandler_) { signalHandler_->StopOnAll(); PopulateSignals(); }
    });
    signalsTab_->AddChild(stopAllButton_);
    
    stopNoneButton_ = std::make_shared<UltraCanvasButton>("stop_none", 0, 96, y, 80, 24);
    stopNoneButton_->SetText("Stop None");
    stopNoneButton_->SetOnClick([this]() {
        if (signalHandler_) { signalHandler_->StopOnNone(); PopulateSignals(); }
    });
    signalsTab_->AddChild(stopNoneButton_);
    
    resetDefaultsButton_ = std::make_shared<UltraCanvasButton>("reset_defaults", 0, 184, y, 100, 24);
    resetDefaultsButton_->SetText("Reset Defaults");
    resetDefaultsButton_->SetOnClick([this]() {
        if (signalHandler_) { signalHandler_->ResetToGDBDefaults(); PopulateSignals(); }
    });
    signalsTab_->AddChild(resetDefaultsButton_);
    y += 32;
    
    auto nameHeader = std::make_shared<UltraCanvasLabel>("name_hdr", 0, 8, y, 100, 18);
    nameHeader->SetText("Signal");
    nameHeader->SetFontWeight(FontWeight::Bold);
    signalsTab_->AddChild(nameHeader);
    
    auto stopHeader = std::make_shared<UltraCanvasLabel>("stop_hdr", 0, 120, y, 50, 18);
    stopHeader->SetText("Stop");
    stopHeader->SetFontWeight(FontWeight::Bold);
    signalsTab_->AddChild(stopHeader);
    
    auto printHeader = std::make_shared<UltraCanvasLabel>("print_hdr", 0, 180, y, 50, 18);
    printHeader->SetText("Print");
    printHeader->SetFontWeight(FontWeight::Bold);
    signalsTab_->AddChild(printHeader);
    
    auto passHeader = std::make_shared<UltraCanvasLabel>("pass_hdr", 0, 240, y, 50, 18);
    passHeader->SetText("Pass");
    passHeader->SetFontWeight(FontWeight::Bold);
    signalsTab_->AddChild(passHeader);
    
    auto descHeader = std::make_shared<UltraCanvasLabel>("desc_hdr", 0, 300, y, 200, 18);
    descHeader->SetText("Description");
    descHeader->SetFontWeight(FontWeight::Bold);
    signalsTab_->AddChild(descHeader);
    y += 24;
    
    signalsTree_ = std::make_shared<UltraCanvasTreeView>(
        "signals_tree", 0, 8, y, signalsTab_->GetWidth() - 16, signalsTab_->GetHeight() - y - 8);
    signalsTree_->SetShowExpandButtons(true);
    signalsTree_->SetShowIcons(false);
    signalsTree_->SetRowHeight(24);
    signalsTab_->AddChild(signalsTree_);
    
    tabControl_->AddTab("Signals", signalsTab_);
}

void UCExceptionSettingsDialog::CreateFiltersTab() {
    filtersTab_ = std::make_shared<UltraCanvasContainer>(
        "filters_tab", 0, 0, 0, tabControl_->GetWidth() - 8, tabControl_->GetHeight() - 32);
    
    int y = 8;
    
    auto helpLabel = std::make_shared<UltraCanvasLabel>("filter_help", 0, 8, y, filtersTab_->GetWidth() - 16, 40);
    helpLabel->SetText("Exception filters customize break behavior using regex patterns.");
    helpLabel->SetTextColor(Color(100, 100, 100));
    filtersTab_->AddChild(helpLabel);
    y += 32;
    
    filtersTree_ = std::make_shared<UltraCanvasTreeView>(
        "filters_tree", 0, 8, y, filtersTab_->GetWidth() - 16, filtersTab_->GetHeight() - y - 70);
    filtersTree_->SetShowExpandButtons(false);
    filtersTree_->SetRowHeight(24);
    filtersTab_->AddChild(filtersTree_);
    
    int inputY = filtersTab_->GetHeight() - 60;
    
    auto patternLabel = std::make_shared<UltraCanvasLabel>("pattern_label", 0, 8, inputY + 4, 60, 20);
    patternLabel->SetText("Pattern:");
    filtersTab_->AddChild(patternLabel);
    
    filterPatternInput_ = std::make_shared<UltraCanvasTextBox>("filter_pattern", 0, 70, inputY, 200, 24);
    filterPatternInput_->SetPlaceholder("std::.*error");
    filtersTab_->AddChild(filterPatternInput_);
    
    auto behaviorLabel = std::make_shared<UltraCanvasLabel>("behavior_label", 0, 280, inputY + 4, 60, 20);
    behaviorLabel->SetText("Action:");
    filtersTab_->AddChild(behaviorLabel);
    
    filterBehaviorDropdown_ = std::make_shared<UltraCanvasDropDown>("filter_behavior", 0, 340, inputY, 120, 24);
    filterBehaviorDropdown_->AddItem("Never break");
    filterBehaviorDropdown_->AddItem("Always break");
    filterBehaviorDropdown_->AddItem("If unhandled");
    filterBehaviorDropdown_->SetSelectedIndex(1);
    filtersTab_->AddChild(filterBehaviorDropdown_);
    
    addFilterButton_ = std::make_shared<UltraCanvasButton>("add_filter", 0, 470, inputY, 60, 24);
    addFilterButton_->SetText("Add");
    addFilterButton_->SetOnClick([this]() { OnAddFilter(); });
    filtersTab_->AddChild(addFilterButton_);
    
    removeFilterButton_ = std::make_shared<UltraCanvasButton>("remove_filter", 0, 540, inputY, 70, 24);
    removeFilterButton_->SetText("Remove");
    removeFilterButton_->SetOnClick([this]() { OnRemoveFilter(); });
    filtersTab_->AddChild(removeFilterButton_);
    
    tabControl_->AddTab("Filters", filtersTab_);
}

void UCExceptionSettingsDialog::CreateButtonPanel() {
    int btnWidth = 80, btnHeight = 28, y = GetHeight() - 48, x = GetWidth() - 16;
    
    cancelButton_ = std::make_shared<UltraCanvasButton>("cancel", 0, x - btnWidth, y, btnWidth, btnHeight);
    cancelButton_->SetText("Cancel");
    cancelButton_->SetOnClick([this]() { OnCancel(); });
    AddChild(cancelButton_);
    x -= btnWidth + 8;
    
    applyButton_ = std::make_shared<UltraCanvasButton>("apply", 0, x - btnWidth, y, btnWidth, btnHeight);
    applyButton_->SetText("Apply");
    applyButton_->SetOnClick([this]() { OnApply(); });
    AddChild(applyButton_);
    x -= btnWidth + 8;
    
    okButton_ = std::make_shared<UltraCanvasButton>("ok", 0, x - btnWidth, y, btnWidth, btnHeight);
    okButton_->SetText("OK");
    okButton_->SetOnClick([this]() { OnOK(); });
    AddChild(okButton_);
}

void UCExceptionSettingsDialog::PopulateCppExceptions() {
    if (!cppExceptionsTree_ || !exceptionManager_) return;
    
    cppExceptionsTree_->Clear();
    cppNodeToName_.clear();
    
    TreeNodeId nodeId = 1;
    for (const auto& exc : exceptionManager_->GetExceptionTypesByCategory(ExceptionCategory::CppException)) {
        TreeNodeData nodeData;
        nodeData.text = exc.displayName;
        nodeData.checked = exc.enabled;
        nodeData.leftIcon.iconPath = "icons/exception.png";
        nodeData.leftIcon.width = 16;
        nodeData.leftIcon.height = 16;
        nodeData.leftIcon.visible = true;
        
        cppExceptionsTree_->AddNode(nodeId, nodeData);
        cppNodeToName_[nodeId] = exc.name;
        nodeId++;
    }
    
    if (breakOnAllCppCheckbox_) breakOnAllCppCheckbox_->SetChecked(exceptionManager_->GetBreakOnAllCppExceptions());
    if (breakOnUnhandledCppCheckbox_) breakOnUnhandledCppCheckbox_->SetChecked(exceptionManager_->GetBreakOnUnhandledCppExceptions());
}

void UCExceptionSettingsDialog::PopulateSignals() {
    if (!signalsTree_ || !signalHandler_) return;
    
    signalsTree_->Clear();
    signalNodeToNumber_.clear();
    
    TreeNodeId nodeId = 1;
    for (const auto& sig : signalHandler_->GetAllSignals()) {
        if (sig.isRealtime) continue;
        
        std::ostringstream text;
        text << std::left << std::setw(12) << sig.name;
        text << (sig.stop ? "  [X]  " : "  [ ]  ");
        text << (sig.print ? "  [X]  " : "  [ ]  ");
        text << (sig.pass ? "  [X]  " : "  [ ]  ");
        text << sig.description;
        
        TreeNodeData nodeData;
        nodeData.text = text.str();
        nodeData.textColor = sig.stop ? Color(200, 0, 0) : Color(0, 0, 0);
        
        signalsTree_->AddNode(nodeId, nodeData);
        signalNodeToNumber_[nodeId] = sig.number;
        nodeId++;
    }
}

void UCExceptionSettingsDialog::PopulateFilters() {
    if (!filtersTree_ || !exceptionManager_) return;
    
    filtersTree_->Clear();
    filterNodeToId_.clear();
    
    TreeNodeId nodeId = 1;
    for (const auto& pair : exceptionManager_->GetFilters()) {
        std::ostringstream text;
        text << pair.second.pattern << "  →  " << ExceptionBreakBehaviorToString(pair.second.behavior);
        
        TreeNodeData nodeData;
        nodeData.text = text.str();
        nodeData.textColor = pair.second.enabled ? Color(0, 0, 0) : Color(150, 150, 150);
        
        filtersTree_->AddNode(nodeId, nodeData);
        filterNodeToId_[nodeId] = pair.first;
        nodeId++;
    }
}

void UCExceptionSettingsDialog::OnExceptionCheckChanged(const std::string& name, bool checked) {
    if (exceptionManager_) {
        exceptionManager_->SetExceptionEnabled(name, checked);
    }
}

void UCExceptionSettingsDialog::OnExceptionBehaviorChanged(const std::string& name, ExceptionBreakBehavior behavior) {
    if (exceptionManager_) {
        exceptionManager_->SetBreakBehavior(name, behavior);
    }
}

void UCExceptionSettingsDialog::OnSignalCheckChanged(int signalNumber, bool stop, bool print, bool pass) {
    if (signalHandler_) {
        signalHandler_->SetDisposition(signalNumber, stop, print, pass);
    }
}

void UCExceptionSettingsDialog::OnAddFilter() {
    if (!exceptionManager_ || !filterPatternInput_) return;
    
    std::string pattern = filterPatternInput_->GetText();
    if (pattern.empty()) return;
    
    ExceptionBreakBehavior behavior = ExceptionBreakBehavior::Always;
    int index = filterBehaviorDropdown_->GetSelectedIndex();
    if (index == 0) behavior = ExceptionBreakBehavior::Never;
    else if (index == 2) behavior = ExceptionBreakBehavior::Unhandled;
    
    ExceptionFilter filter(pattern, behavior);
    exceptionManager_->AddFilter(filter);
    
    filterPatternInput_->SetText("");
    PopulateFilters();
}

void UCExceptionSettingsDialog::OnRemoveFilter() {
    if (!exceptionManager_ || !filtersTree_) return;
    
    auto selected = filtersTree_->GetSelectedNodes();
    if (selected.empty()) return;
    
    auto it = filterNodeToId_.find(selected[0]);
    if (it != filterNodeToId_.end()) {
        exceptionManager_->RemoveFilter(it->second);
        PopulateFilters();
    }
}

void UCExceptionSettingsDialog::OnApply() {
    if (onSettingsChanged) onSettingsChanged();
}

void UCExceptionSettingsDialog::OnOK() {
    OnApply();
    Close();
}

void UCExceptionSettingsDialog::OnCancel() {
    Close();
}

void UCExceptionSettingsDialog::HandleRender(IRenderContext* ctx) {
    UltraCanvasDialog::HandleRender(ctx);
}

// ============================================================================
// UCExceptionCaughtDialog
// ============================================================================

UCExceptionCaughtDialog::UCExceptionCaughtDialog(const std::string& id)
    : UltraCanvasDialog(id, 0, 0, 0, 500, 350) {
    SetTitle("Exception Caught");
    SetResizable(false);
}

void UCExceptionCaughtDialog::Initialize() {
    CreateUI();
}

void UCExceptionCaughtDialog::SetException(const CaughtException& exception) {
    exception_ = exception;
    UpdateContent();
}

void UCExceptionCaughtDialog::CreateUI() {
    int y = 50;
    
    titleLabel_ = std::make_shared<UltraCanvasLabel>("exc_title", 0, 16, y, GetWidth() - 32, 24);
    titleLabel_->SetFontSize(14);
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetTextColor(Color(200, 0, 0));
    AddChild(titleLabel_);
    y += 32;
    
    typeLabel_ = std::make_shared<UltraCanvasLabel>("exc_type", 0, 16, y, GetWidth() - 32, 20);
    AddChild(typeLabel_);
    y += 24;
    
    messageLabel_ = std::make_shared<UltraCanvasLabel>("exc_msg", 0, 16, y, GetWidth() - 32, 40);
    AddChild(messageLabel_);
    y += 48;
    
    locationLabel_ = std::make_shared<UltraCanvasLabel>("exc_loc", 0, 16, y, GetWidth() - 32, 20);
    locationLabel_->SetTextColor(Color(0, 0, 150));
    AddChild(locationLabel_);
    y += 28;
    
    stackTraceBox_ = std::make_shared<UltraCanvasTextBox>("exc_stack", 0, 16, y, GetWidth() - 32, 100);
    stackTraceBox_->SetReadOnly(true);
    stackTraceBox_->SetFont("Consolas", 10);
    AddChild(stackTraceBox_);
    y += 110;
    
    // Buttons
    int btnY = GetHeight() - 48;
    
    breakButton_ = std::make_shared<UltraCanvasButton>("break_btn", 0, 16, btnY, 80, 28);
    breakButton_->SetText("Break");
    breakButton_->SetOnClick([this]() {
        userAction_ = UserAction::Break;
        if (onActionSelected) onActionSelected(userAction_);
        Close();
    });
    AddChild(breakButton_);
    
    continueButton_ = std::make_shared<UltraCanvasButton>("continue_btn", 0, 104, btnY, 80, 28);
    continueButton_->SetText("Continue");
    continueButton_->SetOnClick([this]() {
        userAction_ = UserAction::Continue;
        if (onActionSelected) onActionSelected(userAction_);
        Close();
    });
    AddChild(continueButton_);
    
    ignoreButton_ = std::make_shared<UltraCanvasButton>("ignore_btn", 0, 192, btnY, 80, 28);
    ignoreButton_->SetText("Ignore");
    ignoreButton_->SetOnClick([this]() {
        userAction_ = UserAction::Ignore;
        if (onActionSelected) onActionSelected(userAction_);
        Close();
    });
    AddChild(ignoreButton_);
    
    ignoreAllCheckbox_ = std::make_shared<UltraCanvasCheckbox>("ignore_all", 0, 280, btnY + 4, 200, 20);
    ignoreAllCheckbox_->SetText("Ignore all of this type");
    AddChild(ignoreAllCheckbox_);
}

void UCExceptionCaughtDialog::UpdateContent() {
    if (titleLabel_) {
        titleLabel_->SetText(exception_.isFirstChance ? "First Chance Exception" : "Unhandled Exception");
    }
    
    if (typeLabel_) {
        typeLabel_->SetText("Type: " + exception_.type);
    }
    
    if (messageLabel_) {
        messageLabel_->SetText(exception_.message);
    }
    
    if (locationLabel_) {
        std::ostringstream loc;
        if (!exception_.functionName.empty()) {
            loc << exception_.functionName;
        }
        if (!exception_.sourceFile.empty()) {
            loc << " at " << exception_.sourceFile << ":" << exception_.sourceLine;
        }
        locationLabel_->SetText(loc.str());
    }
    
    if (stackTraceBox_) {
        stackTraceBox_->SetText(exception_.stackTrace);
    }
}

void UCExceptionCaughtDialog::HandleRender(IRenderContext* ctx) {
    UltraCanvasDialog::HandleRender(ctx);
}

// ============================================================================
// UCCrashReportDialog
// ============================================================================

UCCrashReportDialog::UCCrashReportDialog(const std::string& id)
    : UltraCanvasDialog(id, 0, 0, 0, 550, 450) {
    SetTitle("Application Crash");
    SetResizable(true);
}

void UCCrashReportDialog::Initialize() {
    CreateUI();
}

void UCCrashReportDialog::SetCrashInfo(const CaughtException& exception, const std::string& stackTrace) {
    isSignal_ = false;
    exception_ = exception;
    stackTrace_ = stackTrace;
    UpdateContent();
}

void UCCrashReportDialog::SetSignalInfo(const CaughtSignal& signal, const std::string& stackTrace) {
    isSignal_ = true;
    signal_ = signal;
    stackTrace_ = stackTrace;
    UpdateContent();
}

void UCCrashReportDialog::CreateUI() {
    int y = 50;
    
    crashTitleLabel_ = std::make_shared<UltraCanvasLabel>("crash_title", 0, 16, y, GetWidth() - 32, 28);
    crashTitleLabel_->SetFontSize(16);
    crashTitleLabel_->SetFontWeight(FontWeight::Bold);
    crashTitleLabel_->SetTextColor(Color(200, 0, 0));
    crashTitleLabel_->SetText("The application has crashed");
    AddChild(crashTitleLabel_);
    y += 36;
    
    crashTypeLabel_ = std::make_shared<UltraCanvasLabel>("crash_type", 0, 16, y, GetWidth() - 32, 24);
    crashTypeLabel_->SetFontSize(12);
    AddChild(crashTypeLabel_);
    y += 28;
    
    crashLocationLabel_ = std::make_shared<UltraCanvasLabel>("crash_loc", 0, 16, y, GetWidth() - 32, 20);
    crashLocationLabel_->SetTextColor(Color(0, 0, 150));
    AddChild(crashLocationLabel_);
    y += 28;
    
    auto detailsLabel = std::make_shared<UltraCanvasLabel>("details_lbl", 0, 16, y, 100, 20);
    detailsLabel->SetText("Details:");
    AddChild(detailsLabel);
    y += 22;
    
    crashDetailsBox_ = std::make_shared<UltraCanvasTextBox>("crash_details", 0, 16, y, GetWidth() - 32, 60);
    crashDetailsBox_->SetReadOnly(true);
    crashDetailsBox_->SetFont("Consolas", 10);
    AddChild(crashDetailsBox_);
    y += 68;
    
    auto stackLabel = std::make_shared<UltraCanvasLabel>("stack_lbl", 0, 16, y, 100, 20);
    stackLabel->SetText("Stack Trace:");
    AddChild(stackLabel);
    y += 22;
    
    crashStackBox_ = std::make_shared<UltraCanvasTextBox>("crash_stack", 0, 16, y, GetWidth() - 32, 120);
    crashStackBox_->SetReadOnly(true);
    crashStackBox_->SetFont("Consolas", 10);
    AddChild(crashStackBox_);
    
    // Buttons
    int btnY = GetHeight() - 48;
    
    debugButton_ = std::make_shared<UltraCanvasButton>("debug_btn", 0, 16, btnY, 120, 28);
    debugButton_->SetText("View in Debugger");
    debugButton_->SetOnClick([this]() {
        if (onViewInDebugger) onViewInDebugger();
        Close();
    });
    AddChild(debugButton_);
    
    saveButton_ = std::make_shared<UltraCanvasButton>("save_btn", 0, 144, btnY, 100, 28);
    saveButton_->SetText("Save Report");
    saveButton_->SetOnClick([this]() {
        if (onSaveReport) onSaveReport();
    });
    AddChild(saveButton_);
    
    terminateButton_ = std::make_shared<UltraCanvasButton>("terminate_btn", 0, GetWidth() - 96, btnY, 80, 28);
    terminateButton_->SetText("Terminate");
    terminateButton_->SetOnClick([this]() {
        if (onTerminate) onTerminate();
        Close();
    });
    AddChild(terminateButton_);
}

void UCCrashReportDialog::UpdateContent() {
    if (isSignal_) {
        if (crashTypeLabel_) {
            crashTypeLabel_->SetText("Signal: " + signal_.signalName + " - " + signal_.description);
        }
        if (crashLocationLabel_) {
            std::ostringstream loc;
            loc << "Address: 0x" << std::hex << signal_.address;
            crashLocationLabel_->SetText(loc.str());
        }
        if (crashDetailsBox_) {
            crashDetailsBox_->SetText(signal_.additionalInfo);
        }
    } else {
        if (crashTypeLabel_) {
            crashTypeLabel_->SetText("Exception: " + exception_.type);
        }
        if (crashLocationLabel_) {
            std::ostringstream loc;
            if (!exception_.functionName.empty()) loc << exception_.functionName;
            if (!exception_.sourceFile.empty()) loc << " at " << exception_.sourceFile << ":" << exception_.sourceLine;
            crashLocationLabel_->SetText(loc.str());
        }
        if (crashDetailsBox_) {
            crashDetailsBox_->SetText(exception_.message);
        }
    }
    
    if (crashStackBox_) {
        crashStackBox_->SetText(stackTrace_);
    }
}

std::string UCCrashReportDialog::GenerateCrashReport() const {
    std::ostringstream report;
    
    auto now = std::time(nullptr);
    report << "=== ULTRA IDE Crash Report ===" << std::endl;
    report << "Time: " << std::ctime(&now);
    report << std::endl;
    
    if (isSignal_) {
        report << "Signal: " << signal_.signalName << " (" << signal_.signalNumber << ")" << std::endl;
        report << "Description: " << signal_.description << std::endl;
        report << "Address: 0x" << std::hex << signal_.address << std::dec << std::endl;
        report << "Thread: " << signal_.threadId << std::endl;
    } else {
        report << "Exception: " << exception_.type << std::endl;
        report << "Message: " << exception_.message << std::endl;
        report << "Location: " << exception_.functionName;
        if (!exception_.sourceFile.empty()) {
            report << " at " << exception_.sourceFile << ":" << exception_.sourceLine;
        }
        report << std::endl;
    }
    
    report << std::endl << "Stack Trace:" << std::endl;
    report << stackTrace_ << std::endl;
    
    return report.str();
}

void UCCrashReportDialog::HandleRender(IRenderContext* ctx) {
    UltraCanvasDialog::HandleRender(ctx);
}

} // namespace IDE
} // namespace UltraCanvas
