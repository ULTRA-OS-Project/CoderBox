// Apps/IDE/Debug/UI/UCDisassemblyPanel.h
// Disassembly Panel for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasContainer.h"
#include "UltraCanvasTextArea.h"
#include "UltraCanvasToolbar.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasTextBox.h"
#include "UltraCanvasCheckbox.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <map>
#include <set>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Single disassembly instruction
 */
struct DisassemblyInstruction {
    uint64_t address = 0;
    std::string bytes;
    std::string mnemonic;
    std::string operands;
    std::string comment;
    int sourceLine = -1;
    std::string sourceFile;
    bool isCurrentInstruction = false;
    bool hasBreakpoint = false;
    bool isJumpTarget = false;
    bool isCall = false;
    bool isReturn = false;
    bool isBranch = false;
    uint64_t branchTarget = 0;
};

/**
 * @brief Disassembly display options
 */
struct DisassemblyOptions {
    bool showAddresses = true;
    bool showBytes = true;
    bool showSourceLines = true;
    bool showComments = true;
    bool highlightCurrentLine = true;
    bool colorizeInstructions = true;
    int instructionCount = 100;
};

/**
 * @brief Disassembly panel using UltraCanvas components
 */
class UCDisassemblyPanel : public UltraCanvasContainer {
public:
    UCDisassemblyPanel(const std::string& id, int x, int y, int width, int height);
    ~UCDisassemblyPanel() override = default;
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    void Disassemble(uint64_t address, int count = 100);
    void DisassembleFunction(const std::string& functionName);
    void DisassembleRange(uint64_t startAddress, uint64_t endAddress);
    void RefreshDisassembly();
    void SetInstructions(const std::vector<DisassemblyInstruction>& instructions);
    
    void GoToAddress(uint64_t address);
    void GoToAddressString(const std::string& addressExpr);
    void GoToCurrentInstruction();
    void GoToFunction(const std::string& functionName);
    void ScrollToAddress(uint64_t address);
    uint64_t GetCurrentAddress() const { return currentAddress_; }
    
    void SetShowSourceMapping(bool show);
    bool GetShowSourceMapping() const { return options_.showSourceLines; }
    int GetSourceLineForAddress(uint64_t address) const;
    uint64_t GetAddressForSourceLine(const std::string& file, int line) const;
    
    void SetOptions(const DisassemblyOptions& options);
    const DisassemblyOptions& GetOptions() const { return options_; }
    void SetShowAddresses(bool show) { options_.showAddresses = show; Invalidate(); }
    void SetShowBytes(bool show) { options_.showBytes = show; Invalidate(); }
    void SetShowComments(bool show) { options_.showComments = show; Invalidate(); }
    void SetColorizeInstructions(bool colorize) { options_.colorizeInstructions = colorize; Invalidate(); }
    
    void ToggleBreakpoint(uint64_t address);
    void SetBreakpoint(uint64_t address);
    void ClearBreakpoint(uint64_t address);
    void UpdateBreakpoints();
    
    void SetSelectedAddress(uint64_t address);
    uint64_t GetSelectedAddress() const { return selectedAddress_; }
    
    std::function<void(uint64_t address)> onAddressSelected;
    std::function<void(uint64_t address)> onAddressDoubleClicked;
    std::function<void(uint64_t address)> onBreakpointToggled;
    std::function<void(const std::string& file, int line)> onSourceLocationClicked;
    std::function<void(uint64_t address)> onRunToAddress;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    void HandleMouseDown(int x, int y, MouseButton button) override;
    void HandleMouseDoubleClick(int x, int y, MouseButton button) override;
    void HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) override;
    
private:
    void CreateUI();
    void RenderDisassembly(IRenderContext* ctx);
    void RenderInstruction(IRenderContext* ctx, const DisassemblyInstruction& inst, int y, int rowHeight);
    void RenderGutter(IRenderContext* ctx, const DisassemblyInstruction& inst, int y, int rowHeight);
    void RenderSourceLine(IRenderContext* ctx, const DisassemblyInstruction& inst, int y);
    
    Color GetInstructionColor(const DisassemblyInstruction& inst) const;
    Color GetOperandColor(const std::string& operand) const;
    int GetInstructionAtY(int y) const;
    int GetVisibleInstructionCount() const;
    std::string FormatAddress(uint64_t address) const;
    std::string FormatBytes(const std::string& bytes) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasTextBox> addressInput_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
    std::shared_ptr<UltraCanvasCheckbox> showSourceCheckbox_;
    std::shared_ptr<UltraCanvasCheckbox> showBytesCheckbox_;
    
    std::vector<DisassemblyInstruction> instructions_;
    uint64_t currentAddress_ = 0;
    uint64_t selectedAddress_ = 0;
    int scrollOffset_ = 0;
    
    DisassemblyOptions options_;
    
    int headerHeight_ = 60;
    int gutterWidth_ = 24;
    int addressWidth_ = 140;
    int bytesWidth_ = 100;
    int rowHeight_ = 18;
    int leftMargin_ = 8;
    
    std::map<uint64_t, int> addressToIndex_;
    std::set<uint64_t> breakpointAddresses_;
    std::string lastSourceFile_;
    int lastSourceLine_ = -1;
};

} // namespace IDE
} // namespace UltraCanvas
