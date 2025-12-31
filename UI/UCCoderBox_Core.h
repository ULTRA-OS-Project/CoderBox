// Apps/CoderBox/UI/UCCoderBoxApplication.h
// CoderBox - Main Application using UltraCanvas Components
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#pragma once

#include "UltraCanvasApplication.h"
#include "UltraCanvasWindow.h"
#include "UltraCanvasMenu.h"
#include "UltraCanvasToolbar.h"
#include "UltraCanvasSplitPane.h"
#include "UltraCanvasTabbedContainer.h"
#include "UltraCanvasTreeView.h"
#include "UltraCanvasTextArea.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasButton.h"
#include "UltraCanvasDropdown.h"
#include "UltraCanvasCommonTypes.h"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace UltraCanvas {
namespace CoderBox {

// Forward declarations
class UCCoderBoxProject;
class UCCoderBoxProjectManager;
class UCBuildManager;
class IUCCompilerPlugin;

// ============================================================================
// CODERBOX MENU COMMANDS
// ============================================================================

enum class CoderBoxCommand {
    None = 0,
    
    // File Menu
    FileNewProject,
    FileNewFile,
    FileOpen,
    FileOpenProject,
    FileSave,
    FileSaveAs,
    FileSaveAll,
    FileCloseFile,
    FileCloseProject,
    FileExit,
    
    // Edit Menu
    EditUndo,
    EditRedo,
    EditCut,
    EditCopy,
    EditPaste,
    EditDelete,
    EditSelectAll,
    EditFind,
    EditFindNext,
    EditFindPrevious,
    EditReplace,
    EditFindInFiles,
    EditGoToLine,
    EditToggleComment,
    
    // View Menu
    ViewProjectTree,
    ViewOutputConsole,
    ViewToolbar,
    ViewStatusBar,
    ViewFullScreen,
    ViewLineNumbers,
    ViewWordWrap,
    
    // Build Menu
    BuildBuild,
    BuildRebuild,
    BuildClean,
    BuildBuildFile,
    BuildRun,
    BuildDebug,
    BuildStop,
    BuildNextError,
    BuildPreviousError,
    BuildCMakeConfigure,
    BuildCMakeBuild,
    
    // Project Menu
    ProjectAddFile,
    ProjectAddFolder,
    ProjectRemoveFile,
    ProjectRefresh,
    ProjectProperties,
    
    // Help Menu
    HelpContents,
    HelpKeyboardShortcuts,
    HelpAbout
};

// ============================================================================
// IDE CONFIGURATION
// ============================================================================

struct CoderBoxConfiguration {
    // Window settings
    int windowWidth = 1280;
    int windowHeight = 720;
    std::string windowTitle = "CoderBox";
    
    // Layout settings
    float projectTreeWidthRatio = 0.20f;    // 20% of width
    float outputConsoleHeightRatio = 0.25f; // 25% of height
    
    // Editor settings
    int tabSize = 4;
    bool insertSpaces = true;
    bool showLineNumbers = true;
    bool wordWrap = false;
    std::string defaultEncoding = "UTF-8";
    
    // Theme
    bool darkTheme = true;
    
    // Recent projects
    int maxRecentProjects = 10;
};

// ============================================================================
// IDE THEME
// ============================================================================

struct CoderBoxTheme {
    // Background colors
    Color windowBackground = Color(30, 30, 30);
    Color panelBackground = Color(37, 37, 38);
    Color editorBackground = Color(30, 30, 30);
    Color menuBarBackground = Color(60, 60, 60);
    Color toolbarBackground = Color(60, 60, 60);
    Color statusBarBackground = Color(0, 122, 204);
    Color tabBarBackground = Color(45, 45, 45);
    
    // Text colors
    Color textColor = Color(204, 204, 204);
    Color activeTextColor = Color(255, 255, 255);
    Color disabledTextColor = Color(128, 128, 128);
    
    // Accent colors
    Color accentColor = Color(0, 122, 204);
    Color hoverColor = Color(80, 80, 80);
    Color selectedColor = Color(9, 71, 113);
    
    // Status colors
    Color errorColor = Color(229, 20, 0);
    Color warningColor = Color(240, 173, 78);
    Color successColor = Color(78, 201, 176);
    Color infoColor = Color(106, 153, 85);
    
    // Divider
    Color dividerColor = Color(60, 60, 60);
    Color dividerHoverColor = Color(0, 122, 204);
    
    // Factory methods
    static CoderBoxTheme Dark();
    static CoderBoxTheme Light();
    static CoderBoxTheme HighContrast();
};

// ============================================================================
// OPEN EDITOR TAB INFO
// ============================================================================

struct EditorTabInfo {
    std::string filePath;
    std::string fileName;
    bool isModified = false;
    bool isPinned = false;
    std::shared_ptr<UltraCanvasTextArea> editor;
};

// ============================================================================
// OUTPUT TAB TYPE
// ============================================================================

enum class OutputTabType {
    Build = 0,
    Errors,
    Warnings,
    Messages,
    Application,
    Debug
};

// ============================================================================
// UCIDE APPLICATION CLASS
// ============================================================================

/**
 * @brief Main CoderBox Application
 * 
 * This class composes UltraCanvas components to create the full IDE:
 * - UltraCanvasMenu for menu bar
 * - UltraCanvasToolbar for toolbar and status bar
 * - UltraCanvasSplitPane for IDE layout
 * - UltraCanvasTabbedContainer for editor tabs and output console
 * - UltraCanvasTreeView for project file browser
 * - UltraCanvasTextArea for code editing
 * 
 * Layout Structure:
 * ┌─────────────────────────────────────────────────┐
 * │  Menu Bar (UltraCanvasMenu)                      │
 * ├─────────────────────────────────────────────────┤
 * │  Toolbar (UltraCanvasToolbar)                    │
 * ├───────────┬─────────────────────────────────────┤
 * │  Project  │  Editor Tabs (UltraCanvasTabbedCnt)  │
 * │  Tree     ├─────────────────────────────────────┤
 * │  (Tree    │  Code Editor (UltraCanvasTextArea)   │
 * │   View)   ├─────────────────────────────────────┤
 * │           │  Output Console (TabbedContainer)    │
 * ├───────────┴─────────────────────────────────────┤
 * │  Status Bar (UltraCanvasToolbar - StatusBar)     │
 * └─────────────────────────────────────────────────┘
 */
class UCCoderBoxApplication : public UltraCanvasApplication {
public:
    UCCoderBoxApplication();
    virtual ~UCCoderBoxApplication();
    
    // ===== INITIALIZATION =====
    
    /**
     * @brief Initialize the IDE application
     * @return true if successful
     */
    bool Initialize() override;
    
    /**
     * @brief Initialize with custom configuration
     */
    bool Initialize(const CoderBoxConfiguration& config);
    
    /**
     * @brief Shutdown the IDE
     */
    void Shutdown();
    
    // ===== WINDOW MANAGEMENT =====
    
    /**
     * @brief Get the main IDE window
     */
    std::shared_ptr<UltraCanvasWindow> GetMainWindow() { return mainWindow; }
    
    // ===== PROJECT OPERATIONS =====
    
    /**
     * @brief Create a new project
     */
    void NewProject();
    
    /**
     * @brief Open an existing project
     */
    void OpenProject(const std::string& projectPath = "");
    
    /**
     * @brief Close the current project
     */
    void CloseProject();
    
    /**
     * @brief Get the current project
     */
    std::shared_ptr<UCCoderBoxProject> GetCurrentProject() { return currentProject; }
    
    // ===== FILE OPERATIONS =====
    
    /**
     * @brief Create a new file
     */
    void NewFile();
    
    /**
     * @brief Open a file in the editor
     */
    void OpenFile(const std::string& filePath);
    
    /**
     * @brief Save the current file
     */
    void SaveFile();
    
    /**
     * @brief Save current file with new name
     */
    void SaveFileAs();
    
    /**
     * @brief Save all open files
     */
    void SaveAllFiles();
    
    /**
     * @brief Close the current file
     */
    void CloseFile();
    
    /**
     * @brief Close a specific file
     */
    void CloseFile(const std::string& filePath);
    
    /**
     * @brief Check if file is open
     */
    bool IsFileOpen(const std::string& filePath) const;
    
    /**
     * @brief Get the currently active editor
     */
    std::shared_ptr<UltraCanvasTextArea> GetActiveEditor();
    
    // ===== BUILD OPERATIONS =====
    
    /**
     * @brief Build the current project
     */
    void BuildProject();
    
    /**
     * @brief Rebuild the current project
     */
    void RebuildProject();
    
    /**
     * @brief Clean the current project
     */
    void CleanProject();
    
    /**
     * @brief Build the current file only
     */
    void BuildCurrentFile();
    
    /**
     * @brief Run the built executable
     */
    void RunProject();
    
    /**
     * @brief Debug the project (future)
     */
    void DebugProject();
    
    /**
     * @brief Stop the running build or program
     */
    void StopBuild();
    
    /**
     * @brief Navigate to next error
     */
    void GoToNextError();
    
    /**
     * @brief Navigate to previous error
     */
    void GoToPreviousError();
    
    // ===== OUTPUT CONSOLE =====
    
    /**
     * @brief Write to build output
     */
    void WriteBuildOutput(const std::string& text);
    
    /**
     * @brief Write error message
     */
    void WriteError(const std::string& filePath, int line, int column, const std::string& message);
    
    /**
     * @brief Write warning message
     */
    void WriteWarning(const std::string& filePath, int line, int column, const std::string& message);
    
    /**
     * @brief Clear all output tabs
     */
    void ClearOutput();
    
    /**
     * @brief Set active output tab
     */
    void SetActiveOutputTab(OutputTabType type);
    
    // ===== STATUS BAR =====
    
    /**
     * @brief Set status bar message
     */
    void SetStatus(const std::string& message);
    
    /**
     * @brief Set cursor position in status bar
     */
    void SetCursorPosition(int line, int column);
    
    /**
     * @brief Set file encoding in status bar
     */
    void SetEncoding(const std::string& encoding);
    
    /**
     * @brief Set language mode in status bar
     */
    void SetLanguageMode(const std::string& language);
    
    /**
     * @brief Set error/warning counts
     */
    void SetErrorCounts(int errors, int warnings);
    
    // ===== THEME =====
    
    /**
     * @brief Apply theme to IDE
     */
    void ApplyTheme(const CoderBoxTheme& theme);
    
    /**
     * @brief Get current theme
     */
    const CoderBoxTheme& GetTheme() const { return currentTheme; }
    
    // ===== CONFIGURATION =====
    
    /**
     * @brief Get IDE configuration
     */
    CoderBoxConfiguration& GetConfiguration() { return configuration; }
    
    /**
     * @brief Save configuration
     */
    void SaveConfiguration();
    
    /**
     * @brief Load configuration
     */
    void LoadConfiguration();
    
    // ===== CALLBACKS =====
    
    std::function<void(CoderBoxCommand)> onCommand;
    std::function<void(const std::string&)> onFileOpened;
    std::function<void(const std::string&)> onFileClosed;
    std::function<void(const std::string&)> onFileSaved;
    std::function<void(std::shared_ptr<UCCoderBoxProject>)> onProjectOpened;
    std::function<void()> onProjectClosed;
    std::function<void(bool success, int errors, int warnings)> onBuildCompleted;

protected:
    // ===== COMPONENT CREATION =====
    
    void CreateMainWindow();
    void CreateMenuBar();
    void CreateToolbar();
    void CreateStatusBar();
    void CreateMainLayout();
    void CreateProjectTree();
    void CreateEditorTabs();
    void CreateOutputConsole();
    
    // ===== MENU SETUP =====
    
    void SetupFileMenu();
    void SetupEditMenu();
    void SetupViewMenu();
    void SetupBuildMenu();
    void SetupProjectMenu();
    void SetupHelpMenu();
    
    // ===== EVENT HANDLERS =====
    
    void OnMenuCommand(CoderBoxCommand command);
    void OnToolbarCommand(CoderBoxCommand command);
    void OnProjectTreeSelect(TreeNode* node);
    void OnProjectTreeDoubleClick(TreeNode* node);
    void OnEditorTabChange(int index);
    void OnEditorTabClose(int index);
    void OnOutputTabChange(int index);
    void OnOutputLineClick(const std::string& filePath, int line, int column);
    void OnEditorModified(const std::string& filePath);
    void OnEditorCursorMove(int line, int column);
    
    // ===== LAYOUT MANAGEMENT =====
    
    void UpdateLayout();
    void OnWindowResize(int width, int height);
    void ToggleProjectTree();
    void ToggleOutputConsole();
    
    // ===== INTERNAL HELPERS =====
    
    std::shared_ptr<UltraCanvasTextArea> CreateEditor(const std::string& filePath);
    void UpdateEditorTabTitle(const std::string& filePath);
    void NavigateToLocation(const std::string& filePath, int line, int column);
    std::string GetLanguageFromExtension(const std::string& extension);
    void UpdateMenuState();
    void UpdateToolbarState();
    void RefreshProjectTree();
    
private:
    // ===== CONFIGURATION =====
    CoderBoxConfiguration configuration;
    CoderBoxTheme currentTheme;
    
    // ===== MAIN WINDOW =====
    std::shared_ptr<UltraCanvasWindow> mainWindow;
    
    // ===== MENU BAR =====
    std::shared_ptr<UltraCanvasMenu> menuBar;
    std::shared_ptr<UltraCanvasMenu> fileMenu;
    std::shared_ptr<UltraCanvasMenu> editMenu;
    std::shared_ptr<UltraCanvasMenu> viewMenu;
    std::shared_ptr<UltraCanvasMenu> buildMenu;
    std::shared_ptr<UltraCanvasMenu> projectMenu;
    std::shared_ptr<UltraCanvasMenu> helpMenu;
    
    // ===== TOOLBAR =====
    std::shared_ptr<UltraCanvasToolbar> toolbar;
    std::shared_ptr<UltraCanvasDropdown> configDropdown;
    std::shared_ptr<UltraCanvasDropdown> compilerDropdown;
    
    // ===== STATUS BAR =====
    std::shared_ptr<UltraCanvasToolbar> statusBar;
    std::shared_ptr<UltraCanvasLabel> statusLabel;
    std::shared_ptr<UltraCanvasLabel> positionLabel;
    std::shared_ptr<UltraCanvasLabel> encodingLabel;
    std::shared_ptr<UltraCanvasLabel> languageLabel;
    std::shared_ptr<UltraCanvasLabel> errorCountLabel;
    
    // ===== SPLIT PANES =====
    std::shared_ptr<UltraCanvasSplitPane> mainHorizontalSplit;  // ProjectTree | EditorArea
    std::shared_ptr<UltraCanvasSplitPane> editorVerticalSplit;  // Editor | OutputConsole
    
    // ===== PROJECT TREE =====
    std::shared_ptr<UltraCanvasTreeView> projectTree;
    
    // ===== EDITOR AREA =====
    std::shared_ptr<UltraCanvasTabbedContainer> editorTabs;
    std::map<std::string, EditorTabInfo> openEditors;  // filePath -> EditorTabInfo
    std::string activeFilePath;
    
    // ===== OUTPUT CONSOLE =====
    std::shared_ptr<UltraCanvasTabbedContainer> outputConsole;
    std::shared_ptr<UltraCanvasTextArea> buildOutput;
    std::shared_ptr<UltraCanvasTextArea> errorsOutput;
    std::shared_ptr<UltraCanvasTextArea> warningsOutput;
    std::shared_ptr<UltraCanvasTextArea> messagesOutput;
    std::shared_ptr<UltraCanvasTextArea> applicationOutput;
    
    // ===== PROJECT STATE =====
    std::shared_ptr<UCCoderBoxProject> currentProject;
    
    // ===== BUILD STATE =====
    bool isBuildInProgress = false;
    int currentErrorCount = 0;
    int currentWarningCount = 0;
    int currentErrorIndex = -1;
    std::vector<std::tuple<std::string, int, int, std::string>> errorList;  // file, line, col, message
    
    // ===== LAYOUT STATE =====
    bool projectTreeVisible = true;
    bool outputConsoleVisible = true;
    
    // ===== UNIQUE IDS =====
    long nextElementId = 1000;
    long GetNextId() { return nextElementId++; }
};

// ============================================================================
// FACTORY FUNCTION
// ============================================================================

/**
 * @brief Create and initialize the CoderBox application
 */
std::shared_ptr<UCCoderBoxApplication> CreateCoderBox();

/**
 * @brief Create CoderBox with custom configuration
 */
std::shared_ptr<UCCoderBoxApplication> CreateCoderBox(const CoderBoxConfiguration& config);

} // namespace CoderBox
} // namespace UltraCanvas
