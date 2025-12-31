// Apps/CoderBox/UI/UCCoderBoxApplication_Part3.cpp
// CoderBox Application - File Operations, Build, Event Handlers
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#include "UCCoderBoxApplication.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace UltraCanvas {
namespace CoderBox {

// ============================================================================
// FILE OPERATIONS
// ============================================================================

void UCCoderBoxApplication::NewFile() {
    // Create untitled file
    static int untitledCount = 1;
    std::string fileName = "Untitled-" + std::to_string(untitledCount++);
    std::string filePath = "untitled://" + fileName;
    
    // Create new editor
    auto editor = CreateEditor(filePath);
    if (!editor) return;
    
    // Add to editor tabs
    int tabIndex = editorTabs->AddTab(fileName);
    editorTabs->SetTabContent(tabIndex, editor);
    editorTabs->SetActiveTab(tabIndex);
    
    // Track in open editors
    EditorTabInfo info;
    info.filePath = filePath;
    info.fileName = fileName;
    info.isModified = false;
    info.editor = editor;
    openEditors[filePath] = info;
    
    activeFilePath = filePath;
    
    SetStatus("New file created");
    SetLanguageMode("Plain Text");
    SetCursorPosition(1, 1);
    
    std::cout << "CoderBox: New file created: " << fileName << std::endl;
}

void UCCoderBoxApplication::OpenFile(const std::string& filePath) {
    if (filePath.empty()) {
        // TODO: Show file dialog
        std::cout << "CoderBox: File dialog not yet implemented" << std::endl;
        return;
    }
    
    // Check if already open
    if (IsFileOpen(filePath)) {
        // Switch to existing tab
        int tabIndex = 0;
        for (int i = 0; i < editorTabs->GetTabCount(); i++) {
            if (editorTabs->GetTabTitle(i) == std::filesystem::path(filePath).filename().string()) {
                editorTabs->SetActiveTab(i);
                activeFilePath = filePath;
                return;
            }
        }
    }
    
    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "CoderBox: Failed to open file: " << filePath << std::endl;
        SetStatus("Failed to open file");
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string content = buffer.str();
    std::string fileName = std::filesystem::path(filePath).filename().string();
    
    // Create editor
    auto editor = CreateEditor(filePath);
    if (!editor) return;
    
    editor->SetText(content);
    
    // Add to editor tabs
    int tabIndex = editorTabs->AddTab(fileName);
    editorTabs->SetTabContent(tabIndex, editor);
    editorTabs->SetActiveTab(tabIndex);
    
    // Track in open editors
    EditorTabInfo info;
    info.filePath = filePath;
    info.fileName = fileName;
    info.isModified = false;
    info.editor = editor;
    openEditors[filePath] = info;
    
    activeFilePath = filePath;
    
    // Update status
    std::string extension = std::filesystem::path(filePath).extension().string();
    SetLanguageMode(GetLanguageFromExtension(extension));
    SetStatus("Opened: " + fileName);
    SetCursorPosition(1, 1);
    
    if (onFileOpened) {
        onFileOpened(filePath);
    }
    
    std::cout << "CoderBox: Opened file: " << filePath << std::endl;
}

void UCCoderBoxApplication::SaveFile() {
    if (activeFilePath.empty()) return;
    
    // Check if it's an untitled file
    if (activeFilePath.find("untitled://") == 0) {
        SaveFileAs();
        return;
    }
    
    auto it = openEditors.find(activeFilePath);
    if (it == openEditors.end() || !it->second.editor) return;
    
    // Get content from editor
    std::string content = it->second.editor->GetText();
    
    // Write to file
    std::ofstream file(activeFilePath);
    if (!file.is_open()) {
        std::cerr << "CoderBox: Failed to save file: " << activeFilePath << std::endl;
        SetStatus("Failed to save file");
        return;
    }
    
    file << content;
    file.close();
    
    // Update state
    it->second.isModified = false;
    UpdateEditorTabTitle(activeFilePath);
    
    SetStatus("Saved: " + it->second.fileName);
    
    if (onFileSaved) {
        onFileSaved(activeFilePath);
    }
    
    std::cout << "CoderBox: Saved file: " << activeFilePath << std::endl;
}

void UCCoderBoxApplication::SaveFileAs() {
    // TODO: Show save dialog
    std::cout << "CoderBox: Save As dialog not yet implemented" << std::endl;
}

void UCCoderBoxApplication::SaveAllFiles() {
    for (auto& pair : openEditors) {
        if (pair.second.isModified) {
            std::string previousActive = activeFilePath;
            activeFilePath = pair.first;
            SaveFile();
            activeFilePath = previousActive;
        }
    }
    SetStatus("All files saved");
}

void UCCoderBoxApplication::CloseFile() {
    CloseFile(activeFilePath);
}

void UCCoderBoxApplication::CloseFile(const std::string& filePath) {
    auto it = openEditors.find(filePath);
    if (it == openEditors.end()) return;
    
    // Check for unsaved changes
    if (it->second.isModified) {
        // TODO: Show confirmation dialog
        std::cout << "CoderBox: Unsaved changes in " << it->second.fileName << std::endl;
    }
    
    // Find and remove tab
    for (int i = 0; i < editorTabs->GetTabCount(); i++) {
        // Match by tab content
        auto tabContent = editorTabs->GetTabContent(i);
        if (tabContent.get() == it->second.editor.get()) {
            editorTabs->RemoveTab(i);
            break;
        }
    }
    
    // Remove from tracking
    openEditors.erase(it);
    
    // Update active file
    if (filePath == activeFilePath) {
        if (editorTabs->GetTabCount() > 0) {
            int activeIdx = editorTabs->GetActiveTabIndex();
            // Find the file path for the now-active tab
            for (auto& pair : openEditors) {
                auto tabContent = editorTabs->GetTabContent(activeIdx);
                if (tabContent.get() == pair.second.editor.get()) {
                    activeFilePath = pair.first;
                    break;
                }
            }
        } else {
            activeFilePath.clear();
            SetLanguageMode("Plain Text");
        }
    }
    
    if (onFileClosed) {
        onFileClosed(filePath);
    }
    
    std::cout << "CoderBox: Closed file: " << filePath << std::endl;
}

bool UCCoderBoxApplication::IsFileOpen(const std::string& filePath) const {
    return openEditors.find(filePath) != openEditors.end();
}

std::shared_ptr<UltraCanvasTextArea> UCCoderBoxApplication::GetActiveEditor() {
    auto it = openEditors.find(activeFilePath);
    if (it != openEditors.end()) {
        return it->second.editor;
    }
    return nullptr;
}

// ============================================================================
// PROJECT OPERATIONS
// ============================================================================

void UCCoderBoxApplication::NewProject() {
    // TODO: Show new project dialog
    std::cout << "CoderBox: New Project dialog not yet implemented" << std::endl;
}

void UCCoderBoxApplication::OpenProject(const std::string& projectPath) {
    if (projectPath.empty()) {
        // TODO: Show project open dialog
        std::cout << "CoderBox: Open Project dialog not yet implemented" << std::endl;
        return;
    }
    
    // TODO: Load project from .ucproj file
    std::cout << "CoderBox: Opening project: " << projectPath << std::endl;
    
    RefreshProjectTree();
    
    if (onProjectOpened && currentProject) {
        onProjectOpened(currentProject);
    }
}

void UCCoderBoxApplication::CloseProject() {
    if (!currentProject) return;
    
    // Close all open files
    std::vector<std::string> filesToClose;
    for (auto& pair : openEditors) {
        filesToClose.push_back(pair.first);
    }
    for (const auto& filePath : filesToClose) {
        CloseFile(filePath);
    }
    
    currentProject = nullptr;
    
    // Reset project tree
    auto rootNode = std::make_unique<TreeNode>("No Project Open");
    rootNode->data.iconPath = "icons/folder.png";
    projectTree->SetRootNode(std::move(rootNode));
    
    SetStatus("Project closed");
    
    if (onProjectClosed) {
        onProjectClosed();
    }
}

// ============================================================================
// BUILD OPERATIONS
// ============================================================================

void UCCoderBoxApplication::BuildProject() {
    if (isBuildInProgress) {
        std::cout << "CoderBox: Build already in progress" << std::endl;
        return;
    }
    
    isBuildInProgress = true;
    currentErrorCount = 0;
    currentWarningCount = 0;
    errorList.clear();
    
    // Clear output
    ClearOutput();
    SetActiveOutputTab(OutputTabType::Build);
    
    // Update UI
    SetStatus("Building...");
    UpdateToolbarState();
    
    WriteBuildOutput("=== Build Started ===\n");
    
    // TODO: Actual build implementation with UCBuildManager
    // For now, simulate build completion
    WriteBuildOutput("Build command: g++ main.cpp -o output\n");
    WriteBuildOutput("\n=== Build Completed Successfully ===\n");
    
    isBuildInProgress = false;
    SetErrorCounts(currentErrorCount, currentWarningCount);
    SetStatus("Build succeeded");
    UpdateToolbarState();
    
    if (onBuildCompleted) {
        onBuildCompleted(true, currentErrorCount, currentWarningCount);
    }
}

void UCCoderBoxApplication::RebuildProject() {
    CleanProject();
    BuildProject();
}

void UCCoderBoxApplication::CleanProject() {
    ClearOutput();
    WriteBuildOutput("=== Clean Started ===\n");
    WriteBuildOutput("Cleaning build output...\n");
    WriteBuildOutput("=== Clean Completed ===\n");
    SetStatus("Clean completed");
}

void UCCoderBoxApplication::BuildCurrentFile() {
    if (activeFilePath.empty()) {
        SetStatus("No file to build");
        return;
    }
    
    ClearOutput();
    SetActiveOutputTab(OutputTabType::Build);
    WriteBuildOutput("=== Building: " + activeFilePath + " ===\n");
    
    // TODO: Build single file
    
    WriteBuildOutput("=== Build Completed ===\n");
    SetStatus("Build completed");
}

void UCCoderBoxApplication::RunProject() {
    if (isBuildInProgress) {
        std::cout << "CoderBox: Cannot run while build is in progress" << std::endl;
        return;
    }
    
    SetActiveOutputTab(OutputTabType::Application);
    applicationOutput->Clear();
    applicationOutput->AppendText("=== Running Application ===\n");
    
    // TODO: Execute built application
    
    SetStatus("Application running");
}

void UCCoderBoxApplication::DebugProject() {
    SetStatus("Debug mode not yet implemented");
    std::cout << "CoderBox: Debug mode not yet implemented" << std::endl;
}

void UCCoderBoxApplication::StopBuild() {
    if (isBuildInProgress) {
        isBuildInProgress = false;
        WriteBuildOutput("\n=== Build Cancelled ===\n");
        SetStatus("Build cancelled");
        UpdateToolbarState();
    }
}

void UCCoderBoxApplication::GoToNextError() {
    if (errorList.empty()) return;
    
    currentErrorIndex++;
    if (currentErrorIndex >= static_cast<int>(errorList.size())) {
        currentErrorIndex = 0;
    }
    
    auto& error = errorList[currentErrorIndex];
    NavigateToLocation(std::get<0>(error), std::get<1>(error), std::get<2>(error));
    
    SetActiveOutputTab(OutputTabType::Errors);
}

void UCCoderBoxApplication::GoToPreviousError() {
    if (errorList.empty()) return;
    
    currentErrorIndex--;
    if (currentErrorIndex < 0) {
        currentErrorIndex = static_cast<int>(errorList.size()) - 1;
    }
    
    auto& error = errorList[currentErrorIndex];
    NavigateToLocation(std::get<0>(error), std::get<1>(error), std::get<2>(error));
    
    SetActiveOutputTab(OutputTabType::Errors);
}

// ============================================================================
// OUTPUT CONSOLE
// ============================================================================

void UCCoderBoxApplication::WriteBuildOutput(const std::string& text) {
    if (buildOutput) {
        buildOutput->AppendText(text);
    }
}

void UCCoderBoxApplication::WriteError(const std::string& filePath, int line, int column, 
                                   const std::string& message) {
    currentErrorCount++;
    
    std::stringstream ss;
    ss << filePath << ":" << line << ":" << column << ": error: " << message << "\n";
    
    if (errorsOutput) {
        errorsOutput->AppendText(ss.str());
    }
    if (buildOutput) {
        buildOutput->AppendText(ss.str());
    }
    
    // Track error for navigation
    errorList.push_back(std::make_tuple(filePath, line, column, message));
    
    SetErrorCounts(currentErrorCount, currentWarningCount);
    
    // Update tab badge
    outputConsole->SetTabBadge(1, std::to_string(currentErrorCount));
}

void UCCoderBoxApplication::WriteWarning(const std::string& filePath, int line, int column,
                                     const std::string& message) {
    currentWarningCount++;
    
    std::stringstream ss;
    ss << filePath << ":" << line << ":" << column << ": warning: " << message << "\n";
    
    if (warningsOutput) {
        warningsOutput->AppendText(ss.str());
    }
    if (buildOutput) {
        buildOutput->AppendText(ss.str());
    }
    
    SetErrorCounts(currentErrorCount, currentWarningCount);
    
    // Update tab badge
    outputConsole->SetTabBadge(2, std::to_string(currentWarningCount));
}

void UCCoderBoxApplication::ClearOutput() {
    if (buildOutput) buildOutput->Clear();
    if (errorsOutput) errorsOutput->Clear();
    if (warningsOutput) warningsOutput->Clear();
    if (messagesOutput) messagesOutput->Clear();
    
    currentErrorCount = 0;
    currentWarningCount = 0;
    currentErrorIndex = -1;
    errorList.clear();
    
    SetErrorCounts(0, 0);
    
    // Clear badges
    outputConsole->SetTabBadge(1, "0");
    outputConsole->SetTabBadge(2, "0");
}

void UCCoderBoxApplication::SetActiveOutputTab(OutputTabType type) {
    outputConsole->SetActiveTab(static_cast<int>(type));
}

// ============================================================================
// STATUS BAR
// ============================================================================

void UCCoderBoxApplication::SetStatus(const std::string& message) {
    if (statusLabel) {
        statusLabel->SetText(message);
    }
}

void UCCoderBoxApplication::SetCursorPosition(int line, int column) {
    if (positionLabel) {
        std::stringstream ss;
        ss << "Ln " << line << ", Col " << column;
        positionLabel->SetText(ss.str());
    }
}

void UCCoderBoxApplication::SetEncoding(const std::string& encoding) {
    if (encodingLabel) {
        encodingLabel->SetText(encoding);
    }
}

void UCCoderBoxApplication::SetLanguageMode(const std::string& language) {
    if (languageLabel) {
        languageLabel->SetText(language);
    }
}

void UCCoderBoxApplication::SetErrorCounts(int errors, int warnings) {
    if (errorCountLabel) {
        std::stringstream ss;
        ss << "✗ " << errors << "  ⚠ " << warnings;
        errorCountLabel->SetText(ss.str());
        
        // Change status bar color based on errors
        if (errors > 0) {
            statusBar->SetAppearance(ToolbarAppearance{
                .backgroundColor = currentTheme.errorColor
            });
        } else if (warnings > 0) {
            statusBar->SetAppearance(ToolbarAppearance{
                .backgroundColor = currentTheme.warningColor
            });
        } else {
            statusBar->SetAppearance(ToolbarAppearance{
                .backgroundColor = currentTheme.statusBarBackground
            });
        }
    }
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void UCCoderBoxApplication::OnMenuCommand(CoderBoxCommand command) {
    switch (command) {
        // File commands
        case CoderBoxCommand::FileNewProject: NewProject(); break;
        case CoderBoxCommand::FileNewFile: NewFile(); break;
        case CoderBoxCommand::FileOpen: OpenFile(""); break;
        case CoderBoxCommand::FileOpenProject: OpenProject(""); break;
        case CoderBoxCommand::FileSave: SaveFile(); break;
        case CoderBoxCommand::FileSaveAs: SaveFileAs(); break;
        case CoderBoxCommand::FileSaveAll: SaveAllFiles(); break;
        case CoderBoxCommand::FileCloseFile: CloseFile(); break;
        case CoderBoxCommand::FileCloseProject: CloseProject(); break;
        case CoderBoxCommand::FileExit: 
            Shutdown();
            RequestExit();
            break;
            
        // Edit commands - forward to active editor
        case CoderBoxCommand::EditUndo:
            if (auto editor = GetActiveEditor()) editor->Undo();
            break;
        case CoderBoxCommand::EditRedo:
            if (auto editor = GetActiveEditor()) editor->Redo();
            break;
        case CoderBoxCommand::EditCut:
            if (auto editor = GetActiveEditor()) editor->Cut();
            break;
        case CoderBoxCommand::EditCopy:
            if (auto editor = GetActiveEditor()) editor->Copy();
            break;
        case CoderBoxCommand::EditPaste:
            if (auto editor = GetActiveEditor()) editor->Paste();
            break;
        case CoderBoxCommand::EditSelectAll:
            if (auto editor = GetActiveEditor()) editor->SelectAll();
            break;
            
        // Build commands
        case CoderBoxCommand::BuildBuild: BuildProject(); break;
        case CoderBoxCommand::BuildRebuild: RebuildProject(); break;
        case CoderBoxCommand::BuildClean: CleanProject(); break;
        case CoderBoxCommand::BuildBuildFile: BuildCurrentFile(); break;
        case CoderBoxCommand::BuildRun: RunProject(); break;
        case CoderBoxCommand::BuildDebug: DebugProject(); break;
        case CoderBoxCommand::BuildStop: StopBuild(); break;
        case CoderBoxCommand::BuildNextError: GoToNextError(); break;
        case CoderBoxCommand::BuildPreviousError: GoToPreviousError(); break;
            
        // Project commands
        case CoderBoxCommand::ProjectRefresh: RefreshProjectTree(); break;
            
        // Help commands
        case CoderBoxCommand::HelpAbout:
            std::cout << "CoderBox v2.0.0 - Built on UltraCanvas Framework" << std::endl;
            break;
            
        default:
            std::cout << "CoderBox: Unhandled command: " << static_cast<int>(command) << std::endl;
            break;
    }
    
    if (onCommand) {
        onCommand(command);
    }
}

void UCCoderBoxApplication::OnToolbarCommand(CoderBoxCommand command) {
    OnMenuCommand(command);
}

void UCCoderBoxApplication::OnProjectTreeSelect(TreeNode* node) {
    if (!node) return;
    std::cout << "CoderBox: Selected: " << node->data.label << std::endl;
}

void UCCoderBoxApplication::OnProjectTreeDoubleClick(TreeNode* node) {
    if (!node) return;
    
    // If it's a file, open it
    if (!node->HasChildren() && node->data.userData) {
        std::string* filePath = static_cast<std::string*>(node->data.userData);
        OpenFile(*filePath);
    }
}

void UCCoderBoxApplication::OnEditorTabChange(int index) {
    if (index < 0) return;
    
    // Find the file path for this tab
    auto tabContent = editorTabs->GetTabContent(index);
    for (auto& pair : openEditors) {
        if (tabContent.get() == pair.second.editor.get()) {
            activeFilePath = pair.first;
            
            // Update status bar
            std::string extension = std::filesystem::path(pair.first).extension().string();
            SetLanguageMode(GetLanguageFromExtension(extension));
            
            break;
        }
    }
}

void UCCoderBoxApplication::OnEditorTabClose(int index) {
    auto tabContent = editorTabs->GetTabContent(index);
    
    for (auto& pair : openEditors) {
        if (tabContent.get() == pair.second.editor.get()) {
            CloseFile(pair.first);
            break;
        }
    }
}

void UCCoderBoxApplication::OnOutputTabChange(int index) {
    // Could update UI based on active output tab
}

void UCCoderBoxApplication::OnOutputLineClick(const std::string& filePath, int line, int column) {
    NavigateToLocation(filePath, line, column);
}

void UCCoderBoxApplication::OnEditorModified(const std::string& filePath) {
    auto it = openEditors.find(filePath);
    if (it != openEditors.end()) {
        it->second.isModified = true;
        UpdateEditorTabTitle(filePath);
    }
}

void UCCoderBoxApplication::OnEditorCursorMove(int line, int column) {
    SetCursorPosition(line, column);
}

} // namespace CoderBox
} // namespace UltraCanvas
