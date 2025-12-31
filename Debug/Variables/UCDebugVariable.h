// Apps/IDE/Debug/Variables/UCDebugVariable.h
// Debug Variable Representation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Types/UCDebugStructs.h"
#include <string>
#include <vector>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Enhanced variable class with tree structure support
 */
class UCDebugVariable {
public:
    UCDebugVariable() = default;
    explicit UCDebugVariable(const Variable& var);
    
    // Basic accessors
    const std::string& GetName() const { return data_.name; }
    const std::string& GetValue() const { return data_.value; }
    const std::string& GetType() const { return data_.type; }
    VariableCategory GetCategory() const { return data_.category; }
    
    void SetName(const std::string& name) { data_.name = name; }
    void SetValue(const std::string& value) { data_.value = value; }
    void SetType(const std::string& type) { data_.type = type; }
    void SetCategory(VariableCategory cat) { data_.category = cat; }
    
    // Display
    std::string GetDisplayName() const;
    std::string GetDisplayValue() const;
    std::string GetDisplayType() const;
    std::string GetTooltip() const;
    
    // Children
    bool HasChildren() const { return data_.hasChildren; }
    bool IsExpanded() const { return data_.isExpanded; }
    void SetExpanded(bool expanded) { data_.isExpanded = expanded; }
    int GetChildCount() const { return data_.numChildren; }
    
    const std::vector<UCDebugVariable>& GetChildren() const { return children_; }
    void SetChildren(const std::vector<Variable>& children);
    void ClearChildren();
    
    // State
    bool IsReadOnly() const { return data_.isReadOnly; }
    bool IsPointer() const { return data_.isPointer; }
    bool IsReference() const { return data_.isReference; }
    bool HasError() const { return !data_.errorMessage.empty(); }
    const std::string& GetErrorMessage() const { return data_.errorMessage; }
    
    // Reference for fetching children
    int GetVariablesReference() const { return data_.variablesReference; }
    void SetVariablesReference(int ref) { data_.variablesReference = ref; }
    
    // Evaluation
    const std::string& GetEvaluateName() const { return data_.evaluateName; }
    void SetEvaluateName(const std::string& name) { data_.evaluateName = name; }
    
    // Context
    int GetFrameLevel() const { return data_.frameLevel; }
    int GetThreadId() const { return data_.threadId; }
    void SetContext(int frameLevel, int threadId) {
        data_.frameLevel = frameLevel;
        data_.threadId = threadId;
    }
    
    // Conversion
    const Variable& GetData() const { return data_; }
    Variable& GetData() { return data_; }
    operator Variable() const { return data_; }
    
private:
    Variable data_;
    std::vector<UCDebugVariable> children_;
};

/**
 * @brief Variable scope (locals, arguments, globals)
 */
enum class VariableScope {
    Local,
    Argument,
    Global,
    Watch,
    Hover
};

/**
 * @brief Collection of variables in a scope
 */
struct VariableScopeData {
    VariableScope scope;
    std::string name;
    std::vector<UCDebugVariable> variables;
    int variablesReference = 0;
    bool isExpensive = false;  // Should be fetched lazily
};

} // namespace IDE
} // namespace UltraCanvas
