// Apps/IDE/Debug/UI/UCDisassemblyPanel.cpp
// Disassembly Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDisassemblyPanel.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace UltraCanvas {
namespace IDE {

UCDisassemblyPanel::UCDisassemblyPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    SetBackgroundColor(Color(255, 255, 255));
}

void UCDisassemblyPanel::Initialize() {
    CreateUI();
}

void UCDisassemblyPanel::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            GoToCurrentInstruction();
        };
        
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            if (newState == DebugSessionState::Inactive || 
                newState == DebugSessionState::Terminated) {
                instructions_.clear();
                Invalidate();
            }
        };
    }
}

void UCDisassemblyPanel::CreateUI() {
    int yOffset = 0;
    
    // Title and address input
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "disasm_title", 0, 8, yOffset + 6, 60, 20);
    titleLabel_->SetText("Address:");
    titleLabel_->SetFontSize(11);
    AddChild(titleLabel_);
    
    addressInput_ = std::make_shared<UltraCanvasTextBox>(
        "address_input", 0, 70, yOffset + 4, 180, 22);
    addressInput_->SetFont("Consolas", 11);
    addressInput_->SetPlaceholder("0x00000000 or function name");
    addressInput_->SetOnSubmit([this](const std::string& text) {
        GoToAddressString(text);
    });
    AddChild(addressInput_);
    yOffset += 30;
    
    // Toolbar
    toolbar_ = std::make_shared<UltraCanvasToolbar>(
        "disasm_toolbar", 0, 0, yOffset, GetWidth(), 26);
    toolbar_->SetBackgroundColor(Color(240, 240, 240));
    toolbar_->SetSpacing(2);
    
    auto refreshBtn = std::make_shared<UltraCanvasToolbarButton>("refresh");
    refreshBtn->SetIcon("icons/refresh.png");
    refreshBtn->SetTooltip("Refresh Disassembly");
    refreshBtn->SetOnClick([this]() { RefreshDisassembly(); });
    toolbar_->AddButton(refreshBtn);
    
    auto gotoCurrentBtn = std::make_shared<UltraCanvasToolbarButton>("goto_current");
    gotoCurrentBtn->SetIcon("icons/goto_current.png");
    gotoCurrentBtn->SetTooltip("Go to Current Instruction");
    gotoCurrentBtn->SetOnClick([this]() { GoToCurrentInstruction(); });
    toolbar_->AddButton(gotoCurrentBtn);
    
    toolbar_->AddSeparator();
    
    auto stepIntoBtn = std::make_shared<UltraCanvasToolbarButton>("step_into");
    stepIntoBtn->SetIcon("icons/step_into.png");
    stepIntoBtn->SetTooltip("Step Into (instruction level)");
    stepIntoBtn->SetOnClick([this]() {
        if (debugManager_) debugManager_->StepInstruction();
    });
    toolbar_->AddButton(stepIntoBtn);
    
    auto stepOverBtn = std::make_shared<UltraCanvasToolbarButton>("step_over");
    stepOverBtn->SetIcon("icons/step_over.png");
    stepOverBtn->SetTooltip("Step Over (instruction level)");
    stepOverBtn->SetOnClick([this]() {
        if (debugManager_) debugManager_->StepOverInstruction();
    });
    toolbar_->AddButton(stepOverBtn);
    
    AddChild(toolbar_);
    yOffset += 28;
    
    // Options checkboxes
    showSourceCheckbox_ = std::make_shared<UltraCanvasCheckbox>(
        "show_source", 0, 8, yOffset, 120, 20);
    showSourceCheckbox_->SetText("Show Source");
    showSourceCheckbox_->SetChecked(true);
    showSourceCheckbox_->SetOnChange([this](bool checked) {
        options_.showSourceLines = checked;
        Invalidate();
    });
    AddChild(showSourceCheckbox_);
    
    showBytesCheckbox_ = std::make_shared<UltraCanvasCheckbox>(
        "show_bytes", 0, 140, yOffset, 120, 20);
    showBytesCheckbox_->SetText("Show Bytes");
    showBytesCheckbox_->SetChecked(true);
    showBytesCheckbox_->SetOnChange([this](bool checked) {
        options_.showBytes = checked;
        Invalidate();
    });
    AddChild(showBytesCheckbox_);
    
    headerHeight_ = yOffset + 24;
}

void UCDisassemblyPanel::Disassemble(uint64_t address, int count) {
    if (!debugManager_ || !debugManager_->IsDebugging()) return;
    
    currentAddress_ = address;
    options_.instructionCount = count;
    
    auto result = debugManager_->Disassemble(address, count);
    
    instructions_.clear();
    addressToIndex_.clear();
    
    int index = 0;
    for (const auto& inst : result) {
        DisassemblyInstruction di;
        di.address = inst.address;
        di.bytes = inst.bytes;
        di.mnemonic = inst.mnemonic;
        di.operands = inst.operands;
        di.comment = inst.comment;
        di.sourceLine = inst.sourceLine;
        di.sourceFile = inst.sourceFile;
        
        // Detect instruction types
        std::string mnemLower = di.mnemonic;
        std::transform(mnemLower.begin(), mnemLower.end(), mnemLower.begin(), ::tolower);
        
        di.isCall = (mnemLower == "call" || mnemLower == "callq");
        di.isReturn = (mnemLower == "ret" || mnemLower == "retq" || mnemLower == "retn");
        di.isBranch = (mnemLower.substr(0, 1) == "j" || mnemLower == "jmp");
        
        // Check if this is the current instruction
        di.isCurrentInstruction = (inst.address == debugManager_->GetCurrentInstructionAddress());
        
        // Check for breakpoints
        di.hasBreakpoint = (breakpointAddresses_.find(inst.address) != breakpointAddresses_.end());
        
        instructions_.push_back(di);
        addressToIndex_[inst.address] = index++;
    }
    
    // Update address input
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << address;
    addressInput_->SetText(ss.str());
    
    Invalidate();
}

void UCDisassemblyPanel::DisassembleFunction(const std::string& functionName) {
    if (!debugManager_) return;
    
    // Get function address
    auto result = debugManager_->Evaluate("&" + functionName);
    if (result.success) {
        GoToAddressString(result.variable.value);
    }
}

void UCDisassemblyPanel::DisassembleRange(uint64_t startAddress, uint64_t endAddress) {
    int count = static_cast<int>((endAddress - startAddress) / 4);  // Estimate
    Disassemble(startAddress, std::max(count, 50));
}

void UCDisassemblyPanel::RefreshDisassembly() {
    if (currentAddress_ != 0) {
        Disassemble(currentAddress_, options_.instructionCount);
    }
}

void UCDisassemblyPanel::SetInstructions(const std::vector<DisassemblyInstruction>& instructions) {
    instructions_ = instructions;
    addressToIndex_.clear();
    
    for (size_t i = 0; i < instructions_.size(); i++) {
        addressToIndex_[instructions_[i].address] = static_cast<int>(i);
    }
    
    Invalidate();
}

void UCDisassemblyPanel::GoToAddress(uint64_t address) {
    Disassemble(address, options_.instructionCount);
}

void UCDisassemblyPanel::GoToAddressString(const std::string& addressExpr) {
    if (addressExpr.empty()) return;
    
    uint64_t address = 0;
    
    // Try to parse as hex address
    try {
        std::string expr = addressExpr;
        if (expr.substr(0, 2) == "0x" || expr.substr(0, 2) == "0X") {
            expr = expr.substr(2);
        }
        address = std::stoull(expr, nullptr, 16);
        GoToAddress(address);
        return;
    } catch (...) {}
    
    // Try as function name
    if (debugManager_) {
        auto result = debugManager_->Evaluate("&" + addressExpr);
        if (result.success) {
            try {
                std::string value = result.variable.value;
                if (value.substr(0, 2) == "0x") {
                    value = value.substr(2);
                }
                address = std::stoull(value, nullptr, 16);
                GoToAddress(address);
            } catch (...) {}
        }
    }
}

void UCDisassemblyPanel::GoToCurrentInstruction() {
    if (!debugManager_ || !debugManager_->IsDebugging()) return;
    
    uint64_t pc = debugManager_->GetCurrentInstructionAddress();
    if (pc != 0) {
        GoToAddress(pc);
        SetSelectedAddress(pc);
    }
}

void UCDisassemblyPanel::GoToFunction(const std::string& functionName) {
    DisassembleFunction(functionName);
}

void UCDisassemblyPanel::ScrollToAddress(uint64_t address) {
    auto it = addressToIndex_.find(address);
    if (it != addressToIndex_.end()) {
        int visibleRows = GetVisibleInstructionCount();
        scrollOffset_ = std::max(0, it->second - visibleRows / 2);
        Invalidate();
    }
}

void UCDisassemblyPanel::SetShowSourceMapping(bool show) {
    options_.showSourceLines = show;
    Invalidate();
}

int UCDisassemblyPanel::GetSourceLineForAddress(uint64_t address) const {
    auto it = addressToIndex_.find(address);
    if (it != addressToIndex_.end() && it->second < static_cast<int>(instructions_.size())) {
        return instructions_[it->second].sourceLine;
    }
    return -1;
}

uint64_t UCDisassemblyPanel::GetAddressForSourceLine(const std::string& file, int line) const {
    for (const auto& inst : instructions_) {
        if (inst.sourceLine == line && inst.sourceFile == file) {
            return inst.address;
        }
    }
    return 0;
}

void UCDisassemblyPanel::SetOptions(const DisassemblyOptions& options) {
    options_ = options;
    RefreshDisassembly();
}

void UCDisassemblyPanel::ToggleBreakpoint(uint64_t address) {
    if (breakpointAddresses_.find(address) != breakpointAddresses_.end()) {
        ClearBreakpoint(address);
    } else {
        SetBreakpoint(address);
    }
}

void UCDisassemblyPanel::SetBreakpoint(uint64_t address) {
    breakpointAddresses_.insert(address);
    
    // Update instruction
    auto it = addressToIndex_.find(address);
    if (it != addressToIndex_.end()) {
        instructions_[it->second].hasBreakpoint = true;
    }
    
    if (onBreakpointToggled) {
        onBreakpointToggled(address);
    }
    
    Invalidate();
}

void UCDisassemblyPanel::ClearBreakpoint(uint64_t address) {
    breakpointAddresses_.erase(address);
    
    auto it = addressToIndex_.find(address);
    if (it != addressToIndex_.end()) {
        instructions_[it->second].hasBreakpoint = false;
    }
    
    if (onBreakpointToggled) {
        onBreakpointToggled(address);
    }
    
    Invalidate();
}

void UCDisassemblyPanel::UpdateBreakpoints() {
    if (!debugManager_) return;
    
    breakpointAddresses_.clear();
    auto bps = debugManager_->GetBreakpointManager()->GetAllBreakpoints();
    
    for (const auto& bp : bps) {
        if (bp.GetType() == BreakpointType::AddressBreakpoint) {
            breakpointAddresses_.insert(bp.GetAddress());
        }
    }
    
    for (auto& inst : instructions_) {
        inst.hasBreakpoint = (breakpointAddresses_.find(inst.address) != breakpointAddresses_.end());
    }
    
    Invalidate();
}

void UCDisassemblyPanel::SetSelectedAddress(uint64_t address) {
    selectedAddress_ = address;
    ScrollToAddress(address);
    
    if (onAddressSelected) {
        onAddressSelected(address);
    }
    
    Invalidate();
}

void UCDisassemblyPanel::HandleRender(IRenderContext* ctx) {
    UltraCanvasContainer::HandleRender(ctx);
    RenderDisassembly(ctx);
}

void UCDisassemblyPanel::RenderDisassembly(IRenderContext* ctx) {
    int startY = GetY() + headerHeight_;
    int visibleRows = GetVisibleInstructionCount();
    
    ctx->SetFont("Consolas", 11);
    
    lastSourceFile_.clear();
    lastSourceLine_ = -1;
    
    for (int i = 0; i < visibleRows && (scrollOffset_ + i) < static_cast<int>(instructions_.size()); i++) {
        const auto& inst = instructions_[scrollOffset_ + i];
        int y = startY + i * rowHeight_;
        
        // Render source line separator if enabled and changed
        if (options_.showSourceLines && inst.sourceLine > 0) {
            if (inst.sourceFile != lastSourceFile_ || inst.sourceLine != lastSourceLine_) {
                RenderSourceLine(ctx, inst, y);
                lastSourceFile_ = inst.sourceFile;
                lastSourceLine_ = inst.sourceLine;
            }
        }
        
        RenderGutter(ctx, inst, y, rowHeight_);
        RenderInstruction(ctx, inst, y, rowHeight_);
    }
}

void UCDisassemblyPanel::RenderGutter(IRenderContext* ctx, const DisassemblyInstruction& inst,
                                       int y, int rowHeight) {
    int gutterX = GetX();
    
    // Background for current instruction
    if (inst.isCurrentInstruction) {
        ctx->DrawFilledRectangle(
            Rect2Di(gutterX, y, gutterWidth_, rowHeight),
            Color(255, 255, 180));
    }
    
    // Breakpoint indicator
    if (inst.hasBreakpoint) {
        int cx = gutterX + gutterWidth_ / 2;
        int cy = y + rowHeight / 2;
        ctx->DrawFilledCircle(Point2Di(cx, cy), 6, Color(255, 0, 0));
    }
    
    // Current instruction arrow
    if (inst.isCurrentInstruction) {
        int arrowX = gutterX + 4;
        int arrowY = y + rowHeight / 2;
        std::vector<Point2Di> arrow = {
            Point2Di(arrowX, arrowY - 4),
            Point2Di(arrowX + 8, arrowY),
            Point2Di(arrowX, arrowY + 4)
        };
        ctx->DrawFilledPolygon(arrow, Color(255, 200, 0));
    }
}

void UCDisassemblyPanel::RenderInstruction(IRenderContext* ctx, const DisassemblyInstruction& inst,
                                            int y, int rowHeight) {
    int x = GetX() + gutterWidth_ + leftMargin_;
    
    // Background for selected line
    if (inst.address == selectedAddress_) {
        ctx->DrawFilledRectangle(
            Rect2Di(GetX() + gutterWidth_, y, GetWidth() - gutterWidth_, rowHeight),
            Color(51, 153, 255, 50));
    }
    
    // Address
    if (options_.showAddresses) {
        std::string addrStr = FormatAddress(inst.address);
        ctx->DrawText(addrStr, Point2Di(x, y + 2), Color(0, 0, 128));
        x += addressWidth_;
    }
    
    // Bytes
    if (options_.showBytes) {
        std::string bytesStr = FormatBytes(inst.bytes);
        ctx->DrawText(bytesStr, Point2Di(x, y + 2), Color(128, 128, 128));
        x += bytesWidth_;
    }
    
    // Mnemonic
    Color mnemonicColor = options_.colorizeInstructions ? GetInstructionColor(inst) : Color(0, 0, 0);
    ctx->DrawText(inst.mnemonic, Point2Di(x, y + 2), mnemonicColor);
    x += 80;
    
    // Operands
    if (!inst.operands.empty()) {
        Color operandColor = options_.colorizeInstructions ? GetOperandColor(inst.operands) : Color(0, 0, 0);
        ctx->DrawText(inst.operands, Point2Di(x, y + 2), operandColor);
    }
    
    // Comment
    if (options_.showComments && !inst.comment.empty()) {
        int commentX = GetX() + GetWidth() - 200;
        ctx->DrawText("; " + inst.comment, Point2Di(commentX, y + 2), Color(0, 128, 0));
    }
}

void UCDisassemblyPanel::RenderSourceLine(IRenderContext* ctx, const DisassemblyInstruction& inst, int y) {
    // Draw source file:line above the instruction
    std::ostringstream ss;
    ss << inst.sourceFile << ":" << inst.sourceLine;
    
    ctx->DrawFilledRectangle(
        Rect2Di(GetX() + gutterWidth_, y - rowHeight_, GetWidth() - gutterWidth_, rowHeight_),
        Color(240, 240, 255));
    ctx->DrawText(ss.str(), Point2Di(GetX() + gutterWidth_ + leftMargin_, y - rowHeight_ + 2),
                  Color(100, 100, 150));
}

Color UCDisassemblyPanel::GetInstructionColor(const DisassemblyInstruction& inst) const {
    if (inst.isCall) return Color(128, 0, 128);  // Purple for calls
    if (inst.isReturn) return Color(128, 0, 0);  // Dark red for returns
    if (inst.isBranch) return Color(0, 0, 200);  // Blue for jumps
    
    std::string mnemLower = inst.mnemonic;
    std::transform(mnemLower.begin(), mnemLower.end(), mnemLower.begin(), ::tolower);
    
    if (mnemLower == "nop") return Color(150, 150, 150);
    if (mnemLower == "push" || mnemLower == "pop") return Color(0, 128, 128);
    if (mnemLower.find("mov") != std::string::npos) return Color(0, 100, 0);
    
    return Color(0, 0, 0);
}

Color UCDisassemblyPanel::GetOperandColor(const std::string& operand) const {
    if (operand.find("0x") != std::string::npos) return Color(0, 100, 200);
    if (operand.find('[') != std::string::npos) return Color(128, 64, 0);
    return Color(0, 0, 0);
}

int UCDisassemblyPanel::GetInstructionAtY(int y) const {
    int relY = y - GetY() - headerHeight_;
    if (relY < 0) return -1;
    return scrollOffset_ + relY / rowHeight_;
}

int UCDisassemblyPanel::GetVisibleInstructionCount() const {
    return (GetHeight() - headerHeight_) / rowHeight_;
}

std::string UCDisassemblyPanel::FormatAddress(uint64_t address) const {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << address;
    return ss.str();
}

std::string UCDisassemblyPanel::FormatBytes(const std::string& bytes) const {
    if (bytes.length() > 20) {
        return bytes.substr(0, 20) + "...";
    }
    return bytes;
}

void UCDisassemblyPanel::HandleMouseDown(int x, int y, MouseButton button) {
    UltraCanvasContainer::HandleMouseDown(x, y, button);
    
    int instIndex = GetInstructionAtY(y);
    if (instIndex >= 0 && instIndex < static_cast<int>(instructions_.size())) {
        const auto& inst = instructions_[instIndex];
        
        // Check if click is in gutter (for breakpoints)
        if (x < GetX() + gutterWidth_) {
            ToggleBreakpoint(inst.address);
        } else {
            SetSelectedAddress(inst.address);
        }
    }
}

void UCDisassemblyPanel::HandleMouseDoubleClick(int x, int y, MouseButton button) {
    int instIndex = GetInstructionAtY(y);
    if (instIndex >= 0 && instIndex < static_cast<int>(instructions_.size())) {
        const auto& inst = instructions_[instIndex];
        
        if (onAddressDoubleClicked) {
            onAddressDoubleClicked(inst.address);
        }
        
        // If has source, navigate to it
        if (inst.sourceLine > 0 && !inst.sourceFile.empty() && onSourceLocationClicked) {
            onSourceLocationClicked(inst.sourceFile, inst.sourceLine);
        }
    }
}

void UCDisassemblyPanel::HandleKeyPress(int keyCode, bool ctrl, bool shift, bool alt) {
    if (keyCode == 38) {  // Up
        if (scrollOffset_ > 0) {
            scrollOffset_--;
            Invalidate();
        }
    } else if (keyCode == 40) {  // Down
        if (scrollOffset_ < static_cast<int>(instructions_.size()) - 1) {
            scrollOffset_++;
            Invalidate();
        }
    } else if (keyCode == 33) {  // Page Up
        scrollOffset_ = std::max(0, scrollOffset_ - GetVisibleInstructionCount());
        Invalidate();
    } else if (keyCode == 34) {  // Page Down
        scrollOffset_ = std::min(static_cast<int>(instructions_.size()) - 1, 
                                  scrollOffset_ + GetVisibleInstructionCount());
        Invalidate();
    } else if (ctrl && keyCode == 'G') {  // Ctrl+G - Go to address
        addressInput_->Focus();
    } else if (keyCode == 116) {  // F5 - Refresh
        RefreshDisassembly();
    }
}

} // namespace IDE
} // namespace UltraCanvas
