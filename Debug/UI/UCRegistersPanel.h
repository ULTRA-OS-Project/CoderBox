// Apps/IDE/Debug/UI/UCRegistersPanel.h
// Registers Panel for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasContainer.h"
#include "UltraCanvasTreeView.h"
#include "UltraCanvasToolbar.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasDropDown.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <map>
#include <set>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Register category for grouping
 */
enum class RegisterCategory {
    General,        // GPR: RAX, RBX, RCX, RDX, etc.
    Segment,        // CS, DS, ES, FS, GS, SS
    Flags,          // EFLAGS/RFLAGS
    FloatingPoint,  // FPU: ST0-ST7
    SSE,            // XMM0-XMM15
    AVX,            // YMM0-YMM15
    AVX512,         // ZMM0-ZMM31
    Control,        // CR0-CR4
    Debug,          // DR0-DR7
    System,         // GDTR, LDTR, IDTR, TR
    Other           // Any other registers
};

/**
 * @brief Register display format
 */
enum class RegisterFormat {
    Hexadecimal,
    Decimal,
    SignedDecimal,
    Binary,
    Float,
    Double
};

/**
 * @brief Single register value
 */
struct RegisterValue {
    std::string name;
    std::string value;
    std::string previousValue;
    RegisterCategory category = RegisterCategory::General;
    int bitSize = 64;
    bool changed = false;
    bool valid = true;
    
    RegisterValue() = default;
    RegisterValue(const std::string& n, const std::string& v, RegisterCategory cat = RegisterCategory::General)
        : name(n), value(v), category(cat) {}
};

/**
 * @brief Registers panel using UltraCanvas components
 * 
 * Displays CPU registers grouped by category with support for
 * different display formats and change highlighting.
 */
class UCRegistersPanel : public UltraCanvasContainer {
public:
    UCRegistersPanel(const std::string& id, int x, int y, int width, int height);
    ~UCRegistersPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // REGISTER MANAGEMENT
    // =========================================================================
    
    void RefreshRegisters();
    void SetRegisters(const std::vector<RegisterValue>& registers);
    void UpdateRegister(const std::string& name, const std::string& value);
    void Clear();
    
    // =========================================================================
    // DISPLAY OPTIONS
    // =========================================================================
    
    void SetDisplayFormat(RegisterFormat format);
    RegisterFormat GetDisplayFormat() const { return displayFormat_; }
    
    void SetShowCategory(RegisterCategory category, bool show);
    bool GetShowCategory(RegisterCategory category) const;
    
    void SetHighlightChanges(bool highlight) { highlightChanges_ = highlight; RefreshRegisters(); }
    bool GetHighlightChanges() const { return highlightChanges_; }
    
    void SetGroupByCategory(bool group) { groupByCategory_ = group; RefreshRegisters(); }
    bool GetGroupByCategory() const { return groupByCategory_; }
    
    // =========================================================================
    // REGISTER EDITING
    // =========================================================================
    
    void BeginEditRegister(const std::string& regName);
    void CommitRegisterEdit(const std::string& regName, const std::string& newValue);
    void CancelRegisterEdit();
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const std::string& regName)> onRegisterSelected;
    std::function<void(const std::string& regName, const std::string& newValue)> onRegisterEdited;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateRegisterTree();
    void AddCategoryGroup(RegisterCategory category);
    void AddRegisterNode(const RegisterValue& reg, TreeNodeId parentId = 0);
    std::string FormatRegisterValue(const RegisterValue& reg) const;
    std::string FormatValueAs(const std::string& hexValue, RegisterFormat format, int bitSize) const;
    std::string GetCategoryName(RegisterCategory category) const;
    std::string GetCategoryIcon(RegisterCategory category) const;
    Color GetRegisterColor(const RegisterValue& reg) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
    std::shared_ptr<UltraCanvasDropDown> formatDropDown_;
    std::shared_ptr<UltraCanvasTreeView> registerTree_;
    
    // Register data
    std::vector<RegisterValue> registers_;
    std::map<std::string, RegisterValue> previousRegisters_;
    
    // Node mapping
    std::map<TreeNodeId, std::string> nodeToRegName_;
    std::map<std::string, TreeNodeId> regNameToNode_;
    std::map<RegisterCategory, TreeNodeId> categoryNodes_;
    TreeNodeId nextNodeId_ = 1;
    
    // Display options
    RegisterFormat displayFormat_ = RegisterFormat::Hexadecimal;
    std::set<RegisterCategory> visibleCategories_;
    bool highlightChanges_ = true;
    bool groupByCategory_ = true;
    
    // Edit state
    std::string editingRegister_;
};

/**
 * @brief Helper functions
 */
std::string RegisterCategoryToString(RegisterCategory category);
RegisterCategory StringToRegisterCategory(const std::string& str);
std::string RegisterFormatToString(RegisterFormat format);
RegisterFormat StringToRegisterFormat(const std::string& str);

} // namespace IDE
} // namespace UltraCanvas
