// Apps/IDE/Rendering/UCIDERenderContext.h
// UltraCanvas Rendering Integration for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../UI/UCIDELayout.h"
#include "../UI/UCIDEMenuBar.h"
#include "../UI/UCIDEToolbar.h"
#include "../UI/UCIDEStatusBar.h"
#include "../UI/UCIDEOutputConsole.h"
#include "../Editor/UCIDEEditor.h"
#include "../Project/UCIDEProjectTreeView.h"

// UltraCanvas Core Types
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <cstdint>

namespace UltraCanvas {

// Forward declarations for UltraCanvas types
template<typename T> struct Point2D;
template<typename T> struct Rect2D;
using Point2Df = Point2D<float>;
using Point2Di = Point2D<int>;
using Rect2Df = Rect2D<float>;
using Rect2Di = Rect2D<int>;

struct Color;
class IRenderContext;
class IPixelBuffer;
class UltraCanvasWindow;

namespace IDE {

// ============================================================================
// IDE ICON SET
// ============================================================================

/**
 * @brief Icon identifiers for IDE
 */
enum class IDEIcon {
    // File operations
    FileNew,
    FileOpen,
    FileSave,
    FileSaveAll,
    FileClose,
    FolderOpen,
    FolderClosed,
    
    // Edit operations
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    Delete,
    
    // Build operations
    Build,
    Rebuild,
    Clean,
    Run,
    Debug,
    Stop,
    
    // Navigation
    Search,
    SearchInFiles,
    GoToLine,
    GoToDefinition,
    
    // Markers
    Breakpoint,
    BreakpointDisabled,
    Bookmark,
    Error,
    Warning,
    Info,
    
    // File types
    FileSource,
    FileHeader,
    FileConfig,
    FileText,
    FileImage,
    FileUnknown,
    
    // Tree view
    ChevronRight,
    ChevronDown,
    
    // UI
    Close,
    Minimize,
    Maximize,
    Menu,
    Settings,
    
    // Misc
    Check,
    Cross,
    Plus,
    Minus,
    
    Count
};

// ============================================================================
// IDE THEME COLORS
// ============================================================================

/**
 * @brief Complete IDE theme colors
 */
struct IDEThemeColors {
    // Editor colors
    uint32_t editorBackground;
    uint32_t editorForeground;
    uint32_t editorLineHighlight;
    uint32_t editorSelection;
    uint32_t editorCursor;
    uint32_t editorLineNumber;
    uint32_t editorLineNumberActive;
    uint32_t editorGutter;
    
    // Syntax colors
    uint32_t syntaxKeyword;
    uint32_t syntaxType;
    uint32_t syntaxString;
    uint32_t syntaxNumber;
    uint32_t syntaxComment;
    uint32_t syntaxPreprocessor;
    uint32_t syntaxFunction;
    uint32_t syntaxVariable;
    uint32_t syntaxOperator;
    
    // UI colors
    uint32_t windowBackground;
    uint32_t panelBackground;
    uint32_t sidebarBackground;
    uint32_t statusBarBackground;
    uint32_t tabBarBackground;
    uint32_t tabActive;
    uint32_t tabInactive;
    uint32_t tabHover;
    
    // Control colors
    uint32_t buttonBackground;
    uint32_t buttonHover;
    uint32_t buttonPressed;
    uint32_t buttonDisabled;
    uint32_t inputBackground;
    uint32_t inputBorder;
    uint32_t inputFocus;
    
    // Text colors
    uint32_t textPrimary;
    uint32_t textSecondary;
    uint32_t textDisabled;
    uint32_t textAccent;
    
    // Semantic colors
    uint32_t errorColor;
    uint32_t warningColor;
    uint32_t successColor;
    uint32_t infoColor;
    
    // Dividers and borders
    uint32_t dividerColor;
    uint32_t borderColor;
    uint32_t focusBorderColor;
    
    // Scrollbar
    uint32_t scrollbarTrack;
    uint32_t scrollbarThumb;
    uint32_t scrollbarThumbHover;
    
    /**
     * @brief Get dark theme colors
     */
    static IDEThemeColors GetDarkTheme();
    
    /**
     * @brief Get light theme colors
     */
    static IDEThemeColors GetLightTheme();
    
    /**
     * @brief Get high contrast theme colors
     */
    static IDEThemeColors GetHighContrastTheme();
};

// ============================================================================
// IDE RENDERER CLASS
// ============================================================================

/**
 * @brief Main IDE renderer - integrates all IDE components with UltraCanvas
 */
class UCIDERenderer {
public:
    UCIDERenderer();
    ~UCIDERenderer();
    
    // ===== INITIALIZATION =====
    
    /**
     * @brief Initialize renderer with render context
     */
    void Initialize(IRenderContext* context, int width, int height);
    
    /**
     * @brief Set render context
     */
    void SetRenderContext(IRenderContext* context) { renderContext = context; }
    
    /**
     * @brief Resize renderer
     */
    void Resize(int width, int height);
    
    // ===== COMPONENT BINDING =====
    
    /**
     * @brief Bind IDE layout
     */
    void BindLayout(UCIDELayout* layout) { ideLayout = layout; }
    
    /**
     * @brief Bind menu bar
     */
    void BindMenuBar(UCIDEMenuBar* menuBar) { ideMenuBar = menuBar; }
    
    /**
     * @brief Bind toolbar
     */
    void BindToolbar(UCIDEToolbar* toolbar) { ideToolbar = toolbar; }
    
    /**
     * @brief Bind status bar
     */
    void BindStatusBar(UCIDEStatusBar* statusBar) { ideStatusBar = statusBar; }
    
    /**
     * @brief Bind output console
     */
    void BindOutputConsole(UCIDEOutputConsole* console) { ideOutputConsole = console; }
    
    /**
     * @brief Bind project tree
     */
    void BindProjectTree(UCIDEProjectTreeView* tree) { ideProjectTree = tree; }
    
    /**
     * @brief Bind editor
     */
    void BindEditor(UCIDEEditor* editor) { ideEditor = editor; }
    
    // ===== RENDERING =====
    
    /**
     * @brief Render entire IDE
     */
    void Render();
    
    /**
     * @brief Render specific region (for partial updates)
     */
    void RenderRegion(const Rect2Di& region);
    
    /**
     * @brief Force full redraw
     */
    void Invalidate();
    
    /**
     * @brief Invalidate specific region
     */
    void InvalidateRegion(const Rect2Di& region);
    
    // ===== INDIVIDUAL RENDERERS =====
    
    /**
     * @brief Render menu bar
     */
    void RenderMenuBar();
    
    /**
     * @brief Render toolbar
     */
    void RenderToolbar();
    
    /**
     * @brief Render project tree
     */
    void RenderProjectTree();
    
    /**
     * @brief Render editor tabs
     */
    void RenderEditorTabs();
    
    /**
     * @brief Render code editor
     */
    void RenderEditor();
    
    /**
     * @brief Render output console
     */
    void RenderOutputConsole();
    
    /**
     * @brief Render status bar
     */
    void RenderStatusBar();
    
    /**
     * @brief Render split dividers
     */
    void RenderDividers();
    
    // ===== THEME =====
    
    /**
     * @brief Set theme colors
     */
    void SetTheme(const IDEThemeColors& theme);
    
    /**
     * @brief Get current theme
     */
    const IDEThemeColors& GetTheme() const { return themeColors; }
    
    /**
     * @brief Apply dark theme
     */
    void ApplyDarkTheme();
    
    /**
     * @brief Apply light theme
     */
    void ApplyLightTheme();
    
    // ===== ICONS =====
    
    /**
     * @brief Render icon at position
     */
    void RenderIcon(IDEIcon icon, int x, int y, int size = 16, uint32_t color = 0xFFFFFFFF);
    
    /**
     * @brief Get icon for file type
     */
    IDEIcon GetFileTypeIcon(const std::string& extension);
    
    // ===== CURSOR BLINK =====
    
    /**
     * @brief Update cursor blink state
     */
    void UpdateCursorBlink(int deltaMs);
    
    /**
     * @brief Check if cursor is visible
     */
    bool IsCursorVisible() const { return cursorVisible; }

private:
    // ===== HELPER METHODS =====
    
    void RenderEditorGutter(const LayoutRegion& region);
    void RenderEditorContent(const LayoutRegion& region);
    void RenderEditorSelection();
    void RenderEditorCursor();
    void RenderSyntaxHighlightedLine(int lineIndex, int y);
    
    void RenderTreeNode(const TreeNode* node, int& y, int clipBottom);
    void RenderScrollbar(const LayoutRegion& region, int contentHeight, int scrollOffset);
    
    void RenderDropdownMenu(const Menu& menu, int menuIndex);
    void RenderMenuItem(const MenuItem& item, int x, int y, int width, bool isHovered);
    
    void RenderToolbarButton(const ToolbarItem& item, bool isHovered, bool isPressed);
    void RenderToolbarComboBox(const ToolbarItem& item);
    
    void RenderOutputTab(const OutputTab& tab, int x, int y, int width, bool isActive);
    void RenderOutputLine(const OutputLine& line, int x, int y, int width);
    
    void SetFillColor(uint32_t color);
    void SetStrokeColor(uint32_t color);
    void SetTextColor(uint32_t color);
    
    void DrawText(const std::string& text, int x, int y);
    void DrawTextInRect(const std::string& text, const Rect2Di& rect, int alignment = 0);
    
    // ===== STATE =====
    
    IRenderContext* renderContext = nullptr;
    int windowWidth = 1280;
    int windowHeight = 720;
    
    // Bound components
    UCIDELayout* ideLayout = nullptr;
    UCIDEMenuBar* ideMenuBar = nullptr;
    UCIDEToolbar* ideToolbar = nullptr;
    UCIDEStatusBar* ideStatusBar = nullptr;
    UCIDEOutputConsole* ideOutputConsole = nullptr;
    UCIDEProjectTreeView* ideProjectTree = nullptr;
    UCIDEEditor* ideEditor = nullptr;
    
    // Theme
    IDEThemeColors themeColors;
    
    // Cursor blink
    bool cursorVisible = true;
    int cursorBlinkTimer = 0;
    int cursorBlinkRate = 500;
    
    // Dirty tracking
    bool needsFullRedraw = true;
    std::vector<Rect2Di> dirtyRegions;
};

// ============================================================================
// IDE WINDOW CLASS
// ============================================================================

/**
 * @brief Main IDE window - UltraCanvas window subclass
 */
class UCIDEWindow {
public:
    UCIDEWindow();
    ~UCIDEWindow();
    
    // ===== LIFECYCLE =====
    
    /**
     * @brief Create and show window
     */
    bool Create(int width = 1280, int height = 720, const std::string& title = "ULTRA IDE");
    
    /**
     * @brief Close window
     */
    void Close();
    
    /**
     * @brief Check if window is open
     */
    bool IsOpen() const { return isOpen; }
    
    // ===== MAIN LOOP =====
    
    /**
     * @brief Process events
     */
    void ProcessEvents();
    
    /**
     * @brief Update (call every frame)
     */
    void Update(float deltaTime);
    
    /**
     * @brief Render (call every frame)
     */
    void Render();
    
    // ===== PROJECT =====
    
    /**
     * @brief Open project
     */
    bool OpenProject(const std::string& path);
    
    /**
     * @brief Close project
     */
    void CloseProject();
    
    /**
     * @brief Get current project
     */
    UCIDEProject* GetProject() { return currentProject.get(); }
    
    // ===== FILE OPERATIONS =====
    
    /**
     * @brief Open file in editor
     */
    bool OpenFile(const std::string& path);
    
    /**
     * @brief Save current file
     */
    bool SaveCurrentFile();
    
    /**
     * @brief Save all files
     */
    void SaveAllFiles();
    
    /**
     * @brief Close current file
     */
    void CloseCurrentFile();
    
    // ===== BUILD =====
    
    /**
     * @brief Build project
     */
    void BuildProject();
    
    /**
     * @brief Run project
     */
    void RunProject();
    
    /**
     * @brief Stop running
     */
    void StopRunning();
    
    // ===== COMPONENT ACCESS =====
    
    UCIDELayout* GetLayout() { return layout.get(); }
    UCIDEMenuBar* GetMenuBar() { return menuBar.get(); }
    UCIDEToolbar* GetToolbar() { return toolbar.get(); }
    UCIDEStatusBar* GetStatusBar() { return statusBar.get(); }
    UCIDEOutputConsole* GetOutputConsole() { return outputConsole.get(); }
    UCIDEProjectTreeView* GetProjectTree() { return projectTree.get(); }
    UCIDEEditor* GetEditor() { return activeEditor; }
    UCIDERenderer* GetRenderer() { return renderer.get(); }
    
    // ===== CALLBACKS =====
    
    std::function<void()> onClose;
    std::function<void(const std::string&)> onFileOpen;
    std::function<void()> onBuildStart;
    std::function<void(bool success)> onBuildComplete;

private:
    // ===== EVENT HANDLERS =====
    
    void OnMenuCommand(MenuCommand cmd);
    void OnToolbarCommand(MenuCommand cmd);
    void OnFileOpen(const std::string& path);
    void OnTabClose(const std::string& path);
    void OnProjectTreeFileOpen(const std::string& path);
    void OnErrorClick(const std::string& file, int line, int column);
    
    void HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt);
    void HandleMouseClick(int x, int y, int button, bool isDoubleClick);
    void HandleMouseMove(int x, int y);
    void HandleMouseScroll(int delta);
    void HandleResize(int width, int height);
    
    // ===== INTERNAL =====
    
    void InitializeComponents();
    void SetupCallbacks();
    void UpdateWindowTitle();
    
    // ===== STATE =====
    
    bool isOpen = false;
    int width = 1280;
    int height = 720;
    std::string windowTitle;
    
    // Components
    std::unique_ptr<UCIDELayout> layout;
    std::unique_ptr<UCIDEMenuBar> menuBar;
    std::unique_ptr<UCIDEToolbar> toolbar;
    std::unique_ptr<UCIDEStatusBar> statusBar;
    std::unique_ptr<UCIDEOutputConsole> outputConsole;
    std::unique_ptr<UCIDEProjectTreeView> projectTree;
    std::unique_ptr<UCIDERenderer> renderer;
    
    // Project
    std::unique_ptr<UCIDEProject> currentProject;
    
    // Editors (multiple files)
    std::map<std::string, std::unique_ptr<UCIDEEditor>> editors;
    UCIDEEditor* activeEditor = nullptr;
    
    // Build manager
    std::unique_ptr<UCBuildManager> buildManager;
    
    // Platform window handle (would be platform-specific)
    void* nativeWindow = nullptr;
    IRenderContext* renderContext = nullptr;
};

} // namespace IDE
} // namespace UltraCanvas
