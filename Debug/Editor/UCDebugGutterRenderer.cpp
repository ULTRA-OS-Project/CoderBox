// Apps/IDE/Debug/Editor/UCDebugGutterRenderer.cpp
// Debug Gutter Renderer Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDebugGutterRenderer.h"
#include <algorithm>
#include <cmath>

namespace UltraCanvas {
namespace IDE {

UCDebugGutterRenderer::UCDebugGutterRenderer() = default;

void UCDebugGutterRenderer::SetTextArea(UltraCanvasTextArea* textArea) {
    textArea_ = textArea;
}

void UCDebugGutterRenderer::SetBreakpointManager(UCBreakpointManager* manager) {
    breakpointManager_ = manager;
    
    if (breakpointManager_) {
        // Subscribe to breakpoint changes
        breakpointManager_->onBreakpointAdd = [this](const UCBreakpoint& bp) {
            RefreshDecorations();
        };
        
        breakpointManager_->onBreakpointRemove = [this](int id) {
            RefreshDecorations();
        };
        
        breakpointManager_->onBreakpointChange = [this](const UCBreakpoint& bp) {
            RefreshDecorations();
        };
    }
}

void UCDebugGutterRenderer::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        // Subscribe to stop events
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            if (textArea_ && !loc.filePath.empty()) {
                // Check if this is our file
                // In real implementation, compare normalized paths
                SetCurrentLine(loc.line, true);
                
                // Update call stack lines
                ClearCallStackFrames();
                auto frames = debugManager_->GetCallStack();
                for (size_t i = 1; i < frames.size(); i++) {
                    if (!frames[i].location.filePath.empty()) {
                        AddCallStackFrame(frames[i].location.line);
                    }
                }
            }
        };
        
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            if (newState == DebugSessionState::Running ||
                newState == DebugSessionState::Inactive ||
                newState == DebugSessionState::Terminated) {
                ClearCurrentLine();
                ClearCallStackFrames();
            }
        };
    }
}

// ============================================================================
// RENDERING
// ============================================================================

void UCDebugGutterRenderer::Render(IRenderContext* ctx, const Rect2Di& gutterRect,
                                    int firstVisibleLine, int lastVisibleLine, 
                                    int lineHeight) {
    // Cache for hit testing
    gutterRect_ = gutterRect;
    firstVisibleLine_ = firstVisibleLine;
    lineHeight_ = lineHeight;
    
    // Draw background for breakpoint margin
    int marginWidth = GetRequiredWidth();
    ctx->DrawFilledRectangle(
        Rect2Di(gutterRect.x, gutterRect.y, marginWidth, gutterRect.height),
        Color(245, 245, 245));
    
    // Draw separator line
    ctx->DrawLine(
        Point2Di(gutterRect.x + marginWidth - 1, gutterRect.y),
        Point2Di(gutterRect.x + marginWidth - 1, gutterRect.y + gutterRect.height),
        Color(220, 220, 220), 1);
    
    // Render decorations for visible lines
    for (int line = firstVisibleLine; line <= lastVisibleLine; line++) {
        int y = gutterRect.y + (line - firstVisibleLine) * lineHeight;
        int iconX = gutterRect.x + gutterPadding_;
        int iconY = y + (lineHeight - iconSize_) / 2;
        
        GutterDecoration decoration = GetDecorationForLine(line);
        
        // Draw background highlight if set
        if (decoration.backgroundColor != Colors::Transparent) {
            ctx->DrawFilledRectangle(
                Rect2Di(gutterRect.x, y, marginWidth, lineHeight),
                decoration.backgroundColor);
        }
        
        // Draw icon
        if (decoration.iconType != GutterIconType::None) {
            RenderIcon(ctx, decoration.iconType, iconX, iconY, iconSize_);
        }
    }
}

void UCDebugGutterRenderer::RenderIcon(IRenderContext* ctx, GutterIconType type,
                                        int x, int y, int size) {
    switch (type) {
        case GutterIconType::Breakpoint:
            RenderBreakpointIcon(ctx, x, y, size, true, false, false);
            break;
            
        case GutterIconType::BreakpointDisabled:
            RenderBreakpointIcon(ctx, x, y, size, false, false, false);
            break;
            
        case GutterIconType::BreakpointConditional:
            RenderBreakpointIcon(ctx, x, y, size, true, true, false);
            break;
            
        case GutterIconType::BreakpointLogpoint:
            RenderBreakpointIcon(ctx, x, y, size, true, false, true);
            break;
            
        case GutterIconType::BreakpointPending:
            ctx->DrawFilledCircle(Point2Di(x + size/2, y + size/2), size/2 - 1,
                                  Color(180, 180, 180));
            break;
            
        case GutterIconType::BreakpointInvalid:
            ctx->DrawFilledCircle(Point2Di(x + size/2, y + size/2), size/2 - 1,
                                  Color(200, 100, 100));
            // Draw X
            ctx->DrawLine(Point2Di(x + 3, y + 3), Point2Di(x + size - 3, y + size - 3),
                         Colors::White, 2);
            ctx->DrawLine(Point2Di(x + size - 3, y + 3), Point2Di(x + 3, y + size - 3),
                         Colors::White, 2);
            break;
            
        case GutterIconType::CurrentLine:
            RenderCurrentLineIcon(ctx, x, y, size, false);
            break;
            
        case GutterIconType::CurrentLineBreakpoint:
            RenderCurrentLineIcon(ctx, x, y, size, true);
            break;
            
        case GutterIconType::CallStackFrame:
            RenderCallStackIcon(ctx, x, y, size);
            break;
            
        case GutterIconType::Bookmark:
            RenderBookmarkIcon(ctx, x, y, size);
            break;
            
        default:
            break;
    }
}

void UCDebugGutterRenderer::RenderBreakpointIcon(IRenderContext* ctx, 
                                                  int x, int y, int size,
                                                  bool enabled, bool conditional,
                                                  bool logpoint) {
    int cx = x + size / 2;
    int cy = y + size / 2;
    int radius = size / 2 - 1;
    
    Color color = enabled ? breakpointColor_ : breakpointDisabledColor_;
    
    if (logpoint) {
        // Diamond shape for logpoint
        std::vector<Point2Di> diamond = {
            Point2Di(cx, y + 1),
            Point2Di(x + size - 1, cy),
            Point2Di(cx, y + size - 1),
            Point2Di(x + 1, cy)
        };
        ctx->DrawFilledPolygon(diamond, color);
    } else {
        // Circle for breakpoint
        if (enabled) {
            ctx->DrawFilledCircle(Point2Di(cx, cy), radius, color);
        } else {
            ctx->DrawCircle(Point2Di(cx, cy), radius, color, 2);
        }
        
        // Conditional marker (? inside)
        if (conditional && enabled) {
            ctx->SetFont("Sans", static_cast<float>(size - 4));
            ctx->DrawText("?", Point2Di(cx - 2, cy + 3), Colors::White);
        }
    }
}

void UCDebugGutterRenderer::RenderCurrentLineIcon(IRenderContext* ctx,
                                                   int x, int y, int size,
                                                   bool hasBreakpoint) {
    // Draw arrow pointing right
    int arrowWidth = size - 2;
    int arrowHeight = size - 4;
    int cy = y + size / 2;
    
    std::vector<Point2Di> arrow = {
        Point2Di(x, cy - arrowHeight/2),
        Point2Di(x + arrowWidth * 2/3, cy - arrowHeight/2),
        Point2Di(x + arrowWidth * 2/3, cy - arrowHeight/2 - 2),
        Point2Di(x + arrowWidth, cy),
        Point2Di(x + arrowWidth * 2/3, cy + arrowHeight/2 + 2),
        Point2Di(x + arrowWidth * 2/3, cy + arrowHeight/2),
        Point2Di(x, cy + arrowHeight/2)
    };
    
    ctx->DrawFilledPolygon(arrow, currentLineColor_);
    ctx->DrawPolygon(arrow, Color(180, 160, 0), 1);
    
    // If also has breakpoint, draw small red dot
    if (hasBreakpoint) {
        ctx->DrawFilledCircle(Point2Di(x + 3, y + 3), 3, breakpointColor_);
    }
}

void UCDebugGutterRenderer::RenderCallStackIcon(IRenderContext* ctx,
                                                 int x, int y, int size) {
    // Smaller green arrow for other stack frames
    int arrowWidth = size - 4;
    int arrowHeight = size - 6;
    int cy = y + size / 2;
    int offsetX = x + 2;
    
    std::vector<Point2Di> arrow = {
        Point2Di(offsetX, cy - arrowHeight/2),
        Point2Di(offsetX + arrowWidth * 2/3, cy - arrowHeight/2),
        Point2Di(offsetX + arrowWidth * 2/3, cy - arrowHeight/2 - 1),
        Point2Di(offsetX + arrowWidth, cy),
        Point2Di(offsetX + arrowWidth * 2/3, cy + arrowHeight/2 + 1),
        Point2Di(offsetX + arrowWidth * 2/3, cy + arrowHeight/2),
        Point2Di(offsetX, cy + arrowHeight/2)
    };
    
    ctx->DrawFilledPolygon(arrow, callStackColor_);
}

void UCDebugGutterRenderer::RenderBookmarkIcon(IRenderContext* ctx,
                                                int x, int y, int size) {
    // Flag/bookmark shape
    int flagWidth = size - 4;
    int flagHeight = size - 2;
    
    std::vector<Point2Di> flag = {
        Point2Di(x + 2, y + 1),
        Point2Di(x + 2 + flagWidth, y + 1),
        Point2Di(x + 2 + flagWidth, y + 1 + flagHeight * 2/3),
        Point2Di(x + 2 + flagWidth/2, y + 1 + flagHeight/2),
        Point2Di(x + 2, y + 1 + flagHeight * 2/3)
    };
    
    ctx->DrawFilledPolygon(flag, bookmarkColor_);
    
    // Pole
    ctx->DrawLine(Point2Di(x + 2, y + 1), Point2Di(x + 2, y + size - 1),
                  Color(80, 120, 200), 2);
}

// ============================================================================
// DECORATION MANAGEMENT
// ============================================================================

void UCDebugGutterRenderer::RefreshDecorations() {
    decorations_.clear();
    
    // Add breakpoint decorations
    if (breakpointManager_ && textArea_) {
        std::string currentFile;  // TODO: Get from text area
        
        auto breakpoints = breakpointManager_->GetAllBreakpoints();
        for (const auto& bp : breakpoints) {
            // In real implementation, compare file paths
            int line = bp.GetLine();
            GutterDecoration dec;
            dec.line = line;
            dec.iconType = GetBreakpointIconType(bp.GetData());
            dec.breakpointId = bp.GetId();
            dec.tooltip = "Breakpoint";
            
            if (!bp.GetCondition().empty()) {
                dec.tooltip += " (condition: " + bp.GetCondition() + ")";
            }
            
            decorations_[line] = dec;
        }
    }
    
    // Request redraw
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::ClearDecorations() {
    decorations_.clear();
    currentLine_ = -1;
    callStackLines_.clear();
    // Keep bookmarks
}

void UCDebugGutterRenderer::SetCurrentLine(int line, bool isTopFrame) {
    currentLine_ = line;
    currentLineIsTopFrame_ = isTopFrame;
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::ClearCurrentLine() {
    currentLine_ = -1;
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::AddCallStackFrame(int line) {
    callStackLines_.insert(line);
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::ClearCallStackFrames() {
    callStackLines_.clear();
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::AddBookmark(int line) {
    bookmarks_.insert(line);
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::RemoveBookmark(int line) {
    bookmarks_.erase(line);
    
    if (textArea_) {
        textArea_->Invalidate();
    }
}

void UCDebugGutterRenderer::ToggleBookmark(int line) {
    if (HasBookmark(line)) {
        RemoveBookmark(line);
    } else {
        AddBookmark(line);
    }
}

bool UCDebugGutterRenderer::HasBookmark(int line) const {
    return bookmarks_.find(line) != bookmarks_.end();
}

std::vector<int> UCDebugGutterRenderer::GetBookmarks() const {
    return std::vector<int>(bookmarks_.begin(), bookmarks_.end());
}

// ============================================================================
// HIT TESTING
// ============================================================================

bool UCDebugGutterRenderer::IsInBreakpointMargin(int x, int y) const {
    int marginWidth = GetRequiredWidth();
    return x >= gutterRect_.x && x < gutterRect_.x + marginWidth &&
           y >= gutterRect_.y && y < gutterRect_.y + gutterRect_.height;
}

int UCDebugGutterRenderer::GetLineAtY(int y) const {
    if (lineHeight_ <= 0) return -1;
    
    int relativeY = y - gutterRect_.y;
    if (relativeY < 0) return -1;
    
    return firstVisibleLine_ + relativeY / lineHeight_;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

GutterDecoration UCDebugGutterRenderer::GetDecorationForLine(int line) const {
    GutterDecoration decoration;
    decoration.line = line;
    
    // Check if this is the current execution line
    if (line == currentLine_) {
        auto it = decorations_.find(line);
        if (it != decorations_.end() && 
            it->second.iconType == GutterIconType::Breakpoint) {
            decoration.iconType = GutterIconType::CurrentLineBreakpoint;
        } else {
            decoration.iconType = GutterIconType::CurrentLine;
        }
        decoration.backgroundColor = Color(255, 255, 200, 100);
        return decoration;
    }
    
    // Check if this is a call stack frame
    if (callStackLines_.find(line) != callStackLines_.end()) {
        decoration.iconType = GutterIconType::CallStackFrame;
        decoration.backgroundColor = Color(200, 255, 200, 50);
        return decoration;
    }
    
    // Check for breakpoint
    auto it = decorations_.find(line);
    if (it != decorations_.end()) {
        return it->second;
    }
    
    // Check for bookmark
    if (HasBookmark(line)) {
        decoration.iconType = GutterIconType::Bookmark;
        return decoration;
    }
    
    return decoration;
}

GutterIconType UCDebugGutterRenderer::GetBreakpointIconType(const Breakpoint& bp) const {
    if (!bp.enabled) {
        return GutterIconType::BreakpointDisabled;
    }
    
    switch (bp.state) {
        case BreakpointState::Invalid:
            return GutterIconType::BreakpointInvalid;
        case BreakpointState::Pending:
            return GutterIconType::BreakpointPending;
        default:
            break;
    }
    
    if (bp.type == BreakpointType::Logpoint) {
        return GutterIconType::BreakpointLogpoint;
    }
    
    if (bp.type == BreakpointType::ConditionalBreakpoint || !bp.condition.empty()) {
        return GutterIconType::BreakpointConditional;
    }
    
    return GutterIconType::Breakpoint;
}

} // namespace IDE
} // namespace UltraCanvas
