// Apps/CoderBox/UI/UCCoderBoxApplication_Part4.cpp
// CoderBox Application - Helpers, Theme, Configuration, Factory Functions
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#include "UCCoderBoxApplication.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace UltraCanvas {
namespace CoderBox {

// ============================================================================
// EDITOR CREATION
// ============================================================================

std::shared_ptr<UltraCanvasTextArea> UCCoderBoxApplication::CreateEditor(const std::string& filePath) {
    auto editor = std::make_shared<UltraCanvasTextArea>(
        "Editor_" + std::to_string(GetNextId()), GetNextId(),
        0, 0, 800, 600  // Will be resized by tab container
    );
    
    // Apply editor settings
    editor->SetBackgroundColor(currentTheme.editorBackground);
    editor->SetTextColor(currentTheme.textColor);
    editor->SetShowLineNumbers(configuration.showLineNumbers);
    editor->SetWordWrap(configuration.wordWrap);
    editor->SetTabSize(configuration.tabSize);
    editor->SetInsertSpaces(configuration.insertSpaces);
    
    // Set font
    editor->SetFontFamily("Consolas");
    editor->SetFontSize(13);
    
    // Set up syntax highlighting based on file extension
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::string language = GetLanguageFromExtension(extension);
    
    // TODO: Apply syntax tokenizer based on language
    // editor->SetSyntaxTokenizer(GetTokenizerForLanguage(language));
    
    // Set up editor callbacks
    editor->onTextChanged = [this, filePath]() {
        OnEditorModified(filePath);
    };
    
    editor->onCursorPositionChanged = [this](int line, int column) {
        OnEditorCursorMove(line, column);
    };
    
    return editor;
}

void UCCoderBoxApplication::UpdateEditorTabTitle(const std::string& filePath) {
    auto it = openEditors.find(filePath);
    if (it == openEditors.end()) return;
    
    std::string title = it->second.fileName;
    if (it->second.isModified) {
        title = "• " + title;
    }
    
    // Find the tab and update title
    for (int i = 0; i < editorTabs->GetTabCount(); i++) {
        auto tabContent = editorTabs->GetTabContent(i);
        if (tabContent.get() == it->second.editor.get()) {
            editorTabs->SetTabTitle(i, title);
            break;
        }
    }
}

void UCCoderBoxApplication::NavigateToLocation(const std::string& filePath, int line, int column) {
    // Open file if not already open
    if (!IsFileOpen(filePath)) {
        OpenFile(filePath);
    } else {
        // Switch to the file's tab
        for (int i = 0; i < editorTabs->GetTabCount(); i++) {
            auto it = openEditors.find(filePath);
            if (it != openEditors.end()) {
                auto tabContent = editorTabs->GetTabContent(i);
                if (tabContent.get() == it->second.editor.get()) {
                    editorTabs->SetActiveTab(i);
                    break;
                }
            }
        }
    }
    
    // Navigate to line/column
    auto it = openEditors.find(filePath);
    if (it != openEditors.end() && it->second.editor) {
        it->second.editor->GoToLine(line);
        it->second.editor->SetCursorPosition(line, column);
        it->second.editor->CenterOnCursor();
    }
    
    SetCursorPosition(line, column);
}

std::string UCCoderBoxApplication::GetLanguageFromExtension(const std::string& extension) {
    // Convert to lowercase
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Language mappings
    static const std::map<std::string, std::string> languageMap = {
        // C/C++
        {".c", "C"},
        {".h", "C"},
        {".cpp", "C++"},
        {".cxx", "C++"},
        {".cc", "C++"},
        {".hpp", "C++"},
        {".hxx", "C++"},
        {".c++", "C++"},
        
        // Pascal
        {".pas", "Pascal"},
        {".pp", "Pascal"},
        {".p", "Pascal"},
        {".lpr", "Pascal"},
        {".dpr", "Delphi"},
        
        // Other languages
        {".py", "Python"},
        {".java", "Java"},
        {".js", "JavaScript"},
        {".ts", "TypeScript"},
        {".rs", "Rust"},
        {".go", "Go"},
        {".rb", "Ruby"},
        {".php", "PHP"},
        {".cs", "C#"},
        {".swift", "Swift"},
        {".kt", "Kotlin"},
        
        // Web
        {".html", "HTML"},
        {".htm", "HTML"},
        {".css", "CSS"},
        {".scss", "SCSS"},
        {".less", "LESS"},
        {".xml", "XML"},
        {".json", "JSON"},
        {".yaml", "YAML"},
        {".yml", "YAML"},
        
        // Shell/Scripts
        {".sh", "Shell"},
        {".bash", "Bash"},
        {".zsh", "Zsh"},
        {".bat", "Batch"},
        {".cmd", "Batch"},
        {".ps1", "PowerShell"},
        
        // Build systems
        {".cmake", "CMake"},
        {".make", "Makefile"},
        {".mk", "Makefile"},
        
        // Config
        {".ini", "INI"},
        {".toml", "TOML"},
        {".conf", "Config"},
        
        // Documentation
        {".md", "Markdown"},
        {".rst", "reStructuredText"},
        {".txt", "Plain Text"},
        {".log", "Log"},
        
        // Data
        {".sql", "SQL"},
        {".lua", "Lua"},
        {".r", "R"},
        {".m", "MATLAB"}
    };
    
    auto it = languageMap.find(ext);
    if (it != languageMap.end()) {
        return it->second;
    }
    
    return "Plain Text";
}

// ============================================================================
// MENU/TOOLBAR STATE
// ============================================================================

void UCCoderBoxApplication::UpdateMenuState() {
    // Enable/disable menu items based on current state
    bool hasProject = (currentProject != nullptr);
    bool hasFile = !activeFilePath.empty();
    bool hasEditor = GetActiveEditor() != nullptr;
    bool isBuilding = isBuildInProgress;
    
    // TODO: Update menu item states when UltraCanvasMenu supports it
}

void UCCoderBoxApplication::UpdateToolbarState() {
    if (!toolbar) return;
    
    bool hasProject = (currentProject != nullptr);
    bool hasFile = !activeFilePath.empty();
    bool hasEditor = GetActiveEditor() != nullptr;
    bool isBuilding = isBuildInProgress;
    
    // Update button states
    if (auto* saveBtn = toolbar->GetItem("save")) {
        saveBtn->SetEnabled(hasFile);
    }
    if (auto* undoBtn = toolbar->GetItem("undo")) {
        undoBtn->SetEnabled(hasEditor);
    }
    if (auto* redoBtn = toolbar->GetItem("redo")) {
        redoBtn->SetEnabled(hasEditor);
    }
    if (auto* buildBtn = toolbar->GetItem("build")) {
        buildBtn->SetEnabled(!isBuilding);
    }
    if (auto* rebuildBtn = toolbar->GetItem("rebuild")) {
        rebuildBtn->SetEnabled(!isBuilding);
    }
    if (auto* stopBtn = toolbar->GetItem("stop")) {
        stopBtn->SetEnabled(isBuilding);
    }
    if (auto* runBtn = toolbar->GetItem("run")) {
        runBtn->SetEnabled(!isBuilding);
    }
}

void UCCoderBoxApplication::RefreshProjectTree() {
    if (!projectTree) return;
    
    if (!currentProject) {
        auto rootNode = std::make_unique<TreeNode>("No Project Open");
        rootNode->data.iconPath = "icons/folder.png";
        projectTree->SetRootNode(std::move(rootNode));
        return;
    }
    
    // TODO: Populate project tree from currentProject->rootFolder
    std::cout << "CoderBox: Refreshing project tree" << std::endl;
}

// ============================================================================
// THEME
// ============================================================================

void UCCoderBoxApplication::ApplyTheme(const CoderBoxTheme& theme) {
    currentTheme = theme;
    
    // Apply to main window
    if (mainWindow) {
        mainWindow->SetBackgroundColor(theme.windowBackground);
    }
    
    // Apply to menu bar
    if (menuBar) {
        MenuStyle menuStyle = menuBar->GetStyle();
        menuStyle.backgroundColor = theme.menuBarBackground;
        menuStyle.textColor = theme.textColor;
        menuStyle.hoverColor = theme.hoverColor;
        menuStyle.hoverTextColor = theme.activeTextColor;
        menuBar->SetStyle(menuStyle);
    }
    
    // Apply to toolbar
    if (toolbar) {
        ToolbarAppearance appearance;
        appearance.backgroundColor = theme.toolbarBackground;
        appearance.hoverColor = theme.hoverColor;
        appearance.activeColor = theme.selectedColor;
        toolbar->SetAppearance(appearance);
    }
    
    // Apply to status bar
    if (statusBar) {
        ToolbarAppearance appearance;
        appearance.backgroundColor = theme.statusBarBackground;
        statusBar->SetAppearance(appearance);
    }
    
    // Apply to project tree
    if (projectTree) {
        projectTree->SetBackgroundColor(theme.panelBackground);
        projectTree->SetTextColor(theme.textColor);
        projectTree->SetSelectionColor(theme.selectedColor);
        projectTree->SetHoverColor(theme.hoverColor);
    }
    
    // Apply to editor tabs
    if (editorTabs) {
        editorTabs->SetTabBarColor(theme.tabBarBackground);
        editorTabs->SetActiveTabColor(theme.editorBackground);
        editorTabs->SetInactiveTabColor(theme.tabBarBackground);
        editorTabs->SetActiveTabTextColor(theme.activeTextColor);
        editorTabs->SetInactiveTabTextColor(theme.textColor);
        editorTabs->SetContentAreaColor(theme.editorBackground);
    }
    
    // Apply to output console
    if (outputConsole) {
        outputConsole->SetTabBarColor(theme.panelBackground);
        outputConsole->SetActiveTabColor(theme.editorBackground);
        outputConsole->SetInactiveTabColor(theme.panelBackground);
        outputConsole->SetActiveTabTextColor(theme.activeTextColor);
        outputConsole->SetInactiveTabTextColor(theme.textColor);
        outputConsole->SetContentAreaColor(theme.editorBackground);
    }
    
    // Apply to split panes
    if (mainHorizontalSplit) {
        mainHorizontalSplit->SetSplitterColor(theme.dividerColor);
        mainHorizontalSplit->SetSplitterHoverColor(theme.dividerHoverColor);
    }
    if (editorVerticalSplit) {
        editorVerticalSplit->SetSplitterColor(theme.dividerColor);
        editorVerticalSplit->SetSplitterHoverColor(theme.dividerHoverColor);
    }
    
    // Apply to all open editors
    for (auto& pair : openEditors) {
        if (pair.second.editor) {
            pair.second.editor->SetBackgroundColor(theme.editorBackground);
            pair.second.editor->SetTextColor(theme.textColor);
        }
    }
    
    // Apply to output text areas
    if (buildOutput) {
        buildOutput->SetBackgroundColor(theme.editorBackground);
        buildOutput->SetTextColor(theme.textColor);
    }
    if (errorsOutput) {
        errorsOutput->SetBackgroundColor(theme.editorBackground);
        errorsOutput->SetTextColor(theme.errorColor);
    }
    if (warningsOutput) {
        warningsOutput->SetBackgroundColor(theme.editorBackground);
        warningsOutput->SetTextColor(theme.warningColor);
    }
    if (messagesOutput) {
        messagesOutput->SetBackgroundColor(theme.editorBackground);
        messagesOutput->SetTextColor(theme.infoColor);
    }
    if (applicationOutput) {
        applicationOutput->SetBackgroundColor(theme.editorBackground);
        applicationOutput->SetTextColor(theme.textColor);
    }
    
    // Apply to status bar labels
    Color labelColor = Colors::White;
    if (statusLabel) statusLabel->SetTextColor(labelColor);
    if (positionLabel) positionLabel->SetTextColor(labelColor);
    if (encodingLabel) encodingLabel->SetTextColor(labelColor);
    if (languageLabel) languageLabel->SetTextColor(labelColor);
    if (errorCountLabel) errorCountLabel->SetTextColor(labelColor);
    
    std::cout << "CoderBox: Theme applied" << std::endl;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void UCCoderBoxApplication::SaveConfiguration() {
    // TODO: Save configuration to JSON file
    std::cout << "CoderBox: Configuration saved" << std::endl;
}

void UCCoderBoxApplication::LoadConfiguration() {
    // TODO: Load configuration from JSON file
    std::cout << "CoderBox: Configuration loaded" << std::endl;
}

// ============================================================================
// FACTORY FUNCTIONS
// ============================================================================

std::shared_ptr<UCCoderBoxApplication> CreateCoderBox() {
    return CreateCoderBox(CoderBoxConfiguration());
}

std::shared_ptr<UCCoderBoxApplication> CreateCoderBox(const CoderBoxConfiguration& config) {
    auto ide = std::make_shared<UCCoderBoxApplication>();
    
    if (!ide->Initialize(config)) {
        std::cerr << "CoderBox: Failed to initialize" << std::endl;
        return nullptr;
    }
    
    return ide;
}

} // namespace CoderBox
} // namespace UltraCanvas
