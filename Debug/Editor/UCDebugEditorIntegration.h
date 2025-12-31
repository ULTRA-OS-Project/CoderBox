// Apps/IDE/Debug/Editor/UCDebugEditorIntegration.h
// Debug Editor Integration for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCDebugGutterRenderer.h"
#include "UCCurrentLineHighlighter.h"
#include "UCHoverEvaluator.h"
#include "UltraCanvasTextArea.h"
#include "../Core/UCDebugManager.h"
#include "../Breakpoints/UCBreakpointManager.h"
#include <functional>
#include <memory>
#include <string>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Inline value display mode
 */
enum class InlineValueMode {
    Off,           // No inline values
    Simple,        // Show simple values inline
    All            // Show all values inline (can be verbose)
};

/**
 * @brief Debug decoration at a specific position
 */
struct InlineDecoration {
    int line;
    int column;
    std::string text;
    Color color;
    bool isError = false;
    
    InlineDecoration(int l, int c, const std::string& t, const Color& clr)
        : line(l), column(c), text(t), color(clr) {}
};

/**
 * @brief Main coordinator for debug-editor integration
 * 
 * This class coordinates all debug visualizations in the code editor:
 * - Gutter icons (breakpoints, current line)
 * - Line highlighting
 * - Hover evaluation
 * - Inline value display
 * - Exception decorations
 * 
 * Usage:
 *   auto integration = std::make_unique<UCDebugEditorIntegration>();
 *   integration->SetTextArea(editor);
 *   integration->SetDebugManager(debugManager);
 *   integration->SetBreakpointManager(bpManager);
 *   integration->Initialize();
 */
class UCDebugEditorIntegration {
public:
    UCDebugEditorIntegration();
    ~UCDebugEditorIntegration();
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void SetTextArea(UltraCanvasTextArea* textArea);
    void SetDebugManager(UCDebugManager* manager);
    void SetBreakpointManager(UCBreakpointManager* manager);
    
    /**
     * @brief Initialize all components
     * Must be called after setting text area and managers
     */
    void Initialize();
    
    /**
     * @brief Cleanup and release resources
     */
    void Shutdown();
    
    // =========================================================================
    // FILE ASSOCIATION
    // =========================================================================
    
    /**
     * @brief Set the file path associated with this editor
     */
    void SetFilePath(const std::string& path);
    std::string GetFilePath() const { return filePath_; }
    
    /**
     * @brief Check if this editor is showing the current debug location
     */
    bool IsAtCurrentDebugLocation() const;
    
    // =========================================================================
    // RENDERING
    // =========================================================================
    
    /**
     * @brief Render all debug decorations
     * Called during editor's render pass
     */
    void Render(IRenderContext* ctx, const Rect2Di& gutterRect,
                const Rect2Di& textRect, int firstVisibleLine,
                int lastVisibleLine, int lineHeight, int scrollX);
    
    /**
     * @brief Render inline value decorations (after text)
     */
    void RenderInlineValues(IRenderContext* ctx, const Rect2Di& textRect,
                            int firstVisibleLine, int lastVisibleLine,
                            int lineHeight);
    
    // =========================================================================
    // EVENT HANDLING
    // =========================================================================
    
    /**
     * @brief Handle mouse click in gutter area
     * @return true if event was handled
     */
    bool HandleGutterClick(int x, int y, bool rightClick);
    
    /**
     * @brief Handle mouse move for hover evaluation
     */
    void HandleMouseMove(int x, int y);
    
    /**
     * @brief Handle mouse leave
     */
    void HandleMouseLeave();
    
    /**
     * @brief Handle key press for debug shortcuts
     * @return true if event was handled
     */
    bool HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt);
    
    /**
     * @brief Update timer-based functionality
     */
    void Update();
    
    // =========================================================================
    // BREAKPOINT OPERATIONS
    // =========================================================================
    
    /**
     * @brief Toggle breakpoint at line
     */
    void ToggleBreakpoint(int line);
    
    /**
     * @brief Add breakpoint at line
     */
    int AddBreakpoint(int line);
    
    /**
     * @brief Remove breakpoint at line
     */
    bool RemoveBreakpoint(int line);
    
    /**
     * @brief Add conditional breakpoint at line
     */
    int AddConditionalBreakpoint(int line, const std::string& condition);
    
    /**
     * @brief Add logpoint at line
     */
    int AddLogpoint(int line, const std::string& message);
    
    /**
     * @brief Check if line has a breakpoint
     */
    bool HasBreakpoint(int line) const;
    
    // =========================================================================
    // NAVIGATION
    // =========================================================================
    
    /**
     * @brief Navigate to current debug location
     */
    void GoToCurrentLocation();
    
    /**
     * @brief Navigate to specific line
     */
    void GoToLine(int line);
    
    /**
     * @brief Navigate to next breakpoint
     */
    void GoToNextBreakpoint();
    
    /**
     * @brief Navigate to previous breakpoint
     */
    void GoToPreviousBreakpoint();
    
    // =========================================================================
    // INLINE VALUES
    // =========================================================================
    
    void SetInlineValueMode(InlineValueMode mode) { inlineValueMode_ = mode; }
    InlineValueMode GetInlineValueMode() const { return inlineValueMode_; }
    
    void RefreshInlineValues();
    void ClearInlineValues();
    
    // =========================================================================
    // COMPONENT ACCESS
    // =========================================================================
    
    UCDebugGutterRenderer* GetGutterRenderer() { return gutterRenderer_.get(); }
    UCCurrentLineHighlighter* GetLineHighlighter() { return lineHighlighter_.get(); }
    UCHoverEvaluator* GetHoverEvaluator() { return hoverEvaluator_.get(); }
    
    // =========================================================================
    // SETTINGS
    // =========================================================================
    
    void SetHoverEvaluationEnabled(bool enabled);
    bool IsHoverEvaluationEnabled() const;
    
    void SetGutterWidth(int width) { gutterWidth_ = width; }
    int GetGutterWidth() const { return gutterWidth_; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(int line)> onBreakpointToggled;
    std::function<void(int line)> onRunToCursor;
    std::function<void(int line)> onSetNextStatement;
    std::function<void(const std::string&)> onAddWatch;
    std::function<void()> onRequestBreakpointDialog;
    
private:
    void SetupTextAreaCallbacks();
    void SetupDebugManagerCallbacks();
    void UpdateFromDebugState();
    void UpdateInlineValuesForLine(int line);
    std::string NormalizePath(const std::string& path) const;
    bool PathsMatch(const std::string& path1, const std::string& path2) const;
    
    UltraCanvasTextArea* textArea_ = nullptr;
    UCDebugManager* debugManager_ = nullptr;
    UCBreakpointManager* breakpointManager_ = nullptr;
    
    std::unique_ptr<UCDebugGutterRenderer> gutterRenderer_;
    std::unique_ptr<UCCurrentLineHighlighter> lineHighlighter_;
    std::unique_ptr<UCHoverEvaluator> hoverEvaluator_;
    
    std::string filePath_;
    int gutterWidth_ = 22;
    InlineValueMode inlineValueMode_ = InlineValueMode::Simple;
    
    std::vector<InlineDecoration> inlineDecorations_;
    bool initialized_ = false;
};

} // namespace IDE
} // namespace UltraCanvas
