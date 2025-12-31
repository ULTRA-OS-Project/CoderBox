// Apps/IDE/Debug/UI/UCMemoryPanel.h
// Memory Panel for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasContainer.h"
#include "UltraCanvasTextBox.h"
#include "UltraCanvasToolbar.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasDropDown.h"
#include "UltraCanvasScrollBar.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <vector>
#include <cstdint>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Memory display format
 */
enum class MemoryDisplayFormat {
    Bytes,          // 1 byte per cell
    Words,          // 2 bytes per cell
    DWords,         // 4 bytes per cell
    QWords          // 8 bytes per cell
};

/**
 * @brief Memory data representation
 */
enum class MemoryDataType {
    Hexadecimal,
    Decimal,
    SignedDecimal,
    Binary,
    ASCII,
    Float,
    Double
};

/**
 * @brief Memory region info
 */
struct MemoryRegion {
    uint64_t startAddress = 0;
    uint64_t endAddress = 0;
    std::string name;
    bool readable = true;
    bool writable = true;
    bool executable = false;
};

/**
 * @brief Memory edit operation
 */
struct MemoryEdit {
    uint64_t address = 0;
    std::vector<uint8_t> oldData;
    std::vector<uint8_t> newData;
};

/**
 * @brief Memory panel using UltraCanvas components
 * 
 * Displays memory contents in a hex editor style view with
 * address, hex bytes, and ASCII representation.
 */
class UCMemoryPanel : public UltraCanvasContainer {
public:
    UCMemoryPanel(const std::string& id, int x, int y, int width, int height);
    ~UCMemoryPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // MEMORY NAVIGATION
    // =========================================================================
    
    void GoToAddress(uint64_t address);
    void GoToAddressString(const std::string& addressExpr);
    uint64_t GetCurrentAddress() const { return currentAddress_; }
    
    void ScrollUp(int lines = 1);
    void ScrollDown(int lines = 1);
    void PageUp();
    void PageDown();
    
    // =========================================================================
    // MEMORY READING
    // =========================================================================
    
    void RefreshMemory();
    void SetMemoryData(uint64_t address, const std::vector<uint8_t>& data);
    
    // =========================================================================
    // DISPLAY OPTIONS
    // =========================================================================
    
    void SetDisplayFormat(MemoryDisplayFormat format);
    MemoryDisplayFormat GetDisplayFormat() const { return displayFormat_; }
    
    void SetDataType(MemoryDataType type);
    MemoryDataType GetDataType() const { return dataType_; }
    
    void SetBytesPerRow(int bytes);
    int GetBytesPerRow() const { return bytesPerRow_; }
    
    void SetShowASCII(bool show) { showASCII_ = show; Invalidate(); }
    bool GetShowASCII() const { return showASCII_; }
    
    void SetShowAddresses(bool show) { showAddresses_ = show; Invalidate(); }
    bool GetShowAddresses() const { return showAddresses_; }
    
    void SetHighlightChanges(bool highlight) { highlightChanges_ = highlight; }
    bool GetHighlightChanges() const { return highlightChanges_; }
    
    // =========================================================================
    // MEMORY EDITING
    // =========================================================================
    
    void BeginEdit(uint64_t address);
    void CommitEdit();
    void CancelEdit();
    bool IsEditing() const { return isEditing_; }
    
    void WriteMemory(uint64_t address, const std::vector<uint8_t>& data);
    void WriteByte(uint64_t address, uint8_t value);
    
    // =========================================================================
    // SELECTION
    // =========================================================================
    
    void SetSelection(uint64_t startAddress, uint64_t length);
    void ClearSelection();
    bool HasSelection() const { return selectionLength_ > 0; }
    uint64_t GetSelectionStart() const { return selectionStart_; }
    uint64_t GetSelectionLength() const { return selectionLength_; }
    std::vector<uint8_t> GetSelectedData() const;
    
    // =========================================================================
    // SEARCH
    // =========================================================================
    
    bool SearchBytes(const std::vector<uint8_t>& pattern, bool forward = true);
    bool SearchString(const std::string& text, bool forward = true);
    void HighlightSearchResults(const std::vector<uint64_t>& addresses);
    void ClearSearchHighlights();
    
    // =========================================================================
    // WATCHPOINTS
    // =========================================================================
    
    void AddWatchedAddress(uint64_t address, int size = 1);
    void RemoveWatchedAddress(uint64_t address);
    void ClearWatchedAddresses();
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(uint64_t address)> onAddressChanged;
    std::function<void(uint64_t address, const std::vector<uint8_t>& data)> onMemoryEdited;
    std::function<void(uint64_t address)> onSelectionChanged;
    std::function<void(uint64_t address)> onAddWatchpoint;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    void HandleMouseDown(int x, int y, MouseButton button) override;
    void HandleMouseUp(int x, int y, MouseButton button) override;
    void HandleMouseMove(int x, int y) override;
    void HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) override;
    void HandleMouseWheel(int delta) override;
    
private:
    void CreateUI();
    void RenderMemoryView(IRenderContext* ctx);
    void RenderAddressColumn(IRenderContext* ctx, int x, int y, uint64_t address);
    void RenderHexColumn(IRenderContext* ctx, int x, int y, int row);
    void RenderASCIIColumn(IRenderContext* ctx, int x, int y, int row);
    
    int GetRowCount() const;
    int GetVisibleRows() const;
    int GetRowAtY(int y) const;
    int GetByteAtX(int x) const;
    uint64_t GetAddressAtPosition(int x, int y) const;
    Rect2Di GetByteRect(uint64_t address) const;
    
    std::string FormatAddress(uint64_t address) const;
    std::string FormatByte(uint8_t byte) const;
    std::string FormatCell(const uint8_t* data, int size) const;
    char ByteToASCII(uint8_t byte) const;
    Color GetByteColor(uint64_t address) const;
    
    void UpdateScrollBar();
    void EnsureAddressVisible(uint64_t address);
    
    UCDebugManager* debugManager_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasTextBox> addressInput_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
    std::shared_ptr<UltraCanvasDropDown> formatDropDown_;
    std::shared_ptr<UltraCanvasScrollBar> scrollBar_;
    
    // Memory data
    uint64_t currentAddress_ = 0;
    std::vector<uint8_t> memoryData_;
    std::vector<uint8_t> previousData_;
    int memorySize_ = 1024;  // Bytes to read
    
    // Display settings
    MemoryDisplayFormat displayFormat_ = MemoryDisplayFormat::Bytes;
    MemoryDataType dataType_ = MemoryDataType::Hexadecimal;
    int bytesPerRow_ = 16;
    bool showASCII_ = true;
    bool showAddresses_ = true;
    bool highlightChanges_ = true;
    
    // Layout metrics
    int addressWidth_ = 120;
    int cellWidth_ = 24;
    int cellHeight_ = 18;
    int asciiWidth_ = 10;
    int headerHeight_ = 60;
    int leftMargin_ = 8;
    
    // Selection
    uint64_t selectionStart_ = 0;
    uint64_t selectionLength_ = 0;
    bool isSelecting_ = false;
    
    // Editing
    bool isEditing_ = false;
    uint64_t editAddress_ = 0;
    std::string editBuffer_;
    int editNibble_ = 0;
    
    // Search highlights
    std::vector<uint64_t> searchHighlights_;
    
    // Watched addresses
    std::set<uint64_t> watchedAddresses_;
    
    // Changed bytes since last refresh
    std::set<uint64_t> changedBytes_;
};

} // namespace IDE
} // namespace UltraCanvas
