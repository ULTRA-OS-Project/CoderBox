// Apps/IDE/Debug/UI/UCVariablesPanel.cpp
// Variables Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCVariablesPanel.h"
#include "UltraCanvasButton.h"
#include <sstream>

namespace UltraCanvas {
namespace IDE {

UCVariablesPanel::UCVariablesPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    
    SetBackgroundColor(Color(250, 250, 250));
}

void UCVariablesPanel::Initialize() {
    CreateUI();
}

void UCVariablesPanel::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            RefreshFromDebugger();
        };
    }
}

void UCVariablesPanel::CreateUI() {
    int yOffset = 4;
    
    // Title label
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "variables_title", 0, 4, yOffset, GetWidth() - 8, 20);
    titleLabel_->SetText("Variables");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetFontSize(12);
    AddChild(titleLabel_);
    yOffset += 24;
    
    // Scope tabs container
    tabContainer_ = std::make_shared<UltraCanvasContainer>(
        "scope_tabs", 0, 4, yOffset, GetWidth() - 8, 24);
    tabContainer_->SetBackgroundColor(Color(235, 235, 235));
    
    // Create tab buttons
    int tabX = 2;
    auto localsTab = std::make_shared<UltraCanvasButton>(
        "tab_locals", 0, tabX, 2, 60, 20);
    localsTab->SetText("Locals");
    localsTab->SetOnClick([this]() { SetScope(VariablePanelScope::Locals); });
    tabContainer_->AddChild(localsTab);
    tabX += 64;
    
    auto argsTab = std::make_shared<UltraCanvasButton>(
        "tab_args", 0, tabX, 2, 60, 20);
    argsTab->SetText("Args");
    argsTab->SetOnClick([this]() { SetScope(VariablePanelScope::Arguments); });
    tabContainer_->AddChild(argsTab);
    tabX += 64;
    
    auto watchTab = std::make_shared<UltraCanvasButton>(
        "tab_watch", 0, tabX, 2, 60, 20);
    watchTab->SetText("Watch");
    watchTab->SetOnClick([this]() { SetScope(VariablePanelScope::Watches); });
    tabContainer_->AddChild(watchTab);
    
    AddChild(tabContainer_);
    yOffset += 28;
    
    // Variables tree view
    variablesTree_ = std::make_shared<UltraCanvasTreeView>(
        "variables_tree", 0, 4, yOffset, GetWidth() - 8, GetHeight() - yOffset - 4);
    variablesTree_->SetShowExpandButtons(true);
    variablesTree_->SetShowIcons(true);
    variablesTree_->SetRowHeight(20);
    variablesTree_->SetIndentWidth(16);
    variablesTree_->SetSelectionMode(TreeSelectionMode::Single);
    
    // Handle selection
    variablesTree_->onSelectionChange = [this](const std::vector<TreeNodeId>& selected) {
        // Variable selection handling
    };
    
    // Handle expansion
    variablesTree_->onNodeExpand = [this](TreeNodeId nodeId) {
        ExpandVariable(nodeId);
    };
    
    // Handle double-click for editing
    variablesTree_->onNodeDoubleClick = [this](TreeNodeId nodeId) {
        // Start editing the value
    };
    
    AddChild(variablesTree_);
}

void UCVariablesPanel::SetLocals(const std::vector<Variable>& locals) {
    currentLocals_ = locals;
    if (currentScope_ == VariablePanelScope::Locals || 
        currentScope_ == VariablePanelScope::All) {
        PopulateTree(locals);
    }
}

void UCVariablesPanel::SetArguments(const std::vector<Variable>& args) {
    currentArgs_ = args;
    if (currentScope_ == VariablePanelScope::Arguments ||
        currentScope_ == VariablePanelScope::All) {
        PopulateTree(args);
    }
}

void UCVariablesPanel::SetWatches(const std::vector<WatchEntry>& watches) {
    currentWatches_ = watches;
    if (currentScope_ == VariablePanelScope::Watches) {
        // Convert watch entries to variables for display
        std::vector<Variable> watchVars;
        for (const auto& watch : watches) {
            Variable var = watch.value;
            var.name = watch.expression;
            if (watch.hasError) {
                var.value = watch.errorMessage;
                var.errorMessage = watch.errorMessage;
            }
            watchVars.push_back(var);
        }
        PopulateTree(watchVars);
    }
}

void UCVariablesPanel::Clear() {
    currentLocals_.clear();
    currentArgs_.clear();
    currentWatches_.clear();
    nodeToVariableRef_.clear();
    nextNodeId_ = 1;
    
    if (variablesTree_) {
        variablesTree_->Clear();
    }
}

void UCVariablesPanel::RefreshFromDebugger() {
    if (!debugManager_ || !debugManager_->IsDebugging()) {
        Clear();
        return;
    }
    
    switch (currentScope_) {
        case VariablePanelScope::Locals:
            SetLocals(debugManager_->GetLocals());
            break;
            
        case VariablePanelScope::Arguments:
            SetArguments(debugManager_->GetArguments());
            break;
            
        case VariablePanelScope::Watches:
            debugManager_->RefreshWatches();
            SetWatches(debugManager_->GetWatchManager().GetAllWatches());
            break;
            
        case VariablePanelScope::All: {
            std::vector<Variable> all;
            auto locals = debugManager_->GetLocals();
            auto args = debugManager_->GetArguments();
            all.insert(all.end(), args.begin(), args.end());
            all.insert(all.end(), locals.begin(), locals.end());
            PopulateTree(all);
            break;
        }
    }
}

void UCVariablesPanel::SetScope(VariablePanelScope scope) {
    if (currentScope_ == scope) return;
    
    currentScope_ = scope;
    RefreshFromDebugger();
    
    // Update title
    if (titleLabel_) {
        switch (scope) {
            case VariablePanelScope::Locals:
                titleLabel_->SetText("Local Variables");
                break;
            case VariablePanelScope::Arguments:
                titleLabel_->SetText("Arguments");
                break;
            case VariablePanelScope::Watches:
                titleLabel_->SetText("Watch Expressions");
                break;
            case VariablePanelScope::All:
                titleLabel_->SetText("All Variables");
                break;
        }
    }
}

void UCVariablesPanel::PopulateTree(const std::vector<Variable>& variables, TreeNodeId parentId) {
    if (!variablesTree_) return;
    
    if (parentId == 0) {
        variablesTree_->Clear();
        nodeToVariableRef_.clear();
        nextNodeId_ = 1;
    }
    
    for (const auto& var : variables) {
        AddVariableNode(var, parentId);
    }
}

void UCVariablesPanel::AddVariableNode(const Variable& var, TreeNodeId parentId) {
    TreeNodeId nodeId = nextNodeId_++;
    
    TreeNodeData nodeData;
    
    // Format: name = value : type
    std::ostringstream text;
    text << var.name;
    if (!var.value.empty()) {
        text << " = " << FormatVariableValue(var);
    }
    if (!var.type.empty()) {
        text << " : " << var.type;
    }
    nodeData.text = text.str();
    
    // Set icon based on variable type
    nodeData.leftIcon.iconPath = GetVariableIcon(var);
    nodeData.leftIcon.width = 16;
    nodeData.leftIcon.height = 16;
    nodeData.leftIcon.visible = true;
    
    // Set text color based on value
    nodeData.textColor = GetValueColor(var);
    
    // Add node
    if (parentId == 0) {
        variablesTree_->AddNode(nodeId, nodeData);
    } else {
        variablesTree_->AddNode(nodeId, nodeData, parentId);
    }
    
    // Track variable reference for expansion
    if (var.hasChildren && var.variablesReference > 0) {
        nodeToVariableRef_[nodeId] = var.variablesReference;
        
        // Add placeholder child so expand button shows
        TreeNodeData placeholder;
        placeholder.text = "Loading...";
        variablesTree_->AddNode(nextNodeId_++, placeholder, nodeId);
    }
    
    // Add children if already loaded
    if (!var.children.empty()) {
        for (const auto& child : var.children) {
            AddVariableNode(child, nodeId);
        }
    }
}

void UCVariablesPanel::ExpandVariable(TreeNodeId nodeId) {
    auto it = nodeToVariableRef_.find(nodeId);
    if (it == nodeToVariableRef_.end()) return;
    
    if (!debugManager_ || !debugManager_->IsDebugging()) return;
    
    int variablesRef = it->second;
    auto children = debugManager_->ExpandVariable(variablesRef);
    
    // Remove placeholder and add real children
    auto existingChildren = variablesTree_->GetChildren(nodeId);
    for (auto childId : existingChildren) {
        variablesTree_->RemoveNode(childId);
    }
    
    for (const auto& child : children) {
        AddVariableNode(child, nodeId);
    }
    
    // Don't expand again
    nodeToVariableRef_.erase(it);
}

std::string UCVariablesPanel::GetVariableIcon(const Variable& var) const {
    switch (var.category) {
        case VariableCategory::Primitive:
            return "icons/var_primitive.png";
        case VariableCategory::Pointer:
            return "icons/var_pointer.png";
        case VariableCategory::Reference:
            return "icons/var_reference.png";
        case VariableCategory::Array:
            return "icons/var_array.png";
        case VariableCategory::Struct:
            return "icons/var_struct.png";
        case VariableCategory::Class:
            return "icons/var_class.png";
        case VariableCategory::StringType:
            return "icons/var_string.png";
        case VariableCategory::Container:
            return "icons/var_container.png";
        case VariableCategory::Enum:
            return "icons/var_enum.png";
        default:
            return "icons/var_unknown.png";
    }
}

std::string UCVariablesPanel::FormatVariableValue(const Variable& var) const {
    std::string value = var.value;
    
    // Truncate very long values
    const size_t maxLen = 100;
    if (value.length() > maxLen) {
        value = value.substr(0, maxLen) + "...";
    }
    
    return value;
}

Color UCVariablesPanel::GetValueColor(const Variable& var) const {
    if (!var.errorMessage.empty()) {
        return Color(200, 0, 0);  // Red for errors
    }
    
    switch (var.category) {
        case VariableCategory::StringType:
            return Color(163, 21, 21);  // Brown for strings
        case VariableCategory::Primitive:
            return Color(0, 128, 0);    // Green for primitives
        case VariableCategory::Pointer:
        case VariableCategory::Reference:
            return Color(0, 0, 200);    // Blue for pointers
        default:
            return Color(0, 0, 0);      // Black for others
    }
}

void UCVariablesPanel::BeginEditVariable(const std::string& varName) {
    // Start inline editing in tree view
}

void UCVariablesPanel::CommitVariableEdit(const std::string& varName, const std::string& newValue) {
    if (debugManager_ && debugManager_->IsDebugging()) {
        if (debugManager_->SetVariable(varName, newValue)) {
            RefreshFromDebugger();
        }
    }
    
    if (onVariableEdit) {
        onVariableEdit(varName, newValue);
    }
}

void UCVariablesPanel::CancelVariableEdit() {
    // Cancel inline editing
}

void UCVariablesPanel::HandleRender(IRenderContext* ctx) {
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
