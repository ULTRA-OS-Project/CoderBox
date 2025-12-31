// Apps/IDE/Debug/Editor/UCCurrentLineHighlighter.h
// Current Line Highlighter for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTextArea.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <map>
#include <string>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Line highlight type
 */
enum class LineHighlightType {
    None,
    CurrentStatement,     // Yellow - current execution point
    CallStackFrame,       // Light green - other frames in call stack
    Exception,            // Red - exception location
    HitBreakpoint,        // Orange - breakpoint just hit
    ChangedValue,         // Light yellow - value changed since last step
    SearchResult,         // Light blue - search match
    Bookmark              // Light purple - bookmarked line
};

/**
 * @brief Line highlight configuration
 */
struct LineHighlight {
    int line = 0;
    LineHighlightType type = LineHighlightType::None;
    Color backgroundColor;
    Color borderColor = Colors::Transparent;
    int borderWidth = 0;
    bool fullWidth = true;
    float opacity = 1.0f;
    
    LineHighlight() = default;
    LineHighlight(int l, LineHighlightType t) : line(l), type(t) {
        ApplyDefaultColors();
    }
    
    void ApplyDefaultColors() {
        switch (type) {
            case LineHighlightType::CurrentStatement:
                backgroundColor = Color(255, 255, 180);  // Yellow
                borderColor = Color(255, 238, 98);
                borderWidth = 1;
                break;
            case LineHighlightType::CallStackFrame:
                backgroundColor = Color(200, 255, 200);  // Light green
                break;
            case LineHighlightType::Exception:
                backgroundColor = Color(255, 200, 200);  // Light red
                borderColor = Color(255, 100, 100);
                borderWidth = 2;
                break;
            case LineHighlightType::HitBreakpoint:
                backgroundColor = Color(255, 220, 180);  // Orange
                break;
            case LineHighlightType::ChangedValue:
                backgroundColor = Color(255, 255, 220);  // Light yellow
                break;
            case LineHighlightType::SearchResult:
                backgroundColor = Color(200, 220, 255);  // Light blue
                break;
            case LineHighlightType::Bookmark:
                backgroundColor = Color(230, 220, 255);  // Light purple
                break;
            default:
                backgroundColor = Colors::Transparent;
                break;
        }
    }
};

/**
 * @brief Current line highlighter for debug sessions
 * 
 * Manages line highlighting in the editor during debugging,
 * including current statement, call stack frames, and exceptions.
 */
class UCCurrentLineHighlighter {
public:
    UCCurrentLineHighlighter();
    ~UCCurrentLineHighlighter() = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void SetTextArea(UltraCanvasTextArea* textArea);
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // RENDERING
    // =========================================================================
    
    /**
     * @brief Render line highlights
     * Called during editor's render pass, before text rendering
     */
    void RenderHighlights(IRenderContext* ctx, const Rect2Di& textRect,
                          int firstVisibleLine, int lastVisibleLine,
                          int lineHeight, int scrollX);
    
    // =========================================================================
    // HIGHLIGHT MANAGEMENT
    // =========================================================================
    
    void SetCurrentLine(int line, LineHighlightType type = LineHighlightType::CurrentStatement);
    void ClearCurrentLine();
    int GetCurrentLine() const { return currentLine_; }
    
    void AddHighlight(int line, LineHighlightType type);
    void AddHighlight(const LineHighlight& highlight);
    void RemoveHighlight(int line);
    void RemoveHighlights(LineHighlightType type);
    void ClearAllHighlights();
    
    bool HasHighlight(int line) const;
    LineHighlight GetHighlight(int line) const;
    std::vector<LineHighlight> GetAllHighlights() const;
    
    // =========================================================================
    // CALL STACK HIGHLIGHTS
    // =========================================================================
    
    void SetCallStackFrames(const std::vector<int>& lines);
    void ClearCallStackFrames();
    
    // =========================================================================
    // EXCEPTION HIGHLIGHTS
    // =========================================================================
    
    void SetExceptionLine(int line, const std::string& message = "");
    void ClearExceptionLine();
    std::string GetExceptionMessage() const { return exceptionMessage_; }
    
    // =========================================================================
    // CHANGED VALUES
    // =========================================================================
    
    void MarkLineAsChanged(int line);
    void ClearChangedLines();
    
    // =========================================================================
    // NAVIGATION
    // =========================================================================
    
    void ScrollToCurrentLine();
    void ScrollToLine(int line);
    
    // =========================================================================
    // SETTINGS
    // =========================================================================
    
    void SetHighlightOpacity(float opacity) { highlightOpacity_ = opacity; }
    float GetHighlightOpacity() const { return highlightOpacity_; }
    
    void SetAnimateHighlights(bool animate) { animateHighlights_ = animate; }
    bool GetAnimateHighlights() const { return animateHighlights_; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(int line)> onCurrentLineChanged;
    std::function<void(int line)> onHighlightClicked;
    
private:
    void UpdateHighlightsFromDebugger();
    void RenderHighlight(IRenderContext* ctx, const LineHighlight& highlight,
                         const Rect2Di& textRect, int firstVisibleLine,
                         int lineHeight, int scrollX);
    Color ApplyOpacity(const Color& color, float opacity) const;
    
    UltraCanvasTextArea* textArea_ = nullptr;
    UCDebugManager* debugManager_ = nullptr;
    
    // Highlights by line
    std::map<int, LineHighlight> highlights_;
    
    // Current execution state
    int currentLine_ = -1;
    LineHighlightType currentLineType_ = LineHighlightType::CurrentStatement;
    
    // Exception state
    int exceptionLine_ = -1;
    std::string exceptionMessage_;
    
    // Settings
    float highlightOpacity_ = 1.0f;
    bool animateHighlights_ = true;
    float animationPhase_ = 0.0f;
};

} // namespace IDE
} // namespace UltraCanvas
