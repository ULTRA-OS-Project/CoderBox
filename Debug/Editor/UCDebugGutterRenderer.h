// Apps/IDE/Debug/Editor/UCDebugGutterRenderer.h
// Debug Gutter Renderer for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTextArea.h"
#include "UltraCanvasUIElement.h"
#include "../Breakpoints/UCBreakpointManager.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <set>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Gutter icon types for debug visualization
 */
enum class GutterIconType {
    None,
    Breakpoint,              // Red filled circle
    BreakpointDisabled,      // Red hollow circle
    BreakpointConditional,   // Red circle with question mark
    BreakpointLogpoint,      // Red diamond
    BreakpointPending,       // Gray circle
    BreakpointInvalid,       // Red circle with X
    CurrentLine,             // Yellow arrow
    CurrentLineBreakpoint,   // Yellow arrow + red circle
    CallStackFrame,          // Green arrow (other stack frames)
    Bookmark                 // Blue flag
};

/**
 * @brief Gutter decoration for a single line
 */
struct GutterDecoration {
    int line = 0;
    GutterIconType iconType = GutterIconType::None;
    std::string tooltip;
    int breakpointId = -1;
    Color backgroundColor = Colors::Transparent;
    
    GutterDecoration() = default;
    GutterDecoration(int l, GutterIconType t, const std::string& tip = "")
        : line(l), iconType(t), tooltip(tip) {}
};

/**
 * @brief Debug gutter renderer for UltraCanvasTextArea
 * 
 * Renders breakpoint icons, current line indicator, and other debug
 * decorations in the editor's gutter area.
 */
class UCDebugGutterRenderer {
public:
    UCDebugGutterRenderer();
    ~UCDebugGutterRenderer() = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void SetTextArea(UltraCanvasTextArea* textArea);
    void SetBreakpointManager(UCBreakpointManager* manager);
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // RENDERING
    // =========================================================================
    
    /**
     * @brief Render gutter decorations
     * Called during editor's render pass
     */
    void Render(IRenderContext* ctx, const Rect2Di& gutterRect, 
                int firstVisibleLine, int lastVisibleLine, int lineHeight);
    
    /**
     * @brief Get required gutter width for debug icons
     */
    int GetRequiredWidth() const { return iconSize_ + gutterPadding_ * 2; }
    
    // =========================================================================
    // DECORATION MANAGEMENT
    // =========================================================================
    
    void RefreshDecorations();
    void ClearDecorations();
    
    void SetCurrentLine(int line, bool isTopFrame = true);
    void ClearCurrentLine();
    
    void AddCallStackFrame(int line);
    void ClearCallStackFrames();
    
    void AddBookmark(int line);
    void RemoveBookmark(int line);
    void ToggleBookmark(int line);
    bool HasBookmark(int line) const;
    std::vector<int> GetBookmarks() const;
    
    // =========================================================================
    // HIT TESTING
    // =========================================================================
    
    /**
     * @brief Check if a point is in the breakpoint margin
     */
    bool IsInBreakpointMargin(int x, int y) const;
    
    /**
     * @brief Get line number at y coordinate
     */
    int GetLineAtY(int y) const;
    
    // =========================================================================
    // SETTINGS
    // =========================================================================
    
    void SetIconSize(int size) { iconSize_ = size; }
    int GetIconSize() const { return iconSize_; }
    
    void SetGutterPadding(int padding) { gutterPadding_ = padding; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(int line)> onBreakpointMarginClicked;
    std::function<void(int line)> onBreakpointMarginRightClicked;
    
private:
    void RenderIcon(IRenderContext* ctx, GutterIconType type, 
                    int x, int y, int size);
    void RenderBreakpointIcon(IRenderContext* ctx, int x, int y, int size, 
                               bool enabled, bool conditional, bool logpoint);
    void RenderCurrentLineIcon(IRenderContext* ctx, int x, int y, int size,
                                bool hasBreakpoint);
    void RenderCallStackIcon(IRenderContext* ctx, int x, int y, int size);
    void RenderBookmarkIcon(IRenderContext* ctx, int x, int y, int size);
    
    GutterDecoration GetDecorationForLine(int line) const;
    GutterIconType GetBreakpointIconType(const Breakpoint& bp) const;
    
    UltraCanvasTextArea* textArea_ = nullptr;
    UCBreakpointManager* breakpointManager_ = nullptr;
    UCDebugManager* debugManager_ = nullptr;
    
    // Cached decorations by line
    std::map<int, GutterDecoration> decorations_;
    
    // Current execution state
    int currentLine_ = -1;
    bool currentLineIsTopFrame_ = true;
    std::set<int> callStackLines_;
    std::set<int> bookmarks_;
    
    // Appearance
    int iconSize_ = 14;
    int gutterPadding_ = 4;
    
    // Colors
    Color breakpointColor_ = Color(255, 0, 0);
    Color breakpointDisabledColor_ = Color(128, 128, 128);
    Color currentLineColor_ = Color(255, 220, 0);
    Color callStackColor_ = Color(100, 200, 100);
    Color bookmarkColor_ = Color(100, 150, 255);
    
    // Cached gutter rect for hit testing
    Rect2Di gutterRect_;
    int firstVisibleLine_ = 0;
    int lineHeight_ = 16;
};

} // namespace IDE
} // namespace UltraCanvas
