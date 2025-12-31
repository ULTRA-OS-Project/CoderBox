// Apps/IDE/Debug/UI/UCRegistersPanel.cpp
// Registers Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCRegistersPanel.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace UltraCanvas {
namespace IDE {

UCRegistersPanel::UCRegistersPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    SetBackgroundColor(Color(250, 250, 250));
    
    // Default visible categories
    visibleCategories_.insert(RegisterCategory::General);
    visibleCategories_.insert(RegisterCategory::Flags);
    visibleCategories_.insert(RegisterCategory::Segment);
}

void UCRegistersPanel::Initialize() {
    CreateUI();
}

void UCRegistersPanel::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            RefreshRegisters();
        };
        
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            if (newState == DebugSessionState::Inactive || 
                newState == DebugSessionState::Terminated) {
                Clear();
            }
        };
    }
}

void UCRegistersPanel::CreateUI() {
    int yOffset = 0;
    
    // Title label
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "registers_title", 0, 8, yOffset + 4, 80, 20);
    titleLabel_->SetText("Registers");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetFontSize(12);
    AddChild(titleLabel_);
    
    // Format dropdown
    formatDropDown_ = std::make_shared<UltraCanvasDropDown>(
        "format_dropdown", 0, GetWidth() - 108, yOffset + 2, 100, 22);
    formatDropDown_->AddItem("Hexadecimal");
    formatDropDown_->AddItem("Decimal");
    formatDropDown_->AddItem("Signed");
    formatDropDown_->AddItem("Binary");
    formatDropDown_->AddItem("Float");
    formatDropDown_->AddItem("Double");
    formatDropDown_->SetSelectedIndex(0);
    formatDropDown_->SetOnSelectionChange([this](int index, const std::string&) {
        SetDisplayFormat(static_cast<RegisterFormat>(index));
    });
    AddChild(formatDropDown_);
    yOffset += 28;
    
    // Toolbar
    toolbar_ = std::make_shared<UltraCanvasToolbar>(
        "registers_toolbar", 0, 0, yOffset, GetWidth(), 26);
    toolbar_->SetBackgroundColor(Color(240, 240, 240));
    toolbar_->SetSpacing(2);
    
    auto refreshBtn = std::make_shared<UltraCanvasToolbarButton>("refresh");
    refreshBtn->SetIcon("icons/refresh.png");
    refreshBtn->SetTooltip("Refresh Registers");
    refreshBtn->SetOnClick([this]() { RefreshRegisters(); });
    toolbar_->AddButton(refreshBtn);
    
    toolbar_->AddSeparator();
    
    auto gprBtn = std::make_shared<UltraCanvasToolbarButton>("show_gpr");
    gprBtn->SetIcon("icons/reg_gpr.png");
    gprBtn->SetTooltip("General Purpose Registers");
    gprBtn->SetToggle(true);
    gprBtn->SetPressed(true);
    gprBtn->SetOnClick([this]() { 
        SetShowCategory(RegisterCategory::General, !GetShowCategory(RegisterCategory::General));
    });
    toolbar_->AddButton(gprBtn);
    
    auto fpuBtn = std::make_shared<UltraCanvasToolbarButton>("show_fpu");
    fpuBtn->SetIcon("icons/reg_fpu.png");
    fpuBtn->SetTooltip("Floating Point Registers");
    fpuBtn->SetToggle(true);
    fpuBtn->SetOnClick([this]() {
        SetShowCategory(RegisterCategory::FloatingPoint, !GetShowCategory(RegisterCategory::FloatingPoint));
    });
    toolbar_->AddButton(fpuBtn);
    
    auto sseBtn = std::make_shared<UltraCanvasToolbarButton>("show_sse");
    sseBtn->SetIcon("icons/reg_sse.png");
    sseBtn->SetTooltip("SSE/AVX Registers");
    sseBtn->SetToggle(true);
    sseBtn->SetOnClick([this]() {
        SetShowCategory(RegisterCategory::SSE, !GetShowCategory(RegisterCategory::SSE));
    });
    toolbar_->AddButton(sseBtn);
    
    AddChild(toolbar_);
    yOffset += 28;
    
    // Register tree view
    registerTree_ = std::make_shared<UltraCanvasTreeView>(
        "register_tree", 0, 0, yOffset, GetWidth(), GetHeight() - yOffset);
    registerTree_->SetShowExpandButtons(true);
    registerTree_->SetShowIcons(true);
    registerTree_->SetRowHeight(20);
    registerTree_->SetSelectionMode(TreeSelectionMode::Single);
    registerTree_->SetBackgroundColor(Colors::White);
    registerTree_->SetFont("Consolas", 11);
    
    registerTree_->onSelectionChange = [this](const std::vector<TreeNodeId>& selected) {
        if (!selected.empty()) {
            auto it = nodeToRegName_.find(selected[0]);
            if (it != nodeToRegName_.end() && onRegisterSelected) {
                onRegisterSelected(it->second);
            }
        }
    };
    
    registerTree_->onNodeDoubleClick = [this](TreeNodeId nodeId) {
        auto it = nodeToRegName_.find(nodeId);
        if (it != nodeToRegName_.end()) {
            BeginEditRegister(it->second);
        }
    };
    
    AddChild(registerTree_);
}

void UCRegistersPanel::RefreshRegisters() {
    if (!debugManager_ || !debugManager_->IsDebugging()) {
        Clear();
        return;
    }
    
    // Save previous values for change detection
    previousRegisters_.clear();
    for (const auto& reg : registers_) {
        previousRegisters_[reg.name] = reg;
    }
    
    // Get registers from debugger
    auto regs = debugManager_->GetRegisters();
    
    std::vector<RegisterValue> newRegisters;
    for (const auto& reg : regs) {
        RegisterValue rv;
        rv.name = reg.name;
        rv.value = reg.value;
        rv.bitSize = reg.bitSize;
        rv.valid = true;
        
        // Categorize register
        std::string nameLower = reg.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        
        if (nameLower.find("xmm") != std::string::npos) {
            rv.category = RegisterCategory::SSE;
        } else if (nameLower.find("ymm") != std::string::npos) {
            rv.category = RegisterCategory::AVX;
        } else if (nameLower.find("zmm") != std::string::npos) {
            rv.category = RegisterCategory::AVX512;
        } else if (nameLower.find("st") != std::string::npos && nameLower.length() <= 3) {
            rv.category = RegisterCategory::FloatingPoint;
        } else if (nameLower == "cs" || nameLower == "ds" || nameLower == "es" ||
                   nameLower == "fs" || nameLower == "gs" || nameLower == "ss") {
            rv.category = RegisterCategory::Segment;
        } else if (nameLower.find("flag") != std::string::npos || nameLower == "eflags" || nameLower == "rflags") {
            rv.category = RegisterCategory::Flags;
        } else if (nameLower.find("cr") == 0 && nameLower.length() <= 3) {
            rv.category = RegisterCategory::Control;
        } else if (nameLower.find("dr") == 0 && nameLower.length() <= 3) {
            rv.category = RegisterCategory::Debug;
        } else {
            rv.category = RegisterCategory::General;
        }
        
        // Check for changes
        auto prevIt = previousRegisters_.find(reg.name);
        if (prevIt != previousRegisters_.end()) {
            rv.previousValue = prevIt->second.value;
            rv.changed = (rv.value != rv.previousValue);
        }
        
        newRegisters.push_back(rv);
    }
    
    SetRegisters(newRegisters);
}

void UCRegistersPanel::SetRegisters(const std::vector<RegisterValue>& registers) {
    registers_ = registers;
    PopulateRegisterTree();
}

void UCRegistersPanel::UpdateRegister(const std::string& name, const std::string& value) {
    for (auto& reg : registers_) {
        if (reg.name == name) {
            reg.previousValue = reg.value;
            reg.value = value;
            reg.changed = true;
            break;
        }
    }
    PopulateRegisterTree();
}

void UCRegistersPanel::Clear() {
    registers_.clear();
    previousRegisters_.clear();
    registerTree_->Clear();
    nodeToRegName_.clear();
    regNameToNode_.clear();
    categoryNodes_.clear();
    nextNodeId_ = 1;
}

void UCRegistersPanel::SetDisplayFormat(RegisterFormat format) {
    displayFormat_ = format;
    PopulateRegisterTree();
}

void UCRegistersPanel::SetShowCategory(RegisterCategory category, bool show) {
    if (show) {
        visibleCategories_.insert(category);
    } else {
        visibleCategories_.erase(category);
    }
    PopulateRegisterTree();
}

bool UCRegistersPanel::GetShowCategory(RegisterCategory category) const {
    return visibleCategories_.find(category) != visibleCategories_.end();
}

void UCRegistersPanel::PopulateRegisterTree() {
    registerTree_->Clear();
    nodeToRegName_.clear();
    regNameToNode_.clear();
    categoryNodes_.clear();
    nextNodeId_ = 1;
    
    if (groupByCategory_) {
        // Group by category
        std::map<RegisterCategory, std::vector<RegisterValue>> grouped;
        for (const auto& reg : registers_) {
            if (visibleCategories_.find(reg.category) != visibleCategories_.end()) {
                grouped[reg.category].push_back(reg);
            }
        }
        
        for (const auto& pair : grouped) {
            AddCategoryGroup(pair.first);
            TreeNodeId parentId = categoryNodes_[pair.first];
            
            for (const auto& reg : pair.second) {
                AddRegisterNode(reg, parentId);
            }
            
            // Expand category by default
            registerTree_->ExpandNode(parentId);
        }
    } else {
        // Flat list
        for (const auto& reg : registers_) {
            if (visibleCategories_.find(reg.category) != visibleCategories_.end()) {
                AddRegisterNode(reg);
            }
        }
    }
}

void UCRegistersPanel::AddCategoryGroup(RegisterCategory category) {
    TreeNodeId nodeId = nextNodeId_++;
    
    TreeNodeData nodeData;
    nodeData.text = GetCategoryName(category);
    nodeData.fontWeight = FontWeight::Bold;
    nodeData.leftIcon.iconPath = GetCategoryIcon(category);
    nodeData.leftIcon.width = 16;
    nodeData.leftIcon.height = 16;
    nodeData.leftIcon.visible = true;
    
    registerTree_->AddNode(nodeId, nodeData);
    categoryNodes_[category] = nodeId;
}

void UCRegistersPanel::AddRegisterNode(const RegisterValue& reg, TreeNodeId parentId) {
    TreeNodeId nodeId = nextNodeId_++;
    
    TreeNodeData nodeData;
    nodeData.text = FormatRegisterValue(reg);
    nodeData.textColor = GetRegisterColor(reg);
    
    if (parentId > 0) {
        registerTree_->AddNode(nodeId, nodeData, parentId);
    } else {
        registerTree_->AddNode(nodeId, nodeData);
    }
    
    nodeToRegName_[nodeId] = reg.name;
    regNameToNode_[reg.name] = nodeId;
}

std::string UCRegistersPanel::FormatRegisterValue(const RegisterValue& reg) const {
    std::ostringstream ss;
    
    // Register name (right-padded)
    ss << std::left << std::setw(8) << reg.name << " = ";
    
    // Formatted value
    ss << FormatValueAs(reg.value, displayFormat_, reg.bitSize);
    
    // Show previous value if changed
    if (highlightChanges_ && reg.changed && !reg.previousValue.empty()) {
        ss << "  (was: " << FormatValueAs(reg.previousValue, displayFormat_, reg.bitSize) << ")";
    }
    
    return ss.str();
}

std::string UCRegistersPanel::FormatValueAs(const std::string& hexValue, 
                                             RegisterFormat format, int bitSize) const {
    if (hexValue.empty()) return "???";
    
    // Parse hex value
    uint64_t value = 0;
    try {
        std::string cleanHex = hexValue;
        if (cleanHex.substr(0, 2) == "0x" || cleanHex.substr(0, 2) == "0X") {
            cleanHex = cleanHex.substr(2);
        }
        value = std::stoull(cleanHex, nullptr, 16);
    } catch (...) {
        return hexValue;  // Return original if parse fails
    }
    
    std::ostringstream ss;
    
    switch (format) {
        case RegisterFormat::Hexadecimal:
            ss << "0x" << std::hex << std::uppercase;
            if (bitSize <= 8) ss << std::setw(2);
            else if (bitSize <= 16) ss << std::setw(4);
            else if (bitSize <= 32) ss << std::setw(8);
            else ss << std::setw(16);
            ss << std::setfill('0') << value;
            break;
            
        case RegisterFormat::Decimal:
            ss << std::dec << value;
            break;
            
        case RegisterFormat::SignedDecimal:
            if (bitSize <= 8) ss << static_cast<int8_t>(value);
            else if (bitSize <= 16) ss << static_cast<int16_t>(value);
            else if (bitSize <= 32) ss << static_cast<int32_t>(value);
            else ss << static_cast<int64_t>(value);
            break;
            
        case RegisterFormat::Binary: {
            int digits = (bitSize <= 8) ? 8 : (bitSize <= 16) ? 16 : (bitSize <= 32) ? 32 : 64;
            for (int i = digits - 1; i >= 0; i--) {
                ss << ((value >> i) & 1);
                if (i > 0 && i % 4 == 0) ss << "'";
            }
            break;
        }
            
        case RegisterFormat::Float:
            if (bitSize == 32) {
                float f;
                uint32_t v32 = static_cast<uint32_t>(value);
                std::memcpy(&f, &v32, sizeof(f));
                ss << std::fixed << std::setprecision(6) << f;
            } else {
                ss << hexValue;
            }
            break;
            
        case RegisterFormat::Double:
            if (bitSize == 64) {
                double d;
                std::memcpy(&d, &value, sizeof(d));
                ss << std::fixed << std::setprecision(10) << d;
            } else {
                ss << hexValue;
            }
            break;
    }
    
    return ss.str();
}

std::string UCRegistersPanel::GetCategoryName(RegisterCategory category) const {
    switch (category) {
        case RegisterCategory::General: return "General Purpose";
        case RegisterCategory::Segment: return "Segment";
        case RegisterCategory::Flags: return "Flags";
        case RegisterCategory::FloatingPoint: return "Floating Point (x87)";
        case RegisterCategory::SSE: return "SSE (XMM)";
        case RegisterCategory::AVX: return "AVX (YMM)";
        case RegisterCategory::AVX512: return "AVX-512 (ZMM)";
        case RegisterCategory::Control: return "Control";
        case RegisterCategory::Debug: return "Debug";
        case RegisterCategory::System: return "System";
        case RegisterCategory::Other: return "Other";
    }
    return "Unknown";
}

std::string UCRegistersPanel::GetCategoryIcon(RegisterCategory category) const {
    switch (category) {
        case RegisterCategory::General: return "icons/reg_gpr.png";
        case RegisterCategory::FloatingPoint: return "icons/reg_fpu.png";
        case RegisterCategory::SSE:
        case RegisterCategory::AVX:
        case RegisterCategory::AVX512: return "icons/reg_sse.png";
        case RegisterCategory::Flags: return "icons/reg_flags.png";
        default: return "icons/register.png";
    }
}

Color UCRegistersPanel::GetRegisterColor(const RegisterValue& reg) const {
    if (!reg.valid) {
        return Color(128, 128, 128);  // Gray for invalid
    }
    
    if (highlightChanges_ && reg.changed) {
        return Color(200, 0, 0);  // Red for changed
    }
    
    return Color(0, 0, 0);  // Black for normal
}

void UCRegistersPanel::BeginEditRegister(const std::string& regName) {
    editingRegister_ = regName;
    // TODO: Show edit dialog or inline edit
}

void UCRegistersPanel::CommitRegisterEdit(const std::string& regName, const std::string& newValue) {
    if (debugManager_) {
        debugManager_->SetRegister(regName, newValue);
    }
    
    if (onRegisterEdited) {
        onRegisterEdited(regName, newValue);
    }
    
    editingRegister_.clear();
    RefreshRegisters();
}

void UCRegistersPanel::CancelRegisterEdit() {
    editingRegister_.clear();
}

void UCRegistersPanel::HandleRender(IRenderContext* ctx) {
    UltraCanvasContainer::HandleRender(ctx);
}

// Helper functions
std::string RegisterCategoryToString(RegisterCategory category) {
    switch (category) {
        case RegisterCategory::General: return "general";
        case RegisterCategory::Segment: return "segment";
        case RegisterCategory::Flags: return "flags";
        case RegisterCategory::FloatingPoint: return "fpu";
        case RegisterCategory::SSE: return "sse";
        case RegisterCategory::AVX: return "avx";
        case RegisterCategory::AVX512: return "avx512";
        case RegisterCategory::Control: return "control";
        case RegisterCategory::Debug: return "debug";
        case RegisterCategory::System: return "system";
        case RegisterCategory::Other: return "other";
    }
    return "unknown";
}

RegisterCategory StringToRegisterCategory(const std::string& str) {
    if (str == "general") return RegisterCategory::General;
    if (str == "segment") return RegisterCategory::Segment;
    if (str == "flags") return RegisterCategory::Flags;
    if (str == "fpu") return RegisterCategory::FloatingPoint;
    if (str == "sse") return RegisterCategory::SSE;
    if (str == "avx") return RegisterCategory::AVX;
    if (str == "avx512") return RegisterCategory::AVX512;
    if (str == "control") return RegisterCategory::Control;
    if (str == "debug") return RegisterCategory::Debug;
    if (str == "system") return RegisterCategory::System;
    return RegisterCategory::Other;
}

std::string RegisterFormatToString(RegisterFormat format) {
    switch (format) {
        case RegisterFormat::Hexadecimal: return "hex";
        case RegisterFormat::Decimal: return "decimal";
        case RegisterFormat::SignedDecimal: return "signed";
        case RegisterFormat::Binary: return "binary";
        case RegisterFormat::Float: return "float";
        case RegisterFormat::Double: return "double";
    }
    return "hex";
}

RegisterFormat StringToRegisterFormat(const std::string& str) {
    if (str == "decimal") return RegisterFormat::Decimal;
    if (str == "signed") return RegisterFormat::SignedDecimal;
    if (str == "binary") return RegisterFormat::Binary;
    if (str == "float") return RegisterFormat::Float;
    if (str == "double") return RegisterFormat::Double;
    return RegisterFormat::Hexadecimal;
}

} // namespace IDE
} // namespace UltraCanvas
