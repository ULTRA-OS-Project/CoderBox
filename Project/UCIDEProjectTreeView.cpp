// Apps/IDE/Project/UCIDEProjectTreeView.cpp
// Project File Tree View Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCIDEProjectTreeView.h"
#include "../Build/UCBuildOutput.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCIDEProjectTreeView::UCIDEProjectTreeView() {
    style = TreeViewStyle();
}

UCIDEProjectTreeView::~UCIDEProjectTreeView() {
    Clear();
}

// ============================================================================
// PROJECT BINDING
// ============================================================================

void UCIDEProjectTreeView::SetProject(std::shared_ptr<UCIDEProject> project) {
    currentProject = project;
    Refresh();
}

void UCIDEProjectTreeView::Refresh() {
    Clear();
    
    if (!currentProject) {
        return;
    }
    
    // Create root node for project
    rootNode = std::make_unique<TreeNode>();
    rootNode->id = GenerateNodeId();
    rootNode->name = currentProject->name;
    rootNode->path = "";
    rootNode->absolutePath = currentProject->rootDirectory;
    rootNode->type = TreeNodeType::Root;
    rootNode->isExpanded = true;
    rootNode->parent = nullptr;
    
    // Register in lookup maps
    pathToNodeMap[""] = rootNode.get();
    idToNodeMap[rootNode->id] = rootNode.get();
    
    // Build tree from project's root folder
    BuildTree(currentProject->rootFolder, rootNode.get());
    
    // Apply any active filter
    if (!filterText.empty()) {
        ApplyFilter(rootNode.get());
    }
}

void UCIDEProjectTreeView::Clear() {
    rootNode.reset();
    selectedNode = nullptr;
    hoveredNode = nullptr;
    pathToNodeMap.clear();
    idToNodeMap.clear();
    nextNodeId = 1;
    scrollOffset = 0;
}

// ============================================================================
// TREE OPERATIONS
// ============================================================================

TreeNode* UCIDEProjectTreeView::FindNode(const std::string& path) {
    auto it = pathToNodeMap.find(path);
    if (it != pathToNodeMap.end()) {
        return it->second;
    }
    return nullptr;
}

const TreeNode* UCIDEProjectTreeView::FindNode(const std::string& path) const {
    auto it = pathToNodeMap.find(path);
    if (it != pathToNodeMap.end()) {
        return it->second;
    }
    return nullptr;
}

TreeNode* UCIDEProjectTreeView::FindNodeById(const std::string& id) {
    auto it = idToNodeMap.find(id);
    if (it != idToNodeMap.end()) {
        return it->second;
    }
    return nullptr;
}

void UCIDEProjectTreeView::SelectNode(const std::string& path) {
    TreeNode* node = FindNode(path);
    SelectNode(node);
}

void UCIDEProjectTreeView::SelectNode(TreeNode* node) {
    if (selectedNode == node) {
        return;
    }
    
    // Deselect previous
    if (selectedNode) {
        selectedNode->isSelected = false;
    }
    
    selectedNode = node;
    
    if (selectedNode) {
        selectedNode->isSelected = true;
        
        // Make sure node is visible (expand parents)
        TreeNode* parent = selectedNode->parent;
        while (parent) {
            if (!parent->isExpanded) {
                parent->isExpanded = true;
            }
            parent = parent->parent;
        }
        
        // Scroll to make visible
        ScrollToNode(selectedNode);
    }
    
    if (onSelectionChange) {
        onSelectionChange(selectedNode);
    }
}

void UCIDEProjectTreeView::ExpandNode(TreeNode* node) {
    if (!node || !node->IsFolder() || node->isExpanded) {
        return;
    }
    
    node->isExpanded = true;
    
    if (onNodeExpand) {
        onNodeExpand(node);
    }
}

void UCIDEProjectTreeView::CollapseNode(TreeNode* node) {
    if (!node || !node->IsFolder() || !node->isExpanded) {
        return;
    }
    
    node->isExpanded = false;
    
    if (onNodeCollapse) {
        onNodeCollapse(node);
    }
}

void UCIDEProjectTreeView::ToggleNode(TreeNode* node) {
    if (!node || !node->IsFolder()) {
        return;
    }
    
    if (node->isExpanded) {
        CollapseNode(node);
    } else {
        ExpandNode(node);
    }
}

void UCIDEProjectTreeView::ExpandAll() {
    if (!rootNode) return;
    
    std::function<void(TreeNode*)> expandRecursive = [&](TreeNode* node) {
        if (node->IsFolder()) {
            node->isExpanded = true;
            for (auto& child : node->children) {
                expandRecursive(child.get());
            }
        }
    };
    
    expandRecursive(rootNode.get());
}

void UCIDEProjectTreeView::CollapseAll() {
    if (!rootNode) return;
    
    std::function<void(TreeNode*)> collapseRecursive = [&](TreeNode* node) {
        if (node->IsFolder() && node != rootNode.get()) {
            node->isExpanded = false;
        }
        for (auto& child : node->children) {
            collapseRecursive(child.get());
        }
    };
    
    collapseRecursive(rootNode.get());
}

void UCIDEProjectTreeView::RevealPath(const std::string& path) {
    TreeNode* node = FindNode(path);
    if (!node) {
        return;
    }
    
    // Expand all parents
    TreeNode* parent = node->parent;
    while (parent) {
        parent->isExpanded = true;
        parent = parent->parent;
    }
    
    // Select and scroll to node
    SelectNode(node);
}

// ============================================================================
// FILTERING
// ============================================================================

void UCIDEProjectTreeView::SetFilter(const std::string& filter) {
    filterText = filter;
    
    // Convert to lowercase for case-insensitive matching
    for (char& c : filterText) {
        c = std::tolower(c);
    }
    
    if (rootNode) {
        ApplyFilter(rootNode.get());
    }
}

void UCIDEProjectTreeView::ClearFilter() {
    filterText.clear();
    
    // Make all nodes visible
    if (rootNode) {
        std::function<void(TreeNode*)> showAll = [&](TreeNode* node) {
            node->isVisible = true;
            for (auto& child : node->children) {
                showAll(child.get());
            }
        };
        showAll(rootNode.get());
    }
}

void UCIDEProjectTreeView::SetFileTypeFilter(const std::vector<ProjectFileType>& types) {
    fileTypeFilter = types;
    if (rootNode) {
        ApplyFilter(rootNode.get());
    }
}

void UCIDEProjectTreeView::ClearFileTypeFilter() {
    fileTypeFilter.clear();
    if (!filterText.empty() && rootNode) {
        ApplyFilter(rootNode.get());
    } else {
        ClearFilter();
    }
}

void UCIDEProjectTreeView::ApplyFilter(TreeNode* node) {
    if (!node) return;
    
    if (node->IsFolder()) {
        // Folder is visible if any child is visible
        bool anyChildVisible = false;
        
        for (auto& child : node->children) {
            ApplyFilter(child.get());
            if (child->isVisible) {
                anyChildVisible = true;
            }
        }
        
        // Root is always visible, other folders depend on children
        node->isVisible = (node == rootNode.get()) || anyChildVisible;
        
        // Auto-expand folders with visible children during filtering
        if (anyChildVisible && !filterText.empty()) {
            node->isExpanded = true;
        }
    } else {
        // File visibility
        node->isVisible = MatchesFilter(node);
    }
}

bool UCIDEProjectTreeView::MatchesFilter(const TreeNode* node) const {
    // Check file type filter
    if (!fileTypeFilter.empty()) {
        bool matchesType = false;
        for (auto type : fileTypeFilter) {
            if (node->fileType == type) {
                matchesType = true;
                break;
            }
        }
        if (!matchesType) {
            return false;
        }
    }
    
    // Check text filter
    if (filterText.empty()) {
        return true;
    }
    
    // Case-insensitive name matching
    std::string lowerName = node->name;
    for (char& c : lowerName) {
        c = std::tolower(c);
    }
    
    return lowerName.find(filterText) != std::string::npos;
}

// ============================================================================
// FILE MODIFICATION TRACKING
// ============================================================================

void UCIDEProjectTreeView::SetFileModified(const std::string& path, bool modified) {
    TreeNode* node = FindNode(path);
    if (node) {
        node->isModified = modified;
    }
}

void UCIDEProjectTreeView::SetFileOpen(const std::string& path, bool open) {
    TreeNode* node = FindNode(path);
    if (node) {
        node->isOpen = open;
    }
}

std::vector<std::string> UCIDEProjectTreeView::GetModifiedFiles() const {
    std::vector<std::string> modified;
    
    if (!rootNode) return modified;
    
    std::function<void(const TreeNode*)> collect = [&](const TreeNode* node) {
        if (node->isModified && node->IsFile()) {
            modified.push_back(node->path);
        }
        for (const auto& child : node->children) {
            collect(child.get());
        }
    };
    
    collect(rootNode.get());
    return modified;
}

// ============================================================================
// RENDERING
// ============================================================================

std::vector<TreeNode*> UCIDEProjectTreeView::GetVisibleNodes() const {
    std::vector<TreeNode*> nodes;
    
    if (rootNode) {
        CollectVisibleNodes(rootNode.get(), nodes);
    }
    
    return nodes;
}

int UCIDEProjectTreeView::GetVisibleRowCount() const {
    return static_cast<int>(GetVisibleNodes().size());
}

TreeNode* UCIDEProjectTreeView::GetNodeAtRow(int row) {
    auto nodes = GetVisibleNodes();
    if (row >= 0 && row < static_cast<int>(nodes.size())) {
        return nodes[row];
    }
    return nullptr;
}

int UCIDEProjectTreeView::GetRowOfNode(const TreeNode* node) const {
    if (!node) return -1;
    
    auto nodes = GetVisibleNodes();
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        if (nodes[i] == node) {
            return i;
        }
    }
    
    return -1;
}

void UCIDEProjectTreeView::CollectVisibleNodes(TreeNode* node, std::vector<TreeNode*>& nodes) const {
    if (!node->isVisible) {
        return;
    }
    
    nodes.push_back(node);
    
    if (node->IsFolder() && node->isExpanded) {
        for (auto& child : node->children) {
            CollectVisibleNodes(child.get(), nodes);
        }
    }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

bool UCIDEProjectTreeView::HandleClick(int x, int y, bool isDoubleClick, bool isRightClick) {
    // Calculate which row was clicked
    int row = (y + scrollOffset) / style.rowHeight;
    TreeNode* clickedNode = GetNodeAtRow(row);
    
    if (!clickedNode) {
        return false;
    }
    
    // Calculate expand/collapse toggle area
    int depth = clickedNode->GetDepth();
    int toggleX = depth * style.indentWidth;
    int toggleWidth = style.iconSize;
    
    if (isRightClick) {
        // Show context menu
        SelectNode(clickedNode);
        if (onContextMenu) {
            onContextMenu(clickedNode, x, y);
        }
        return true;
    }
    
    // Check if clicked on toggle arrow (for folders)
    if (clickedNode->IsFolder() && x >= toggleX && x < toggleX + toggleWidth) {
        ToggleNode(clickedNode);
        return true;
    }
    
    // Select node
    SelectNode(clickedNode);
    
    if (isDoubleClick) {
        if (clickedNode->IsFolder()) {
            // Toggle folder on double-click
            ToggleNode(clickedNode);
        } else {
            // Open file on double-click
            if (onFileOpen) {
                onFileOpen(clickedNode->path);
            }
        }
    }
    
    return true;
}

bool UCIDEProjectTreeView::HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) {
    if (!selectedNode) {
        return false;
    }
    
    // Key codes (simplified - would use platform-specific codes)
    const int KEY_UP = 38;
    const int KEY_DOWN = 40;
    const int KEY_LEFT = 37;
    const int KEY_RIGHT = 39;
    const int KEY_ENTER = 13;
    const int KEY_DELETE = 46;
    const int KEY_F2 = 113;
    
    switch (keyCode) {
        case KEY_UP: {
            // Move selection up
            auto nodes = GetVisibleNodes();
            int currentRow = GetRowOfNode(selectedNode);
            if (currentRow > 0) {
                SelectNode(nodes[currentRow - 1]);
            }
            return true;
        }
        
        case KEY_DOWN: {
            // Move selection down
            auto nodes = GetVisibleNodes();
            int currentRow = GetRowOfNode(selectedNode);
            if (currentRow < static_cast<int>(nodes.size()) - 1) {
                SelectNode(nodes[currentRow + 1]);
            }
            return true;
        }
        
        case KEY_LEFT: {
            if (selectedNode->IsFolder() && selectedNode->isExpanded) {
                CollapseNode(selectedNode);
            } else if (selectedNode->parent && selectedNode->parent != rootNode.get()) {
                SelectNode(selectedNode->parent);
            }
            return true;
        }
        
        case KEY_RIGHT: {
            if (selectedNode->IsFolder()) {
                if (!selectedNode->isExpanded) {
                    ExpandNode(selectedNode);
                } else if (!selectedNode->children.empty()) {
                    // Select first child
                    for (auto& child : selectedNode->children) {
                        if (child->isVisible) {
                            SelectNode(child.get());
                            break;
                        }
                    }
                }
            }
            return true;
        }
        
        case KEY_ENTER: {
            if (selectedNode->IsFolder()) {
                ToggleNode(selectedNode);
            } else {
                if (onFileOpen) {
                    onFileOpen(selectedNode->path);
                }
            }
            return true;
        }
        
        case KEY_DELETE: {
            if (!ctrl && onFileDelete) {
                onFileDelete(selectedNode->path);
            }
            return true;
        }
        
        case KEY_F2: {
            // Rename (would trigger inline edit)
            if (onContextAction) {
                onContextAction(selectedNode, TreeContextAction::Rename);
            }
            return true;
        }
    }
    
    return false;
}

void UCIDEProjectTreeView::HandleScroll(int delta) {
    SetScrollOffset(scrollOffset - delta * style.rowHeight);
}

void UCIDEProjectTreeView::SetScrollOffset(int offset) {
    int maxScroll = GetContentHeight() - viewHeight;
    scrollOffset = std::max(0, std::min(offset, maxScroll));
}

void UCIDEProjectTreeView::ScrollToNode(TreeNode* node) {
    if (!node) return;
    
    int row = GetRowOfNode(node);
    if (row < 0) return;
    
    int nodeTop = row * style.rowHeight;
    int nodeBottom = nodeTop + style.rowHeight;
    
    if (nodeTop < scrollOffset) {
        SetScrollOffset(nodeTop);
    } else if (nodeBottom > scrollOffset + viewHeight) {
        SetScrollOffset(nodeBottom - viewHeight);
    }
}

int UCIDEProjectTreeView::GetContentHeight() const {
    return GetVisibleRowCount() * style.rowHeight;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UCIDEProjectTreeView::BuildTree(const ProjectFolder& folder, TreeNode* parent) {
    // Add subfolders first
    for (const auto& subfolder : folder.subfolders) {
        auto node = CreateFolderNode(subfolder, parent);
        TreeNode* nodePtr = node.get();
        
        // Register in lookup
        pathToNodeMap[node->path] = nodePtr;
        idToNodeMap[node->id] = nodePtr;
        
        parent->children.push_back(std::move(node));
        
        // Recursively build children
        BuildTree(subfolder, nodePtr);
    }
    
    // Add files
    for (const auto& file : folder.files) {
        auto node = CreateFileNode(file, parent);
        
        // Register in lookup
        pathToNodeMap[node->path] = node.get();
        idToNodeMap[node->id] = node.get();
        
        parent->children.push_back(std::move(node));
    }
    
    // Sort children
    SortChildren(parent);
}

std::unique_ptr<TreeNode> UCIDEProjectTreeView::CreateFileNode(const ProjectFile& file, TreeNode* parent) {
    auto node = std::make_unique<TreeNode>();
    
    node->id = GenerateNodeId();
    node->name = file.fileName;
    node->path = file.relativePath;
    node->absolutePath = file.absolutePath;
    node->type = GetTreeNodeType(file.type);
    node->fileType = file.type;
    node->isExpanded = false;
    node->isSelected = false;
    node->isModified = file.isModified;
    node->isOpen = file.isOpen;
    node->isVisible = true;
    node->parent = parent;
    
    return node;
}

std::unique_ptr<TreeNode> UCIDEProjectTreeView::CreateFolderNode(const ProjectFolder& folder, TreeNode* parent) {
    auto node = std::make_unique<TreeNode>();
    
    node->id = GenerateNodeId();
    node->name = folder.name;
    node->path = folder.relativePath;
    node->absolutePath = folder.absolutePath;
    node->type = TreeNodeType::Folder;
    node->fileType = ProjectFileType::Unknown;
    node->isExpanded = folder.isExpanded;
    node->isSelected = false;
    node->isModified = false;
    node->isOpen = false;
    node->isVisible = true;
    node->parent = parent;
    
    return node;
}

TreeNodeType UCIDEProjectTreeView::GetTreeNodeType(ProjectFileType fileType) const {
    switch (fileType) {
        case ProjectFileType::SourceC:
        case ProjectFileType::SourceCpp:
        case ProjectFileType::SourcePascal:
        case ProjectFileType::SourceRust:
        case ProjectFileType::SourcePython:
            return TreeNodeType::SourceFile;
            
        case ProjectFileType::Header:
            return TreeNodeType::HeaderFile;
            
        case ProjectFileType::Resource:
            return TreeNodeType::ResourceFile;
            
        case ProjectFileType::CMakeFile:
        case ProjectFileType::Makefile:
        case ProjectFileType::BuildConfig:
            return TreeNodeType::ConfigFile;
            
        case ProjectFileType::Documentation:
            return TreeNodeType::DocumentFile;
            
        case ProjectFileType::Data:
            return TreeNodeType::DataFile;
            
        default:
            return TreeNodeType::OtherFile;
    }
}

std::string UCIDEProjectTreeView::GenerateNodeId() {
    return "node_" + std::to_string(nextNodeId++);
}

void UCIDEProjectTreeView::SortChildren(TreeNode* node) {
    if (!node || node->children.empty()) return;
    
    // Sort: folders first, then files; alphabetically within each group
    std::sort(node->children.begin(), node->children.end(),
        [](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
            // Folders before files
            if (a->IsFolder() != b->IsFolder()) {
                return a->IsFolder();
            }
            
            // Alphabetical (case-insensitive)
            std::string nameA = a->name;
            std::string nameB = b->name;
            for (char& c : nameA) c = std::tolower(c);
            for (char& c : nameB) c = std::tolower(c);
            
            return nameA < nameB;
        }
    );
}

} // namespace IDE
} // namespace UltraCanvas
