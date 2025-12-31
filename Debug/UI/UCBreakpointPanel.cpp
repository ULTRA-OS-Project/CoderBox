// Apps/IDE/Debug/UI/UCBreakpointPanel.cpp
// Breakpoint Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCBreakpointPanel.h"
#include <sstream>

namespace UltraCanvas {
namespace IDE {

UCBreakpointPanel::UCBreakpointPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    
    SetBackgroundColor(Color(250, 250, 250));
}

void UCBreakpointPanel::Initialize() {
    CreateUI();
}

void UCBreakpointPanel::SetBreakpointManager(UCBreakpointManager* manager) {
    breakpointManager_ = manager;
    
    if (breakpointManager_) {
        // Subscribe to breakpoint changes
        breakpointManager_->onBreakpointAdd = [this](const UCBreakpoint& bp) {
            AddBreakpoint(bp.GetData());
        };
        
        breakpointManager_->onBreakpointRemove = [this](int id) {
            RemoveBreakpoint(id);
        };
        
        breakpointManager_->onBreakpointChange = [this](const UCBreakpoint& bp) {
            UpdateBreakpoint(bp.GetData());
        };
        
        // Initial population
        Refresh();
    }
}

void UCBreakpointPanel::CreateUI() {
    int yOffset = 4;
    
    // Title label
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "breakpoints_title", 0, 4, yOffset, GetWidth() - 8, 20);
    titleLabel_->SetText("Breakpoints");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetFontSize(12);
    AddChild(titleLabel_);
    yOffset += 24;
    
    // Toolbar with Add/Remove All buttons
    toolbar_ = std::make_shared<UltraCanvasToolbar>(
        "breakpoints_toolbar", 0, 4, yOffset, GetWidth() - 8, 24);
    
    ToolbarAppearance toolbarAppearance;
    toolbarAppearance.backgroundColor = Color(245, 245, 245);
    toolbarAppearance.buttonSpacing = 2;
    toolbarAppearance.padding = 2;
    toolbar_->SetAppearance(toolbarAppearance);
    
    toolbar_->AddButton("bp_add", "", "icons/breakpoint_add.png", [this]() {
        if (onAddBreakpointClicked) onAddBreakpointClicked();
    });
    if (auto btn = toolbar_->GetItem("bp_add")) {
        auto* toolbarBtn = dynamic_cast<UltraCanvasToolbarButton*>(btn.get());
        if (toolbarBtn) toolbarBtn->SetTooltip("Add Breakpoint");
    }
    
    toolbar_->AddButton("bp_remove_all", "", "icons/breakpoint_remove_all.png", [this]() {
        if (onRemoveAllClicked) onRemoveAllClicked();
    });
    if (auto btn = toolbar_->GetItem("bp_remove_all")) {
        auto* toolbarBtn = dynamic_cast<UltraCanvasToolbarButton*>(btn.get());
        if (toolbarBtn) toolbarBtn->SetTooltip("Remove All Breakpoints");
    }
    
    toolbar_->AddSeparator("sep1");
    
    toolbar_->AddButton("bp_enable_all", "", "icons/breakpoint_enable_all.png", [this]() {
        if (breakpointManager_) {
            for (const auto& bp : breakpointManager_->GetAllBreakpoints()) {
                breakpointManager_->SetBreakpointEnabled(bp.GetId(), true);
            }
        }
    });
    
    toolbar_->AddButton("bp_disable_all", "", "icons/breakpoint_disable_all.png", [this]() {
        if (breakpointManager_) {
            for (const auto& bp : breakpointManager_->GetAllBreakpoints()) {
                breakpointManager_->SetBreakpointEnabled(bp.GetId(), false);
            }
        }
    });
    
    AddChild(toolbar_);
    yOffset += 28;
    
    // Breakpoint tree view
    breakpointTree_ = std::make_shared<UltraCanvasTreeView>(
        "breakpoints_tree", 0, 4, yOffset, GetWidth() - 8, GetHeight() - yOffset - 4);
    breakpointTree_->SetShowExpandButtons(false);  // Flat list
    breakpointTree_->SetShowIcons(true);
    breakpointTree_->SetShowCheckboxes(true);      // For enable/disable
    breakpointTree_->SetRowHeight(22);
    breakpointTree_->SetSelectionMode(TreeSelectionMode::Single);
    
    // Handle selection
    breakpointTree_->onSelectionChange = [this](const std::vector<TreeNodeId>& selected) {
        if (!selected.empty()) {
            selectedBreakpointId_ = NodeIdToBreakpointId(selected[0]);
            if (onBreakpointSelected) {
                onBreakpointSelected(selectedBreakpointId_);
            }
        }
    };
    
    // Handle double-click to navigate
    breakpointTree_->onNodeDoubleClick = [this](TreeNodeId nodeId) {
        int bpId = NodeIdToBreakpointId(nodeId);
        if (bpId >= 0 && onBreakpointDoubleClicked) {
            onBreakpointDoubleClicked(bpId);
        }
    };
    
    // Handle checkbox toggle for enable/disable
    breakpointTree_->onNodeCheckChange = [this](TreeNodeId nodeId, bool checked) {
        int bpId = NodeIdToBreakpointId(nodeId);
        if (bpId >= 0) {
            if (breakpointManager_) {
                breakpointManager_->SetBreakpointEnabled(bpId, checked);
            }
            if (onBreakpointEnableChanged) {
                onBreakpointEnableChanged(bpId, checked);
            }
        }
    };
    
    AddChild(breakpointTree_);
}

void UCBreakpointPanel::Refresh() {
    Clear();
    
    if (!breakpointManager_) return;
    
    PopulateBreakpoints();
}

void UCBreakpointPanel::PopulateBreakpoints() {
    if (!breakpointTree_ || !breakpointManager_) return;
    
    auto breakpoints = breakpointManager_->GetAllBreakpoints();
    
    for (const auto& bp : breakpoints) {
        AddBreakpoint(bp.GetData());
    }
}

void UCBreakpointPanel::AddBreakpoint(const Breakpoint& bp) {
    if (!breakpointTree_) return;
    
    TreeNodeId nodeId = nextNodeId_++;
    
    TreeNodeData nodeData;
    nodeData.text = FormatBreakpoint(bp);
    nodeData.leftIcon.iconPath = GetBreakpointIcon(bp);
    nodeData.leftIcon.width = 16;
    nodeData.leftIcon.height = 16;
    nodeData.leftIcon.visible = true;
    nodeData.isChecked = bp.enabled;
    
    // Set color based on state
    if (bp.state == BreakpointState::Invalid) {
        nodeData.textColor = Color(200, 0, 0);  // Red for invalid
    } else if (bp.state == BreakpointState::Pending) {
        nodeData.textColor = Color(128, 128, 128);  // Gray for pending
    } else if (!bp.enabled) {
        nodeData.textColor = Color(160, 160, 160);  // Dimmed for disabled
    }
    
    breakpointTree_->AddNode(nodeId, nodeData);
    
    bpIdToNodeId_[bp.id] = nodeId;
    nodeIdToBpId_[nodeId] = bp.id;
}

void UCBreakpointPanel::RemoveBreakpoint(int id) {
    auto it = bpIdToNodeId_.find(id);
    if (it == bpIdToNodeId_.end()) return;
    
    TreeNodeId nodeId = it->second;
    
    if (breakpointTree_) {
        breakpointTree_->RemoveNode(nodeId);
    }
    
    nodeIdToBpId_.erase(nodeId);
    bpIdToNodeId_.erase(it);
    
    if (selectedBreakpointId_ == id) {
        selectedBreakpointId_ = -1;
    }
}

void UCBreakpointPanel::UpdateBreakpoint(const Breakpoint& bp) {
    auto it = bpIdToNodeId_.find(bp.id);
    if (it == bpIdToNodeId_.end()) return;
    
    TreeNodeId nodeId = it->second;
    
    if (breakpointTree_) {
        TreeNodeData nodeData;
        nodeData.text = FormatBreakpoint(bp);
        nodeData.leftIcon.iconPath = GetBreakpointIcon(bp);
        nodeData.leftIcon.width = 16;
        nodeData.leftIcon.height = 16;
        nodeData.leftIcon.visible = true;
        nodeData.isChecked = bp.enabled;
        
        breakpointTree_->UpdateNode(nodeId, nodeData);
    }
}

void UCBreakpointPanel::Clear() {
    if (breakpointTree_) {
        breakpointTree_->Clear();
    }
    
    bpIdToNodeId_.clear();
    nodeIdToBpId_.clear();
    nextNodeId_ = 1;
    selectedBreakpointId_ = -1;
}

void UCBreakpointPanel::SelectBreakpoint(int id) {
    auto it = bpIdToNodeId_.find(id);
    if (it == bpIdToNodeId_.end()) return;
    
    if (breakpointTree_) {
        breakpointTree_->SetSelectedNode(it->second);
    }
    
    selectedBreakpointId_ = id;
}

TreeNodeId UCBreakpointPanel::BreakpointIdToNodeId(int bpId) const {
    auto it = bpIdToNodeId_.find(bpId);
    return (it != bpIdToNodeId_.end()) ? it->second : 0;
}

int UCBreakpointPanel::NodeIdToBreakpointId(TreeNodeId nodeId) const {
    auto it = nodeIdToBpId_.find(nodeId);
    return (it != nodeIdToBpId_.end()) ? it->second : -1;
}

std::string UCBreakpointPanel::FormatBreakpoint(const Breakpoint& bp) const {
    std::ostringstream ss;
    
    switch (bp.type) {
        case BreakpointType::LineBreakpoint:
        case BreakpointType::ConditionalBreakpoint: {
            // Get filename
            std::string filename = bp.location.filePath;
            size_t pos = filename.find_last_of("/\\");
            if (pos != std::string::npos) {
                filename = filename.substr(pos + 1);
            }
            ss << filename << ":" << bp.location.line;
            
            if (!bp.condition.empty()) {
                ss << " [" << bp.condition << "]";
            }
            break;
        }
        
        case BreakpointType::FunctionBreakpoint:
            ss << "func: " << bp.functionName;
            break;
            
        case BreakpointType::AddressBreakpoint:
            ss << "addr: 0x" << std::hex << bp.address;
            break;
            
        case BreakpointType::Logpoint:
            ss << "log: " << bp.location.filePath << ":" << bp.location.line;
            ss << " \"" << bp.logMessage << "\"";
            break;
            
        default:
            ss << "breakpoint " << bp.id;
            break;
    }
    
    // Add hit count if > 0
    if (bp.hitCount > 0) {
        ss << " (hit " << bp.hitCount << "x)";
    }
    
    return ss.str();
}

std::string UCBreakpointPanel::GetBreakpointIcon(const Breakpoint& bp) const {
    if (!bp.enabled) {
        return "icons/breakpoint_disabled.png";
    }
    
    switch (bp.state) {
        case BreakpointState::Invalid:
            return "icons/breakpoint_invalid.png";
        case BreakpointState::Pending:
            return "icons/breakpoint_pending.png";
        default:
            break;
    }
    
    switch (bp.type) {
        case BreakpointType::ConditionalBreakpoint:
            return "icons/breakpoint_conditional.png";
        case BreakpointType::Logpoint:
            return "icons/breakpoint_logpoint.png";
        case BreakpointType::FunctionBreakpoint:
            return "icons/breakpoint_function.png";
        default:
            return "icons/breakpoint.png";
    }
}

void UCBreakpointPanel::HandleRender(IRenderContext* ctx) {
    // Draw panel background
    ctx->DrawFilledRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        GetBackgroundColor());
    
    // Draw border
    ctx->DrawRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        Color(200, 200, 200), 1);
    
    // Render children
    UltraCanvasContainer::HandleRender(ctx);
}

} // namespace IDE
} // namespace UltraCanvas
