// Apps/IDE/Debug/Editor/UCHoverEvaluator.h
// Hover Evaluator for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTextArea.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasTreeView.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <chrono>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Hover evaluation result
 */
struct HoverEvaluationResult {
    bool success = false;
    std::string expression;
    Variable variable;
    std::string errorMessage;
    Point2Di hoverPosition;
    Rect2Di expressionBounds;
};

/**
 * @brief Hover tooltip popup for variable evaluation
 */
class UCHoverTooltip : public UltraCanvasContainer {
public:
    UCHoverTooltip(const std::string& id);
    ~UCHoverTooltip() override = default;
    
    void SetResult(const HoverEvaluationResult& result);
    void SetDebugManager(UCDebugManager* manager);
    
    void Show(int x, int y);
    void Hide();
    bool IsVisible() const { return visible_; }
    
    void ExpandVariable(int variableRef);
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void UpdateContent(const HoverEvaluationResult& result);
    void AddVariableNode(const Variable& var, TreeNodeId parentId = 0);
    Color GetValueColor(const Variable& var) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    std::shared_ptr<UltraCanvasLabel> expressionLabel_;
    std::shared_ptr<UltraCanvasLabel> valueLabel_;
    std::shared_ptr<UltraCanvasLabel> typeLabel_;
    std::shared_ptr<UltraCanvasTreeView> childrenTree_;
    
    HoverEvaluationResult currentResult_;
    bool visible_ = false;
    TreeNodeId nextNodeId_ = 1;
    std::map<TreeNodeId, int> nodeToVarRef_;
};

/**
 * @brief Hover evaluator for debug sessions
 * 
 * Evaluates expressions when the mouse hovers over code during debugging.
 * Shows a tooltip with the evaluated value.
 */
class UCHoverEvaluator {
public:
    UCHoverEvaluator();
    ~UCHoverEvaluator() = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void SetTextArea(UltraCanvasTextArea* textArea);
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // HOVER HANDLING
    // =========================================================================
    
    /**
     * @brief Called when mouse moves over the text area
     */
    void OnMouseMove(int x, int y);
    
    /**
     * @brief Called when mouse leaves the text area
     */
    void OnMouseLeave();
    
    /**
     * @brief Update timer - call periodically to handle hover delay
     */
    void Update();
    
    /**
     * @brief Force hide the tooltip
     */
    void HideTooltip();
    
    // =========================================================================
    // EVALUATION
    // =========================================================================
    
    /**
     * @brief Manually evaluate an expression at a position
     */
    HoverEvaluationResult EvaluateAt(int x, int y);
    
    /**
     * @brief Evaluate a specific expression
     */
    HoverEvaluationResult EvaluateExpression(const std::string& expression);
    
    // =========================================================================
    // SETTINGS
    // =========================================================================
    
    void SetHoverDelay(int milliseconds) { hoverDelayMs_ = milliseconds; }
    int GetHoverDelay() const { return hoverDelayMs_; }
    
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }
    
    void SetEvaluateOnlyDuringDebug(bool only) { onlyDuringDebug_ = only; }
    bool GetEvaluateOnlyDuringDebug() const { return onlyDuringDebug_; }
    
    // =========================================================================
    // TOOLTIP ACCESS
    // =========================================================================
    
    UCHoverTooltip* GetTooltip() const { return tooltip_.get(); }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const HoverEvaluationResult&)> onEvaluationComplete;
    std::function<void()> onTooltipShown;
    std::function<void()> onTooltipHidden;
    
private:
    std::string GetExpressionAtPosition(int line, int column);
    std::string GetWordAtPosition(int line, int column);
    std::string GetSelectionOrWordAtPosition(int line, int column);
    Rect2Di GetExpressionBounds(int line, int column, const std::string& expr);
    bool IsValidIdentifierChar(char c) const;
    bool IsValidExpressionChar(char c) const;
    bool ShouldEvaluate() const;
    void StartHoverTimer();
    void CancelHoverTimer();
    void OnHoverTimerExpired();
    void ShowTooltip(const HoverEvaluationResult& result);
    
    UltraCanvasTextArea* textArea_ = nullptr;
    UCDebugManager* debugManager_ = nullptr;
    std::unique_ptr<UCHoverTooltip> tooltip_;
    
    // Hover state
    bool enabled_ = true;
    bool onlyDuringDebug_ = true;
    int hoverDelayMs_ = 500;
    
    Point2Di lastMousePos_;
    Point2Di hoverStartPos_;
    std::chrono::steady_clock::time_point hoverStartTime_;
    bool isHovering_ = false;
    bool timerExpired_ = false;
    
    // Last evaluated expression to avoid re-evaluation
    std::string lastExpression_;
    Point2Di lastEvalPos_;
};

} // namespace IDE
} // namespace UltraCanvas
