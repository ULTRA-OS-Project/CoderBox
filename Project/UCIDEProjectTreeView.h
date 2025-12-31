// Apps/IDE/Project/UCIDEProjectTreeView.h
// Project File Tree View UI Component for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCIDEProject.h"
#include "../Build/IUCCompilerPlugin.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// TREE NODE TYPES
// ============================================================================

/**
 * @brief Type of node in the project tree
 */
enum class TreeNodeType {
    Root,               // Project root node
    Folder,             // Directory/folder
    SourceFile,         // Compilable source file
    HeaderFile,         // Header file
    ResourceFile,       // Resource file
    ConfigFile,         // Configuration file (CMake, etc.)
    DocumentFile,       // Documentation file
    DataFile,           // Data file (JSON, XML, etc.)
    OtherFile           // Other file types
};

/**
 * @brief Convert TreeNodeType to icon name
 */
inline std::string GetTreeNodeIcon(TreeNodeType type) {
    switch (type) {
        case TreeNodeType::Root:         return "project";
        case TreeNodeType::Folder:       return "folder";
        case TreeNodeType::SourceFile:   return "file-code";
        case TreeNodeType::HeaderFile:   return "file-header";
        case TreeNodeType::ResourceFile: return "file-resource";
        case TreeNodeType::ConfigFile:   return "file-config";
        case TreeNodeType::DocumentFile: return "file-document";
        case TreeNodeType::DataFile:     return "file-data";
        case TreeNodeType::OtherFile:    return "file";
        default:                         return "file";
    }
}

// ============================================================================
// TREE NODE STRUCTURE
// ============================================================================

/**
 * @brief Represents a single node in the project tree
 */
struct TreeNode {
    std::string id;                     // Unique node ID
    std::string name;                   // Display name
    std::string path;                   // Relative path
    std::string absolutePath;           // Full path
    TreeNodeType type = TreeNodeType::OtherFile;
    
    // State
    bool isExpanded = false;            // Folder expanded state
    bool isSelected = false;            // Currently selected
    bool isModified = false;            // File has unsaved changes
    bool isOpen = false;                // File is open in editor
    bool isVisible = true;              // Visible (for filtering)
    
    // Hierarchy
    TreeNode* parent = nullptr;
    std::vector<std::unique_ptr<TreeNode>> children;
    
    // File info
    ProjectFileType fileType = ProjectFileType::Unknown;
    size_t fileSize = 0;
    std::string lastModified;
    
    /**
     * @brief Check if this is a folder node
     */
    bool IsFolder() const {
        return type == TreeNodeType::Root || type == TreeNodeType::Folder;
    }
    
    /**
     * @brief Check if this is a file node
     */
    bool IsFile() const {
        return !IsFolder();
    }
    
    /**
     * @brief Get child count
     */
    size_t GetChildCount() const {
        return children.size();
    }
    
    /**
     * @brief Get visible child count
     */
    size_t GetVisibleChildCount() const {
        size_t count = 0;
        for (const auto& child : children) {
            if (child->isVisible) count++;
        }
        return count;
    }
    
    /**
     * @brief Find child by name
     */
    TreeNode* FindChild(const std::string& childName) {
        for (auto& child : children) {
            if (child->name == childName) {
                return child.get();
            }
        }
        return nullptr;
    }
    
    /**
     * @brief Get depth in tree
     */
    int GetDepth() const {
        int depth = 0;
        const TreeNode* node = parent;
        while (node) {
            depth++;
            node = node->parent;
        }
        return depth;
    }
};

// ============================================================================
// TREE VIEW STYLE
// ============================================================================

/**
 * @brief Visual style configuration for the tree view
 */
struct TreeViewStyle {
    // Colors (RGBA 0-255)
    uint32_t backgroundColor = 0x1E1E1EFF;      // Dark background
    uint32_t textColor = 0xCCCCCCFF;            // Light text
    uint32_t selectedBgColor = 0x094771FF;      // Selection background
    uint32_t selectedTextColor = 0xFFFFFFFF;    // Selection text
    uint32_t hoverBgColor = 0x2A2D2EFF;         // Hover background
    uint32_t modifiedColor = 0xE8AB53FF;        // Modified indicator (orange)
    uint32_t folderColor = 0xDCB67AFF;          // Folder icon color
    uint32_t sourceFileColor = 0x519ABA;        // Source file color (blue)
    uint32_t headerFileColor = 0x9876AA;        // Header file color (purple)
    
    // Dimensions
    int rowHeight = 22;                          // Height of each row
    int indentWidth = 16;                        // Indent per level
    int iconSize = 16;                           // Icon size
    int iconMargin = 4;                          // Margin around icons
    int textMargin = 4;                          // Margin around text
    
    // Font
    std::string fontFamily = "Consolas";
    int fontSize = 12;
    
    // Behavior
    bool showIcons = true;
    bool showFileExtensions = true;
    bool showModifiedIndicator = true;
    bool animateExpansion = true;
    float animationSpeed = 0.2f;                 // Seconds
};

// ============================================================================
// CONTEXT MENU ACTIONS
// ============================================================================

/**
 * @brief Context menu action types
 */
enum class TreeContextAction {
    // File actions
    Open,
    OpenWith,
    Rename,
    Delete,
    CopyPath,
    CopyRelativePath,
    RevealInExplorer,
    
    // Folder actions
    NewFile,
    NewFolder,
    AddExistingFile,
    Collapse,
    Expand,
    ExpandAll,
    CollapseAll,
    
    // Build actions
    Compile,
    ExcludeFromBuild,
    IncludeInBuild,
    
    // Project actions
    Refresh,
    ProjectSettings,
    CloseProject
};

// ============================================================================
// PROJECT TREE VIEW CLASS
// ============================================================================

/**
 * @brief Project file tree view UI component
 * 
 * Displays the project file structure in a hierarchical tree.
 * Integrates with UCIDEProject for file management.
 */
class UCIDEProjectTreeView {
public:
    UCIDEProjectTreeView();
    ~UCIDEProjectTreeView();
    
    // ===== PROJECT BINDING =====
    
    /**
     * @brief Set the project to display
     */
    void SetProject(std::shared_ptr<UCIDEProject> project);
    
    /**
     * @brief Get current project
     */
    std::shared_ptr<UCIDEProject> GetProject() const { return currentProject; }
    
    /**
     * @brief Refresh tree from project
     */
    void Refresh();
    
    /**
     * @brief Clear the tree
     */
    void Clear();
    
    // ===== TREE OPERATIONS =====
    
    /**
     * @brief Get root node
     */
    TreeNode* GetRoot() { return rootNode.get(); }
    const TreeNode* GetRoot() const { return rootNode.get(); }
    
    /**
     * @brief Find node by path
     */
    TreeNode* FindNode(const std::string& path);
    const TreeNode* FindNode(const std::string& path) const;
    
    /**
     * @brief Find node by ID
     */
    TreeNode* FindNodeById(const std::string& id);
    
    /**
     * @brief Get selected node
     */
    TreeNode* GetSelectedNode() { return selectedNode; }
    const TreeNode* GetSelectedNode() const { return selectedNode; }
    
    /**
     * @brief Select node by path
     */
    void SelectNode(const std::string& path);
    
    /**
     * @brief Select node directly
     */
    void SelectNode(TreeNode* node);
    
    /**
     * @brief Expand node
     */
    void ExpandNode(TreeNode* node);
    
    /**
     * @brief Collapse node
     */
    void CollapseNode(TreeNode* node);
    
    /**
     * @brief Toggle node expansion
     */
    void ToggleNode(TreeNode* node);
    
    /**
     * @brief Expand all nodes
     */
    void ExpandAll();
    
    /**
     * @brief Collapse all nodes
     */
    void CollapseAll();
    
    /**
     * @brief Expand to show a specific path
     */
    void RevealPath(const std::string& path);
    
    // ===== FILTERING =====
    
    /**
     * @brief Set filter text (shows matching files)
     */
    void SetFilter(const std::string& filter);
    
    /**
     * @brief Clear filter
     */
    void ClearFilter();
    
    /**
     * @brief Get current filter
     */
    const std::string& GetFilter() const { return filterText; }
    
    /**
     * @brief Set file type filter
     */
    void SetFileTypeFilter(const std::vector<ProjectFileType>& types);
    
    /**
     * @brief Clear file type filter
     */
    void ClearFileTypeFilter();
    
    // ===== FILE MODIFICATION TRACKING =====
    
    /**
     * @brief Mark file as modified
     */
    void SetFileModified(const std::string& path, bool modified);
    
    /**
     * @brief Mark file as open in editor
     */
    void SetFileOpen(const std::string& path, bool open);
    
    /**
     * @brief Get list of modified files
     */
    std::vector<std::string> GetModifiedFiles() const;
    
    // ===== RENDERING =====
    
    /**
     * @brief Get visible nodes for rendering (flattened)
     */
    std::vector<TreeNode*> GetVisibleNodes() const;
    
    /**
     * @brief Get total visible row count
     */
    int GetVisibleRowCount() const;
    
    /**
     * @brief Get node at row index
     */
    TreeNode* GetNodeAtRow(int row);
    
    /**
     * @brief Get row index of node
     */
    int GetRowOfNode(const TreeNode* node) const;
    
    // ===== STYLE =====
    
    /**
     * @brief Get/set style
     */
    TreeViewStyle& GetStyle() { return style; }
    const TreeViewStyle& GetStyle() const { return style; }
    void SetStyle(const TreeViewStyle& newStyle) { style = newStyle; }
    
    // ===== CALLBACKS =====
    
    /**
     * @brief Called when a file is double-clicked (open)
     */
    std::function<void(const std::string& path)> onFileOpen;
    
    /**
     * @brief Called when selection changes
     */
    std::function<void(TreeNode* node)> onSelectionChange;
    
    /**
     * @brief Called when a node is expanded
     */
    std::function<void(TreeNode* node)> onNodeExpand;
    
    /**
     * @brief Called when a node is collapsed
     */
    std::function<void(TreeNode* node)> onNodeCollapse;
    
    /**
     * @brief Called for context menu
     */
    std::function<void(TreeNode* node, int x, int y)> onContextMenu;
    
    /**
     * @brief Called when context action is selected
     */
    std::function<void(TreeNode* node, TreeContextAction action)> onContextAction;
    
    /**
     * @brief Called when file is renamed
     */
    std::function<void(const std::string& oldPath, const std::string& newPath)> onFileRename;
    
    /**
     * @brief Called when file is deleted
     */
    std::function<void(const std::string& path)> onFileDelete;
    
    /**
     * @brief Called when new file is requested
     */
    std::function<void(const std::string& parentPath)> onNewFile;
    
    /**
     * @brief Called when new folder is requested
     */
    std::function<void(const std::string& parentPath)> onNewFolder;
    
    // ===== INPUT HANDLING =====
    
    /**
     * @brief Handle mouse click
     * @return true if handled
     */
    bool HandleClick(int x, int y, bool isDoubleClick, bool isRightClick);
    
    /**
     * @brief Handle key press
     * @return true if handled
     */
    bool HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt);
    
    /**
     * @brief Handle scroll
     */
    void HandleScroll(int delta);
    
    // ===== SCROLL STATE =====
    
    /**
     * @brief Get/set scroll offset
     */
    int GetScrollOffset() const { return scrollOffset; }
    void SetScrollOffset(int offset);
    
    /**
     * @brief Scroll to make node visible
     */
    void ScrollToNode(TreeNode* node);
    
    /**
     * @brief Get view height (for scrollbar)
     */
    void SetViewHeight(int height) { viewHeight = height; }
    int GetViewHeight() const { return viewHeight; }
    
    /**
     * @brief Get content height
     */
    int GetContentHeight() const;

private:
    // ===== INTERNAL METHODS =====
    
    /**
     * @brief Build tree from project folder
     */
    void BuildTree(const ProjectFolder& folder, TreeNode* parent);
    
    /**
     * @brief Create node from project file
     */
    std::unique_ptr<TreeNode> CreateFileNode(const ProjectFile& file, TreeNode* parent);
    
    /**
     * @brief Create node from project folder
     */
    std::unique_ptr<TreeNode> CreateFolderNode(const ProjectFolder& folder, TreeNode* parent);
    
    /**
     * @brief Determine tree node type from file type
     */
    TreeNodeType GetTreeNodeType(ProjectFileType fileType) const;
    
    /**
     * @brief Generate unique node ID
     */
    std::string GenerateNodeId();
    
    /**
     * @brief Apply filter to nodes
     */
    void ApplyFilter(TreeNode* node);
    
    /**
     * @brief Check if node matches filter
     */
    bool MatchesFilter(const TreeNode* node) const;
    
    /**
     * @brief Collect visible nodes recursively
     */
    void CollectVisibleNodes(TreeNode* node, std::vector<TreeNode*>& nodes) const;
    
    /**
     * @brief Sort children of node
     */
    void SortChildren(TreeNode* node);
    
    // ===== STATE =====
    
    std::shared_ptr<UCIDEProject> currentProject;
    std::unique_ptr<TreeNode> rootNode;
    TreeNode* selectedNode = nullptr;
    TreeNode* hoveredNode = nullptr;
    
    TreeViewStyle style;
    
    std::string filterText;
    std::vector<ProjectFileType> fileTypeFilter;
    
    int scrollOffset = 0;
    int viewHeight = 400;
    
    int nextNodeId = 1;
    
    // Node lookup cache
    std::map<std::string, TreeNode*> pathToNodeMap;
    std::map<std::string, TreeNode*> idToNodeMap;
};

// ============================================================================
// DRAG AND DROP SUPPORT (Future)
// ============================================================================

/**
 * @brief Drag data for tree nodes
 */
struct TreeDragData {
    std::vector<std::string> paths;     // Paths being dragged
    bool isCopy = false;                // Copy or move
    TreeNode* sourceNode = nullptr;     // Source node
};

/**
 * @brief Drop target info
 */
struct TreeDropTarget {
    TreeNode* targetNode = nullptr;
    enum class Position {
        Before,
        After,
        Inside
    } position = Position::Inside;
    bool isValid = false;
};

} // namespace IDE
} // namespace UltraCanvas
