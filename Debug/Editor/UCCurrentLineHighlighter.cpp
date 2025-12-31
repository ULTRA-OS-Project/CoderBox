// Apps/IDE/Debug/Editor/UCCurrentLineHighlighter.cpp
// Current Line Highlighter Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCCurrentLineHighlighter.h"
#include <algorithm>
#include <cmath>

namespace UltraCanvas {
namespace IDE {

UCCurrentLineHighlighter::UCCurrentLineHighlighter() = default;

void UCCurrentLineHighlighter::SetTextArea(UltraCanvasTextArea* textArea) {
    textArea_ = textArea;
}

void UCCurrentLineHighlighter::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        // Subscribe to stop events
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            UpdateHighlightsFromDebugger();
            
            // Set current line highlight type based on stop reason
            LineHighlightType type = LineHighlightType::CurrentStatement;
            if (reason == StopReason::Exception) {
                type = LineHighlightType::Exception;
            } else if (reason == StopReason::Breakpoint) {
                type = LineHighlightType::HitBreakpoint;
            }
            
            if (loc.line > 0) {
                SetCurrentLine(loc.line, type);
                ScrollToCurrentLine();
            }
        };
        
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            if (newState == DebugSessionState::Running ||
                newState == DebugSessionState::Inactive ||
                newState == DebugSessionState::Terminated) {
                ClearCurrentLine();
                ClearCallStackFrames();
                ClearExceptionLine();
            }
        };
    }
}

// ============================================================================
// RENDERING
// ============================================================================

void UCCurrentLineHighlighter::RenderHighlights(IRenderContext* ctx, 
                                                 const Rect2Di& textRect,
                                                 int firstVisibleLine, 
                                                 int lastVisibleLine,
                                                 int lineHeight, int scrollX) {
    // Update animation phase
    if (animateHighlights_) {
        animationPhase_ += 0.05f;
        if (animationPhase_ > 2.0f * 3.14159f) {
            animationPhase_ -= 2.0f * 3.14159f;
        }
    }
    
    // Collect highlights for visible lines
    std::vector<LineHighlight> visibleHighlights;
    
    for (int line = firstVisibleLine; line <= lastVisibleLine; line++) {
        auto it = highlights_.find(line);
        if (it != highlights_.end()) {
            visibleHighlights.push_back(it->second);
        }
    }
    
    // Add current line if visible
    if (currentLine_ >= firstVisibleLine && currentLine_ <= lastVisibleLine) {
        bool alreadyHighlighted = false;
        for (const auto& h : visibleHighlights) {
            if (h.line == currentLine_) {
                alreadyHighlighted = true;
                break;
            }
        }
        
        if (!alreadyHighlighted) {
            visibleHighlights.push_back(LineHighlight(currentLine_, currentLineType_));
        }
    }
    
    // Add exception line if visible
    if (exceptionLine_ >= firstVisibleLine && exceptionLine_ <= lastVisibleLine) {
        LineHighlight exHl(exceptionLine_, LineHighlightType::Exception);
        visibleHighlights.push_back(exHl);
    }
    
    // Sort by priority (less important first, so important ones render on top)
    std::sort(visibleHighlights.begin(), visibleHighlights.end(),
              [](const LineHighlight& a, const LineHighlight& b) {
                  // Exception highest, then current statement
                  auto priority = [](LineHighlightType t) {
                      switch (t) {
                          case LineHighlightType::Exception: return 100;
                          case LineHighlightType::CurrentStatement: return 90;
                          case LineHighlightType::HitBreakpoint: return 80;
                          case LineHighlightType::CallStackFrame: return 70;
                          default: return 50;
                      }
                  };
                  return priority(a.type) < priority(b.type);
              });
    
    // Render highlights
    for (const auto& highlight : visibleHighlights) {
        RenderHighlight(ctx, highlight, textRect, firstVisibleLine, lineHeight, scrollX);
    }
}

void UCCurrentLineHighlighter::RenderHighlight(IRenderContext* ctx,
                                                const LineHighlight& highlight,
                                                const Rect2Di& textRect,
                                                int firstVisibleLine,
                                                int lineHeight, int scrollX) {
    int y = textRect.y + (highlight.line - firstVisibleLine) * lineHeight;
    int width = highlight.fullWidth ? textRect.width : textRect.width - scrollX;
    int x = textRect.x;
    
    // Apply opacity
    Color bgColor = ApplyOpacity(highlight.backgroundColor, 
                                  highlight.opacity * highlightOpacity_);
    
    // Apply animation for current line
    if (animateHighlights_ && 
        (highlight.type == LineHighlightType::CurrentStatement ||
         highlight.type == LineHighlightType::Exception)) {
        // Subtle pulse effect
        float pulse = 0.9f + 0.1f * std::sin(animationPhase_);
        bgColor = Color(
            static_cast<uint8_t>(bgColor.r * pulse),
            static_cast<uint8_t>(bgColor.g * pulse),
            static_cast<uint8_t>(bgColor.b * pulse),
            bgColor.a
        );
    }
    
    // Draw background
    ctx->DrawFilledRectangle(Rect2Di(x, y, width, lineHeight), bgColor);
    
    // Draw border if specified
    if (highlight.borderWidth > 0 && highlight.borderColor != Colors::Transparent) {
        Color borderColor = ApplyOpacity(highlight.borderColor, highlightOpacity_);
        
        // Top border
        ctx->DrawLine(Point2Di(x, y), Point2Di(x + width, y),
                      borderColor, highlight.borderWidth);
        // Bottom border
        ctx->DrawLine(Point2Di(x, y + lineHeight - 1), 
                      Point2Di(x + width, y + lineHeight - 1),
                      borderColor, highlight.borderWidth);
    }
}

// ============================================================================
// HIGHLIGHT MANAGEMENT
// ============================================================================

void UCCurrentLineHighlighter::SetCurrentLine(int line, LineHighlightType type) {
    int oldLine = currentLine_;
    currentLine_ = line;
    currentLineType_ = type;
    
    if (oldLine != line && onCurrentLineChanged) {
        onCurrentLineChanged(line);
    }
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCCurrentLineHighlighter::ClearCurrentLine() {
    if (currentLine_ >= 0) {
        currentLine_ = -1;
        
        if (textArea_) {
            textArea_->Invalidate();
        }
    }
}

void UCCurrentLineHighlighter::AddHighlight(int line, LineHighlightType type) {
    AddHighlight(LineHighlight(line, type));
}

void UCCurrentLineHighlighter::AddHighlight(const LineHighlight& highlight) {
    highlights_[highlight.line] = highlight;
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCCurrentLineHighlighter::RemoveHighlight(int line) {
    auto it = highlights_.find(line);
    if (it != highlights_.end()) {
        highlights_.erase(it);
        
        if (textArea_) {
            textArea_->Invalidate();
        }
    }
}

void UCCurrentLineHighlighter::RemoveHighlights(LineHighlightType type) {
    bool changed = false;
    
    for (auto it = highlights_.begin(); it != highlights_.end(); ) {
        if (it->second.type == type) {
            it = highlights_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    
    if (changed && textArea_) {
        textArea_->Invalidate();
    }
}

void UCCurrentLineHighlighter::ClearAllHighlights() {
    if (!highlights_.empty()) {
        highlights_.clear();
        
        if (textArea_) {
            textArea_->Invalidate();
        }
    }
}

bool UCCurrentLineHighlighter::HasHighlight(int line) const {
    return highlights_.find(line) != highlights_.end() ||
           line == currentLine_ ||
           line == exceptionLine_;
}

LineHighlight UCCurrentLineHighlighter::GetHighlight(int line) const {
    auto it = highlights_.find(line);
    if (it != highlights_.end()) {
        return it->second;
    }
    
    if (line == currentLine_) {
        return LineHighlight(line, currentLineType_);
    }
    
    if (line == exceptionLine_) {
        return LineHighlight(line, LineHighlightType::Exception);
    }
    
    return LineHighlight();
}

std::vector<LineHighlight> UCCurrentLineHighlighter::GetAllHighlights() const {
    std::vector<LineHighlight> result;
    result.reserve(highlights_.size() + 2);
    
    for (const auto& pair : highlights_) {
        result.push_back(pair.second);
    }
    
    if (currentLine_ >= 0) {
        result.push_back(LineHighlight(currentLine_, currentLineType_));
    }
    
    if (exceptionLine_ >= 0) {
        result.push_back(LineHighlight(exceptionLine_, LineHighlightType::Exception));
    }
    
    return result;
}

// ============================================================================
// CALL STACK HIGHLIGHTS
// ============================================================================

void UCCurrentLineHighlighter::SetCallStackFrames(const std::vector<int>& lines) {
    // Remove existing call stack highlights
    RemoveHighlights(LineHighlightType::CallStackFrame);
    
    // Add new ones (skip first frame - that's the current line)
    for (size_t i = 1; i < lines.size(); i++) {
        if (lines[i] > 0) {
            AddHighlight(lines[i], LineHighlightType::CallStackFrame);
        }
    }
}

void UCCurrentLineHighlighter::ClearCallStackFrames() {
    RemoveHighlights(LineHighlightType::CallStackFrame);
}

// ============================================================================
// EXCEPTION HIGHLIGHTS
// ============================================================================

void UCCurrentLineHighlighter::SetExceptionLine(int line, const std::string& message) {
    exceptionLine_ = line;
    exceptionMessage_ = message;
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCCurrentLineHighlighter::ClearExceptionLine() {
    if (exceptionLine_ >= 0) {
        exceptionLine_ = -1;
        exceptionMessage_.clear();
        
        if (textArea_) {
            textArea_->Invalidate();
        }
    }
}

// ============================================================================
// CHANGED VALUES
// ============================================================================

void UCCurrentLineHighlighter::MarkLineAsChanged(int line) {
    AddHighlight(line, LineHighlightType::ChangedValue);
}

void UCCurrentLineHighlighter::ClearChangedLines() {
    RemoveHighlights(LineHighlightType::ChangedValue);
}

// ============================================================================
// NAVIGATION
// ============================================================================

void UCCurrentLineHighlighter::ScrollToCurrentLine() {
    if (currentLine_ > 0) {
        ScrollToLine(currentLine_);
    }
}

void UCCurrentLineHighlighter::ScrollToLine(int line) {
    if (textArea_ && line > 0) {
        textArea_->ScrollToLine(line);
        textArea_->SetCursorPosition(line, 1);
    }
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

void UCCurrentLineHighlighter::UpdateHighlightsFromDebugger() {
    if (!debugManager_) return;
    
    // Clear call stack highlights
    ClearCallStackFrames();
    
    // Get call stack and add highlights
    auto frames = debugManager_->GetCallStack();
    std::vector<int> frameLines;
    for (const auto& frame : frames) {
        if (frame.hasDebugInfo && frame.location.line > 0) {
            frameLines.push_back(frame.location.line);
        }
    }
    SetCallStackFrames(frameLines);
}

Color UCCurrentLineHighlighter::ApplyOpacity(const Color& color, float opacity) const {
    return Color(color.r, color.g, color.b,
                 static_cast<uint8_t>(color.a * opacity));
}

} // namespace IDE
} // namespace UltraCanvas
