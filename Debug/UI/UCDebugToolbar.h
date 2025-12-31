// Apps/IDE/Debug/UI/UCDebugToolbar.h
// Debug Toolbar for ULTRA IDE using UltraCanvas Toolbar
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasToolbar.h"
#include "UltraCanvasUIElement.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Debug toolbar using UltraCanvasToolbar
 * 
 * Provides standard debug controls: Start, Stop, Pause, Step Over, Step Into, Step Out
 * Uses UltraCanvasToolbar and UltraCanvasToolbarButton from the framework.
 */
class UCDebugToolbar : public UltraCanvasToolbar {
public:
    UCDebugToolbar(const std::string& id, int x, int y, int width, int height);
    ~UCDebugToolbar() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // STATE UPDATES
    // =========================================================================
    
    void UpdateState(DebugSessionState state);
    void EnableControls(bool running, bool paused);
    
    // =========================================================================
    // BUTTON ACCESS
    // =========================================================================
    
    UltraCanvasToolbarButton* GetStartButton() const { return startButton_; }
    UltraCanvasToolbarButton* GetStopButton() const { return stopButton_; }
    UltraCanvasToolbarButton* GetPauseButton() const { return pauseButton_; }
    UltraCanvasToolbarButton* GetContinueButton() const { return continueButton_; }
    UltraCanvasToolbarButton* GetStepOverButton() const { return stepOverButton_; }
    UltraCanvasToolbarButton* GetStepIntoButton() const { return stepIntoButton_; }
    UltraCanvasToolbarButton* GetStepOutButton() const { return stepOutButton_; }
    UltraCanvasToolbarButton* GetRestartButton() const { return restartButton_; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void()> onStartDebug;
    std::function<void()> onStopDebug;
    std::function<void()> onPauseDebug;
    std::function<void()> onContinueDebug;
    std::function<void()> onStepOver;
    std::function<void()> onStepInto;
    std::function<void()> onStepOut;
    std::function<void()> onRestartDebug;
    
private:
    void CreateButtons();
    void SetupCallbacks();
    void UpdateButtonStates();
    
    UCDebugManager* debugManager_ = nullptr;
    DebugSessionState currentState_ = DebugSessionState::Inactive;
    
    // Button references (owned by toolbar)
    UltraCanvasToolbarButton* startButton_ = nullptr;
    UltraCanvasToolbarButton* stopButton_ = nullptr;
    UltraCanvasToolbarButton* pauseButton_ = nullptr;
    UltraCanvasToolbarButton* continueButton_ = nullptr;
    UltraCanvasToolbarButton* stepOverButton_ = nullptr;
    UltraCanvasToolbarButton* stepIntoButton_ = nullptr;
    UltraCanvasToolbarButton* stepOutButton_ = nullptr;
    UltraCanvasToolbarButton* restartButton_ = nullptr;
};

} // namespace IDE
} // namespace UltraCanvas
