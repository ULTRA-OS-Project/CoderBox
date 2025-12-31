// Apps/IDE/Debug/UI/UCMemoryPanel.cpp
// Memory Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCMemoryPanel.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace UltraCanvas {
namespace IDE {

UCMemoryPanel::UCMemoryPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    SetBackgroundColor(Color(255, 255, 255));
}

void UCMemoryPanel::Initialize() {
    CreateUI();
}

void UCMemoryPanel::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            RefreshMemory();
        };
        
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            if (newState == DebugSessionState::Inactive || 
                newState == DebugSessionState::Terminated) {
                memoryData_.clear();
                previousData_.clear();
                changedBytes_.clear();
                Invalidate();
            }
        };
    }
}

void UCMemoryPanel::CreateUI() {
    int yOffset = 0;
    
    // Title and address input row
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "memory_title", 0, 8, yOffset + 6, 60, 20);
    titleLabel_->SetText("Address:");
    titleLabel_->SetFontSize(11);
    AddChild(titleLabel_);
    
    addressInput_ = std::make_shared<UltraCanvasTextBox>(
        "address_input", 0, 70, yOffset + 4, 150, 22);
    addressInput_->SetFont("Consolas", 11);
    addressInput_->SetPlaceholder("0x00000000");
    addressInput_->SetOnSubmit([this](const std::string& text) {
        GoToAddressString(text);
    });
    AddChild(addressInput_);
    
    // Format dropdown
    formatDropDown_ = std::make_shared<UltraCanvasDropDown>(
        "format_dropdown", 0, 230, yOffset + 4, 100, 22);
    formatDropDown_->AddItem("Bytes");
    formatDropDown_->AddItem("Words");
    formatDropDown_->AddItem("DWords");
    formatDropDown_->AddItem("QWords");
    formatDropDown_->SetSelectedIndex(0);
    formatDropDown_->SetOnSelectionChange([this](int index, const std::string&) {
        SetDisplayFormat(static_cast<MemoryDisplayFormat>(index));
    });
    AddChild(formatDropDown_);
    yOffset += 30;
    
    // Toolbar
    toolbar_ = std::make_shared<UltraCanvasToolbar>(
        "memory_toolbar", 0, 0, yOffset, GetWidth() - 16, 26);
    toolbar_->SetBackgroundColor(Color(240, 240, 240));
    toolbar_->SetSpacing(2);
    
    auto refreshBtn = std::make_shared<UltraCanvasToolbarButton>("refresh");
    refreshBtn->SetIcon("icons/refresh.png");
    refreshBtn->SetTooltip("Refresh Memory");
    refreshBtn->SetOnClick([this]() { RefreshMemory(); });
    toolbar_->AddButton(refreshBtn);
    
    toolbar_->AddSeparator();
    
    auto goToBtn = std::make_shared<UltraCanvasToolbarButton>("goto");
    goToBtn->SetIcon("icons/goto.png");
    goToBtn->SetTooltip("Go to Address");
    goToBtn->SetOnClick([this]() { addressInput_->Focus(); });
    toolbar_->AddButton(goToBtn);
    
    auto watchBtn = std::make_shared<UltraCanvasToolbarButton>("watch");
    watchBtn->SetIcon("icons/watch.png");
    watchBtn->SetTooltip("Add Watchpoint at Selection");
    watchBtn->SetOnClick([this]() {
        if (HasSelection() && onAddWatchpoint) {
            onAddWatchpoint(selectionStart_);
        }
    });
    toolbar_->AddButton(watchBtn);
    
    toolbar_->AddSeparator();
    
    auto asciiBtn = std::make_shared<UltraCanvasToolbarButton>("ascii");
    asciiBtn->SetIcon("icons/ascii.png");
    asciiBtn->SetTooltip("Toggle ASCII Column");
    asciiBtn->SetToggle(true);
    asciiBtn->SetPressed(true);
    asciiBtn->SetOnClick([this]() { SetShowASCII(!showASCII_); });
    toolbar_->AddButton(asciiBtn);
    
    AddChild(toolbar_);
    yOffset += 28;
    
    headerHeight_ = yOffset + 2;
    
    // Scroll bar
    scrollBar_ = std::make_shared<UltraCanvasScrollBar>(
        "memory_scroll", 0, GetWidth() - 16, headerHeight_, 16, GetHeight() - headerHeight_);
    scrollBar_->SetOrientation(ScrollBarOrientation::Vertical);
    scrollBar_->SetOnScroll([this](int position) {
        currentAddress_ = position * bytesPerRow_;
        RefreshMemory();
    });
    AddChild(scrollBar_);
}

void UCMemoryPanel::GoToAddress(uint64_t address) {
    // Align to row boundary
    currentAddress_ = (address / bytesPerRow_) * bytesPerRow_;
    
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << address;
    addressInput_->SetText(ss.str());
    
    RefreshMemory();
    
    if (onAddressChanged) {
        onAddressChanged(currentAddress_);
    }
}

void UCMemoryPanel::GoToAddressString(const std::string& addressExpr) {
    uint64_t address = 0;
    
    try {
        std::string expr = addressExpr;
        
        // Remove 0x prefix if present
        if (expr.length() > 2 && (expr.substr(0, 2) == "0x" || expr.substr(0, 2) == "0X")) {
            expr = expr.substr(2);
        }
        
        address = std::stoull(expr, nullptr, 16);
    } catch (...) {
        // Try to evaluate as expression
        if (debugManager_) {
            auto result = debugManager_->Evaluate(addressExpr);
            if (result.success) {
                try {
                    std::string value = result.variable.value;
                    if (value.substr(0, 2) == "0x") {
                        value = value.substr(2);
                    }
                    address = std::stoull(value, nullptr, 16);
                } catch (...) {
                    return;
                }
            }
        }
    }
    
    GoToAddress(address);
}

void UCMemoryPanel::ScrollUp(int lines) {
    if (currentAddress_ >= static_cast<uint64_t>(lines * bytesPerRow_)) {
        currentAddress_ -= lines * bytesPerRow_;
        RefreshMemory();
    }
}

void UCMemoryPanel::ScrollDown(int lines) {
    currentAddress_ += lines * bytesPerRow_;
    RefreshMemory();
}

void UCMemoryPanel::PageUp() {
    ScrollUp(GetVisibleRows());
}

void UCMemoryPanel::PageDown() {
    ScrollDown(GetVisibleRows());
}

void UCMemoryPanel::RefreshMemory() {
    if (!debugManager_ || !debugManager_->IsDebugging()) {
        return;
    }
    
    // Save previous data for change detection
    previousData_ = memoryData_;
    
    // Read memory
    memoryData_ = debugManager_->ReadMemory(currentAddress_, memorySize_);
    
    // Detect changes
    changedBytes_.clear();
    if (previousData_.size() == memoryData_.size()) {
        for (size_t i = 0; i < memoryData_.size(); i++) {
            if (memoryData_[i] != previousData_[i]) {
                changedBytes_.insert(currentAddress_ + i);
            }
        }
    }
    
    UpdateScrollBar();
    Invalidate();
}

void UCMemoryPanel::SetMemoryData(uint64_t address, const std::vector<uint8_t>& data) {
    if (address == currentAddress_) {
        previousData_ = memoryData_;
        memoryData_ = data;
        
        changedBytes_.clear();
        if (previousData_.size() == memoryData_.size()) {
            for (size_t i = 0; i < memoryData_.size(); i++) {
                if (memoryData_[i] != previousData_[i]) {
                    changedBytes_.insert(address + i);
                }
            }
        }
        
        Invalidate();
    }
}

void UCMemoryPanel::SetDisplayFormat(MemoryDisplayFormat format) {
    displayFormat_ = format;
    
    // Adjust cell width based on format
    switch (format) {
        case MemoryDisplayFormat::Bytes:
            cellWidth_ = 24;
            break;
        case MemoryDisplayFormat::Words:
            cellWidth_ = 44;
            break;
        case MemoryDisplayFormat::DWords:
            cellWidth_ = 80;
            break;
        case MemoryDisplayFormat::QWords:
            cellWidth_ = 150;
            break;
    }
    
    Invalidate();
}

void UCMemoryPanel::SetDataType(MemoryDataType type) {
    dataType_ = type;
    Invalidate();
}

void UCMemoryPanel::SetBytesPerRow(int bytes) {
    bytesPerRow_ = bytes;
    RefreshMemory();
}

void UCMemoryPanel::SetSelection(uint64_t startAddress, uint64_t length) {
    selectionStart_ = startAddress;
    selectionLength_ = length;
    
    if (onSelectionChanged) {
        onSelectionChanged(startAddress);
    }
    
    Invalidate();
}

void UCMemoryPanel::ClearSelection() {
    selectionStart_ = 0;
    selectionLength_ = 0;
    Invalidate();
}

std::vector<uint8_t> UCMemoryPanel::GetSelectedData() const {
    if (!HasSelection()) return {};
    
    std::vector<uint8_t> result;
    
    uint64_t startOffset = selectionStart_ - currentAddress_;
    for (uint64_t i = 0; i < selectionLength_; i++) {
        uint64_t offset = startOffset + i;
        if (offset < memoryData_.size()) {
            result.push_back(memoryData_[offset]);
        }
    }
    
    return result;
}

void UCMemoryPanel::BeginEdit(uint64_t address) {
    isEditing_ = true;
    editAddress_ = address;
    editBuffer_.clear();
    editNibble_ = 0;
    Invalidate();
}

void UCMemoryPanel::CommitEdit() {
    if (!isEditing_ || editBuffer_.empty()) {
        CancelEdit();
        return;
    }
    
    // Parse edit buffer as hex
    std::vector<uint8_t> newData;
    for (size_t i = 0; i < editBuffer_.length(); i += 2) {
        std::string byteStr = editBuffer_.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        newData.push_back(byte);
    }
    
    WriteMemory(editAddress_, newData);
    
    isEditing_ = false;
    editBuffer_.clear();
    Invalidate();
}

void UCMemoryPanel::CancelEdit() {
    isEditing_ = false;
    editBuffer_.clear();
    Invalidate();
}

void UCMemoryPanel::WriteMemory(uint64_t address, const std::vector<uint8_t>& data) {
    if (!debugManager_) return;
    
    debugManager_->WriteMemory(address, data);
    
    if (onMemoryEdited) {
        onMemoryEdited(address, data);
    }
    
    RefreshMemory();
}

void UCMemoryPanel::WriteByte(uint64_t address, uint8_t value) {
    WriteMemory(address, {value});
}

void UCMemoryPanel::AddWatchedAddress(uint64_t address, int size) {
    for (int i = 0; i < size; i++) {
        watchedAddresses_.insert(address + i);
    }
    Invalidate();
}

void UCMemoryPanel::RemoveWatchedAddress(uint64_t address) {
    watchedAddresses_.erase(address);
    Invalidate();
}

void UCMemoryPanel::ClearWatchedAddresses() {
    watchedAddresses_.clear();
    Invalidate();
}

int UCMemoryPanel::GetVisibleRows() const {
    return (GetHeight() - headerHeight_) / cellHeight_;
}

void UCMemoryPanel::UpdateScrollBar() {
    int totalRows = memorySize_ / bytesPerRow_;
    int visibleRows = GetVisibleRows();
    
    scrollBar_->SetRange(0, totalRows);
    scrollBar_->SetPageSize(visibleRows);
    scrollBar_->SetPosition(static_cast<int>(currentAddress_ / bytesPerRow_));
}

void UCMemoryPanel::HandleRender(IRenderContext* ctx) {
    // Render base container
    UltraCanvasContainer::HandleRender(ctx);
    
    // Render memory view
    RenderMemoryView(ctx);
}

void UCMemoryPanel::RenderMemoryView(IRenderContext* ctx) {
    int startY = GetY() + headerHeight_;
    int startX = GetX() + leftMargin_;
    int visibleRows = GetVisibleRows();
    
    ctx->SetFont("Consolas", 11);
    
    // Column headers
    int headerY = startY;
    
    if (showAddresses_) {
        ctx->DrawText("Address", Point2Di(startX, headerY), Color(100, 100, 100));
    }
    
    // Hex column headers
    int hexStartX = startX + (showAddresses_ ? addressWidth_ : 0);
    for (int i = 0; i < bytesPerRow_; i++) {
        std::ostringstream ss;
        ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << i;
        ctx->DrawText(ss.str(), Point2Di(hexStartX + i * cellWidth_, headerY), Color(100, 100, 100));
    }
    
    // ASCII header
    if (showASCII_) {
        int asciiStartX = hexStartX + bytesPerRow_ * cellWidth_ + 16;
        ctx->DrawText("ASCII", Point2Di(asciiStartX, headerY), Color(100, 100, 100));
    }
    
    startY += cellHeight_;
    
    // Draw separator line
    ctx->DrawLine(
        Point2Di(GetX(), startY - 2),
        Point2Di(GetX() + GetWidth() - 16, startY - 2),
        Color(200, 200, 200), 1);
    
    // Render rows
    for (int row = 0; row < visibleRows; row++) {
        int y = startY + row * cellHeight_;
        uint64_t rowAddress = currentAddress_ + row * bytesPerRow_;
        
        if (showAddresses_) {
            RenderAddressColumn(ctx, startX, y, rowAddress);
        }
        
        RenderHexColumn(ctx, hexStartX, y, row);
        
        if (showASCII_) {
            int asciiX = hexStartX + bytesPerRow_ * cellWidth_ + 16;
            RenderASCIIColumn(ctx, asciiX, y, row);
        }
    }
}

void UCMemoryPanel::RenderAddressColumn(IRenderContext* ctx, int x, int y, uint64_t address) {
    std::string addrStr = FormatAddress(address);
    ctx->DrawText(addrStr, Point2Di(x, y), Color(0, 0, 128));
}

void UCMemoryPanel::RenderHexColumn(IRenderContext* ctx, int x, int y, int row) {
    int dataOffset = row * bytesPerRow_;
    
    int cellSize = 1;
    switch (displayFormat_) {
        case MemoryDisplayFormat::Bytes: cellSize = 1; break;
        case MemoryDisplayFormat::Words: cellSize = 2; break;
        case MemoryDisplayFormat::DWords: cellSize = 4; break;
        case MemoryDisplayFormat::QWords: cellSize = 8; break;
    }
    
    for (int i = 0; i < bytesPerRow_; i += cellSize) {
        int offset = dataOffset + i;
        uint64_t address = currentAddress_ + offset;
        
        if (offset >= static_cast<int>(memoryData_.size())) {
            ctx->DrawText("??", Point2Di(x + (i / cellSize) * cellWidth_, y), Color(200, 200, 200));
            continue;
        }
        
        // Check if in selection
        bool inSelection = HasSelection() && 
                          address >= selectionStart_ && 
                          address < selectionStart_ + selectionLength_;
        
        if (inSelection) {
            ctx->DrawFilledRectangle(
                Rect2Di(x + (i / cellSize) * cellWidth_ - 2, y - 2, cellWidth_ - 2, cellHeight_),
                Color(51, 153, 255, 100));
        }
        
        // Check if editing this cell
        if (isEditing_ && address == editAddress_) {
            ctx->DrawRectangle(
                Rect2Di(x + (i / cellSize) * cellWidth_ - 2, y - 2, cellWidth_ - 2, cellHeight_),
                Color(255, 0, 0), 2);
        }
        
        std::string cellText = FormatCell(&memoryData_[offset], cellSize);
        Color color = GetByteColor(address);
        
        ctx->DrawText(cellText, Point2Di(x + (i / cellSize) * cellWidth_, y), color);
    }
}

void UCMemoryPanel::RenderASCIIColumn(IRenderContext* ctx, int x, int y, int row) {
    int dataOffset = row * bytesPerRow_;
    
    std::string ascii;
    for (int i = 0; i < bytesPerRow_; i++) {
        int offset = dataOffset + i;
        if (offset < static_cast<int>(memoryData_.size())) {
            ascii += ByteToASCII(memoryData_[offset]);
        } else {
            ascii += ' ';
        }
    }
    
    ctx->DrawText(ascii, Point2Di(x, y), Color(0, 100, 0));
}

std::string UCMemoryPanel::FormatAddress(uint64_t address) const {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << address;
    return ss.str();
}

std::string UCMemoryPanel::FormatByte(uint8_t byte) const {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    return ss.str();
}

std::string UCMemoryPanel::FormatCell(const uint8_t* data, int size) const {
    std::ostringstream ss;
    ss << std::hex << std::uppercase;
    
    // Little-endian display
    for (int i = size - 1; i >= 0; i--) {
        ss << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    
    return ss.str();
}

char UCMemoryPanel::ByteToASCII(uint8_t byte) const {
    if (byte >= 32 && byte < 127) {
        return static_cast<char>(byte);
    }
    return '.';
}

Color UCMemoryPanel::GetByteColor(uint64_t address) const {
    // Watched address
    if (watchedAddresses_.find(address) != watchedAddresses_.end()) {
        return Color(128, 0, 128);  // Purple
    }
    
    // Changed byte
    if (highlightChanges_ && changedBytes_.find(address) != changedBytes_.end()) {
        return Color(255, 0, 0);  // Red
    }
    
    // Search highlight
    for (uint64_t searchAddr : searchHighlights_) {
        if (address == searchAddr) {
            return Color(255, 165, 0);  // Orange
        }
    }
    
    // Null byte
    int offset = static_cast<int>(address - currentAddress_);
    if (offset >= 0 && offset < static_cast<int>(memoryData_.size())) {
        if (memoryData_[offset] == 0) {
            return Color(150, 150, 150);  // Gray
        }
    }
    
    return Color(0, 0, 0);  // Black
}

void UCMemoryPanel::HandleMouseDown(int x, int y, MouseButton button) {
    UltraCanvasContainer::HandleMouseDown(x, y, button);
    
    uint64_t address = GetAddressAtPosition(x, y);
    if (address != 0) {
        if (button == MouseButton::Left) {
            selectionStart_ = address;
            selectionLength_ = 1;
            isSelecting_ = true;
            Invalidate();
        } else if (button == MouseButton::Right) {
            // Context menu would go here
        }
    }
}

void UCMemoryPanel::HandleMouseUp(int x, int y, MouseButton button) {
    UltraCanvasContainer::HandleMouseUp(x, y, button);
    isSelecting_ = false;
}

void UCMemoryPanel::HandleMouseMove(int x, int y) {
    UltraCanvasContainer::HandleMouseMove(x, y);
    
    if (isSelecting_) {
        uint64_t address = GetAddressAtPosition(x, y);
        if (address != 0 && address >= selectionStart_) {
            selectionLength_ = address - selectionStart_ + 1;
            Invalidate();
        }
    }
}

void UCMemoryPanel::HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) {
    if (isEditing_) {
        if (keyCode >= '0' && keyCode <= '9') {
            editBuffer_ += static_cast<char>(keyCode);
            editNibble_++;
            if (editNibble_ >= 2) {
                CommitEdit();
                BeginEdit(editAddress_ + 1);
            }
            Invalidate();
        } else if ((keyCode >= 'A' && keyCode <= 'F') || (keyCode >= 'a' && keyCode <= 'f')) {
            editBuffer_ += static_cast<char>(std::toupper(keyCode));
            editNibble_++;
            if (editNibble_ >= 2) {
                CommitEdit();
                BeginEdit(editAddress_ + 1);
            }
            Invalidate();
        } else if (keyCode == 27) {  // Escape
            CancelEdit();
        } else if (keyCode == 13) {  // Enter
            CommitEdit();
        }
        return;
    }
    
    // Navigation
    if (keyCode == 38) {  // Up
        ScrollUp(1);
    } else if (keyCode == 40) {  // Down
        ScrollDown(1);
    } else if (keyCode == 33) {  // Page Up
        PageUp();
    } else if (keyCode == 34) {  // Page Down
        PageDown();
    } else if (ctrl && keyCode == 'G') {  // Ctrl+G - Go to
        addressInput_->Focus();
    }
}

void UCMemoryPanel::HandleMouseWheel(int delta) {
    if (delta > 0) {
        ScrollUp(3);
    } else {
        ScrollDown(3);
    }
}

uint64_t UCMemoryPanel::GetAddressAtPosition(int x, int y) const {
    int relY = y - GetY() - headerHeight_ - cellHeight_;
    if (relY < 0) return 0;
    
    int row = relY / cellHeight_;
    if (row >= GetVisibleRows()) return 0;
    
    int hexStartX = GetX() + leftMargin_ + (showAddresses_ ? addressWidth_ : 0);
    int relX = x - hexStartX;
    if (relX < 0) return 0;
    
    int col = relX / cellWidth_;
    if (col >= bytesPerRow_) return 0;
    
    return currentAddress_ + row * bytesPerRow_ + col;
}

bool UCMemoryPanel::SearchBytes(const std::vector<uint8_t>& pattern, bool forward) {
    // Search implementation
    return false;
}

bool UCMemoryPanel::SearchString(const std::string& text, bool forward) {
    std::vector<uint8_t> pattern(text.begin(), text.end());
    return SearchBytes(pattern, forward);
}

void UCMemoryPanel::HighlightSearchResults(const std::vector<uint64_t>& addresses) {
    searchHighlights_ = addresses;
    Invalidate();
}

void UCMemoryPanel::ClearSearchHighlights() {
    searchHighlights_.clear();
    Invalidate();
}

} // namespace IDE
} // namespace UltraCanvas
