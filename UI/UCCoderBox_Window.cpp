// Apps/CoderBox/UI/UCCoderBoxApplication_Part2.cpp
// CoderBox Application - Toolbar, StatusBar, Layout Implementation
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#include "UCCoderBoxApplication.h"
#include <iostream>
#include <sstream>

namespace UltraCanvas {
namespace CoderBox {

// ============================================================================
// TOOLBAR CREATION
// ============================================================================

void UCCoderBoxApplication::CreateToolbar() {
    if (!mainWindow) return;
    
    // Create toolbar using UltraCanvasToolbarBuilder
    toolbar = UltraCanvasToolbarBuilder("CoderBoxToolbar")
        .SetOrientation(ToolbarOrientation::Horizontal)
        .SetStyle(ToolbarStyle::Standard)
        .SetDimensions(0, 24, configuration.windowWidth, 32)
        
        // File operations
        .AddButton("new", "New", "icons/file-plus.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::FileNewFile);
        })
        .AddButton("open", "Open", "icons/folder-open.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::FileOpen);
        })
        .AddButton("save", "Save", "icons/save.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::FileSave);
        })
        .AddButton("save-all", "Save All", "icons/save-all.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::FileSaveAll);
        })
        
        .AddSeparator()
        
        // Edit operations
        .AddButton("undo", "Undo", "icons/undo.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::EditUndo);
        })
        .AddButton("redo", "Redo", "icons/redo.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::EditRedo);
        })
        
        .AddSeparator()
        
        // Build operations
        .AddButton("run", "Run", "icons/play.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::BuildRun);
        })
        .AddButton("debug", "Debug", "icons/bug.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::BuildDebug);
        })
        .AddButton("build", "Build", "icons/hammer.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::BuildBuild);
        })
        .AddButton("rebuild", "Rebuild", "icons/refresh.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::BuildRebuild);
        })
        .AddButton("stop", "Stop", "icons/stop.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::BuildStop);
        })
        
        .AddSeparator()
        
        .AddStretch(1.0f)
        
        // Search
        .AddButton("find", "Find", "icons/search.png", [this]() {
            OnToolbarCommand(CoderBoxCommand::EditFind);
        })
        
        .Build();
    
    // Apply toolbar styling
    ToolbarAppearance appearance;
    appearance.backgroundColor = currentTheme.toolbarBackground;
    appearance.hoverColor = currentTheme.hoverColor;
    appearance.activeColor = currentTheme.selectedColor;
    toolbar->SetAppearance(appearance);
    
    // Create configuration dropdown
    configDropdown = std::make_shared<UltraCanvasDropdown>(
        "ConfigDropdown", GetNextId(), 
        configuration.windowWidth - 280, 28, 120, 24
    );
    configDropdown->AddOption("Debug");
    configDropdown->AddOption("Release");
    configDropdown->SetSelectedIndex(0);
    configDropdown->onSelectionChange = [this](int index, const std::string& value) {
        std::cout << "CoderBox: Configuration changed to " << value << std::endl;
    };
    
    // Create compiler dropdown
    compilerDropdown = std::make_shared<UltraCanvasDropdown>(
        "CompilerDropdown", GetNextId(),
        configuration.windowWidth - 150, 28, 140, 24
    );
    compilerDropdown->AddOption("GCC");
    compilerDropdown->AddOption("G++");
    compilerDropdown->AddOption("FreePascal");
    compilerDropdown->AddOption("Clang");
    compilerDropdown->SetSelectedIndex(1);  // Default to G++
    compilerDropdown->onSelectionChange = [this](int index, const std::string& value) {
        std::cout << "CoderBox: Compiler changed to " << value << std::endl;
    };
    
    // Disable stop button initially
    if (auto* stopBtn = toolbar->GetItem("stop")) {
        stopBtn->SetEnabled(false);
    }
    
    mainWindow->AddChild(toolbar);
    mainWindow->AddChild(configDropdown);
    mainWindow->AddChild(compilerDropdown);
    
    std::cout << "CoderBox: Toolbar created" << std::endl;
}

// ============================================================================
// STATUS BAR CREATION
// ============================================================================

void UCCoderBoxApplication::CreateStatusBar() {
    if (!mainWindow) return;
    
    int statusBarY = configuration.windowHeight - 22;
    
    // Create status bar using UltraCanvasToolbar with StatusBar style
    statusBar = ToolbarPresets::CreateStatusBar("CoderBoxStatusBar");
    statusBar->SetBounds(0, statusBarY, configuration.windowWidth, 22);
    
    // Apply status bar styling
    ToolbarAppearance appearance;
    appearance.backgroundColor = currentTheme.statusBarBackground;
    statusBar->SetAppearance(appearance);
    
    // Create status segments as labels
    int x = 8;
    
    // Status message (stretches)
    statusLabel = std::make_shared<UltraCanvasLabel>(
        "StatusLabel", GetNextId(), x, statusBarY + 3, 200, 16
    );
    statusLabel->SetText("Ready");
    statusLabel->SetTextColor(Colors::White);
    statusLabel->SetFontSize(11);
    x += 210;
    
    // Position label
    positionLabel = std::make_shared<UltraCanvasLabel>(
        "PositionLabel", GetNextId(), x, statusBarY + 3, 100, 16
    );
    positionLabel->SetText("Ln 1, Col 1");
    positionLabel->SetTextColor(Colors::White);
    positionLabel->SetFontSize(11);
    x += 110;
    
    // Encoding label
    encodingLabel = std::make_shared<UltraCanvasLabel>(
        "EncodingLabel", GetNextId(), x, statusBarY + 3, 60, 16
    );
    encodingLabel->SetText("UTF-8");
    encodingLabel->SetTextColor(Colors::White);
    encodingLabel->SetFontSize(11);
    x += 70;
    
    // Language label
    languageLabel = std::make_shared<UltraCanvasLabel>(
        "LanguageLabel", GetNextId(), x, statusBarY + 3, 80, 16
    );
    languageLabel->SetText("Plain Text");
    languageLabel->SetTextColor(Colors::White);
    languageLabel->SetFontSize(11);
    x += 90;
    
    // Error count label (right-aligned)
    errorCountLabel = std::make_shared<UltraCanvasLabel>(
        "ErrorCountLabel", GetNextId(), 
        configuration.windowWidth - 120, statusBarY + 3, 110, 16
    );
    errorCountLabel->SetText("✗ 0  ⚠ 0");
    errorCountLabel->SetTextColor(Colors::White);
    errorCountLabel->SetFontSize(11);
    
    mainWindow->AddChild(statusBar);
    mainWindow->AddChild(statusLabel);
    mainWindow->AddChild(positionLabel);
    mainWindow->AddChild(encodingLabel);
    mainWindow->AddChild(languageLabel);
    mainWindow->AddChild(errorCountLabel);
    
    std::cout << "CoderBox: Status bar created" << std::endl;
}

// ============================================================================
// MAIN LAYOUT CREATION
// ============================================================================

void UCCoderBoxApplication::CreateMainLayout() {
    if (!mainWindow) return;
    
    // Calculate content area (between toolbar and status bar)
    int contentY = 56;  // Menu (24) + Toolbar (32)
    int contentHeight = configuration.windowHeight - 56 - 22;  // Minus status bar
    
    // Create main horizontal split pane: Project Tree | Editor Area
    mainHorizontalSplit = CreateHorizontalSplitPane(
        "MainHSplit", GetNextId(),
        Rect2D(0, contentY, configuration.windowWidth, contentHeight),
        configuration.projectTreeWidthRatio
    );
    mainHorizontalSplit->SetMinimumSizes(150, 300);
    mainHorizontalSplit->SetSplitterStyle(SplitterStyle::Simple);
    mainHorizontalSplit->SetSplitterColor(currentTheme.dividerColor);
    mainHorizontalSplit->SetSplitterHoverColor(currentTheme.dividerHoverColor);
    
    // Create project tree
    CreateProjectTree();
    
    // Calculate right pane dimensions
    int rightPaneWidth = static_cast<int>(configuration.windowWidth * (1.0f - configuration.projectTreeWidthRatio));
    int rightPaneX = configuration.windowWidth - rightPaneWidth;
    
    // Create vertical split pane for editor area: Editor | Output Console
    editorVerticalSplit = CreateVerticalSplitPane(
        "EditorVSplit", GetNextId(),
        Rect2D(rightPaneX, contentY, rightPaneWidth, contentHeight),
        1.0f - configuration.outputConsoleHeightRatio
    );
    editorVerticalSplit->SetMinimumSizes(100, 80);
    editorVerticalSplit->SetSplitterStyle(SplitterStyle::Simple);
    editorVerticalSplit->SetSplitterColor(currentTheme.dividerColor);
    editorVerticalSplit->SetSplitterHoverColor(currentTheme.dividerHoverColor);
    
    // Create editor tabs
    CreateEditorTabs();
    
    // Create output console
    CreateOutputConsole();
    
    // Set up split pane contents
    mainHorizontalSplit->SetPanes(projectTree.get(), editorVerticalSplit.get());
    editorVerticalSplit->SetPanes(editorTabs.get(), outputConsole.get());
    
    // Set up split pane callbacks
    mainHorizontalSplit->onSplitRatioChanged = [this](float ratio) {
        configuration.projectTreeWidthRatio = ratio;
    };
    
    editorVerticalSplit->onSplitRatioChanged = [this](float ratio) {
        configuration.outputConsoleHeightRatio = 1.0f - ratio;
    };
    
    mainWindow->AddChild(mainHorizontalSplit);
    
    std::cout << "CoderBox: Main layout created" << std::endl;
}

// ============================================================================
// PROJECT TREE CREATION
// ============================================================================

void UCCoderBoxApplication::CreateProjectTree() {
    // Calculate project tree dimensions from split pane
    int treeWidth = static_cast<int>(configuration.windowWidth * configuration.projectTreeWidthRatio);
    int contentY = 56;
    int contentHeight = configuration.windowHeight - 56 - 22;
    
    projectTree = std::make_shared<UltraCanvasTreeView>(
        "ProjectTree", GetNextId(),
        0, contentY, treeWidth, contentHeight
    );
    
    // Apply tree view styling
    projectTree->SetBackgroundColor(currentTheme.panelBackground);
    projectTree->SetTextColor(currentTheme.textColor);
    projectTree->SetSelectionColor(currentTheme.selectedColor);
    projectTree->SetHoverColor(currentTheme.hoverColor);
    projectTree->SetShowRootNode(false);
    projectTree->SetIndentation(16);
    projectTree->SetRowHeight(22);
    
    // Set up tree callbacks
    projectTree->onNodeSelected = [this](TreeNode* node) {
        OnProjectTreeSelect(node);
    };
    
    projectTree->onNodeDoubleClicked = [this](TreeNode* node) {
        OnProjectTreeDoubleClick(node);
    };
    
    // Create default root node
    auto rootNode = std::make_unique<TreeNode>("No Project Open");
    rootNode->data.iconPath = "icons/folder.png";
    projectTree->SetRootNode(std::move(rootNode));
    
    std::cout << "CoderBox: Project tree created" << std::endl;
}

// ============================================================================
// EDITOR TABS CREATION
// ============================================================================

void UCCoderBoxApplication::CreateEditorTabs() {
    // Calculate editor area dimensions
    int rightPaneWidth = static_cast<int>(configuration.windowWidth * (1.0f - configuration.projectTreeWidthRatio));
    int editorHeight = static_cast<int>((configuration.windowHeight - 56 - 22) * (1.0f - configuration.outputConsoleHeightRatio));
    int contentY = 56;
    
    editorTabs = std::make_shared<UltraCanvasTabbedContainer>(
        "EditorTabs", GetNextId(),
        0, 0, rightPaneWidth, editorHeight
    );
    
    // Configure tab appearance
    editorTabs->SetTabPosition(TabPosition::Top);
    editorTabs->SetTabStyle(TabStyle::Rounded);
    editorTabs->SetCloseMode(TabCloseMode::CloseButton);
    editorTabs->SetTabHeight(28);
    editorTabs->SetTabMinWidth(80);
    editorTabs->SetTabMaxWidth(200);
    
    // Apply colors
    editorTabs->SetTabBarColor(currentTheme.tabBarBackground);
    editorTabs->SetActiveTabColor(currentTheme.editorBackground);
    editorTabs->SetInactiveTabColor(currentTheme.tabBarBackground);
    editorTabs->SetActiveTabTextColor(currentTheme.activeTextColor);
    editorTabs->SetInactiveTabTextColor(currentTheme.textColor);
    editorTabs->SetContentAreaColor(currentTheme.editorBackground);
    
    // Enable tab features
    editorTabs->SetEnableNewTabButton(false);
    editorTabs->SetEnableTabScrolling(true);
    editorTabs->SetEnableTabDragReorder(true);
    
    // Set up tab callbacks
    editorTabs->onTabChange = [this](int index) {
        OnEditorTabChange(index);
    };
    
    editorTabs->onTabClose = [this](int index) {
        OnEditorTabClose(index);
        return true;  // Allow close
    };
    
    std::cout << "CoderBox: Editor tabs created" << std::endl;
}

// ============================================================================
// OUTPUT CONSOLE CREATION
// ============================================================================

void UCCoderBoxApplication::CreateOutputConsole() {
    // Calculate output console dimensions
    int rightPaneWidth = static_cast<int>(configuration.windowWidth * (1.0f - configuration.projectTreeWidthRatio));
    int outputHeight = static_cast<int>((configuration.windowHeight - 56 - 22) * configuration.outputConsoleHeightRatio);
    
    outputConsole = std::make_shared<UltraCanvasTabbedContainer>(
        "OutputConsole", GetNextId(),
        0, 0, rightPaneWidth, outputHeight
    );
    
    // Configure output console tabs
    outputConsole->SetTabPosition(TabPosition::Top);
    outputConsole->SetTabStyle(TabStyle::Simple);
    outputConsole->SetCloseMode(TabCloseMode::NoClose);
    outputConsole->SetTabHeight(24);
    
    // Apply colors
    outputConsole->SetTabBarColor(currentTheme.panelBackground);
    outputConsole->SetActiveTabColor(currentTheme.editorBackground);
    outputConsole->SetInactiveTabColor(currentTheme.panelBackground);
    outputConsole->SetActiveTabTextColor(currentTheme.activeTextColor);
    outputConsole->SetInactiveTabTextColor(currentTheme.textColor);
    outputConsole->SetContentAreaColor(currentTheme.editorBackground);
    
    // Create output text areas for each tab
    auto createOutputArea = [this](const std::string& name) {
        auto textArea = std::make_shared<UltraCanvasTextArea>(
            name, GetNextId(), 0, 0, 100, 100
        );
        textArea->SetReadOnly(true);
        textArea->SetShowLineNumbers(false);
        textArea->SetBackgroundColor(currentTheme.editorBackground);
        textArea->SetTextColor(currentTheme.textColor);
        textArea->SetFontFamily("Consolas");
        textArea->SetFontSize(11);
        return textArea;
    };
    
    buildOutput = createOutputArea("BuildOutput");
    errorsOutput = createOutputArea("ErrorsOutput");
    warningsOutput = createOutputArea("WarningsOutput");
    messagesOutput = createOutputArea("MessagesOutput");
    applicationOutput = createOutputArea("ApplicationOutput");
    
    // Add tabs
    int buildTabIdx = outputConsole->AddTab("Build");
    outputConsole->SetTabContent(buildTabIdx, buildOutput);
    
    int errorsTabIdx = outputConsole->AddTab("Errors");
    outputConsole->SetTabContent(errorsTabIdx, errorsOutput);
    outputConsole->SetTabBadge(errorsTabIdx, "0");
    
    int warningsTabIdx = outputConsole->AddTab("Warnings");
    outputConsole->SetTabContent(warningsTabIdx, warningsOutput);
    outputConsole->SetTabBadge(warningsTabIdx, "0");
    
    int messagesTabIdx = outputConsole->AddTab("Messages");
    outputConsole->SetTabContent(messagesTabIdx, messagesOutput);
    
    int appTabIdx = outputConsole->AddTab("Application");
    outputConsole->SetTabContent(appTabIdx, applicationOutput);
    
    // Set active tab to Build
    outputConsole->SetActiveTab(0);
    
    // Set up tab change callback
    outputConsole->onTabChange = [this](int index) {
        OnOutputTabChange(index);
    };
    
    std::cout << "CoderBox: Output console created with 5 tabs" << std::endl;
}

// ============================================================================
// LAYOUT UPDATE
// ============================================================================

void UCCoderBoxApplication::UpdateLayout() {
    if (!mainWindow) return;
    
    int width, height;
    mainWindow->GetSize(width, height);
    
    // Update menu bar
    if (menuBar) {
        menuBar->SetBounds(Rect2Di(0, 0, width, 24));
    }
    
    // Update toolbar
    if (toolbar) {
        int toolbarY = menuBar && menuBar->IsVisible() ? 24 : 0;
        toolbar->SetBounds(Rect2Di(0, toolbarY, width, 32));
        
        // Update dropdown positions
        if (configDropdown) {
            configDropdown->SetPosition(width - 280, toolbarY + 4);
        }
        if (compilerDropdown) {
            compilerDropdown->SetPosition(width - 150, toolbarY + 4);
        }
    }
    
    // Calculate content area
    int contentY = 0;
    if (menuBar && menuBar->IsVisible()) contentY += 24;
    if (toolbar && toolbar->IsVisible()) contentY += 32;
    
    int contentHeight = height - contentY;
    if (statusBar && statusBar->IsVisible()) contentHeight -= 22;
    
    // Update main split pane
    if (mainHorizontalSplit) {
        mainHorizontalSplit->SetBounds(Rect2Di(0, contentY, width, contentHeight));
    }
    
    // Update status bar
    if (statusBar) {
        int statusY = height - 22;
        statusBar->SetBounds(Rect2Di(0, statusY, width, 22));
        
        // Update status bar labels
        if (statusLabel) statusLabel->SetPosition(8, statusY + 3);
        if (positionLabel) positionLabel->SetPosition(218, statusY + 3);
        if (encodingLabel) encodingLabel->SetPosition(328, statusY + 3);
        if (languageLabel) languageLabel->SetPosition(398, statusY + 3);
        if (errorCountLabel) errorCountLabel->SetPosition(width - 120, statusY + 3);
    }
}

void UCCoderBoxApplication::OnWindowResize(int width, int height) {
    configuration.windowWidth = width;
    configuration.windowHeight = height;
    UpdateLayout();
}

void UCCoderBoxApplication::ToggleProjectTree() {
    if (mainHorizontalSplit) {
        if (projectTreeVisible) {
            mainHorizontalSplit->SetSplitRatio(configuration.projectTreeWidthRatio);
        } else {
            mainHorizontalSplit->CollapseLeftPane(true);
        }
    }
}

void UCCoderBoxApplication::ToggleOutputConsole() {
    if (editorVerticalSplit) {
        if (outputConsoleVisible) {
            editorVerticalSplit->SetSplitRatio(1.0f - configuration.outputConsoleHeightRatio);
        } else {
            editorVerticalSplit->CollapseRightPane(true);
        }
    }
}

} // namespace CoderBox
} // namespace UltraCanvas
