// Apps/IDE/Debug/UI/UCDebugConsole.h
// Debug Console for ULTRA IDE using UltraCanvas TextArea
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTextArea.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasTextBox.h"
#include "UltraCanvasButton.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <deque>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Output message type
 */
enum class DebugOutputType {
    Info,
    Warning,
    Error,
    DebuggerOutput,
    TargetOutput,
    UserInput
};

/**
 * @brief Debug console entry
 */
struct DebugConsoleEntry {
    std::string message;
    DebugOutputType type;
    std::chrono::system_clock::time_point timestamp;
    
    DebugConsoleEntry(const std::string& msg, DebugOutputType t)
        : message(msg), type(t), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Debug console using UltraCanvasTextArea
 * 
 * Displays debugger output and allows command input.
 * Uses UltraCanvasTextArea for the output display.
 */
class UCDebugConsole : public UltraCanvasContainer {
public:
    UCDebugConsole(const std::string& id, int x, int y, int width, int height);
    ~UCDebugConsole() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // OUTPUT
    // =========================================================================
    
    void AppendOutput(const std::string& text, DebugOutputType type = DebugOutputType::Info);
    void AppendLine(const std::string& text, DebugOutputType type = DebugOutputType::Info);
    void AppendError(const std::string& text);
    void AppendWarning(const std::string& text);
    void Clear();
    
    // =========================================================================
    // INPUT
    // =========================================================================
    
    void SetInputEnabled(bool enabled);
    bool IsInputEnabled() const { return inputEnabled_; }
    void FocusInput();
    
    // =========================================================================
    // SETTINGS
    // =========================================================================
    
    void SetMaxLines(int maxLines) { maxLines_ = maxLines; }
    int GetMaxLines() const { return maxLines_; }
    void SetShowTimestamps(bool show) { showTimestamps_ = show; }
    bool GetShowTimestamps() const { return showTimestamps_; }
    void SetWordWrap(bool wrap);
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const std::string&)> onCommandEntered;
    std::function<void()> onClearRequested;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void UpdateOutput();
    void ProcessCommand(const std::string& command);
    void AddToHistory(const std::string& command);
    std::string GetPreviousCommand();
    std::string GetNextCommand();
    Color GetColorForType(DebugOutputType type) const;
    std::string FormatEntry(const DebugConsoleEntry& entry) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasTextArea> outputArea_;
    std::shared_ptr<UltraCanvasContainer> inputContainer_;
    std::shared_ptr<UltraCanvasLabel> promptLabel_;
    std::shared_ptr<UltraCanvasTextBox> inputBox_;
    std::shared_ptr<UltraCanvasButton> clearButton_;
    
    // State
    std::deque<DebugConsoleEntry> entries_;
    std::deque<std::string> commandHistory_;
    int historyIndex_ = -1;
    int maxLines_ = 1000;
    bool showTimestamps_ = false;
    bool inputEnabled_ = true;
    bool autoScroll_ = true;
};

} // namespace IDE
} // namespace UltraCanvas
