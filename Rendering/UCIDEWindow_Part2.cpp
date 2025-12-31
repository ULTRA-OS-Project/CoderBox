// Apps/IDE/Rendering/UCIDEWindow_Part2.cpp
// Main IDE Window Implementation Part 2 for ULTRA IDE
// Event handling and utilities

#include "UCIDERenderContext.h"
#include "../Build/UCBuildManager.h"
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// RUN / DEBUG OPERATIONS (continued)
// ============================================================================

void UCIDEWindow::RunProject() {
    if (!currentProject) {
        statusBar->ShowNotification("No project open", 2000);
        return;
    }
    
    // Build first
    buildManager->onBuildComplete = [this](bool success) {
        if (success) {
            // Run the executable
            buildManager->Run(currentProject.get());
            outputConsole->SetActiveTab(OutputTabType::Application);
            statusBar->SetStatus("Running...");
        }
        
        // Restore original callback
        // This is simplified - in production would use proper callback chain
    };
    
    BuildProject();
}

void UCIDEWindow::StopRunning() {
    buildManager->Stop();
    statusBar->SetStatus("Stopped");
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void UCIDEWindow::OnMenuCommand(MenuCommand cmd) {
    switch (cmd) {
        // File menu
        case MenuCommand::FileNewProject:
            // Show new project dialog
            break;
        case MenuCommand::FileNewFile:
            // Show new file dialog or create untitled file
            break;
        case MenuCommand::FileOpen:
            // Show file open dialog
            break;
        case MenuCommand::FileOpenProject:
            // Show project open dialog
            break;
        case MenuCommand::FileSave:
            SaveCurrentFile();
            break;
        case MenuCommand::FileSaveAs:
            // Show save as dialog
            break;
        case MenuCommand::FileSaveAll:
            SaveAllFiles();
            break;
        case MenuCommand::FileCloseFile:
            CloseCurrentFile();
            break;
        case MenuCommand::FileCloseProject:
            CloseProject();
            break;
        case MenuCommand::FileExit:
            Close();
            break;
            
        // Edit menu
        case MenuCommand::EditUndo:
            if (activeEditor) activeEditor->Undo();
            break;
        case MenuCommand::EditRedo:
            if (activeEditor) activeEditor->Redo();
            break;
        case MenuCommand::EditCut:
            if (activeEditor) activeEditor->Cut();
            break;
        case MenuCommand::EditCopy:
            if (activeEditor) activeEditor->Copy();
            break;
        case MenuCommand::EditPaste:
            // Get text from clipboard and paste
            if (activeEditor) {
                // std::string clipboardText = GetClipboardText();
                // activeEditor->Paste(clipboardText);
            }
            break;
        case MenuCommand::EditDelete:
            if (activeEditor) activeEditor->Delete();
            break;
        case MenuCommand::EditSelectAll:
            if (activeEditor) activeEditor->SelectAll();
            break;
        case MenuCommand::EditFind:
            // Show find dialog
            break;
        case MenuCommand::EditFindNext:
            // Find next occurrence
            break;
        case MenuCommand::EditFindPrevious:
            // Find previous occurrence
            break;
        case MenuCommand::EditReplace:
            // Show replace dialog
            break;
        case MenuCommand::EditFindInFiles:
            // Show find in files dialog
            break;
        case MenuCommand::EditGoToLine:
            // Show go to line dialog
            break;
        case MenuCommand::EditGoToDefinition:
            // Go to definition (requires language server)
            break;
        case MenuCommand::EditToggleComment:
            if (activeEditor) activeEditor->ToggleComment();
            break;
        case MenuCommand::EditIndent:
            if (activeEditor) activeEditor->Indent();
            break;
        case MenuCommand::EditOutdent:
            if (activeEditor) activeEditor->Outdent();
            break;
            
        // View menu
        case MenuCommand::ViewProjectTree:
            layout->TogglePanelVisible(LayoutPanel::ProjectTree);
            menuBar->SetItemChecked(cmd, layout->IsPanelVisible(LayoutPanel::ProjectTree));
            break;
        case MenuCommand::ViewOutputConsole:
            layout->TogglePanelVisible(LayoutPanel::OutputConsole);
            menuBar->SetItemChecked(cmd, layout->IsPanelVisible(LayoutPanel::OutputConsole));
            break;
        case MenuCommand::ViewToolbar:
            layout->TogglePanelVisible(LayoutPanel::Toolbar);
            menuBar->SetItemChecked(cmd, layout->IsPanelVisible(LayoutPanel::Toolbar));
            break;
        case MenuCommand::ViewStatusBar:
            layout->TogglePanelVisible(LayoutPanel::StatusBar);
            menuBar->SetItemChecked(cmd, layout->IsPanelVisible(LayoutPanel::StatusBar));
            break;
        case MenuCommand::ViewFullScreen:
            // Toggle fullscreen
            break;
        case MenuCommand::ViewZoomIn:
            // Zoom in editor
            break;
        case MenuCommand::ViewZoomOut:
            // Zoom out editor
            break;
        case MenuCommand::ViewZoomReset:
            // Reset zoom
            break;
        case MenuCommand::ViewWordWrap:
            if (activeEditor) {
                auto& settings = activeEditor->GetSettings();
                settings.wordWrap = !settings.wordWrap;
                menuBar->SetItemChecked(cmd, settings.wordWrap);
            }
            break;
        case MenuCommand::ViewWhitespace:
            if (activeEditor) {
                auto& settings = activeEditor->GetSettings();
                settings.showWhitespace = !settings.showWhitespace;
                menuBar->SetItemChecked(cmd, settings.showWhitespace);
            }
            break;
        case MenuCommand::ViewLineNumbers:
            if (activeEditor) {
                auto& settings = activeEditor->GetSettings();
                settings.showLineNumbers = !settings.showLineNumbers;
                menuBar->SetItemChecked(cmd, settings.showLineNumbers);
            }
            break;
            
        // Build menu
        case MenuCommand::BuildBuild:
            BuildProject();
            break;
        case MenuCommand::BuildRebuild:
            if (currentProject) {
                buildManager->Clean(currentProject.get());
                BuildProject();
            }
            break;
        case MenuCommand::BuildClean:
            if (currentProject) {
                buildManager->Clean(currentProject.get());
            }
            break;
        case MenuCommand::BuildBuildFile:
            // Build current file only
            break;
        case MenuCommand::BuildRun:
            RunProject();
            break;
        case MenuCommand::BuildDebug:
            // Start debugging
            break;
        case MenuCommand::BuildStop:
            StopRunning();
            break;
        case MenuCommand::BuildNextError:
            outputConsole->GoToNextError();
            break;
        case MenuCommand::BuildPreviousError:
            outputConsole->GoToPreviousError();
            break;
            
        // Project menu
        case MenuCommand::ProjectAddFile:
            // Show add file dialog
            break;
        case MenuCommand::ProjectAddFolder:
            // Show add folder dialog
            break;
        case MenuCommand::ProjectAddExistingFile:
            // Show add existing file dialog
            break;
        case MenuCommand::ProjectRemoveFile:
            // Remove selected file from project
            break;
        case MenuCommand::ProjectRename:
            // Rename selected file
            break;
        case MenuCommand::ProjectProperties:
            // Show project properties dialog
            break;
        case MenuCommand::ProjectRefresh:
            if (currentProject) {
                projectTree->Refresh();
            }
            break;
            
        // Tools menu
        case MenuCommand::ToolsOptions:
            // Show options dialog
            break;
        case MenuCommand::ToolsKeyboardShortcuts:
            // Show keyboard shortcuts dialog
            break;
        case MenuCommand::ToolsPlugins:
            // Show plugins dialog
            break;
        case MenuCommand::ToolsTerminal:
            // Open terminal
            break;
            
        // Help menu
        case MenuCommand::HelpContents:
            // Open help
            break;
        case MenuCommand::HelpKeyboardShortcuts:
            // Show keyboard shortcuts reference
            break;
        case MenuCommand::HelpAbout:
            // Show about dialog
            break;
        case MenuCommand::HelpCheckUpdates:
            // Check for updates
            break;
            
        default:
            break;
    }
    
    renderer->Invalidate();
}

void UCIDEWindow::OnToolbarCommand(MenuCommand cmd) {
    // Most toolbar commands just forward to menu commands
    OnMenuCommand(cmd);
}

void UCIDEWindow::OnFileOpen(const std::string& path) {
    OpenFile(path);
}

void UCIDEWindow::OnTabClose(const std::string& path) {
    // Check for unsaved changes
    auto it = editors.find(path);
    if (it != editors.end() && it->second->IsModified()) {
        // Show save dialog
    }
    
    // Remove editor
    editors.erase(path);
    
    // Update active editor
    const EditorTab* newActiveTab = layout->GetActiveTab();
    if (newActiveTab) {
        auto newIt = editors.find(newActiveTab->filePath);
        if (newIt != editors.end()) {
            activeEditor = newIt->second.get();
            renderer->BindEditor(activeEditor);
        }
    } else {
        activeEditor = nullptr;
        renderer->BindEditor(nullptr);
    }
    
    UpdateWindowTitle();
}

void UCIDEWindow::OnProjectTreeFileOpen(const std::string& path) {
    OpenFile(path);
}

void UCIDEWindow::OnErrorClick(const std::string& file, int line, int column) {
    // Open file at error location
    if (OpenFile(file)) {
        activeEditor->GoToPosition(line - 1, column - 1);
        activeEditor->EnsureCursorVisible();
    }
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void UCIDEWindow::HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) {
    // First, let menu bar handle accelerators
    if (menuBar->HandleKeyPress(keyCode, alt, ctrl, shift)) {
        renderer->Invalidate();
        return;
    }
    
    // Then, let active editor handle the key
    if (activeEditor) {
        if (activeEditor->HandleKeyPress(keyCode, ctrl, shift, alt)) {
            renderer->Invalidate();
            return;
        }
    }
}

void UCIDEWindow::HandleMouseClick(int x, int y, int button, bool isDoubleClick) {
    LayoutPanel panel = layout->GetPanelAtPosition(x, y);
    
    switch (panel) {
        case LayoutPanel::MenuBar:
            if (menuBar->HandleClick(x, y)) {
                renderer->Invalidate();
            }
            break;
            
        case LayoutPanel::Toolbar:
            if (toolbar->HandleClick(x, y)) {
                renderer->Invalidate();
            }
            break;
            
        case LayoutPanel::ProjectTree:
            {
                LayoutRegion region = layout->GetPanelRegion(LayoutPanel::ProjectTree);
                if (projectTree->HandleClick(x - region.x, y - region.y, isDoubleClick, button == 1)) {
                    renderer->Invalidate();
                }
            }
            break;
            
        case LayoutPanel::EditorTabs:
            if (layout->HandleTabClick(x, y, isDoubleClick, button == 1)) {
                renderer->Invalidate();
            }
            break;
            
        case LayoutPanel::EditorArea:
            if (activeEditor) {
                LayoutRegion region = layout->GetPanelRegion(LayoutPanel::EditorArea);
                if (activeEditor->HandleMouseClick(x - region.x, y - region.y, isDoubleClick, false)) {
                    renderer->Invalidate();
                }
            }
            break;
            
        case LayoutPanel::OutputConsole:
            {
                LayoutRegion region = layout->GetPanelRegion(LayoutPanel::OutputConsole);
                if (outputConsole->HandleClick(x - region.x, y - region.y, isDoubleClick)) {
                    renderer->Invalidate();
                }
            }
            break;
            
        case LayoutPanel::StatusBar:
            if (statusBar->HandleClick(x, y)) {
                renderer->Invalidate();
            }
            break;
    }
    
    // Check for divider drag
    if (layout->HandleMouseDown(x, y, button)) {
        renderer->Invalidate();
    }
}

void UCIDEWindow::HandleMouseMove(int x, int y) {
    // Update hover states
    menuBar->HandleMouseMove(x, y);
    toolbar->HandleMouseMove(x, y);
    statusBar->HandleMouseMove(x, y);
    
    // Handle divider dragging
    if (layout->HandleMouseMove(x, y)) {
        renderer->Invalidate();
    }
    
    // Handle editor mouse drag (for selection)
    LayoutPanel panel = layout->GetPanelAtPosition(x, y);
    if (panel == LayoutPanel::EditorArea && activeEditor) {
        LayoutRegion region = layout->GetPanelRegion(LayoutPanel::EditorArea);
        activeEditor->HandleMouseDrag(x - region.x, y - region.y);
    }
}

void UCIDEWindow::HandleMouseScroll(int delta) {
    // Get panel under mouse (would need mouse position)
    // For now, scroll active editor
    if (activeEditor) {
        activeEditor->HandleMouseScroll(delta);
        renderer->Invalidate();
    }
}

void UCIDEWindow::HandleResize(int newWidth, int newHeight) {
    width = newWidth;
    height = newHeight;
    
    layout->Resize(width, height);
    
    // Update component regions
    menuBar->SetRegion(0, 0, width, layout->GetStyle().menuBarHeight);
    toolbar->SetRegion(0, layout->GetStyle().menuBarHeight, width, layout->GetStyle().toolbarHeight);
    statusBar->SetRegion(0, height - layout->GetStyle().statusBarHeight, 
                         width, layout->GetStyle().statusBarHeight);
    
    // Update renderer
    renderer->Resize(width, height);
    renderer->Invalidate();
}

// ============================================================================
// UTILITIES
// ============================================================================

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
    // SetPlatformWindowTitle(nativeWindow, windowTitle);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string GetLanguageFromExtension(const std::string& ext) {
    std::string lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    
    if (lowerExt == "c") return "C";
    if (lowerExt == "cpp" || lowerExt == "cc" || lowerExt == "cxx") return "C++";
    if (lowerExt == "h") return "C Header";
    if (lowerExt == "hpp" || lowerExt == "hxx") return "C++ Header";
    if (lowerExt == "pas" || lowerExt == "pp") return "Pascal";
    if (lowerExt == "lpr" || lowerExt == "dpr") return "Pascal Project";
    if (lowerExt == "py") return "Python";
    if (lowerExt == "js") return "JavaScript";
    if (lowerExt == "ts") return "TypeScript";
    if (lowerExt == "rs") return "Rust";
    if (lowerExt == "cmake") return "CMake";
    if (lowerExt == "json") return "JSON";
    if (lowerExt == "xml") return "XML";
    if (lowerExt == "html" || lowerExt == "htm") return "HTML";
    if (lowerExt == "css") return "CSS";
    if (lowerExt == "md" || lowerExt == "markdown") return "Markdown";
    if (lowerExt == "txt") return "Plain Text";
    
    return "Unknown";
}

} // namespace IDE
} // namespace UltraCanvas
