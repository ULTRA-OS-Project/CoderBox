// Apps/IDE/Debug/Editor/UCDebugEditorIntegration.cpp
// Debug Editor Integration Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDebugEditorIntegration.h"
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

UCDebugEditorIntegration::UCDebugEditorIntegration() {
    gutterRenderer_ = std::make_unique<UCDebugGutterRenderer>();
    lineHighlighter_ = std::make_unique<UCCurrentLineHighlighter>();
    hoverEvaluator_ = std::make_unique<UCHoverEvaluator>();
}

UCDebugEditorIntegration::~UCDebugEditorIntegration() {
    Shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UCDebugEditorIntegration::SetTextArea(UltraCanvasTextArea* textArea) {
    textArea_ = textArea;
    
    gutterRenderer_->SetTextArea(textArea);
    lineHighlighter_->SetTextArea(textArea);
    hoverEvaluator_->SetTextArea(textArea);
}

void UCDebugEditorIntegration::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    gutterRenderer_->SetDebugManager(manager);
    lineHighlighter_->SetDebugManager(manager);
    hoverEvaluator_->SetDebugManager(manager);
}

void UCDebugEditorIntegration::SetBreakpointManager(UCBreakpointManager* manager) {
    breakpointManager_ = manager;
    gutterRenderer_->SetBreakpointManager(manager);
}

void UCDebugEditorIntegration::Initialize() {
    if (initialized_) return;
    
    SetupTextAreaCallbacks();
    SetupDebugManagerCallbacks();
    
    gutterRenderer_->onBreakpointMarginClicked = [this](int line) {
        ToggleBreakpoint(line);
    };
    
    gutterRenderer_->onBreakpointMarginRightClicked = [this](int line) {
        if (onRequestBreakpointDialog) {
            onRequestBreakpointDialog();
        }
    };
    
    gutterRenderer_->RefreshDecorations();
    initialized_ = true;
}

void UCDebugEditorIntegration::Shutdown() {
    if (!initialized_) return;
    ClearInlineValues();
    lineHighlighter_->ClearAllHighlights();
    initialized_ = false;
}

void UCDebugEditorIntegration::SetFilePath(const std::string& path) {
    filePath_ = NormalizePath(path);
    gutterRenderer_->RefreshDecorations();
}

bool UCDebugEditorIntegration::IsAtCurrentDebugLocation() const {
    if (!debugManager_ || !debugManager_->IsDebugging()) return false;
    return lineHighlighter_->GetCurrentLine() >= 0;
}

// ============================================================================
// RENDERING
// ============================================================================

void UCDebugEditorIntegration::Render(IRenderContext* ctx, 
                                       const Rect2Di& gutterRect,
                                       const Rect2Di& textRect,
                                       int firstVisibleLine,
                                       int lastVisibleLine,
                                       int lineHeight, int scrollX) {
    lineHighlighter_->RenderHighlights(ctx, textRect, firstVisibleLine,
                                        lastVisibleLine, lineHeight, scrollX);
    gutterRenderer_->Render(ctx, gutterRect, firstVisibleLine,
                            lastVisibleLine, lineHeight);
}

void UCDebugEditorIntegration::RenderInlineValues(IRenderContext* ctx,
                                                   const Rect2Di& textRect,
                                                   int firstVisibleLine,
                                                   int lastVisibleLine,
                                                   int lineHeight) {
    if (inlineValueMode_ == InlineValueMode::Off || inlineDecorations_.empty()) return;
    
    ctx->SetFont("Consolas", 10);
    
    for (const auto& decoration : inlineDecorations_) {
        if (decoration.line < firstVisibleLine || decoration.line > lastVisibleLine) continue;
        
        int y = textRect.y + (decoration.line - firstVisibleLine) * lineHeight;
        int x = textRect.x + textRect.width - 200;
        int textWidth = static_cast<int>(decoration.text.length()) * 7;
        
        ctx->DrawFilledRectangle(Rect2Di(x - 4, y + 2, textWidth + 8, lineHeight - 4),
                                 Color(255, 255, 220, 200));
        ctx->DrawText(decoration.text, Point2Di(x, y + lineHeight - 4), decoration.color);
    }
}

// ============================================================================
// EVENT HANDLING
// ============================================================================

bool UCDebugEditorIntegration::HandleGutterClick(int x, int y, bool rightClick) {
    if (!gutterRenderer_->IsInBreakpointMargin(x, y)) return false;
    
    int line = gutterRenderer_->GetLineAtY(y);
    if (line < 0) return false;
    
    if (rightClick) {
        if (gutterRenderer_->onBreakpointMarginRightClicked)
            gutterRenderer_->onBreakpointMarginRightClicked(line);
    } else {
        if (gutterRenderer_->onBreakpointMarginClicked)
            gutterRenderer_->onBreakpointMarginClicked(line);
    }
    return true;
}

void UCDebugEditorIntegration::HandleMouseMove(int x, int y) {
    hoverEvaluator_->OnMouseMove(x, y);
}

void UCDebugEditorIntegration::HandleMouseLeave() {
    hoverEvaluator_->OnMouseLeave();
}

bool UCDebugEditorIntegration::HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) {
    // F9 - Toggle breakpoint
    if (keyCode == 120 && !ctrl && !shift && !alt) {
        if (textArea_) {
            ToggleBreakpoint(textArea_->GetCursorLine());
            return true;
        }
    }
    
    // Ctrl+F9 - Disable breakpoint
    if (keyCode == 120 && ctrl && !shift && !alt) {
        if (textArea_ && breakpointManager_) {
            int line = textArea_->GetCursorLine();
            auto bp = breakpointManager_->GetBreakpointAt(filePath_, line);
            if (bp) {
                breakpointManager_->SetBreakpointEnabled(bp->GetId(), !bp->IsEnabled());
                return true;
            }
        }
    }
    
    // Ctrl+Shift+F9 - Remove all breakpoints in file
    if (keyCode == 120 && ctrl && shift && !alt) {
        if (breakpointManager_) {
            auto bps = breakpointManager_->GetBreakpointsInFile(filePath_);
            for (const auto& bp : bps) {
                breakpointManager_->RemoveBreakpoint(bp.GetId());
            }
            return true;
        }
    }
    
    // Ctrl+F10 - Run to cursor
    if (keyCode == 121 && ctrl && !shift && !alt) {
        if (textArea_ && onRunToCursor) {
            onRunToCursor(textArea_->GetCursorLine());
            return true;
        }
    }
    
    // Ctrl+Shift+F10 - Set next statement
    if (keyCode == 121 && ctrl && shift && !alt) {
        if (textArea_ && onSetNextStatement) {
            onSetNextStatement(textArea_->GetCursorLine());
            return true;
        }
    }
    
    return false;
}

void UCDebugEditorIntegration::Update() {
    hoverEvaluator_->Update();
}

// ============================================================================
// BREAKPOINT OPERATIONS
// ============================================================================

void UCDebugEditorIntegration::ToggleBreakpoint(int line) {
    if (!breakpointManager_) return;
    breakpointManager_->ToggleBreakpoint(filePath_, line);
    if (onBreakpointToggled) onBreakpointToggled(line);
}

int UCDebugEditorIntegration::AddBreakpoint(int line) {
    if (!breakpointManager_) return -1;
    return breakpointManager_->AddLineBreakpoint(filePath_, line);
}

bool UCDebugEditorIntegration::RemoveBreakpoint(int line) {
    if (!breakpointManager_) return false;
    auto bp = breakpointManager_->GetBreakpointAt(filePath_, line);
    if (bp) {
        return breakpointManager_->RemoveBreakpoint(bp->GetId());
    }
    return false;
}

int UCDebugEditorIntegration::AddConditionalBreakpoint(int line, const std::string& condition) {
    if (!breakpointManager_) return -1;
    return breakpointManager_->AddConditionalBreakpoint(filePath_, line, condition);
}

int UCDebugEditorIntegration::AddLogpoint(int line, const std::string& message) {
    if (!breakpointManager_) return -1;
    return breakpointManager_->AddLogpoint(filePath_, line, message);
}

bool UCDebugEditorIntegration::HasBreakpoint(int line) const {
    if (!breakpointManager_) return false;
    return breakpointManager_->HasBreakpointAt(filePath_, line);
}

// ============================================================================
// NAVIGATION
// ============================================================================

void UCDebugEditorIntegration::GoToCurrentLocation() {
    int line = lineHighlighter_->GetCurrentLine();
    if (line > 0) GoToLine(line);
}

void UCDebugEditorIntegration::GoToLine(int line) {
    if (textArea_) {
        textArea_->ScrollToLine(line);
        textArea_->SetCursorPosition(line, 1);
    }
}

void UCDebugEditorIntegration::GoToNextBreakpoint() {
    if (!breakpointManager_ || !textArea_) return;
    
    int currentLine = textArea_->GetCursorLine();
    auto lines = breakpointManager_->GetBreakpointLines(filePath_);
    
    for (int bpLine : lines) {
        if (bpLine > currentLine) {
            GoToLine(bpLine);
            return;
        }
    }
    
    // Wrap around
    if (!lines.empty()) {
        GoToLine(lines[0]);
    }
}

void UCDebugEditorIntegration::GoToPreviousBreakpoint() {
    if (!breakpointManager_ || !textArea_) return;
    
    int currentLine = textArea_->GetCursorLine();
    auto lines = breakpointManager_->GetBreakpointLines(filePath_);
    
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (*it < currentLine) {
            GoToLine(*it);
            return;
        }
    }
    
    // Wrap around
    if (!lines.empty()) {
        GoToLine(lines.back());
    }
}

// ============================================================================
// INLINE VALUES
// ============================================================================

void UCDebugEditorIntegration::RefreshInlineValues() {
    ClearInlineValues();
    
    if (inlineValueMode_ == InlineValueMode::Off) return;
    if (!debugManager_ || !debugManager_->IsDebugging()) return;
    if (debugManager_->GetState() != DebugSessionState::Paused) return;
    
    int currentLine = lineHighlighter_->GetCurrentLine();
    if (currentLine < 0) return;
    
    // Get local variables and display their values
    auto locals = debugManager_->GetLocals();
    
    for (const auto& var : locals) {
        if (inlineValueMode_ == InlineValueMode::Simple && var.hasChildren) {
            continue;  // Skip complex types in simple mode
        }
        
        std::string displayValue = var.value;
        if (displayValue.length() > 30) {
            displayValue = displayValue.substr(0, 30) + "...";
        }
        
        std::ostringstream text;
        text << var.name << " = " << displayValue;
        
        Color color = var.errorMessage.empty() ? Color(128, 128, 128) : Color(200, 0, 0);
        
        inlineDecorations_.emplace_back(currentLine, 0, text.str(), color);
    }
}

void UCDebugEditorIntegration::ClearInlineValues() {
    inlineDecorations_.clear();
}

// ============================================================================
// SETTINGS
// ============================================================================

void UCDebugEditorIntegration::SetHoverEvaluationEnabled(bool enabled) {
    hoverEvaluator_->SetEnabled(enabled);
}

bool UCDebugEditorIntegration::IsHoverEvaluationEnabled() const {
    return hoverEvaluator_->IsEnabled();
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void UCDebugEditorIntegration::SetupTextAreaCallbacks() {
    // Text area event integration would go here
}

void UCDebugEditorIntegration::SetupDebugManagerCallbacks() {
    if (!debugManager_) return;
    
    debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
        if (PathsMatch(loc.filePath, filePath_)) {
            UpdateFromDebugState();
            RefreshInlineValues();
        }
    };
    
    debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
        if (newState == DebugSessionState::Running ||
            newState == DebugSessionState::Inactive) {
            ClearInlineValues();
        }
    };
}

void UCDebugEditorIntegration::UpdateFromDebugState() {
    gutterRenderer_->RefreshDecorations();
}

void UCDebugEditorIntegration::UpdateInlineValuesForLine(int line) {
    // Update inline values for specific line
}

std::string UCDebugEditorIntegration::NormalizePath(const std::string& path) const {
    std::string normalized = path;
    
    // Convert backslashes to forward slashes
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // Remove trailing slash
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

bool UCDebugEditorIntegration::PathsMatch(const std::string& path1, 
                                           const std::string& path2) const {
    return NormalizePath(path1) == NormalizePath(path2);
}

} // namespace IDE
} // namespace UltraCanvas
