// Apps/IDE/Debug/UI/UCVariablesPanel.h
// Variables Panel for ULTRA IDE using UltraCanvas TreeView
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTreeView.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasTabControl.h"
#include "../Core/UCDebugManager.h"
#include "../Variables/UCDebugVariable.h"
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Variables panel scope tabs
 */
enum class VariablePanelScope {
    Locals,
    Arguments,
    Watches,
    All
};

/**
 * @brief Variables panel using UltraCanvasTreeView
 * 
 * Displays local variables, arguments, and watch expressions in a tree view.
 * Supports variable expansion for complex types.
 */
class UCVariablesPanel : public UltraCanvasContainer {
public:
    UCVariablesPanel(const std::string& id, int x, int y, int width, int height);
    ~UCVariablesPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // DATA UPDATES
    // =========================================================================
    
    void SetLocals(const std::vector<Variable>& locals);
    void SetArguments(const std::vector<Variable>& args);
    void SetWatches(const std::vector<WatchEntry>& watches);
    void Clear();
    
    void RefreshFromDebugger();
    void SetScope(VariablePanelScope scope);
    VariablePanelScope GetScope() const { return currentScope_; }
    
    // =========================================================================
    // VARIABLE EDITING
    // =========================================================================
    
    void BeginEditVariable(const std::string& varName);
    void CommitVariableEdit(const std::string& varName, const std::string& newValue);
    void CancelVariableEdit();
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const Variable&)> onVariableSelected;
    std::function<void(const Variable&)> onVariableExpand;
    std::function<void(const std::string&, const std::string&)> onVariableEdit;
    std::function<void(const std::string&)> onAddWatch;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateTree(const std::vector<Variable>& variables, TreeNodeId parentId = 0);
    void AddVariableNode(const Variable& var, TreeNodeId parentId);
    void ExpandVariable(TreeNodeId nodeId);
    std::string GetVariableIcon(const Variable& var) const;
    std::string FormatVariableValue(const Variable& var) const;
    Color GetValueColor(const Variable& var) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasTreeView> variablesTree_;
    
    // Scope tabs (simple button-based tabs)
    std::shared_ptr<UltraCanvasContainer> tabContainer_;
    
    // State
    VariablePanelScope currentScope_ = VariablePanelScope::Locals;
    std::vector<Variable> currentLocals_;
    std::vector<Variable> currentArgs_;
    std::vector<WatchEntry> currentWatches_;
    
    std::map<TreeNodeId, int> nodeToVariableRef_;  // For expansion
    TreeNodeId nextNodeId_ = 1;
};

} // namespace IDE
} // namespace UltraCanvas
