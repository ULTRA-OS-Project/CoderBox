// Apps/CoderBox/UI/UCCoderBoxApplication.cpp
// Main CoderBox Application Implementation
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#include "UCCoderBoxApplication.h"
#include <iostream>
#include <algorithm>
#include <filesystem>

namespace UltraCanvas {
namespace CoderBox {

// ============================================================================
// IDE THEME FACTORY METHODS
// ============================================================================

CoderBoxTheme CoderBoxTheme::Dark() {
    CoderBoxTheme theme;
    
    // Dark theme colors (VS Code inspired)
    theme.windowBackground = Color(30, 30, 30);
    theme.panelBackground = Color(37, 37, 38);
    theme.editorBackground = Color(30, 30, 30);
    theme.menuBarBackground = Color(60, 60, 60);
    theme.toolbarBackground = Color(60, 60, 60);
    theme.statusBarBackground = Color(0, 122, 204);
    theme.tabBarBackground = Color(45, 45, 45);
    
    theme.textColor = Color(204, 204, 204);
    theme.activeTextColor = Color(255, 255, 255);
    theme.disabledTextColor = Color(128, 128, 128);
    
    theme.accentColor = Color(0, 122, 204);
    theme.hoverColor = Color(80, 80, 80);
    theme.selectedColor = Color(9, 71, 113);
    
    theme.errorColor = Color(229, 20, 0);
    theme.warningColor = Color(240, 173, 78);
    theme.successColor = Color(78, 201, 176);
    theme.infoColor = Color(106, 153, 85);
    
    theme.dividerColor = Color(60, 60, 60);
    theme.dividerHoverColor = Color(0, 122, 204);
    
    return theme;
}

CoderBoxTheme CoderBoxTheme::Light() {
    CoderBoxTheme theme;
    
    // Light theme colors
    theme.windowBackground = Color(255, 255, 255);
    theme.panelBackground = Color(243, 243, 243);
    theme.editorBackground = Color(255, 255, 255);
    theme.menuBarBackground = Color(236, 236, 236);
    theme.toolbarBackground = Color(236, 236, 236);
    theme.statusBarBackground = Color(0, 122, 204);
    theme.tabBarBackground = Color(236, 236, 236);
    
    theme.textColor = Color(51, 51, 51);
    theme.activeTextColor = Color(0, 0, 0);
    theme.disabledTextColor = Color(160, 160, 160);
    
    theme.accentColor = Color(0, 122, 204);
    theme.hoverColor = Color(224, 224, 224);
    theme.selectedColor = Color(204, 228, 247);
    
    theme.errorColor = Color(229, 20, 0);
    theme.warningColor = Color(215, 151, 6);
    theme.successColor = Color(22, 130, 93);
    theme.infoColor = Color(0, 122, 204);
    
    theme.dividerColor = Color(224, 224, 224);
    theme.dividerHoverColor = Color(0, 122, 204);
    
    return theme;
}

CoderBoxTheme CoderBoxTheme::HighContrast() {
    CoderBoxTheme theme;
    
    // High contrast theme
    theme.windowBackground = Color(0, 0, 0);
    theme.panelBackground = Color(0, 0, 0);
    theme.editorBackground = Color(0, 0, 0);
    theme.menuBarBackground = Color(0, 0, 0);
    theme.toolbarBackground = Color(0, 0, 0);
    theme.statusBarBackground = Color(0, 0, 0);
    theme.tabBarBackground = Color(0, 0, 0);
    
    theme.textColor = Color(255, 255, 255);
    theme.activeTextColor = Color(255, 255, 0);
    theme.disabledTextColor = Color(128, 128, 128);
    
    theme.accentColor = Color(0, 255, 0);
    theme.hoverColor = Color(51, 51, 51);
    theme.selectedColor = Color(0, 128, 0);
    
    theme.errorColor = Color(255, 0, 0);
    theme.warningColor = Color(255, 255, 0);
    theme.successColor = Color(0, 255, 0);
    theme.infoColor = Color(0, 255, 255);
    
    theme.dividerColor = Color(255, 255, 255);
    theme.dividerHoverColor = Color(0, 255, 0);
    
    return theme;
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

UCCoderBoxApplication::UCCoderBoxApplication() {
    currentTheme = CoderBoxTheme::Dark();
}

UCCoderBoxApplication::~UCCoderBoxApplication() {
    Shutdown();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool UCCoderBoxApplication::Initialize() {
    return Initialize(CoderBoxConfiguration());
}

bool UCCoderBoxApplication::Initialize(const CoderBoxConfiguration& config) {
    std::cout << "CoderBox: Initializing..." << std::endl;
    
    configuration = config;
    
    // Initialize base UltraCanvas application
    if (!UltraCanvasApplication::Initialize()) {
        std::cerr << "CoderBox: Failed to initialize UltraCanvas application" << std::endl;
        return false;
    }
    
    // Apply theme
    currentTheme = config.darkTheme ? CoderBoxTheme::Dark() : CoderBoxTheme::Light();
    
    // Create main window
    CreateMainWindow();
    if (!mainWindow) {
        std::cerr << "CoderBox: Failed to create main window" << std::endl;
        return false;
    }
    
    // Create UI components
    CreateMenuBar();
    CreateToolbar();
    CreateStatusBar();
    CreateMainLayout();
    
    // Apply theme to all components
    ApplyTheme(currentTheme);
    
    // Load configuration (recent projects, etc.)
    LoadConfiguration();
    
    // Update initial state
    UpdateMenuState();
    UpdateToolbarState();
    SetStatus("Ready");
    
    // Show main window
    mainWindow->Show();
    
    std::cout << "CoderBox: Initialization complete" << std::endl;
    return true;
}

void UCCoderBoxApplication::Shutdown() {
    std::cout << "CoderBox: Shutting down..." << std::endl;
    
    // Save all modified files prompt
    for (auto& pair : openEditors) {
        if (pair.second.isModified) {
            // TODO: Prompt user to save
        }
    }
    
    // Save configuration
    SaveConfiguration();
    
    // Clear all editors
    openEditors.clear();
    
    // Close project
    currentProject = nullptr;
    
    // Destroy window
    if (mainWindow) {
        mainWindow->Close();
        mainWindow = nullptr;
    }
    
    std::cout << "CoderBox: Shutdown complete" << std::endl;
}

// ============================================================================
// WINDOW CREATION
// ============================================================================

void UCCoderBoxApplication::CreateMainWindow() {
    WindowConfig windowConfig;
    windowConfig.title = configuration.windowTitle;
    windowConfig.width = configuration.windowWidth;
    windowConfig.height = configuration.windowHeight;
    windowConfig.resizable = true;
    windowConfig.backgroundColor = currentTheme.windowBackground;
    
    mainWindow = std::make_shared<UltraCanvasWindow>();
    if (!mainWindow->Create(windowConfig)) {
        std::cerr << "CoderBox: Failed to create main window" << std::endl;
        mainWindow = nullptr;
        return;
    }
    
    // Set up window resize handler
    mainWindow->onWindowResize = [this](int w, int h) {
        OnWindowResize(w, h);
    };
    
    // Set up window close handler
    mainWindow->onWindowClose = [this]() {
        Shutdown();
        RequestExit();
    };
    
    std::cout << "CoderBox: Main window created (" 
              << configuration.windowWidth << "x" << configuration.windowHeight << ")" << std::endl;
}

// ============================================================================
// MENU BAR CREATION
// ============================================================================

void UCCoderBoxApplication::CreateMenuBar() {
    if (!mainWindow) return;
    
    // Create menu bar using UltraCanvasMenu with Menubar type
    menuBar = std::make_shared<UltraCanvasMenu>(
        "CoderBoxMenuBar", GetNextId(), 0, 0, 
        configuration.windowWidth, 24
    );
    menuBar->SetMenuType(MenuType::Menubar);
    menuBar->SetOrientation(MenuOrientation::Horizontal);
    
    // Apply menu bar styling
    MenuStyle menuStyle = MenuStyle::Default();
    menuStyle.backgroundColor = currentTheme.menuBarBackground;
    menuStyle.textColor = currentTheme.textColor;
    menuStyle.hoverColor = currentTheme.hoverColor;
    menuStyle.hoverTextColor = currentTheme.activeTextColor;
    menuStyle.itemHeight = 24;
    menuBar->SetStyle(menuStyle);
    
    // Setup all menus
    SetupFileMenu();
    SetupEditMenu();
    SetupViewMenu();
    SetupBuildMenu();
    SetupProjectMenu();
    SetupHelpMenu();
    
    mainWindow->AddChild(menuBar);
    
    std::cout << "CoderBox: Menu bar created" << std::endl;
}

void UCCoderBoxApplication::SetupFileMenu() {
    menuBar->AddItem(MenuItemData::Submenu("File", {
        MenuItemData::ActionWithShortcut("New Project...", "Ctrl+Shift+N", [this]() {
            OnMenuCommand(CoderBoxCommand::FileNewProject);
        }),
        MenuItemData::ActionWithShortcut("New File", "Ctrl+N", [this]() {
            OnMenuCommand(CoderBoxCommand::FileNewFile);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Open File...", "Ctrl+O", [this]() {
            OnMenuCommand(CoderBoxCommand::FileOpen);
        }),
        MenuItemData::ActionWithShortcut("Open Project...", "Ctrl+Shift+O", [this]() {
            OnMenuCommand(CoderBoxCommand::FileOpenProject);
        }),
        MenuItemData::Submenu("Recent Projects", {}),  // Will be populated dynamically
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Save", "Ctrl+S", [this]() {
            OnMenuCommand(CoderBoxCommand::FileSave);
        }),
        MenuItemData::ActionWithShortcut("Save As...", "Ctrl+Shift+S", [this]() {
            OnMenuCommand(CoderBoxCommand::FileSaveAs);
        }),
        MenuItemData::ActionWithShortcut("Save All", "Ctrl+Alt+S", [this]() {
            OnMenuCommand(CoderBoxCommand::FileSaveAll);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Close File", "Ctrl+W", [this]() {
            OnMenuCommand(CoderBoxCommand::FileCloseFile);
        }),
        MenuItemData::Action("Close Project", [this]() {
            OnMenuCommand(CoderBoxCommand::FileCloseProject);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Exit", "Alt+F4", [this]() {
            OnMenuCommand(CoderBoxCommand::FileExit);
        })
    }));
}

void UCCoderBoxApplication::SetupEditMenu() {
    menuBar->AddItem(MenuItemData::Submenu("Edit", {
        MenuItemData::ActionWithShortcut("Undo", "Ctrl+Z", [this]() {
            OnMenuCommand(CoderBoxCommand::EditUndo);
        }),
        MenuItemData::ActionWithShortcut("Redo", "Ctrl+Y", [this]() {
            OnMenuCommand(CoderBoxCommand::EditRedo);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Cut", "Ctrl+X", [this]() {
            OnMenuCommand(CoderBoxCommand::EditCut);
        }),
        MenuItemData::ActionWithShortcut("Copy", "Ctrl+C", [this]() {
            OnMenuCommand(CoderBoxCommand::EditCopy);
        }),
        MenuItemData::ActionWithShortcut("Paste", "Ctrl+V", [this]() {
            OnMenuCommand(CoderBoxCommand::EditPaste);
        }),
        MenuItemData::ActionWithShortcut("Delete", "Del", [this]() {
            OnMenuCommand(CoderBoxCommand::EditDelete);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Select All", "Ctrl+A", [this]() {
            OnMenuCommand(CoderBoxCommand::EditSelectAll);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Find...", "Ctrl+F", [this]() {
            OnMenuCommand(CoderBoxCommand::EditFind);
        }),
        MenuItemData::ActionWithShortcut("Find Next", "F3", [this]() {
            OnMenuCommand(CoderBoxCommand::EditFindNext);
        }),
        MenuItemData::ActionWithShortcut("Find Previous", "Shift+F3", [this]() {
            OnMenuCommand(CoderBoxCommand::EditFindPrevious);
        }),
        MenuItemData::ActionWithShortcut("Replace...", "Ctrl+H", [this]() {
            OnMenuCommand(CoderBoxCommand::EditReplace);
        }),
        MenuItemData::ActionWithShortcut("Find in Files...", "Ctrl+Shift+F", [this]() {
            OnMenuCommand(CoderBoxCommand::EditFindInFiles);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Go to Line...", "Ctrl+G", [this]() {
            OnMenuCommand(CoderBoxCommand::EditGoToLine);
        }),
        MenuItemData::ActionWithShortcut("Toggle Comment", "Ctrl+/", [this]() {
            OnMenuCommand(CoderBoxCommand::EditToggleComment);
        })
    }));
}

void UCCoderBoxApplication::SetupViewMenu() {
    menuBar->AddItem(MenuItemData::Submenu("View", {
        MenuItemData::Checkbox("Project Explorer", projectTreeVisible, [this](bool checked) {
            projectTreeVisible = checked;
            ToggleProjectTree();
        }),
        MenuItemData::Checkbox("Output Console", outputConsoleVisible, [this](bool checked) {
            outputConsoleVisible = checked;
            ToggleOutputConsole();
        }),
        MenuItemData::Checkbox("Toolbar", true, [this](bool checked) {
            if (toolbar) toolbar->SetVisible(checked);
            UpdateLayout();
        }),
        MenuItemData::Checkbox("Status Bar", true, [this](bool checked) {
            if (statusBar) statusBar->SetVisible(checked);
            UpdateLayout();
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Full Screen", "F11", [this]() {
            OnMenuCommand(CoderBoxCommand::ViewFullScreen);
        }),
        MenuItemData::Separator(),
        MenuItemData::Checkbox("Line Numbers", configuration.showLineNumbers, [this](bool checked) {
            configuration.showLineNumbers = checked;
            for (auto& pair : openEditors) {
                if (pair.second.editor) {
                    pair.second.editor->SetShowLineNumbers(checked);
                }
            }
        }),
        MenuItemData::Checkbox("Word Wrap", configuration.wordWrap, [this](bool checked) {
            configuration.wordWrap = checked;
            for (auto& pair : openEditors) {
                if (pair.second.editor) {
                    pair.second.editor->SetWordWrap(checked);
                }
            }
        })
    }));
}

void UCCoderBoxApplication::SetupBuildMenu() {
    menuBar->AddItem(MenuItemData::Submenu("Build", {
        MenuItemData::ActionWithShortcut("Build Project", "Ctrl+B", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildBuild);
        }),
        MenuItemData::ActionWithShortcut("Rebuild Project", "Ctrl+Shift+B", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildRebuild);
        }),
        MenuItemData::Action("Clean Project", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildClean);
        }),
        MenuItemData::ActionWithShortcut("Build Current File", "Ctrl+F7", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildBuildFile);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Run", "F5", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildRun);
        }),
        MenuItemData::ActionWithShortcut("Debug", "Ctrl+F5", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildDebug);
        }),
        MenuItemData::ActionWithShortcut("Stop", "Shift+F5", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildStop);
        }),
        MenuItemData::Separator(),
        MenuItemData::ActionWithShortcut("Next Error", "Ctrl+.", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildNextError);
        }),
        MenuItemData::ActionWithShortcut("Previous Error", "Ctrl+,", [this]() {
            OnMenuCommand(CoderBoxCommand::BuildPreviousError);
        }),
        MenuItemData::Separator(),
        MenuItemData::Submenu("CMake", {
            MenuItemData::Action("Configure", [this]() {
                OnMenuCommand(CoderBoxCommand::BuildCMakeConfigure);
            }),
            MenuItemData::Action("Build", [this]() {
                OnMenuCommand(CoderBoxCommand::BuildCMakeBuild);
            })
        })
    }));
}

void UCCoderBoxApplication::SetupProjectMenu() {
    menuBar->AddItem(MenuItemData::Submenu("Project", {
        MenuItemData::Action("Add New File...", [this]() {
            OnMenuCommand(CoderBoxCommand::ProjectAddFile);
        }),
        MenuItemData::Action("Add New Folder...", [this]() {
            OnMenuCommand(CoderBoxCommand::ProjectAddFolder);
        }),
        MenuItemData::Separator(),
        MenuItemData::Action("Remove from Project", [this]() {
            OnMenuCommand(CoderBoxCommand::ProjectRemoveFile);
        }),
        MenuItemData::Separator(),
        MenuItemData::Action("Refresh", [this]() {
            OnMenuCommand(CoderBoxCommand::ProjectRefresh);
        }),
        MenuItemData::Separator(),
        MenuItemData::Action("Project Properties...", [this]() {
            OnMenuCommand(CoderBoxCommand::ProjectProperties);
        })
    }));
}

void UCCoderBoxApplication::SetupHelpMenu() {
    menuBar->AddItem(MenuItemData::Submenu("Help", {
        MenuItemData::ActionWithShortcut("Help Contents", "F1", [this]() {
            OnMenuCommand(CoderBoxCommand::HelpContents);
        }),
        MenuItemData::Action("Keyboard Shortcuts...", [this]() {
            OnMenuCommand(CoderBoxCommand::HelpKeyboardShortcuts);
        }),
        MenuItemData::Separator(),
        MenuItemData::Action("About CoderBox", [this]() {
            OnMenuCommand(CoderBoxCommand::HelpAbout);
        })
    }));
}

} // namespace CoderBox
} // namespace UltraCanvas
