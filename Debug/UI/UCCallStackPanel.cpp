// Apps/IDE/Debug/UI/UCCallStackPanel.cpp
// Call Stack Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCCallStackPanel.h"
#include <sstream>
#include <iomanip>

namespace UltraCanvas {
namespace IDE {

UCCallStackPanel::UCCallStackPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    
    SetBackgroundColor(Color(250, 250, 250));
}

void UCCallStackPanel::Initialize() {
    CreateUI();
}

void UCCallStackPanel::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        // Subscribe to stop events to refresh call stack
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            RefreshFromDebugger();
        };
    }
}

void UCCallStackPanel::CreateUI() {
    int yOffset = 4;
    
    // Title label
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "callstack_title", 0, 4, yOffset, GetWidth() - 8, 20);
    titleLabel_->SetText("Call Stack");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetFontSize(12);
    AddChild(titleLabel_);
    yOffset += 24;
    
    // Thread dropdown
    threadDropdown_ = std::make_shared<UltraCanvasDropDown>(
        "thread_dropdown", 0, 4, yOffset, GetWidth() - 8, 24);
    threadDropdown_->SetOnSelectionChange([this](int index, const std::string& item) {
        if (index >= 0 && index < static_cast<int>(currentThreads_.size())) {
            selectedThreadId_ = currentThreads_[index].id;
            if (onThreadSelected) {
                onThreadSelected(selectedThreadId_);
            }
            RefreshFromDebugger();
        }
    });
    AddChild(threadDropdown_);
    yOffset += 28;
    
    // Call stack tree view
    callStackTree_ = std::make_shared<UltraCanvasTreeView>(
        "callstack_tree", 0, 4, yOffset, GetWidth() - 8, GetHeight() - yOffset - 4);
    callStackTree_->SetShowExpandButtons(false);  // Flat list, no expansion
    callStackTree_->SetShowIcons(true);
    callStackTree_->SetRowHeight(20);
    callStackTree_->SetSelectionMode(TreeSelectionMode::Single);
    
    // Handle frame selection
    callStackTree_->onSelectionChange = [this](const std::vector<TreeNodeId>& selected) {
        if (!selected.empty()) {
            // Node ID is the frame level
            selectedFrameLevel_ = static_cast<int>(selected[0]);
            if (onFrameSelected) {
                onFrameSelected(selectedFrameLevel_);
            }
        }
    };
    
    // Handle double-click to navigate to source
    callStackTree_->onNodeDoubleClick = [this](TreeNodeId nodeId) {
        int level = static_cast<int>(nodeId);
        if (level >= 0 && level < static_cast<int>(currentFrames_.size())) {
            if (onFrameDoubleClicked) {
                onFrameDoubleClicked(currentFrames_[level]);
            }
        }
    };
    
    AddChild(callStackTree_);
}

void UCCallStackPanel::SetCallStack(const std::vector<StackFrame>& frames) {
    currentFrames_ = frames;
    PopulateCallStack(frames);
}

void UCCallStackPanel::SetThreads(const std::vector<ThreadInfo>& threads) {
    currentThreads_ = threads;
    PopulateThreadDropdown(threads);
}

void UCCallStackPanel::Clear() {
    currentFrames_.clear();
    currentThreads_.clear();
    
    if (callStackTree_) {
        callStackTree_->Clear();
    }
    
    if (threadDropdown_) {
        threadDropdown_->ClearItems();
        threadDropdown_->AddItem("No threads");
    }
}

void UCCallStackPanel::RefreshFromDebugger() {
    if (!debugManager_ || !debugManager_->IsDebugging()) {
        Clear();
        return;
    }
    
    // Get threads
    auto threads = debugManager_->GetThreads();
    SetThreads(threads);
    
    // Get call stack for current thread
    auto frames = debugManager_->GetCallStack();
    SetCallStack(frames);
}

void UCCallStackPanel::SetCurrentFrame(int level) {
    selectedFrameLevel_ = level;
    
    if (callStackTree_) {
        callStackTree_->SetSelectedNode(static_cast<TreeNodeId>(level));
    }
}

void UCCallStackPanel::PopulateCallStack(const std::vector<StackFrame>& frames) {
    if (!callStackTree_) return;
    
    callStackTree_->Clear();
    
    for (const auto& frame : frames) {
        TreeNodeData nodeData;
        nodeData.text = FormatFrame(frame);
        
        // Set icon based on whether we have debug info
        if (frame.hasDebugInfo) {
            nodeData.leftIcon.iconPath = "icons/stack_frame.png";
        } else {
            nodeData.leftIcon.iconPath = "icons/stack_frame_no_source.png";
        }
        nodeData.leftIcon.width = 16;
        nodeData.leftIcon.height = 16;
        nodeData.leftIcon.visible = true;
        
        // Use frame level as node ID
        callStackTree_->AddNode(static_cast<TreeNodeId>(frame.level), nodeData);
    }
    
    // Select top frame
    if (!frames.empty()) {
        callStackTree_->SetSelectedNode(0);
        selectedFrameLevel_ = 0;
    }
}

void UCCallStackPanel::PopulateThreadDropdown(const std::vector<ThreadInfo>& threads) {
    if (!threadDropdown_) return;
    
    threadDropdown_->ClearItems();
    
    int currentIndex = 0;
    for (size_t i = 0; i < threads.size(); i++) {
        const auto& thread = threads[i];
        
        std::ostringstream label;
        label << "Thread " << thread.id;
        if (!thread.name.empty()) {
            label << " - " << thread.name;
        }
        if (thread.isCurrent) {
            label << " *";
            currentIndex = static_cast<int>(i);
        }
        
        threadDropdown_->AddItem(label.str());
    }
    
    if (!threads.empty()) {
        threadDropdown_->SetSelectedIndex(currentIndex);
        selectedThreadId_ = threads[currentIndex].id;
    }
}

std::string UCCallStackPanel::FormatFrame(const StackFrame& frame) const {
    std::ostringstream ss;
    
    // Frame level
    ss << "[" << frame.level << "] ";
    
    // Function name
    if (!frame.functionName.empty()) {
        ss << frame.functionName;
        
        // Arguments if available
        if (!frame.args.empty()) {
            ss << "(" << frame.args << ")";
        }
    } else {
        // No function name, show address
        ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << frame.address;
    }
    
    // Location
    if (frame.hasDebugInfo && !frame.location.filePath.empty()) {
        // Get just the filename
        std::string filename = frame.location.filePath;
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }
        ss << " at " << filename << ":" << frame.location.line;
    }
    
    return ss.str();
}

void UCCallStackPanel::HandleRender(IRenderContext* ctx) {
    // Draw panel background
    ctx->DrawFilledRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        GetBackgroundColor());
    
    // Draw border
    ctx->DrawRectangle(
        Rect2Di(GetX(), GetY(), GetWidth(), GetHeight()),
        Color(200, 200, 200), 1);
    
    // Render children (label, dropdown, tree)
    UltraCanvasContainer::HandleRender(ctx);
}

} // namespace IDE
} // namespace UltraCanvas
