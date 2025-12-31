// Apps/IDE/Debug/Editor/UCHoverEvaluator.cpp
// Hover Evaluator Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCHoverEvaluator.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// UCHoverTooltip IMPLEMENTATION
// ============================================================================

UCHoverTooltip::UCHoverTooltip(const std::string& id)
    : UltraCanvasContainer(id, 0, 0, 0, 300, 150) {
    
    SetBackgroundColor(Color(255, 255, 225));  // Light yellow tooltip
    SetVisible(false);
    CreateUI();
}

void UCHoverTooltip::CreateUI() {
    int yOffset = 8;
    
    // Expression label (bold)
    expressionLabel_ = std::make_shared<UltraCanvasLabel>(
        "hover_expr", 0, 8, yOffset, GetWidth() - 16, 18);
    expressionLabel_->SetFontWeight(FontWeight::Bold);
    expressionLabel_->SetFontSize(12);
    expressionLabel_->SetTextColor(Color(0, 0, 128));
    AddChild(expressionLabel_);
    yOffset += 22;
    
    // Value label
    valueLabel_ = std::make_shared<UltraCanvasLabel>(
        "hover_value", 0, 8, yOffset, GetWidth() - 16, 18);
    valueLabel_->SetFontSize(11);
    valueLabel_->SetFont("Consolas", 11);
    AddChild(valueLabel_);
    yOffset += 22;
    
    // Type label (gray, smaller)
    typeLabel_ = std::make_shared<UltraCanvasLabel>(
        "hover_type", 0, 8, yOffset, GetWidth() - 16, 16);
    typeLabel_->SetFontSize(10);
    typeLabel_->SetTextColor(Color(100, 100, 100));
    AddChild(typeLabel_);
    yOffset += 20;
    
    // Children tree (for expandable variables)
    childrenTree_ = std::make_shared<UltraCanvasTreeView>(
        "hover_children", 0, 4, yOffset, GetWidth() - 8, 80);
    childrenTree_->SetShowExpandButtons(true);
    childrenTree_->SetShowIcons(false);
    childrenTree_->SetRowHeight(18);
    childrenTree_->SetBackgroundColor(Color(255, 255, 240));
    childrenTree_->SetVisible(false);
    
    childrenTree_->onNodeExpand = [this](TreeNodeId nodeId) {
        auto it = nodeToVarRef_.find(nodeId);
        if (it != nodeToVarRef_.end()) {
            ExpandVariable(it->second);
        }
    };
    
    AddChild(childrenTree_);
}

void UCHoverTooltip::SetResult(const HoverEvaluationResult& result) {
    currentResult_ = result;
    UpdateContent(result);
}

void UCHoverTooltip::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
}

void UCHoverTooltip::Show(int x, int y) {
    // Position tooltip near cursor but not overlapping
    int tooltipX = x + 16;
    int tooltipY = y + 20;
    
    // TODO: Adjust position to stay within screen bounds
    
    SetPosition(tooltipX, tooltipY);
    SetVisible(true);
    visible_ = true;
}

void UCHoverTooltip::Hide() {
    SetVisible(false);
    visible_ = false;
    childrenTree_->Clear();
    nodeToVarRef_.clear();
    nextNodeId_ = 1;
}

void UCHoverTooltip::ExpandVariable(int variableRef) {
    if (!debugManager_ || variableRef <= 0) return;
    
    auto children = debugManager_->ExpandVariable(variableRef);
    
    // Find the node and add children
    for (const auto& pair : nodeToVarRef_) {
        if (pair.second == variableRef) {
            // Remove placeholder children
            auto existingChildren = childrenTree_->GetChildren(pair.first);
            for (auto childId : existingChildren) {
                childrenTree_->RemoveNode(childId);
            }
            
            // Add real children
            for (const auto& child : children) {
                AddVariableNode(child, pair.first);
            }
            
            // Don't expand this again
            nodeToVarRef_.erase(pair.first);
            break;
        }
    }
}

void UCHoverTooltip::UpdateContent(const HoverEvaluationResult& result) {
    if (!result.success) {
        expressionLabel_->SetText(result.expression);
        valueLabel_->SetText(result.errorMessage);
        valueLabel_->SetTextColor(Color(200, 0, 0));
        typeLabel_->SetText("");
        childrenTree_->SetVisible(false);
        
        // Shrink tooltip
        SetSize(300, 70);
        return;
    }
    
    expressionLabel_->SetText(result.expression);
    
    // Truncate long values
    std::string displayValue = result.variable.value;
    if (displayValue.length() > 100) {
        displayValue = displayValue.substr(0, 100) + "...";
    }
    valueLabel_->SetText(displayValue);
    valueLabel_->SetTextColor(GetValueColor(result.variable));
    
    if (!result.variable.type.empty()) {
        typeLabel_->SetText("Type: " + result.variable.type);
    } else {
        typeLabel_->SetText("");
    }
    
    // Show children tree if variable is expandable
    if (result.variable.hasChildren && result.variable.variablesReference > 0) {
        childrenTree_->Clear();
        nodeToVarRef_.clear();
        nextNodeId_ = 1;
        
        // Add placeholder to indicate expandability
        TreeNodeData placeholder;
        placeholder.text = "Click to expand...";
        TreeNodeId nodeId = nextNodeId_++;
        childrenTree_->AddNode(nodeId, placeholder);
        nodeToVarRef_[nodeId] = result.variable.variablesReference;
        
        childrenTree_->SetVisible(true);
        SetSize(300, 150);
    } else {
        childrenTree_->SetVisible(false);
        SetSize(300, 80);
    }
}

void UCHoverTooltip::AddVariableNode(const Variable& var, TreeNodeId parentId) {
    TreeNodeId nodeId = nextNodeId_++;
    
    TreeNodeData nodeData;
    
    std::ostringstream text;
    text << var.name;
    if (!var.value.empty()) {
        std::string displayValue = var.value;
        if (displayValue.length() > 50) {
            displayValue = displayValue.substr(0, 50) + "...";
        }
        text << " = " << displayValue;
    }
    nodeData.text = text.str();
    nodeData.textColor = GetValueColor(var);
    
    if (parentId == 0) {
        childrenTree_->AddNode(nodeId, nodeData);
    } else {
        childrenTree_->AddNode(nodeId, nodeData, parentId);
    }
    
    // Track for expansion
    if (var.hasChildren && var.variablesReference > 0) {
        nodeToVarRef_[nodeId] = var.variablesReference;
        
        // Add placeholder child
        TreeNodeData placeholder;
        placeholder.text = "...";
        childrenTree_->AddNode(nextNodeId_++, placeholder, nodeId);
    }
}

Color UCHoverTooltip::GetValueColor(const Variable& var) const {
    if (!var.errorMessage.empty()) {
        return Color(200, 0, 0);
    }
    
    switch (var.category) {
        case VariableCategory::StringType:
            return Color(163, 21, 21);
        case VariableCategory::Primitive:
            return Color(0, 128, 0);
        case VariableCategory::Pointer:
        case VariableCategory::Reference:
            return Color(0, 0, 200);
        default:
            return Color(0, 0, 0);
    }
}

void UCHoverTooltip::HandleRender(IRenderContext* ctx) {
    if (!visible_) return;
    
    // Draw shadow
    ctx->DrawFilledRectangle(
        Rect2Di(GetX() + 3, GetY() + 3, GetWidth(), GetHeight()),
        Color(0, 0, 0, 50));
    
    // Draw background
    ctx->DrawFilledRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        GetBackgroundColor());
    
    // Draw border
    ctx->DrawRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        Color(150, 150, 100), 1);
    
    // Render children
    UltraCanvasContainer::HandleRender(ctx);
}

// ============================================================================
// UCHoverEvaluator IMPLEMENTATION
// ============================================================================

UCHoverEvaluator::UCHoverEvaluator() {
    tooltip_ = std::make_unique<UCHoverTooltip>("hover_tooltip");
}

void UCHoverEvaluator::SetTextArea(UltraCanvasTextArea* textArea) {
    textArea_ = textArea;
}

void UCHoverEvaluator::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    tooltip_->SetDebugManager(manager);
}

// ============================================================================
// HOVER HANDLING
// ============================================================================

void UCHoverEvaluator::OnMouseMove(int x, int y) {
    lastMousePos_ = Point2Di(x, y);
    
    // Check if mouse moved significantly
    int dx = x - hoverStartPos_.x;
    int dy = y - hoverStartPos_.y;
    
    if (dx * dx + dy * dy > 100) {  // ~10 pixel threshold
        // Reset hover timer
        HideTooltip();
        StartHoverTimer();
        hoverStartPos_ = lastMousePos_;
    }
}

void UCHoverEvaluator::OnMouseLeave() {
    CancelHoverTimer();
    HideTooltip();
}

void UCHoverEvaluator::Update() {
    if (!isHovering_ || timerExpired_) return;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - hoverStartTime_).count();
    
    if (elapsed >= hoverDelayMs_) {
        timerExpired_ = true;
        OnHoverTimerExpired();
    }
}

void UCHoverEvaluator::HideTooltip() {
    if (tooltip_->IsVisible()) {
        tooltip_->Hide();
        
        if (onTooltipHidden) {
            onTooltipHidden();
        }
    }
}

// ============================================================================
// EVALUATION
// ============================================================================

HoverEvaluationResult UCHoverEvaluator::EvaluateAt(int x, int y) {
    HoverEvaluationResult result;
    result.hoverPosition = Point2Di(x, y);
    
    if (!ShouldEvaluate()) {
        result.success = false;
        result.errorMessage = "Evaluation not available";
        return result;
    }
    
    if (!textArea_) {
        result.success = false;
        result.errorMessage = "No text area";
        return result;
    }
    
    // Convert screen position to line/column
    int line = textArea_->GetLineAtY(y);
    int column = textArea_->GetColumnAtX(x, line);
    
    if (line < 0 || column < 0) {
        result.success = false;
        return result;
    }
    
    // Get expression at position
    std::string expression = GetSelectionOrWordAtPosition(line, column);
    
    if (expression.empty()) {
        result.success = false;
        return result;
    }
    
    // Check if same as last evaluation
    if (expression == lastExpression_ && 
        lastEvalPos_.x == x && lastEvalPos_.y == y) {
        // Use cached result would be nice, but for now re-evaluate
    }
    
    result = EvaluateExpression(expression);
    result.hoverPosition = Point2Di(x, y);
    result.expressionBounds = GetExpressionBounds(line, column, expression);
    
    lastExpression_ = expression;
    lastEvalPos_ = Point2Di(x, y);
    
    return result;
}

HoverEvaluationResult UCHoverEvaluator::EvaluateExpression(const std::string& expression) {
    HoverEvaluationResult result;
    result.expression = expression;
    
    if (!debugManager_ || !debugManager_->IsDebugging()) {
        result.success = false;
        result.errorMessage = "No active debug session";
        return result;
    }
    
    auto evalResult = debugManager_->Evaluate(expression);
    
    result.success = evalResult.success;
    result.variable = evalResult.variable;
    result.errorMessage = evalResult.errorMessage;
    
    if (result.success && result.variable.name.empty()) {
        result.variable.name = expression;
    }
    
    return result;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

std::string UCHoverEvaluator::GetExpressionAtPosition(int line, int column) {
    if (!textArea_) return "";
    
    std::string lineText = textArea_->GetLine(line);
    if (lineText.empty() || column >= static_cast<int>(lineText.length())) {
        return "";
    }
    
    // Find start of expression (going backward)
    int start = column;
    int parenDepth = 0;
    int bracketDepth = 0;
    
    while (start > 0) {
        char c = lineText[start - 1];
        
        if (c == ')') parenDepth++;
        else if (c == '(') {
            if (parenDepth > 0) parenDepth--;
            else break;
        }
        else if (c == ']') bracketDepth++;
        else if (c == '[') {
            if (bracketDepth > 0) bracketDepth--;
            else break;
        }
        else if (!IsValidExpressionChar(c) && parenDepth == 0 && bracketDepth == 0) {
            break;
        }
        
        start--;
    }
    
    // Find end of expression
    int end = column;
    parenDepth = 0;
    bracketDepth = 0;
    
    while (end < static_cast<int>(lineText.length())) {
        char c = lineText[end];
        
        if (c == '(') parenDepth++;
        else if (c == ')') {
            if (parenDepth > 0) parenDepth--;
            else break;
        }
        else if (c == '[') bracketDepth++;
        else if (c == ']') {
            if (bracketDepth > 0) bracketDepth--;
            else break;
        }
        else if (!IsValidExpressionChar(c) && parenDepth == 0 && bracketDepth == 0) {
            break;
        }
        
        end++;
    }
    
    if (start >= end) return "";
    
    return lineText.substr(start, end - start);
}

std::string UCHoverEvaluator::GetWordAtPosition(int line, int column) {
    if (!textArea_) return "";
    
    std::string lineText = textArea_->GetLine(line);
    if (lineText.empty() || column >= static_cast<int>(lineText.length())) {
        return "";
    }
    
    // Check if on an identifier character
    if (!IsValidIdentifierChar(lineText[column])) {
        return "";
    }
    
    // Find start of word
    int start = column;
    while (start > 0 && IsValidIdentifierChar(lineText[start - 1])) {
        start--;
    }
    
    // Find end of word
    int end = column;
    while (end < static_cast<int>(lineText.length()) && 
           IsValidIdentifierChar(lineText[end])) {
        end++;
    }
    
    return lineText.substr(start, end - start);
}

std::string UCHoverEvaluator::GetSelectionOrWordAtPosition(int line, int column) {
    if (!textArea_) return "";
    
    // Check if there's a selection
    std::string selection = textArea_->GetSelectedText();
    if (!selection.empty() && selection.length() < 100) {
        return selection;
    }
    
    // Try to get a member expression (e.g., obj.member or ptr->member)
    std::string expr = GetExpressionAtPosition(line, column);
    if (!expr.empty()) {
        return expr;
    }
    
    // Fall back to simple word
    return GetWordAtPosition(line, column);
}

Rect2Di UCHoverEvaluator::GetExpressionBounds(int line, int column, 
                                               const std::string& expr) {
    // TODO: Calculate actual pixel bounds from text area
    return Rect2Di();
}

bool UCHoverEvaluator::IsValidIdentifierChar(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool UCHoverEvaluator::IsValidExpressionChar(char c) const {
    return IsValidIdentifierChar(c) || c == '.' || c == '-' || c == '>' ||
           c == '[' || c == ']' || c == '(' || c == ')' || c == ':';
}

bool UCHoverEvaluator::ShouldEvaluate() const {
    if (!enabled_) return false;
    
    if (onlyDuringDebug_) {
        if (!debugManager_) return false;
        if (!debugManager_->IsDebugging()) return false;
        if (debugManager_->GetState() != DebugSessionState::Paused) return false;
    }
    
    return true;
}

void UCHoverEvaluator::StartHoverTimer() {
    isHovering_ = true;
    timerExpired_ = false;
    hoverStartTime_ = std::chrono::steady_clock::now();
}

void UCHoverEvaluator::CancelHoverTimer() {
    isHovering_ = false;
    timerExpired_ = false;
}

void UCHoverEvaluator::OnHoverTimerExpired() {
    if (!ShouldEvaluate()) return;
    
    HoverEvaluationResult result = EvaluateAt(lastMousePos_.x, lastMousePos_.y);
    
    if (result.success || !result.expression.empty()) {
        ShowTooltip(result);
        
        if (onEvaluationComplete) {
            onEvaluationComplete(result);
        }
    }
}

void UCHoverEvaluator::ShowTooltip(const HoverEvaluationResult& result) {
    tooltip_->SetResult(result);
    tooltip_->Show(result.hoverPosition.x, result.hoverPosition.y);
    
    if (onTooltipShown) {
        onTooltipShown();
    }
}

} // namespace IDE
} // namespace UltraCanvas
