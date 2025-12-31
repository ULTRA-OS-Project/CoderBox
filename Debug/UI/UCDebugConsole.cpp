// Apps/IDE/Debug/UI/UCDebugConsole.cpp
// Debug Console Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDebugConsole.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace UltraCanvas {
namespace IDE {

UCDebugConsole::UCDebugConsole(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    
    SetBackgroundColor(Color(30, 30, 30));  // Dark console background
}

void UCDebugConsole::Initialize() {
    CreateUI();
}

void UCDebugConsole::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        // Subscribe to output
        debugManager_->onOutput = [this](const std::string& output) {
            AppendLine(output, DebugOutputType::TargetOutput);
        };
        
        debugManager_->onError = [this](const std::string& error) {
            AppendError(error);
        };
    }
}

void UCDebugConsole::CreateUI() {
    int yOffset = 4;
    
    // Title bar with clear button
    auto titleBar = std::make_shared<UltraCanvasContainer>(
        "console_titlebar", 0, 0, 0, GetWidth(), 24);
    titleBar->SetBackgroundColor(Color(45, 45, 45));
    
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "console_title", 0, 8, 4, 100, 16);
    titleLabel_->SetText("Debug Console");
    titleLabel_->SetTextColor(Color(200, 200, 200));
    titleLabel_->SetFontSize(11);
    titleBar->AddChild(titleLabel_);
    
    clearButton_ = std::make_shared<UltraCanvasButton>(
        "console_clear", 0, GetWidth() - 60, 2, 50, 20);
    clearButton_->SetText("Clear");
    clearButton_->SetOnClick([this]() {
        Clear();
        if (onClearRequested) onClearRequested();
    });
    titleBar->AddChild(clearButton_);
    
    AddChild(titleBar);
    yOffset += 28;
    
    // Output area using TextArea
    int outputHeight = GetHeight() - yOffset - 32;  // Leave room for input
    outputArea_ = std::make_shared<UltraCanvasTextArea>(
        "console_output", 0, 4, yOffset, GetWidth() - 8, outputHeight);
    outputArea_->SetReadOnly(true);
    outputArea_->SetShowLineNumbers(false);
    outputArea_->SetBackgroundColor(Color(30, 30, 30));
    outputArea_->SetTextColor(Color(200, 200, 200));
    outputArea_->SetFont("Consolas", 11);
    outputArea_->SetWordWrap(true);
    AddChild(outputArea_);
    yOffset += outputHeight + 4;
    
    // Input container
    inputContainer_ = std::make_shared<UltraCanvasContainer>(
        "console_input_container", 0, 0, yOffset, GetWidth(), 28);
    inputContainer_->SetBackgroundColor(Color(40, 40, 40));
    
    // Prompt label
    promptLabel_ = std::make_shared<UltraCanvasLabel>(
        "console_prompt", 0, 8, 6, 20, 16);
    promptLabel_->SetText(">");
    promptLabel_->SetTextColor(Color(100, 200, 100));
    promptLabel_->SetFontSize(12);
    inputContainer_->AddChild(promptLabel_);
    
    // Input text box
    inputBox_ = std::make_shared<UltraCanvasTextBox>(
        "console_input", 0, 28, 4, GetWidth() - 36, 20);
    inputBox_->SetBackgroundColor(Color(50, 50, 50));
    inputBox_->SetTextColor(Color(220, 220, 220));
    inputBox_->SetPlaceholder("Enter command...");
    inputBox_->SetFont("Consolas", 11);
    
    // Handle command entry
    inputBox_->SetOnEnter([this]() {
        std::string command = inputBox_->GetText();
        if (!command.empty()) {
            ProcessCommand(command);
            inputBox_->SetText("");
        }
    });
    
    // Handle history navigation
    inputBox_->SetOnKeyDown([this](int keyCode) {
        if (keyCode == 38) {  // Up arrow
            std::string prev = GetPreviousCommand();
            if (!prev.empty()) {
                inputBox_->SetText(prev);
            }
        } else if (keyCode == 40) {  // Down arrow
            std::string next = GetNextCommand();
            inputBox_->SetText(next);
        }
    });
    
    inputContainer_->AddChild(inputBox_);
    AddChild(inputContainer_);
}

void UCDebugConsole::AppendOutput(const std::string& text, DebugOutputType type) {
    entries_.emplace_back(text, type);
    
    // Trim if too many entries
    while (entries_.size() > static_cast<size_t>(maxLines_)) {
        entries_.pop_front();
    }
    
    UpdateOutput();
}

void UCDebugConsole::AppendLine(const std::string& text, DebugOutputType type) {
    AppendOutput(text + "\n", type);
}

void UCDebugConsole::AppendError(const std::string& text) {
    AppendLine(text, DebugOutputType::Error);
}

void UCDebugConsole::AppendWarning(const std::string& text) {
    AppendLine(text, DebugOutputType::Warning);
}

void UCDebugConsole::Clear() {
    entries_.clear();
    if (outputArea_) {
        outputArea_->SetText("");
    }
}

void UCDebugConsole::SetInputEnabled(bool enabled) {
    inputEnabled_ = enabled;
    
    if (inputContainer_) {
        inputContainer_->SetVisible(enabled);
    }
}

void UCDebugConsole::FocusInput() {
    if (inputBox_ && inputEnabled_) {
        inputBox_->SetFocus(true);
    }
}

void UCDebugConsole::SetWordWrap(bool wrap) {
    if (outputArea_) {
        outputArea_->SetWordWrap(wrap);
    }
}

void UCDebugConsole::UpdateOutput() {
    if (!outputArea_) return;
    
    std::ostringstream ss;
    
    for (const auto& entry : entries_) {
        ss << FormatEntry(entry);
    }
    
    outputArea_->SetText(ss.str());
    
    // Auto-scroll to bottom
    if (autoScroll_) {
        outputArea_->ScrollToEnd();
    }
}

void UCDebugConsole::ProcessCommand(const std::string& command) {
    // Add to history
    AddToHistory(command);
    
    // Echo command
    AppendLine("> " + command, DebugOutputType::UserInput);
    
    // Handle built-in commands
    if (command == "clear" || command == "cls") {
        Clear();
        return;
    }
    
    if (command == "help") {
        AppendLine("Available commands:", DebugOutputType::Info);
        AppendLine("  clear, cls  - Clear console", DebugOutputType::Info);
        AppendLine("  help        - Show this help", DebugOutputType::Info);
        AppendLine("  <expr>      - Evaluate expression", DebugOutputType::Info);
        return;
    }
    
    // Send to debugger for evaluation
    if (debugManager_ && debugManager_->IsDebugging()) {
        auto result = debugManager_->Evaluate(command);
        if (result.success) {
            AppendLine(result.variable.value, DebugOutputType::DebuggerOutput);
        } else {
            AppendError(result.errorMessage);
        }
    } else {
        AppendWarning("No active debug session");
    }
    
    // Call external handler
    if (onCommandEntered) {
        onCommandEntered(command);
    }
}

void UCDebugConsole::AddToHistory(const std::string& command) {
    // Don't add duplicates
    if (!commandHistory_.empty() && commandHistory_.back() == command) {
        return;
    }
    
    commandHistory_.push_back(command);
    
    // Limit history size
    while (commandHistory_.size() > 100) {
        commandHistory_.pop_front();
    }
    
    historyIndex_ = -1;
}

std::string UCDebugConsole::GetPreviousCommand() {
    if (commandHistory_.empty()) return "";
    
    if (historyIndex_ < 0) {
        historyIndex_ = static_cast<int>(commandHistory_.size()) - 1;
    } else if (historyIndex_ > 0) {
        historyIndex_--;
    }
    
    return commandHistory_[historyIndex_];
}

std::string UCDebugConsole::GetNextCommand() {
    if (commandHistory_.empty() || historyIndex_ < 0) return "";
    
    if (historyIndex_ < static_cast<int>(commandHistory_.size()) - 1) {
        historyIndex_++;
        return commandHistory_[historyIndex_];
    } else {
        historyIndex_ = -1;
        return "";
    }
}

Color UCDebugConsole::GetColorForType(DebugOutputType type) const {
    switch (type) {
        case DebugOutputType::Error:
            return Color(255, 100, 100);
        case DebugOutputType::Warning:
            return Color(255, 200, 100);
        case DebugOutputType::DebuggerOutput:
            return Color(150, 200, 255);
        case DebugOutputType::TargetOutput:
            return Color(200, 200, 200);
        case DebugOutputType::UserInput:
            return Color(100, 200, 100);
        case DebugOutputType::Info:
        default:
            return Color(180, 180, 180);
    }
}

std::string UCDebugConsole::FormatEntry(const DebugConsoleEntry& entry) const {
    std::ostringstream ss;
    
    if (showTimestamps_) {
        auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
        auto tm = std::localtime(&time);
        ss << "[" << std::setfill('0')
           << std::setw(2) << tm->tm_hour << ":"
           << std::setw(2) << tm->tm_min << ":"
           << std::setw(2) << tm->tm_sec << "] ";
    }
    
    ss << entry.message;
    
    return ss.str();
}

void UCDebugConsole::HandleRender(IRenderContext* ctx) {
    // Draw panel background
    ctx->DrawFilledRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        GetBackgroundColor());
    
    // Render children
    UltraCanvasContainer::HandleRender(ctx);
}

} // namespace IDE
} // namespace UltraCanvas
