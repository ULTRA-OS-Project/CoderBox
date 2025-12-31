// Apps/IDE/Debug/UI/UCDebugToolbar.cpp
// Debug Toolbar Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDebugToolbar.h"

namespace UltraCanvas {
namespace IDE {

UCDebugToolbar::UCDebugToolbar(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasToolbar(id, 0, x, y, width, height) {
    
    // Configure toolbar appearance
    ToolbarAppearance appearance;
    appearance.backgroundColor = Color(240, 240, 240);
    appearance.borderColor = Color(200, 200, 200);
    appearance.buttonSpacing = 4;
    appearance.padding = 4;
    SetAppearance(appearance);
    
    SetOrientation(ToolbarOrientation::Horizontal);
}

void UCDebugToolbar::Initialize() {
    CreateButtons();
    SetupCallbacks();
    UpdateButtonStates();
}

void UCDebugToolbar::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        // Subscribe to state changes
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            UpdateState(newState);
        };
    }
}

void UCDebugToolbar::CreateButtons() {
    // Start/Continue button (F5)
    AddButton("debug_start", "Start", "icons/debug_start.png", [this]() {
        if (currentState_ == DebugSessionState::Inactive) {
            if (onStartDebug) onStartDebug();
        } else if (currentState_ == DebugSessionState::Paused) {
            if (onContinueDebug) onContinueDebug();
        }
    });
    startButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_start").get());
    if (startButton_) {
        startButton_->SetTooltip("Start Debugging (F5)");
    }
    
    // Pause button
    AddButton("debug_pause", "", "icons/debug_pause.png", [this]() {
        if (onPauseDebug) onPauseDebug();
    });
    pauseButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_pause").get());
    if (pauseButton_) {
        pauseButton_->SetTooltip("Pause Execution");
    }
    
    // Stop button (Shift+F5)
    AddButton("debug_stop", "", "icons/debug_stop.png", [this]() {
        if (onStopDebug) onStopDebug();
    });
    stopButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_stop").get());
    if (stopButton_) {
        stopButton_->SetTooltip("Stop Debugging (Shift+F5)");
    }
    
    // Restart button (Ctrl+Shift+F5)
    AddButton("debug_restart", "", "icons/debug_restart.png", [this]() {
        if (onRestartDebug) onRestartDebug();
    });
    restartButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_restart").get());
    if (restartButton_) {
        restartButton_->SetTooltip("Restart Debugging (Ctrl+Shift+F5)");
    }
    
    // Separator
    AddSeparator("sep1");
    
    // Step Over button (F10)
    AddButton("debug_step_over", "", "icons/debug_step_over.png", [this]() {
        if (onStepOver) onStepOver();
    });
    stepOverButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_step_over").get());
    if (stepOverButton_) {
        stepOverButton_->SetTooltip("Step Over (F10)");
    }
    
    // Step Into button (F11)
    AddButton("debug_step_into", "", "icons/debug_step_into.png", [this]() {
        if (onStepInto) onStepInto();
    });
    stepIntoButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_step_into").get());
    if (stepIntoButton_) {
        stepIntoButton_->SetTooltip("Step Into (F11)");
    }
    
    // Step Out button (Shift+F11)
    AddButton("debug_step_out", "", "icons/debug_step_out.png", [this]() {
        if (onStepOut) onStepOut();
    });
    stepOutButton_ = dynamic_cast<UltraCanvasToolbarButton*>(GetItem("debug_step_out").get());
    if (stepOutButton_) {
        stepOutButton_->SetTooltip("Step Out (Shift+F11)");
    }
}

void UCDebugToolbar::SetupCallbacks() {
    // Default callbacks that use debug manager directly
    if (!onStartDebug) {
        onStartDebug = [this]() {
            if (debugManager_) {
                // Would need project context to start debugging
                // debugManager_->StartDebugging(...);
            }
        };
    }
    
    if (!onStopDebug) {
        onStopDebug = [this]() {
            if (debugManager_) {
                debugManager_->StopDebugging();
            }
        };
    }
    
    if (!onPauseDebug) {
        onPauseDebug = [this]() {
            if (debugManager_) {
                debugManager_->Pause();
            }
        };
    }
    
    if (!onContinueDebug) {
        onContinueDebug = [this]() {
            if (debugManager_) {
                debugManager_->Continue();
            }
        };
    }
    
    if (!onStepOver) {
        onStepOver = [this]() {
            if (debugManager_) {
                debugManager_->StepOver();
            }
        };
    }
    
    if (!onStepInto) {
        onStepInto = [this]() {
            if (debugManager_) {
                debugManager_->StepInto();
            }
        };
    }
    
    if (!onStepOut) {
        onStepOut = [this]() {
            if (debugManager_) {
                debugManager_->StepOut();
            }
        };
    }
    
    if (!onRestartDebug) {
        onRestartDebug = [this]() {
            if (debugManager_) {
                debugManager_->RestartDebugging();
            }
        };
    }
}

void UCDebugToolbar::UpdateState(DebugSessionState state) {
    currentState_ = state;
    UpdateButtonStates();
}

void UCDebugToolbar::EnableControls(bool running, bool paused) {
    // Update based on explicit running/paused state
    if (startButton_) {
        startButton_->SetDisabled(running && !paused);
        if (paused) {
            startButton_->SetText("Continue");
            startButton_->SetIcon("icons/debug_continue.png");
        } else {
            startButton_->SetText("Start");
            startButton_->SetIcon("icons/debug_start.png");
        }
    }
    
    if (pauseButton_) pauseButton_->SetDisabled(!running || paused);
    if (stopButton_) stopButton_->SetDisabled(!running);
    if (restartButton_) restartButton_->SetDisabled(!running);
    
    // Stepping only available when paused
    if (stepOverButton_) stepOverButton_->SetDisabled(!paused);
    if (stepIntoButton_) stepIntoButton_->SetDisabled(!paused);
    if (stepOutButton_) stepOutButton_->SetDisabled(!paused);
}

void UCDebugToolbar::UpdateButtonStates() {
    bool running = false;
    bool paused = false;
    
    switch (currentState_) {
        case DebugSessionState::Inactive:
        case DebugSessionState::Terminated:
            running = false;
            paused = false;
            break;
            
        case DebugSessionState::Starting:
        case DebugSessionState::Running:
        case DebugSessionState::Stepping:
            running = true;
            paused = false;
            break;
            
        case DebugSessionState::Paused:
            running = true;
            paused = true;
            break;
            
        case DebugSessionState::Stopping:
            running = true;
            paused = false;
            break;
            
        case DebugSessionState::ErrorState:
            running = false;
            paused = false;
            break;
    }
    
    EnableControls(running, paused);
}

} // namespace IDE
} // namespace UltraCanvas
