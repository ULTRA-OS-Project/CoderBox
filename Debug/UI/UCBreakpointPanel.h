// Apps/IDE/Debug/UI/UCBreakpointPanel.h
// Breakpoint Panel for ULTRA IDE using UltraCanvas TreeView
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTreeView.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasButton.h"
#include "UltraCanvasToolbar.h"
#include "../Breakpoints/UCBreakpointManager.h"
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Breakpoint panel using UltraCanvasTreeView
 * 
 * Displays all breakpoints with enable/disable checkboxes.
 * Allows navigation to breakpoint locations.
 */
class UCBreakpointPanel : public UltraCanvasContainer {
public:
    UCBreakpointPanel(const std::string& id, int x, int y, int width, int height);
    ~UCBreakpointPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetBreakpointManager(UCBreakpointManager* manager);
    
    // =========================================================================
    // DATA UPDATES
    // =========================================================================
    
    void Refresh();
    void AddBreakpoint(const Breakpoint& bp);
    void RemoveBreakpoint(int id);
    void UpdateBreakpoint(const Breakpoint& bp);
    void Clear();
    
    // =========================================================================
    // SELECTION
    // =========================================================================
    
    int GetSelectedBreakpointId() const { return selectedBreakpointId_; }
    void SelectBreakpoint(int id);
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(int breakpointId)> onBreakpointSelected;
    std::function<void(int breakpointId)> onBreakpointDoubleClicked;
    std::function<void(int breakpointId, bool enabled)> onBreakpointEnableChanged;
    std::function<void()> onAddBreakpointClicked;
    std::function<void()> onRemoveAllClicked;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateBreakpoints();
    TreeNodeId BreakpointIdToNodeId(int bpId) const;
    int NodeIdToBreakpointId(TreeNodeId nodeId) const;
    std::string FormatBreakpoint(const Breakpoint& bp) const;
    std::string GetBreakpointIcon(const Breakpoint& bp) const;
    
    UCBreakpointManager* breakpointManager_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
    std::shared_ptr<UltraCanvasTreeView> breakpointTree_;
    
    // State
    int selectedBreakpointId_ = -1;
    std::map<int, TreeNodeId> bpIdToNodeId_;
    std::map<TreeNodeId, int> nodeIdToBpId_;
    TreeNodeId nextNodeId_ = 1;
};

} // namespace IDE
} // namespace UltraCanvas
