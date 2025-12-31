// Apps/IDE/Rendering/UCIDERenderContext.cpp
// UltraCanvas Rendering Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCIDERenderContext.h"
#include "../Build/UCBuildManager.h"
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// IDE THEME COLORS
// ============================================================================

IDEThemeColors IDEThemeColors::GetDarkTheme() {
    IDEThemeColors theme;
    
    // Editor colors
    theme.editorBackground = 0x1E1E1EFF;
    theme.editorForeground = 0xD4D4D4FF;
    theme.editorLineHighlight = 0x282828FF;
    theme.editorSelection = 0x264F78FF;
    theme.editorCursor = 0xAEAFADFF;
    theme.editorLineNumber = 0x858585FF;
    theme.editorLineNumberActive = 0xC6C6C6FF;
    theme.editorGutter = 0x252526FF;
    
    // Syntax colors (VS Code Dark+ inspired)
    theme.syntaxKeyword = 0x569CD6FF;
    theme.syntaxType = 0x4EC9B0FF;
    theme.syntaxString = 0xCE9178FF;
    theme.syntaxNumber = 0xB5CEA8FF;
    theme.syntaxComment = 0x6A9955FF;
    theme.syntaxPreprocessor = 0xC586C0FF;
    theme.syntaxFunction = 0xDCDCAAFF;
    theme.syntaxVariable = 0x9CDCFEFF;
    theme.syntaxOperator = 0xD4D4D4FF;
    
    // UI colors
    theme.windowBackground = 0x1E1E1EFF;
    theme.panelBackground = 0x252526FF;
    theme.sidebarBackground = 0x252526FF;
    theme.statusBarBackground = 0x007ACCFF;
    theme.tabBarBackground = 0x2D2D2DFF;
    theme.tabActive = 0x1E1E1EFF;
    theme.tabInactive = 0x2D2D2DFF;
    theme.tabHover = 0x3C3C3CFF;
    
    // Control colors
    theme.buttonBackground = 0x3C3C3CFF;
    theme.buttonHover = 0x505050FF;
    theme.buttonPressed = 0x094771FF;
    theme.buttonDisabled = 0x3C3C3CFF;
    theme.inputBackground = 0x3C3C3CFF;
    theme.inputBorder = 0x3C3C3CFF;
    theme.inputFocus = 0x007ACCFF;
    
    // Text colors
    theme.textPrimary = 0xCCCCCCFF;
    theme.textSecondary = 0x808080FF;
    theme.textDisabled = 0x606060FF;
    theme.textAccent = 0x007ACCFF;
    
    // Semantic colors
    theme.errorColor = 0xF14C4CFF;
    theme.warningColor = 0xCCA700FF;
    theme.successColor = 0x4EC9B0FF;
    theme.infoColor = 0x3794FFFF;
    
    // Dividers and borders
    theme.dividerColor = 0x3C3C3CFF;
    theme.borderColor = 0x3C3C3CFF;
    theme.focusBorderColor = 0x007ACCFF;
    
    // Scrollbar
    theme.scrollbarTrack = 0x1E1E1EFF;
    theme.scrollbarThumb = 0x424242FF;
    theme.scrollbarThumbHover = 0x4F4F4FFF;
    
    return theme;
}

IDEThemeColors IDEThemeColors::GetLightTheme() {
    IDEThemeColors theme;
    
    // Editor colors
    theme.editorBackground = 0xFFFFFFFF;
    theme.editorForeground = 0x000000FF;
    theme.editorLineHighlight = 0xF3F3F3FF;
    theme.editorSelection = 0xADD6FFFF;
    theme.editorCursor = 0x000000FF;
    theme.editorLineNumber = 0x237893FF;
    theme.editorLineNumberActive = 0x0B216FFF;
    theme.editorGutter = 0xF3F3F3FF;
    
    // Syntax colors (VS Code Light+ inspired)
    theme.syntaxKeyword = 0x0000FFFF;
    theme.syntaxType = 0x267F99FF;
    theme.syntaxString = 0xA31515FF;
    theme.syntaxNumber = 0x098658FF;
    theme.syntaxComment = 0x008000FF;
    theme.syntaxPreprocessor = 0xAF00DBFF;
    theme.syntaxFunction = 0x795E26FF;
    theme.syntaxVariable = 0x001080FF;
    theme.syntaxOperator = 0x000000FF;
    
    // UI colors
    theme.windowBackground = 0xF3F3F3FF;
    theme.panelBackground = 0xFFFFFFFF;
    theme.sidebarBackground = 0xF3F3F3FF;
    theme.statusBarBackground = 0x007ACCFF;
    theme.tabBarBackground = 0xECECECFF;
    theme.tabActive = 0xFFFFFFFF;
    theme.tabInactive = 0xECECECFF;
    theme.tabHover = 0xE0E0E0FF;
    
    // Control colors
    theme.buttonBackground = 0xDDDDDDFF;
    theme.buttonHover = 0xC0C0C0FF;
    theme.buttonPressed = 0x007ACCFF;
    theme.buttonDisabled = 0xEEEEEEFF;
    theme.inputBackground = 0xFFFFFFFF;
    theme.inputBorder = 0xCECECEFF;
    theme.inputFocus = 0x007ACCFF;
    
    // Text colors
    theme.textPrimary = 0x333333FF;
    theme.textSecondary = 0x717171FF;
    theme.textDisabled = 0xA0A0A0FF;
    theme.textAccent = 0x007ACCFF;
    
    // Semantic colors
    theme.errorColor = 0xE51400FF;
    theme.warningColor = 0xBF8803FF;
    theme.successColor = 0x388A34FF;
    theme.infoColor = 0x1A85FFFF;
    
    // Dividers and borders
    theme.dividerColor = 0xE0E0E0FF;
    theme.borderColor = 0xD0D0D0FF;
    theme.focusBorderColor = 0x007ACCFF;
    
    // Scrollbar
    theme.scrollbarTrack = 0xF3F3F3FF;
    theme.scrollbarThumb = 0xC1C1C1FF;
    theme.scrollbarThumbHover = 0x929292FF;
    
    return theme;
}

IDEThemeColors IDEThemeColors::GetHighContrastTheme() {
    IDEThemeColors theme;
    
    // Editor colors - high contrast
    theme.editorBackground = 0x000000FF;
    theme.editorForeground = 0xFFFFFFFF;
    theme.editorLineHighlight = 0x0D0D0DFF;
    theme.editorSelection = 0x0078D7FF;
    theme.editorCursor = 0xFFFFFFFF;
    theme.editorLineNumber = 0xFFFFFFFF;
    theme.editorLineNumberActive = 0xFFFF00FF;
    theme.editorGutter = 0x000000FF;
    
    // Syntax colors - high contrast
    theme.syntaxKeyword = 0x569CD6FF;
    theme.syntaxType = 0x4FC1FFFF;
    theme.syntaxString = 0xCE9178FF;
    theme.syntaxNumber = 0xB5CEA8FF;
    theme.syntaxComment = 0x7CA668FF;
    theme.syntaxPreprocessor = 0xC586C0FF;
    theme.syntaxFunction = 0xFFFF00FF;
    theme.syntaxVariable = 0x9CDCFEFF;
    theme.syntaxOperator = 0xFFFFFFFF;
    
    // UI colors
    theme.windowBackground = 0x000000FF;
    theme.panelBackground = 0x000000FF;
    theme.sidebarBackground = 0x000000FF;
    theme.statusBarBackground = 0x000000FF;
    theme.tabBarBackground = 0x000000FF;
    theme.tabActive = 0x000000FF;
    theme.tabInactive = 0x000000FF;
    theme.tabHover = 0x333333FF;
    
    // Control colors
    theme.buttonBackground = 0x000000FF;
    theme.buttonHover = 0x0D0D0DFF;
    theme.buttonPressed = 0x0078D7FF;
    theme.buttonDisabled = 0x333333FF;
    theme.inputBackground = 0x000000FF;
    theme.inputBorder = 0xFFFFFFFF;
    theme.inputFocus = 0xF38518FF;
    
    // Text colors
    theme.textPrimary = 0xFFFFFFFF;
    theme.textSecondary = 0xCCCCCCFF;
    theme.textDisabled = 0x666666FF;
    theme.textAccent = 0x3FF23FFF;
    
    // Semantic colors
    theme.errorColor = 0xFF0000FF;
    theme.warningColor = 0xFFFF00FF;
    theme.successColor = 0x00FF00FF;
    theme.infoColor = 0x3FF23FFF;
    
    // Dividers and borders
    theme.dividerColor = 0xFFFFFFFF;
    theme.borderColor = 0xFFFFFFFF;
    theme.focusBorderColor = 0xF38518FF;
    
    // Scrollbar
    theme.scrollbarTrack = 0x000000FF;
    theme.scrollbarThumb = 0x6B6B6BFF;
    theme.scrollbarThumbHover = 0x9E9E9EFF;
    
    return theme;
}

// ============================================================================
// IDE RENDERER IMPLEMENTATION
// ============================================================================

UCIDERenderer::UCIDERenderer() {
    themeColors = IDEThemeColors::GetDarkTheme();
}

UCIDERenderer::~UCIDERenderer() {
}

void UCIDERenderer::Initialize(IRenderContext* context, int width, int height) {
    renderContext = context;
    windowWidth = width;
    windowHeight = height;
    needsFullRedraw = true;
}

void UCIDERenderer::Resize(int width, int height) {
    windowWidth = width;
    windowHeight = height;
    needsFullRedraw = true;
}

void UCIDERenderer::SetTheme(const IDEThemeColors& theme) {
    themeColors = theme;
    needsFullRedraw = true;
}

void UCIDERenderer::ApplyDarkTheme() {
    SetTheme(IDEThemeColors::GetDarkTheme());
}

void UCIDERenderer::ApplyLightTheme() {
    SetTheme(IDEThemeColors::GetLightTheme());
}

void UCIDERenderer::Invalidate() {
    needsFullRedraw = true;
}

void UCIDERenderer::InvalidateRegion(const Rect2Di& region) {
    dirtyRegions.push_back(region);
}

void UCIDERenderer::UpdateCursorBlink(int deltaMs) {
    cursorBlinkTimer += deltaMs;
    if (cursorBlinkTimer >= cursorBlinkRate) {
        cursorBlinkTimer = 0;
        cursorVisible = !cursorVisible;
    }
}

void UCIDERenderer::Render() {
    if (!renderContext) return;
    
    // Clear background
    SetFillColor(themeColors.windowBackground);
    // renderContext->Clear(Color::FromUint32(themeColors.windowBackground));
    
    // Render all components
    RenderMenuBar();
    RenderToolbar();
    RenderProjectTree();
    RenderEditorTabs();
    RenderEditor();
    RenderOutputConsole();
    RenderStatusBar();
    RenderDividers();
    
    needsFullRedraw = false;
    dirtyRegions.clear();
}

void UCIDERenderer::RenderRegion(const Rect2Di& region) {
    // For partial updates - render only affected components
    // This is an optimization for the future
    Render();
}

// ============================================================================
// MENU BAR RENDERING
// ============================================================================

void UCIDERenderer::RenderMenuBar() {
    if (!renderContext || !ideLayout || !ideMenuBar) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::MenuBar)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::MenuBar);
    
    // Background
    SetFillColor(themeColors.panelBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Render menu items
    const auto& menus = ideMenuBar->GetMenus();
    int openMenuIndex = ideMenuBar->GetOpenMenuIndex();
    
    for (size_t i = 0; i < menus.size(); ++i) {
        const Menu& menu = menus[i];
        if (!menu.visible) continue;
        
        int itemX = menu.x;
        int itemWidth = menu.width;
        bool isOpen = (static_cast<int>(i) == openMenuIndex);
        
        // Background
        if (isOpen) {
            SetFillColor(themeColors.buttonPressed);
        } else {
            SetFillColor(themeColors.panelBackground);
        }
        // renderContext->FillRectangle(itemX, region.y, itemWidth, region.height);
        
        // Text (remove & for display)
        std::string displayText = menu.text;
        size_t ampPos = displayText.find('&');
        if (ampPos != std::string::npos) {
            displayText.erase(ampPos, 1);
        }
        
        SetTextColor(themeColors.textPrimary);
        // renderContext->DrawTextInRect(displayText, itemX, region.y, itemWidth, region.height);
        
        // Render dropdown if open
        if (isOpen) {
            RenderDropdownMenu(menu, static_cast<int>(i));
        }
    }
}

void UCIDERenderer::RenderDropdownMenu(const Menu& menu, int menuIndex) {
    if (!renderContext) return;
    
    const auto& menuBarStyle = ideMenuBar->GetStyle();
    
    int dropdownX = menu.x;
    int dropdownY = ideLayout->GetPanelRegion(LayoutPanel::MenuBar).height;
    int dropdownWidth = 200;
    int dropdownHeight = 0;
    
    // Calculate height
    for (const auto& item : menu.items) {
        if (!item.visible) continue;
        if (item.type == MenuItemType::Separator) {
            dropdownHeight += 1;
        } else {
            dropdownHeight += menuBarStyle.dropdownItemHeight;
        }
    }
    dropdownHeight += menuBarStyle.dropdownPadding * 2;
    
    // Background
    SetFillColor(themeColors.panelBackground);
    // renderContext->FillRectangle(dropdownX, dropdownY, dropdownWidth, dropdownHeight);
    
    // Border
    SetStrokeColor(themeColors.borderColor);
    // renderContext->DrawRectangle(dropdownX, dropdownY, dropdownWidth, dropdownHeight);
    
    // Items
    int itemY = dropdownY + menuBarStyle.dropdownPadding;
    for (const auto& item : menu.items) {
        if (!item.visible) continue;
        
        if (item.type == MenuItemType::Separator) {
            SetStrokeColor(themeColors.dividerColor);
            // renderContext->DrawLine(dropdownX + 4, itemY, dropdownX + dropdownWidth - 4, itemY);
            itemY += 1;
        } else {
            RenderMenuItem(item, dropdownX, itemY, dropdownWidth, false);
            itemY += menuBarStyle.dropdownItemHeight;
        }
    }
}

void UCIDERenderer::RenderMenuItem(const MenuItem& item, int x, int y, int width, bool isHovered) {
    if (!renderContext) return;
    
    const auto& style = ideMenuBar->GetStyle();
    
    // Background
    if (isHovered && item.enabled) {
        SetFillColor(themeColors.buttonHover);
        // renderContext->FillRectangle(x, y, width, style.dropdownItemHeight);
    }
    
    // Checkbox indicator
    if (item.type == MenuItemType::Checkbox) {
        if (item.checked) {
            SetTextColor(themeColors.textPrimary);
            // Draw checkmark at x + 4, y + center
        }
    }
    
    // Text
    std::string displayText = item.text;
    size_t ampPos = displayText.find('&');
    if (ampPos != std::string::npos) {
        displayText.erase(ampPos, 1);
    }
    
    uint32_t textColor = item.enabled ? themeColors.textPrimary : themeColors.textDisabled;
    SetTextColor(textColor);
    // renderContext->DrawText(displayText, x + 24, y + style.dropdownItemHeight / 2);
    
    // Shortcut
    if (!item.shortcut.empty()) {
        SetTextColor(themeColors.textSecondary);
        // renderContext->DrawText(item.shortcut, x + width - 60, y + style.dropdownItemHeight / 2);
    }
    
    // Submenu arrow
    if (item.type == MenuItemType::Submenu) {
        RenderIcon(IDEIcon::ChevronRight, x + width - 16, y + (style.dropdownItemHeight - 16) / 2, 16, textColor);
    }
}

// ============================================================================
// TOOLBAR RENDERING
// ============================================================================

void UCIDERenderer::RenderToolbar() {
    if (!renderContext || !ideLayout || !ideToolbar) return;
    if (!ideLayout->IsPanelVisible(LayoutPanel::Toolbar)) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::Toolbar);
    
    // Background
    SetFillColor(themeColors.panelBackground);
    // renderContext->FillRectangle(region.x, region.y, region.width, region.height);
    
    // Render items
    const auto& items = ideToolbar->GetItems();
    const auto& style = ideToolbar->GetStyle();
    
    for (const auto& item : items) {
        if (!item.visible) continue;
        
        int itemY = region.y + (region.height - style.buttonSize) / 2;
        
        switch (item.type) {
            case ToolbarItemType::Button:
                RenderToolbarButton(item, false, false);
                break;
                
            case ToolbarItemType::Separator:
                SetStrokeColor(themeColors.dividerColor);
                // renderContext->DrawLine(item.x, region.y + 4, item.x, region.y + region.height - 4);
                break;
                
            case ToolbarItemType::ComboBox:
                RenderToolbarComboBox(item);
                break;
                
            default:
                break;
        }
    }
}

void UCIDERenderer::RenderToolbarButton(const ToolbarItem& item, bool isHovered, bool isPressed) {
    if (!renderContext || !ideLayout) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::Toolbar);
    const auto& style = ideToolbar->GetStyle();
    
    int buttonY = region.y + (region.height - style.buttonSize) / 2;
    
    // Background
    uint32_t bgColor = style.buttonColor;
    if (isPressed || item.pressed) {
        bgColor = themeColors.buttonPressed;
    } else if (isHovered) {
        bgColor = themeColors.buttonHover;
    }
    
    SetFillColor(bgColor);
    // renderContext->FillRoundedRectangle(item.x, buttonY, style.buttonSize, style.buttonSize, 2);
    
    // Icon
    uint32_t iconColor = item.enabled ? themeColors.textPrimary : themeColors.textDisabled;
    IDEIcon icon = GetIconFromName(item.iconName);
    int iconSize = style.iconSize;
    int iconX = item.x + (style.buttonSize - iconSize) / 2;
    int iconY = buttonY + (style.buttonSize - iconSize) / 2;
    
    RenderIcon(icon, iconX, iconY, iconSize, iconColor);
}

void UCIDERenderer::RenderToolbarComboBox(const ToolbarItem& item) {
    if (!renderContext || !ideLayout) return;
    
    LayoutRegion region = ideLayout->GetPanelRegion(LayoutPanel::Toolbar);
    const auto& style = ideToolbar->GetStyle();
    
    int comboY = region.y + (region.height - style.comboBoxHeight) / 2;
    
    // Background
    SetFillColor(themeColors.inputBackground);
    // renderContext->FillRoundedRectangle(item.x, comboY, style.comboBoxWidth, style.comboBoxHeight, 2);
    
    // Border
    SetStrokeColor(themeColors.inputBorder);
    // renderContext->DrawRoundedRectangle(item.x, comboY, style.comboBoxWidth, style.comboBoxHeight, 2);
    
    // Selected text
    std::string text = "";
    if (!item.options.empty() && item.selectedIndex >= 0 && 
        item.selectedIndex < static_cast<int>(item.options.size())) {
        text = item.options[item.selectedIndex];
    }
    
    SetTextColor(themeColors.textPrimary);
    // renderContext->DrawTextInRect(text, item.x + 4, comboY, style.comboBoxWidth - 20, style.comboBoxHeight);
    
    // Dropdown arrow
    RenderIcon(IDEIcon::ChevronDown, item.x + style.comboBoxWidth - 18, comboY + 3, 14, themeColors.textSecondary);
}
