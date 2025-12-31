// Apps/IDE/Debug/Exceptions/UCExceptionDialog.h
// Exception Dialog for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasDialog.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasTreeView.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasButton.h"
#include "UltraCanvasCheckbox.h"
#include "UltraCanvasTextBox.h"
#include "UltraCanvasDropDown.h"
#include "UltraCanvasTabControl.h"
#include "UCExceptionManager.h"
#include "UCSignalHandler.h"
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Exception settings dialog
 * 
 * Provides UI for configuring exception breakpoints, signal handling,
 * and exception filters.
 */
class UCExceptionSettingsDialog : public UltraCanvasDialog {
public:
    UCExceptionSettingsDialog(const std::string& id, int width = 650, int height = 500);
    ~UCExceptionSettingsDialog() override = default;
    
    void Initialize();
    void SetExceptionManager(UCExceptionManager* manager);
    void SetSignalHandler(UCSignalHandler* handler);
    
    void Refresh();
    
    std::function<void()> onSettingsChanged;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void CreateCppExceptionsTab();
    void CreateSignalsTab();
    void CreateFiltersTab();
    void CreateButtonPanel();
    
    void PopulateCppExceptions();
    void PopulateSignals();
    void PopulateFilters();
    
    void OnExceptionCheckChanged(const std::string& name, bool checked);
    void OnExceptionBehaviorChanged(const std::string& name, ExceptionBreakBehavior behavior);
    void OnSignalCheckChanged(int signalNumber, bool stop, bool print, bool pass);
    
    void OnAddFilter();
    void OnRemoveFilter();
    void OnApply();
    void OnOK();
    void OnCancel();
    
    UCExceptionManager* exceptionManager_ = nullptr;
    UCSignalHandler* signalHandler_ = nullptr;
    
    std::shared_ptr<UltraCanvasTabControl> tabControl_;
    
    // C++ Exceptions tab
    std::shared_ptr<UltraCanvasContainer> cppTab_;
    std::shared_ptr<UltraCanvasCheckbox> breakOnAllCppCheckbox_;
    std::shared_ptr<UltraCanvasCheckbox> breakOnUnhandledCppCheckbox_;
    std::shared_ptr<UltraCanvasTreeView> cppExceptionsTree_;
    std::shared_ptr<UltraCanvasTextBox> addExceptionInput_;
    std::shared_ptr<UltraCanvasButton> addExceptionButton_;
    
    // Signals tab
    std::shared_ptr<UltraCanvasContainer> signalsTab_;
    std::shared_ptr<UltraCanvasTreeView> signalsTree_;
    std::shared_ptr<UltraCanvasButton> stopAllButton_;
    std::shared_ptr<UltraCanvasButton> stopNoneButton_;
    std::shared_ptr<UltraCanvasButton> resetDefaultsButton_;
    
    // Filters tab
    std::shared_ptr<UltraCanvasContainer> filtersTab_;
    std::shared_ptr<UltraCanvasTreeView> filtersTree_;
    std::shared_ptr<UltraCanvasTextBox> filterPatternInput_;
    std::shared_ptr<UltraCanvasDropDown> filterBehaviorDropdown_;
    std::shared_ptr<UltraCanvasButton> addFilterButton_;
    std::shared_ptr<UltraCanvasButton> removeFilterButton_;
    
    // Button panel
    std::shared_ptr<UltraCanvasButton> okButton_;
    std::shared_ptr<UltraCanvasButton> cancelButton_;
    std::shared_ptr<UltraCanvasButton> applyButton_;
    
    // Node mappings
    std::map<TreeNodeId, std::string> cppNodeToName_;
    std::map<TreeNodeId, int> signalNodeToNumber_;
    std::map<TreeNodeId, int> filterNodeToId_;
};

/**
 * @brief Exception caught notification dialog
 * 
 * Shown when an exception is caught during debugging.
 */
class UCExceptionCaughtDialog : public UltraCanvasDialog {
public:
    UCExceptionCaughtDialog(const std::string& id);
    ~UCExceptionCaughtDialog() override = default;
    
    void Initialize();
    void SetException(const CaughtException& exception);
    
    enum class UserAction {
        Break,          // Break at exception
        Continue,       // Continue execution
        Ignore,         // Ignore this exception type
        IgnoreAll       // Ignore all exceptions of this type
    };
    
    UserAction GetUserAction() const { return userAction_; }
    
    std::function<void(UserAction)> onActionSelected;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void UpdateContent();
    
    CaughtException exception_;
    UserAction userAction_ = UserAction::Break;
    
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasLabel> typeLabel_;
    std::shared_ptr<UltraCanvasLabel> messageLabel_;
    std::shared_ptr<UltraCanvasLabel> locationLabel_;
    std::shared_ptr<UltraCanvasTextBox> stackTraceBox_;
    
    std::shared_ptr<UltraCanvasButton> breakButton_;
    std::shared_ptr<UltraCanvasButton> continueButton_;
    std::shared_ptr<UltraCanvasButton> ignoreButton_;
    std::shared_ptr<UltraCanvasCheckbox> ignoreAllCheckbox_;
};

/**
 * @brief Crash report dialog
 * 
 * Shown when a fatal exception or signal occurs.
 */
class UCCrashReportDialog : public UltraCanvasDialog {
public:
    UCCrashReportDialog(const std::string& id);
    ~UCCrashReportDialog() override = default;
    
    void Initialize();
    void SetCrashInfo(const CaughtException& exception, const std::string& stackTrace);
    void SetSignalInfo(const CaughtSignal& signal, const std::string& stackTrace);
    
    std::function<void()> onViewInDebugger;
    std::function<void()> onSaveReport;
    std::function<void()> onTerminate;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void UpdateContent();
    std::string GenerateCrashReport() const;
    
    bool isSignal_ = false;
    CaughtException exception_;
    CaughtSignal signal_;
    std::string stackTrace_;
    
    std::shared_ptr<UltraCanvasLabel> crashTitleLabel_;
    std::shared_ptr<UltraCanvasLabel> crashTypeLabel_;
    std::shared_ptr<UltraCanvasLabel> crashLocationLabel_;
    std::shared_ptr<UltraCanvasTextBox> crashDetailsBox_;
    std::shared_ptr<UltraCanvasTextBox> crashStackBox_;
    
    std::shared_ptr<UltraCanvasButton> debugButton_;
    std::shared_ptr<UltraCanvasButton> saveButton_;
    std::shared_ptr<UltraCanvasButton> terminateButton_;
};

} // namespace IDE
} // namespace UltraCanvas
