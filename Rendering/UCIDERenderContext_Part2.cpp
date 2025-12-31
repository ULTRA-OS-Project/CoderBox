// Apps/IDE/Rendering/UCIDERenderContext_Part2.cpp
// UltraCanvas Rendering Implementation Part 2 for ULTRA IDE
// Editor and Tree View rendering

#include "UCIDERenderContext.h"
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

// Helper function to get icon from name
IDEIcon GetIconFromName(const std::string& name) {
    static std::map<std::string, IDEIcon> iconMap = {
        {"file-plus", IDEIcon::FileNew},
        {"folder-open", IDEIcon::FolderOpen},
        {"save", IDEIcon::FileSave},
        {"save-all", IDEIcon::FileSaveAll},
        {"undo", IDEIcon::Undo},
        {"redo", IDEIcon::Redo},
        {"play", IDEIcon::Run},
        {"bug", IDEIcon::Debug},
        {"hammer", IDEIcon::Build},
        {"refresh", IDEIcon::Rebuild},
        {"stop", IDEIcon::Stop},
        {"search", IDEIcon::Search},
        {"folder-search", IDEIcon::SearchInFiles},
        {"error", IDEIcon::Error},
        {"warning", IDEIcon::Warning}
    };
    
    auto it = iconMap.find(name);
    if (it != iconMap.end()) {
        return it->second;
    }
    return IDEIcon::FileUnknown;
}

// ============================================================================
// PROJECT TREE RENDERING
// ============================================================================

void UCIDERenderer::RenderProjectTree() {
    if (!renderContext || !ideLayout || !ideProjectTree) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::ProjectTree)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::ProjectTree);
    
    // Background
    SetFillColor(themeColors.sidebarBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Set clipping
    // renderContext->SetClipRect(region.x, region.y, region.width, region.height);
    
    // Render tree nodes
    const auto& visibleNodes = ideProjectTree->GetVisibleNodes();
    const auto& style = ideProjectTree->GetStyle();
    
    int y = region.y - ideProjectTree->GetScrollOffset() * style.rowHeight;
    int clipBottom = region.y + region.height;
    
    for (const TreeNode* node : visibleNodes) {
        if (y + style.rowHeight >= region.y && y < clipBottom) {
            RenderTreeNode(node, y, clipBottom);
        }
        y += style.rowHeight;
    }
    
    // Clear clipping
    // renderContext->ClearClipRect();
    
    // Render scrollbar if needed
    int contentHeight = ideProjectTree->GetContentHeight();
    if (contentHeight > region.height) {
        RenderScrollbar(region, contentHeight, ideProjectTree->GetScrollOffset() * style.rowHeight);
    }
}

void UCIDERenderer::RenderTreeNode(const TreeNode* node, int& y, int clipBottom) {
    if (!renderContext || !ideLayout || !node) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::ProjectTree);
    const auto& style = ideProjectTree->GetStyle();
    
    int depth = node->GetDepth();
    int indent = depth * style.indentWidth;
    int x = region.x + indent + 4;
    
    // Selection background
    if (node->isSelected) {
        SetFillColor(themeColors.buttonPressed);
        // renderContext->FillRectangle(region.x, y, region.width, style.rowHeight);
    }
    
    // Expand/collapse icon for folders
    if (node->IsFolder() && !node->children.empty()) {
        IDEIcon expandIcon = node->isExpanded ? IDEIcon::ChevronDown : IDEIcon::ChevronRight;
        RenderIcon(expandIcon, x, y + (style.rowHeight - style.iconSize) / 2, style.iconSize, themeColors.textSecondary);
        x += style.iconSize + 2;
    } else {
        x += style.iconSize + 2;
    }
    
    // File/folder icon
    if (style.showIcons) {
        IDEIcon fileIcon;
        if (node->IsFolder()) {
            fileIcon = node->isExpanded ? IDEIcon::FolderOpen : IDEIcon::FolderClosed;
        } else {
            fileIcon = GetFileTypeIcon(GetFileExtension(node->name));
        }
        
        RenderIcon(fileIcon, x, y + (style.rowHeight - style.iconSize) / 2, style.iconSize, themeColors.textPrimary);
        x += style.iconSize + 4;
    }
    
    // Node name
    std::string displayName = node->name;
    if (!style.showFileExtensions && node->IsFile()) {
        size_t dotPos = displayName.rfind('.');
        if (dotPos != std::string::npos) {
            displayName = displayName.substr(0, dotPos);
        }
    }
    
    uint32_t textColor = themeColors.textPrimary;
    if (node->isModified) {
        textColor = themeColors.warningColor;
        displayName += " •";
    }
    
    SetTextColor(textColor);
    // renderContext->DrawText(displayName, x, y + style.rowHeight / 2);
}

// ============================================================================
// EDITOR TABS RENDERING
// ============================================================================

void UCIDERenderer::RenderEditorTabs() {
    if (!renderContext || !ideLayout) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::EditorTabs)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::EditorTabs);
    
    // Background
    SetFillColor(themeColors.tabBarBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Render tabs
    const auto& tabs = ideLayout->GetTabs();
    const auto& style = ideLayout->GetStyle();
    
    for (size_t i = 0; i < tabs.size(); ++i) {
        const EditorTab& tab = tabs[i];
        
        // Tab background
        uint32_t bgColor = tab.isActive ? themeColors.tabActive : themeColors.tabInactive;
        SetFillColor(bgColor);
        // renderContext->FillRectangle(tab.tabRegion.x, tab.tabRegion.y, tab.tabWidth, tab.tabRegion.height);
        
        // Active tab indicator (top border)
        if (tab.isActive) {
            SetFillColor(themeColors.textAccent);
            // renderContext->FillRectangle(tab.tabRegion.x, tab.tabRegion.y, tab.tabWidth, 2);
        }
        
        // File type icon
        IDEIcon fileIcon = GetFileTypeIcon(GetFileExtension(tab.filePath));
        int iconX = tab.tabRegion.x + style.tabPadding;
        int iconY = tab.tabRegion.y + (tab.tabRegion.height - 16) / 2;
        RenderIcon(fileIcon, iconX, iconY, 16, themeColors.textPrimary);
        
        // Tab title
        uint32_t textColor = tab.isActive ? themeColors.textPrimary : themeColors.textSecondary;
        if (tab.isModified) {
            textColor = themeColors.warningColor;
        }
        
        SetTextColor(textColor);
        int textX = iconX + 20;
        // renderContext->DrawText(tab.title, textX, tab.tabRegion.y + tab.tabRegion.height / 2);
        
        // Close button
        uint32_t closeColor = tab.isModified ? themeColors.warningColor : themeColors.textSecondary;
        RenderIcon(IDEIcon::Close, tab.closeButtonRegion.x, tab.closeButtonRegion.y, 
                   style.tabCloseButtonSize, closeColor);
    }
    
    // Bottom border
    SetStrokeColor(themeColors.dividerColor);
    // renderContext->DrawLine(region.x, region.y + region.height - 1, 
    //                        region.x + region.width, region.y + region.height - 1);
}

// ============================================================================
// CODE EDITOR RENDERING
// ============================================================================

void UCIDERenderer::RenderEditor() {
    if (!renderContext || !ideLayout || !ideEditor) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::EditorArea)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::EditorArea);
    
    // Background
    SetFillColor(themeColors.editorBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Render gutter
    RenderEditorGutter(region);
    
    // Render content
    RenderEditorContent(region);
}

void UCIDERenderer::RenderEditorGutter(const LayoutRegion& region) {
    if (!ideEditor) return;
    
    const auto& style = ideEditor->GetStyle();
    
    // Gutter background
    SetFillColor(themeColors.editorGutter);
    // renderContext->FillRectangle(region.x, region.y, style.gutterWidth, region.height);
    
    // Line numbers
    int firstLine, lastLine;
    ideEditor->GetVisibleLines(firstLine, lastLine);
    
    TextPosition cursorPos = ideEditor->GetCursorPosition();
    
    for (int line = firstLine; line <= lastLine && line < ideEditor->GetLineCount(); ++line) {
        int y = region.y + (line - firstLine) * style.lineHeight;
        
        // Line number
        std::string lineNum = std::to_string(line + 1);
        uint32_t lineNumColor = (line == cursorPos.line) ? 
                                themeColors.editorLineNumberActive : 
                                themeColors.editorLineNumber;
        
        SetTextColor(lineNumColor);
        // renderContext->DrawTextInRect(lineNum, region.x, y, style.gutterWidth - 8, style.lineHeight);
        
        // Markers
        const LineData* lineData = ideEditor->GetLineData(line);
        if (lineData) {
            int markerX = region.x + 4;
            int markerY = y + (style.lineHeight - 12) / 2;
            
            if (lineData->hasBreakpoint) {
                RenderIcon(IDEIcon::Breakpoint, markerX, markerY, 12, themeColors.errorColor);
            }
            if (lineData->hasBookmark) {
                RenderIcon(IDEIcon::Bookmark, markerX + 14, markerY, 12, themeColors.infoColor);
            }
            if (lineData->hasError) {
                RenderIcon(IDEIcon::Error, markerX + 28, markerY, 12, themeColors.errorColor);
            }
            if (lineData->hasWarning) {
                RenderIcon(IDEIcon::Warning, markerX + 28, markerY, 12, themeColors.warningColor);
            }
        }
    }
    
    // Gutter border
    SetStrokeColor(themeColors.dividerColor);
    // renderContext->DrawLine(region.x + style.gutterWidth - 1, region.y,
    //                        region.x + style.gutterWidth - 1, region.y + region.height);
}

void UCIDERenderer::RenderEditorContent(const LayoutRegion& region) {
    if (!ideEditor) return;
    
    const auto& style = ideEditor->GetStyle();
    int contentX = region.x + style.gutterWidth + style.leftPadding;
    int contentWidth = region.width - style.gutterWidth - style.leftPadding;
    
    // Set clipping
    // renderContext->SetClipRect(contentX, region.y, contentWidth, region.height);
    
    int firstLine, lastLine;
    ideEditor->GetVisibleLines(firstLine, lastLine);
    
    TextPosition cursorPos = ideEditor->GetCursorPosition();
    
    // Current line highlight
    if (ideEditor->GetSettings().highlightCurrentLine) {
        int cursorY = region.y + (cursorPos.line - firstLine) * style.lineHeight;
        SetFillColor(themeColors.editorLineHighlight);
        // renderContext->FillRectangle(region.x + style.gutterWidth, cursorY, 
        //                             region.width - style.gutterWidth, style.lineHeight);
    }
    
    // Render selection
    RenderEditorSelection();
    
    // Render lines
    for (int line = firstLine; line <= lastLine && line < ideEditor->GetLineCount(); ++line) {
        int y = region.y + (line - firstLine) * style.lineHeight;
        RenderSyntaxHighlightedLine(line, y);
    }
    
    // Render cursor
    if (cursorVisible) {
        RenderEditorCursor();
    }
    
    // Clear clipping
    // renderContext->ClearClipRect();
    
    // Render scrollbars if needed
    int contentHeight = ideEditor->GetLineCount() * style.lineHeight;
    if (contentHeight > region.height) {
        LayoutRegion scrollRegion = {
            region.x + region.width - 14, region.y,
            14, region.height
        };
        RenderScrollbar(scrollRegion, contentHeight, ideEditor->GetScrollY() * style.lineHeight);
    }
}

void UCIDERenderer::RenderEditorSelection() {
    if (!ideEditor || !ideEditor->HasSelection()) return;
    
    const auto& selection = ideEditor->GetSelection();
    const auto& style = ideEditor->GetStyle();
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::EditorArea);
    
    int contentX = region.x + style.gutterWidth + style.leftPadding;
    int scrollX = ideEditor->GetScrollX();
    int scrollY = ideEditor->GetScrollY();
    
    TextSelection sel = selection;
    if (sel.end < sel.start) {
        std::swap(sel.start, sel.end);
    }
    
    SetFillColor(themeColors.editorSelection);
    
    // Single line selection
    if (sel.start.line == sel.end.line) {
        int y = region.y + (sel.start.line - scrollY) * style.lineHeight;
        int x1 = contentX + sel.start.column * 8 - scrollX;  // Approximate char width
        int x2 = contentX + sel.end.column * 8 - scrollX;
        // renderContext->FillRectangle(x1, y, x2 - x1, style.lineHeight);
    } else {
        // Multi-line selection
        for (int line = sel.start.line; line <= sel.end.line; ++line) {
            int y = region.y + (line - scrollY) * style.lineHeight;
            int lineLen = ideEditor->GetLineText(line).length();
            
            int startCol = (line == sel.start.line) ? sel.start.column : 0;
            int endCol = (line == sel.end.line) ? sel.end.column : lineLen;
            
            int x1 = contentX + startCol * 8 - scrollX;
            int x2 = contentX + endCol * 8 - scrollX;
            // renderContext->FillRectangle(x1, y, x2 - x1, style.lineHeight);
        }
    }
}

void UCIDERenderer::RenderEditorCursor() {
    if (!ideEditor) return;
    
    const auto& style = ideEditor->GetStyle();
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::EditorArea);
    
    TextPosition pos = ideEditor->GetCursorPosition();
    int scrollY = ideEditor->GetScrollY();
    int scrollX = ideEditor->GetScrollX();
    
    int contentX = region.x + style.gutterWidth + style.leftPadding;
    int cursorX = contentX + pos.column * 8 - scrollX;  // Approximate char width
    int cursorY = region.y + (pos.line - scrollY) * style.lineHeight;
    
    SetFillColor(themeColors.editorCursor);
    // renderContext->FillRectangle(cursorX, cursorY, style.cursorWidth, style.lineHeight);
}

void UCIDERenderer::RenderSyntaxHighlightedLine(int lineIndex, int y) {
    if (!ideEditor) return;
    
    const LineData* lineData = ideEditor->GetLineData(lineIndex);
    if (!lineData) return;
    
    const auto& style = ideEditor->GetStyle();
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::EditorArea);
    
    int contentX = region.x + style.gutterWidth + style.leftPadding;
    int scrollX = ideEditor->GetScrollX();
    
    const std::string& text = lineData->text;
    const auto& tokens = lineData->tokens;
    
    if (tokens.empty()) {
        // No syntax highlighting - render as plain text
        SetTextColor(themeColors.editorForeground);
        // renderContext->DrawText(text, contentX - scrollX, y + style.lineHeight / 2);
        return;
    }
    
    // Render each token with appropriate color
    for (const auto& token : tokens) {
        uint32_t color;
        
        switch (token.type) {
            case TokenType::Keyword:
                color = themeColors.syntaxKeyword;
                break;
            case TokenType::Type:
                color = themeColors.syntaxType;
                break;
            case TokenType::String:
            case TokenType::Character:
                color = themeColors.syntaxString;
                break;
            case TokenType::Number:
                color = themeColors.syntaxNumber;
                break;
            case TokenType::Comment:
            case TokenType::MultiLineComment:
                color = themeColors.syntaxComment;
                break;
            case TokenType::Preprocessor:
                color = themeColors.syntaxPreprocessor;
                break;
            case TokenType::Function:
                color = themeColors.syntaxFunction;
                break;
            case TokenType::Variable:
                color = themeColors.syntaxVariable;
                break;
            case TokenType::Operator:
            case TokenType::Punctuation:
                color = themeColors.syntaxOperator;
                break;
            default:
                color = themeColors.editorForeground;
                break;
        }
        
        std::string tokenText = text.substr(token.startColumn, token.length);
        int tokenX = contentX + token.startColumn * 8 - scrollX;  // Approximate char width
        
        SetTextColor(color);
        // renderContext->DrawText(tokenText, tokenX, y + style.lineHeight / 2);
    }
    
    // Render error/warning underlines
    if (lineData->hasError) {
        int errorX = contentX - scrollX;
        int errorY = y + style.lineHeight - 2;
        SetStrokeColor(themeColors.errorColor);
        // Draw wavy underline
    }
    if (lineData->hasWarning) {
        int warnX = contentX - scrollX;
        int warnY = y + style.lineHeight - 2;
        SetStrokeColor(themeColors.warningColor);
        // Draw wavy underline
    }
}

// ============================================================================
// OUTPUT CONSOLE RENDERING
// ============================================================================

void UCIDERenderer::RenderOutputConsole() {
    if (!renderContext || !ideLayout || !ideOutputConsole) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::OutputConsole)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::OutputConsole);
    const auto& style = ideOutputConsole->GetStyle();
    
    // Background
    SetFillColor(themeColors.panelBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Tab bar
    SetFillColor(themeColors.tabBarBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, style.tabBarHeight);
    
    // Render tabs
    const auto& tabs = ideOutputConsole->GetTabs();
    int activeTabIndex = ideOutputConsole->GetActiveTabIndex();
    
    int tabX = region.x;
    for (size_t i = 0; i < tabs.size(); ++i) {
        const OutputTab& tab = tabs[i];
        bool isActive = (static_cast<int>(i) == activeTabIndex);
        
        int tabWidth = static_cast<int>(tab.title.length()) * 8 + style.tabPadding * 2;
        
        // Add badge width
        if (tab.showBadge && tab.badgeCount > 0) {
            tabWidth += style.badgeSize + 4;
        }
        
        RenderOutputTab(tab, tabX, region.y, tabWidth, isActive);
        tabX += tabWidth;
    }
    
    // Render active tab content
    const OutputTab* activeTab = ideOutputConsole->GetActiveTab();
    if (activeTab) {
        int contentY = region.y + style.tabBarHeight;
        int contentHeight = region.height - style.tabBarHeight;
        
        // Content background
        SetFillColor(themeColors.editorBackground);
        // renderContext->FillRectangle(region.x, contentY, region.width, contentHeight);
        
        // Set clipping
        // renderContext->SetClipRect(region.x, contentY, region.width, contentHeight);
        
        // Render lines
        int lineY = contentY - activeTab->scrollOffset * style.lineHeight;
        for (const auto& line : activeTab->lines) {
            if (!activeTab->MatchesFilter(line)) continue;
            
            if (lineY + style.lineHeight >= contentY && lineY < region.y + region.height) {
                RenderOutputLine(line, region.x + style.leftMargin, lineY, region.width);
            }
            lineY += style.lineHeight;
        }
        
        // Clear clipping
        // renderContext->ClearClipRect();
    }
}

void UCIDERenderer::RenderOutputTab(const OutputTab& tab, int x, int y, int width, bool isActive) {
    const auto& style = ideOutputConsole->GetStyle();
    
    // Background
    uint32_t bgColor = isActive ? themeColors.tabActive : themeColors.tabInactive;
    SetFillColor(bgColor);
    // renderContext->FillRectangle(x, y, width, style.tabBarHeight);
    
    // Active indicator
    if (isActive) {
        SetFillColor(themeColors.textAccent);
        // renderContext->FillRectangle(x, y + style.tabBarHeight - 2, width, 2);
    }
    
    // Tab text
    uint32_t textColor = isActive ? themeColors.textPrimary : themeColors.textSecondary;
    SetTextColor(textColor);
    // renderContext->DrawText(tab.title, x + style.tabPadding, y + style.tabBarHeight / 2);
    
    // Badge
    if (tab.showBadge && tab.badgeCount > 0) {
        int badgeX = x + width - style.badgeSize - 4;
        int badgeY = y + (style.tabBarHeight - style.badgeSize) / 2;
        
        uint32_t badgeColor = (tab.type == OutputTabType::Errors) ? 
                              themeColors.errorColor : themeColors.warningColor;
        
        SetFillColor(badgeColor);
        // renderContext->FillCircle(badgeX + style.badgeSize / 2, badgeY + style.badgeSize / 2, 
        //                          style.badgeSize / 2);
        
        SetTextColor(0xFFFFFFFF);
        // renderContext->DrawText(std::to_string(tab.badgeCount), badgeX, badgeY);
    }
}

void UCIDERenderer::RenderOutputLine(const OutputLine& line, int x, int y, int width) {
    const auto& style = ideOutputConsole->GetStyle();
    
    uint32_t textColor;
    switch (line.type) {
        case OutputLineType::Error:
            textColor = themeColors.errorColor;
            break;
        case OutputLineType::Warning:
            textColor = themeColors.warningColor;
            break;
        case OutputLineType::Success:
            textColor = themeColors.successColor;
            break;
        case OutputLineType::Info:
            textColor = themeColors.infoColor;
            break;
        case OutputLineType::Command:
            textColor = themeColors.syntaxKeyword;
            break;
        case OutputLineType::Timestamp:
            textColor = themeColors.textSecondary;
            break;
        default:
            textColor = themeColors.textPrimary;
            break;
    }
    
    // Clickable indicator
    if (line.clickable) {
        SetFillColor(themeColors.textAccent);
        // renderContext->FillCircle(x, y + style.lineHeight / 2, 3);
        x += 10;
    }
    
    SetTextColor(textColor);
    // renderContext->DrawText(line.text, x, y + style.lineHeight / 2);
}

// ============================================================================
// STATUS BAR RENDERING
// ============================================================================

void UCIDERenderer::RenderStatusBar() {
    if (!renderContext || !ideLayout || !ideStatusBar) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::StatusBar)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::StatusBar);
    
    // Background (color depends on state)
    uint32_t bgColor = ideStatusBar->GetCurrentBackgroundColor();
    SetFillColor(bgColor);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Render segments
    const auto& segments = ideStatusBar->GetSegments();
    const auto& style = ideStatusBar->GetStyle();
    
    for (const auto& segment : segments) {
        if (!segment.visible) continue;
        
        // Icon
        if (!segment.iconName.empty()) {
            IDEIcon icon = GetIconFromName(segment.iconName);
            int iconX = segment.x + 4;
            int iconY = region.y + (region.height - style.iconSize) / 2;
            RenderIcon(icon, iconX, iconY, style.iconSize, 0xFFFFFFFF);
        }
        
        // Text
        int textX = segment.x + (segment.iconName.empty() ? 4 : style.iconSize + 8);
        SetTextColor(0xFFFFFFFF);
        // renderContext->DrawText(segment.text, textX, region.y + region.height / 2);
        
        // Separator
        if (segment.computedWidth > 0) {
            SetStrokeColor(0x00000030);  // Semi-transparent black
            // renderContext->DrawLine(segment.x + segment.computedWidth, region.y + 4,
            //                        segment.x + segment.computedWidth, region.y + region.height - 4);
        }
    }
}

// ============================================================================
// DIVIDERS RENDERING
// ============================================================================

void UCIDERenderer::RenderDividers() {
    if (!renderContext || !ideLayout) return;
    
    // Get split panes and render their dividers
    if (auto* mainSplit = ideLayout->GetSplitPane("main_h")) {
        if (mainSplit->dividerVisible) {
            SetFillColor(themeColors.dividerColor);
            // renderContext->FillRectangle(mainSplit->dividerRegion.x, mainSplit->dividerRegion.y,
            //                             mainSplit->dividerRegion.width, mainSplit->dividerRegion.height);
        }
    }
    
    if (auto* editorSplit = ideLayout->GetSplitPane("editor_v")) {
        if (editorSplit->dividerVisible) {
            SetFillColor(themeColors.dividerColor);
            // renderContext->FillRectangle(editorSplit->dividerRegion.x, editorSplit->dividerRegion.y,
            //                             editorSplit->dividerRegion.width, editorSplit->dividerRegion.height);
        }
    }
}

// ============================================================================
// SCROLLBAR RENDERING
// ============================================================================

void UCIDERenderer::RenderScrollbar(const LayoutRegion& region, int contentHeight, int scrollOffset) {
    if (!renderContext || contentHeight <= region.height) return;
    
    // Track
    SetFillColor(themeColors.scrollbarTrack);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Calculate thumb size and position
    float visibleRatio = static_cast<float>(region.height) / contentHeight;
    int thumbHeight = std::max(30, static_cast<int>(region.height * visibleRatio));
    
    float scrollRatio = static_cast<float>(scrollOffset) / (contentHeight - region.height);
    int thumbY = region.y + static_cast<int>(scrollRatio * (region.height - thumbHeight));
    
    // Thumb
    SetFillColor(themeColors.scrollbarThumb);
    // renderContext->FillRoundedRectangle(region.x + 2, thumbY, region.width - 4, thumbHeight, 3);
}

// ============================================================================
// ICON RENDERING
// ============================================================================

void UCIDERenderer::RenderIcon(IDEIcon icon, int x, int y, int size, uint32_t color) {
    if (!renderContext) return;
    
    SetFillColor(color);
    SetStrokeColor(color);
    
    // For now, render simple geometric shapes as icons
    // In production, would use proper icon font or SVG sprites
    
    float cx = x + size / 2.0f;
    float cy = y + size / 2.0f;
    float r = size / 2.0f - 2;
    
    switch (icon) {
        case IDEIcon::FileNew:
        case IDEIcon::FileSource:
        case IDEIcon::FileHeader:
        case IDEIcon::FileText:
            // Rectangle with corner fold
            // renderContext->DrawRectangle(x + 2, y + 1, size - 6, size - 2);
            break;
            
        case IDEIcon::FolderOpen:
        case IDEIcon::FolderClosed:
            // Folder shape
            // renderContext->FillRectangle(x + 1, y + 4, size - 2, size - 6);
            break;
            
        case IDEIcon::Run:
            // Play triangle
            // renderContext->MoveTo(x + 4, y + 2);
            // renderContext->LineTo(x + size - 2, cy);
            // renderContext->LineTo(x + 4, y + size - 2);
            // renderContext->FillPath();
            break;
            
        case IDEIcon::Stop:
            // Square
            // renderContext->FillRectangle(x + 3, y + 3, size - 6, size - 6);
            break;
            
        case IDEIcon::ChevronRight:
            // Right arrow
            // renderContext->MoveTo(x + 4, y + 2);
            // renderContext->LineTo(x + size - 4, cy);
            // renderContext->LineTo(x + 4, y + size - 2);
            // renderContext->StrokePath();
            break;
            
        case IDEIcon::ChevronDown:
            // Down arrow
            // renderContext->MoveTo(x + 2, y + 4);
            // renderContext->LineTo(cx, y + size - 4);
            // renderContext->LineTo(x + size - 2, y + 4);
            // renderContext->StrokePath();
            break;
            
        case IDEIcon::Close:
            // X
            // renderContext->DrawLine(x + 3, y + 3, x + size - 3, y + size - 3);
            // renderContext->DrawLine(x + size - 3, y + 3, x + 3, y + size - 3);
            break;
            
        case IDEIcon::Breakpoint:
            // Filled circle
            // renderContext->FillCircle(cx, cy, r - 1);
            break;
            
        case IDEIcon::Error:
            // Circle with X
            // renderContext->FillCircle(cx, cy, r);
            break;
            
        case IDEIcon::Warning:
            // Triangle
            // renderContext->MoveTo(cx, y + 2);
            // renderContext->LineTo(x + size - 2, y + size - 2);
            // renderContext->LineTo(x + 2, y + size - 2);
            // renderContext->FillPath();
            break;
            
        default:
            // Default: small square
            // renderContext->FillRectangle(x + 2, y + 2, size - 4, size - 4);
            break;
    }
}

IDEIcon UCIDERenderer::GetFileTypeIcon(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "c" || ext == "cpp" || ext == "cc" || ext == "cxx" || 
        ext == "pas" || ext == "pp" || ext == "py" || ext == "rs" ||
        ext == "js" || ext == "ts") {
        return IDEIcon::FileSource;
    }
    if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "hh") {
        return IDEIcon::FileHeader;
    }
    if (ext == "txt" || ext == "md" || ext == "json" || ext == "xml" ||
        ext == "yml" || ext == "yaml" || ext == "cmake") {
        return IDEIcon::FileConfig;
    }
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" ||
        ext == "bmp" || ext == "ico" || ext == "svg") {
        return IDEIcon::FileImage;
    }
    
    return IDEIcon::FileUnknown;
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void UCIDERenderer::SetFillColor(uint32_t color) {
    if (renderContext) {
        // renderContext->SetFillPaint(Color::FromUint32(color));
    }
}

void UCIDERenderer::SetStrokeColor(uint32_t color) {
    if (renderContext) {
        // renderContext->SetStrokePaint(Color::FromUint32(color));
    }
}

void UCIDERenderer::SetTextColor(uint32_t color) {
    if (renderContext) {
        // renderContext->SetTextPaint(Color::FromUint32(color));
    }
}

void UCIDERenderer::DrawText(const std::string& text, int x, int y) {
    if (renderContext) {
        // renderContext->DrawText(text, x, y);
    }
}

void UCIDERenderer::DrawTextInRect(const std::string& text, const Rect2Di& rect, int alignment) {
    if (renderContext) {
        // renderContext->DrawTextInRect(text, rect);
    }
}

} // namespace IDE
} // namespace UltraCanvas
