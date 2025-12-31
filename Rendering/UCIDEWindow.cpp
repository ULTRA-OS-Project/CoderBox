// Apps/IDE/Rendering/UCIDEWindow.cpp
// Main IDE Window Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCIDERenderContext.h"
#include "../Build/UCBuildManager.h"
#include "../Build/UCBuildOutput.h"
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string GetFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string GetFileExtension(const std::string& path) {
    size_t pos = path.rfind('.');
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return "";
}

// ============================================================================
// IDE WINDOW IMPLEMENTATION
// ============================================================================

UCIDEWindow::UCIDEWindow() {
}

UCIDEWindow::~UCIDEWindow() {
    Close();
}

bool UCIDEWindow::Create(int w, int h, const std::string& title) {
    width = w;
    height = h;
    windowTitle = title;
    
    // Initialize components
    InitializeComponents();
    
    // Setup callbacks
    SetupCallbacks();
    
    // Create platform window (platform-specific implementation needed)
    // nativeWindow = PlatformCreateWindow(width, height, title);
    // renderContext = PlatformCreateRenderContext(nativeWindow);
    
    // Initialize renderer
    if (renderContext) {
        renderer->Initialize(renderContext, width, height);
    }
    
    isOpen = true;
    return true;
}

void UCIDEWindow::Close() {
    if (!isOpen) return;
    
    // Save any unsaved files
    for (auto& pair : editors) {
        if (pair.second->IsModified()) {
            // Would show save dialog here
            pair.second->SaveFile();
        }
    }
    
    // Close project
    CloseProject();
    
    // Destroy platform window
    // PlatformDestroyWindow(nativeWindow);
    
    isOpen = false;
    
    if (onClose) {
        onClose();
    }
}

void UCIDEWindow::InitializeComponents() {
    // Create layout
    layout = std::make_unique<UCIDELayout>();
    layout->Initialize(width, height);
    
    // Create menu bar
    menuBar = std::make_unique<UCIDEMenuBar>();
    menuBar->InitializeDefaultMenus();
    menuBar->SetRegion(0, 0, width, layout->GetStyle().menuBarHeight);
    
    // Create toolbar
    toolbar = std::make_unique<UCIDEToolbar>();
    toolbar->InitializeDefaultItems();
    toolbar->SetRegion(0, layout->GetStyle().menuBarHeight, width, layout->GetStyle().toolbarHeight);
    
    // Create status bar
    statusBar = std::make_unique<UCIDEStatusBar>();
    statusBar->InitializeDefaultSegments();
    statusBar->SetRegion(0, height - layout->GetStyle().statusBarHeight, 
                         width, layout->GetStyle().statusBarHeight);
    
    // Create output console
    outputConsole = std::make_unique<UCIDEOutputConsole>();
    outputConsole->InitializeDefaultTabs();
    
    // Create project tree
    projectTree = std::make_unique<UCIDEProjectTreeView>();
    
    // Create renderer
    renderer = std::make_unique<UCIDERenderer>();
    renderer->BindLayout(layout.get());
    renderer->BindMenuBar(menuBar.get());
    renderer->BindToolbar(toolbar.get());
    renderer->BindStatusBar(statusBar.get());
    renderer->BindOutputConsole(outputConsole.get());
    renderer->BindProjectTree(projectTree.get());
    
    // Create build manager
    buildManager = std::make_unique<UCBuildManager>();
}

void UCIDEWindow::SetupCallbacks() {
    // Menu bar commands
    menuBar->onCommand = [this](MenuCommand cmd) {
        OnMenuCommand(cmd);
    };
    
    // Toolbar commands
    toolbar->onCommand = [this](MenuCommand cmd) {
        OnToolbarCommand(cmd);
    };
    
    // Toolbar configuration change
    toolbar->onConfigurationChange = [this](const std::string& config) {
        if (currentProject) {
            currentProject->SetActiveConfiguration(config);
            statusBar->SetConfiguration(config);
        }
    };
    
    // Layout tab callbacks
    layout->onTabActivate = [this](const std::string& path) {
        OnFileOpen(path);
    };
    
    layout->onTabClose = [this](const std::string& path) {
        OnTabClose(path);
    };
    
    // Project tree callbacks
    projectTree->onFileOpen = [this](const std::string& path) {
        OnProjectTreeFileOpen(path);
    };
    
    // Output console error click
    outputConsole->onErrorClick = [this](const std::string& file, int line, int col) {
        OnErrorClick(file, line, col);
    };
    
    // Status bar position click
    statusBar->onPositionClick = [this]() {
        // Show "Go to Line" dialog
    };
    
    // Build manager callbacks
    buildManager->onBuildStart = [this]() {
        toolbar->SetItemEnabled("stop", true);
        toolbar->SetItemEnabled("build", false);
        toolbar->SetItemEnabled("rebuild", false);
        outputConsole->StartBuild();
        statusBar->SetStatus("Building...");
        
        if (onBuildStart) {
            onBuildStart();
        }
    };
    
    buildManager->onBuildComplete = [this](bool success) {
        toolbar->SetItemEnabled("stop", false);
        toolbar->SetItemEnabled("build", true);
        toolbar->SetItemEnabled("rebuild", true);
        
        int errors = outputConsole->GetErrorCount();
        int warnings = outputConsole->GetWarningCount();
        
        outputConsole->EndBuild(success, errors, warnings);
        statusBar->SetErrorCounts(errors, warnings);
        statusBar->ClearBuildProgress();
        
        if (success) {
            statusBar->SetStatus("Build succeeded");
        } else {
            statusBar->SetStatus("Build failed");
        }
        
        if (onBuildComplete) {
            onBuildComplete(success);
        }
    };
    
    buildManager->onBuildOutput = [this](const std::string& output) {
        outputConsole->WriteBuildOutput(output);
    };
    
    buildManager->onBuildError = [this](const CompilerMessage& msg) {
        outputConsole->WriteCompilerMessage(msg);
    };
    
    buildManager->onBuildProgress = [this](float progress, const std::string& status) {
        statusBar->SetBuildProgress(progress, status);
    };
}

void UCIDEWindow::ProcessEvents() {
    // Process platform events - this is platform-specific
    // Would call platform event loop here
}

void UCIDEWindow::Update(float deltaTime) {
    // Update cursor blink
    renderer->UpdateCursorBlink(static_cast<int>(deltaTime * 1000));
    
    // Update build manager
    buildManager->Update();
}

void UCIDEWindow::Render() {
    if (!renderer) return;
    
    renderer->Render();
    
    // Swap buffers (platform-specific)
    // PlatformSwapBuffers(nativeWindow);
}

// ============================================================================
// PROJECT OPERATIONS
// ============================================================================

bool UCIDEWindow::OpenProject(const std::string& path) {
    // Close current project
    CloseProject();
    
    // Load project
    currentProject = std::make_unique<UCIDEProject>();
    if (!currentProject->Load(path)) {
        currentProject.reset();
        return false;
    }
    
    // Update project tree
    projectTree->SetProject(currentProject.get());
    
    // Update toolbar configurations
    std::vector<std::string> configs;
    for (const auto& config : currentProject->GetConfigurations()) {
        configs.push_back(config.name);
    }
    toolbar->UpdateConfigurations(configs);
    
    // Update status bar
    statusBar->SetConfiguration(currentProject->GetActiveConfiguration());
    
    // Update window title
    UpdateWindowTitle();
    
    // Add to recent projects
    menuBar->UpdateRecentProjects({path});
    
    return true;
}

void UCIDEWindow::CloseProject() {
    if (!currentProject) return;
    
    // Close all files
    editors.clear();
    activeEditor = nullptr;
    layout->CloseAllTabs();
    
    // Clear project tree
    projectTree->SetProject(nullptr);
    
    // Clear project
    currentProject.reset();
    
    // Update window title
    UpdateWindowTitle();
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

bool UCIDEWindow::OpenFile(const std::string& path) {
    // Check if already open
    auto it = editors.find(path);
    if (it != editors.end()) {
        // Activate existing tab
        layout->SetActiveTab(path);
        activeEditor = it->second.get();
        renderer->BindEditor(activeEditor);
        return true;
    }
    
    // Create new editor
    auto editor = std::make_unique<UCIDEEditor>();
    if (!editor->LoadFile(path)) {
        return false;
    }
    
    // Setup editor callbacks
    editor->onTextChange = [this, path]() {
        layout->SetTabModified(path, true);
        UpdateWindowTitle();
    };
    
    editor->onCursorChange = [this]() {
        if (activeEditor) {
            TextPosition pos = activeEditor->GetCursorPosition();
            statusBar->SetCursorPosition(pos.line + 1, pos.column + 1);
        }
    };
    
    // Add tab
    layout->AddTab(path);
    
    // Store editor
    editors[path] = std::move(editor);
    activeEditor = editors[path].get();
    
    // Bind to renderer
    renderer->BindEditor(activeEditor);
    
    // Update status bar
    std::string ext = GetFileExtension(path);
    // statusBar->SetFileType(GetLanguageFromExtension(ext));
    statusBar->SetEncoding(activeEditor->GetSettings().encoding);
    
    // Update cursor position
    TextPosition pos = activeEditor->GetCursorPosition();
    statusBar->SetCursorPosition(pos.line + 1, pos.column + 1);
    
    if (onFileOpen) {
        onFileOpen(path);
    }
    
    return true;
}

bool UCIDEWindow::SaveCurrentFile() {
    if (!activeEditor) return false;
    
    if (activeEditor->SaveFile()) {
        std::string path = activeEditor->GetFilePath();
        layout->SetTabModified(path, false);
        UpdateWindowTitle();
        statusBar->ShowNotification("File saved", 2000);
        return true;
    }
    
    return false;
}

void UCIDEWindow::SaveAllFiles() {
    for (auto& pair : editors) {
        UCIDEEditor* editor = pair.second.get();
        if (editor->IsModified()) {
            editor->SaveFile();
            layout->SetTabModified(pair.first, false);
        }
    }
    UpdateWindowTitle();
    statusBar->ShowNotification("All files saved", 2000);
}

void UCIDEWindow::CloseCurrentFile() {
    if (!activeEditor) return;
    
    std::string path = activeEditor->GetFilePath();
    
    // Check for unsaved changes
    if (activeEditor->IsModified()) {
        // Would show save dialog here
        SaveCurrentFile();
    }
    
    // Remove tab
    layout->RemoveTab(path);
    
    // Remove editor
    editors.erase(path);
    
    // Set new active editor
    const EditorTab* newActiveTab = layout->GetActiveTab();
    if (newActiveTab) {
        auto it = editors.find(newActiveTab->filePath);
        if (it != editors.end()) {
            activeEditor = it->second.get();
            renderer->BindEditor(activeEditor);
        }
    } else {
        activeEditor = nullptr;
        renderer->BindEditor(nullptr);
    }
    
    UpdateWindowTitle();
}

// ============================================================================
// BUILD OPERATIONS
// ============================================================================

void UCIDEWindow::BuildProject() {
    if (!currentProject) {
        statusBar->ShowNotification("No project open", 2000);
        return;
    }
    
    // Save all files first
    SaveAllFiles();
    
    // Start build
    buildManager->Build(currentProject.get());
}

void UCIDEWindow::UpdateWindowTitle() {
    std::stringstream ss;
    
    if (activeEditor) {
        if (activeEditor->IsModified()) {
            ss << "• ";
        }
        ss << GetFileName(activeEditor->GetFilePath());
        ss << " - ";
    }
    
    if (currentProject) {
        ss << currentProject->GetName() << " - ";
    }
    
    ss << "ULTRA IDE";
    
    windowTitle = ss.str();
    
    // Update platform window title
    // PlatformSetWindowTitle(nativeWindow, windowTitle);
}

} // namespace IDE
} // namespace UltraCanvas
