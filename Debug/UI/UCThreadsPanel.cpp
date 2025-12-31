// Apps/IDE/Debug/UI/UCThreadsPanel.cpp
// Threads Panel Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCThreadsPanel.h"
#include <sstream>
#include <algorithm>

namespace UltraCanvas {
namespace IDE {

UCThreadsPanel::UCThreadsPanel(const std::string& id, int x, int y, int width, int height)
    : UltraCanvasContainer(id, 0, x, y, width, height) {
    SetBackgroundColor(Color(250, 250, 250));
}

void UCThreadsPanel::Initialize() {
    CreateUI();
}

void UCThreadsPanel::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        debugManager_->onThreadsUpdated = [this](const std::vector<ThreadInfo>& threads) {
            SetThreads(threads);
        };
        
        debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
            RefreshThreads();
        };
        
        debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
            if (newState == DebugSessionState::Inactive || 
                newState == DebugSessionState::Terminated) {
                Clear();
            }
        };
    }
}

void UCThreadsPanel::CreateUI() {
    int yOffset = 0;
    
    // Title label
    titleLabel_ = std::make_shared<UltraCanvasLabel>(
        "threads_title", 0, 8, yOffset + 4, GetWidth() - 16, 20);
    titleLabel_->SetText("Threads");
    titleLabel_->SetFontWeight(FontWeight::Bold);
    titleLabel_->SetFontSize(12);
    AddChild(titleLabel_);
    yOffset += 28;
    
    // Toolbar
    toolbar_ = std::make_shared<UltraCanvasToolbar>(
        "threads_toolbar", 0, 0, yOffset, GetWidth(), 28);
    toolbar_->SetBackgroundColor(Color(240, 240, 240));
    toolbar_->SetSpacing(2);
    
    auto suspendAllBtn = std::make_shared<UltraCanvasToolbarButton>("suspend_all");
    suspendAllBtn->SetIcon("icons/thread_suspend_all.png");
    suspendAllBtn->SetTooltip("Suspend All Threads");
    suspendAllBtn->SetOnClick([this]() { SuspendAllThreads(); });
    toolbar_->AddButton(suspendAllBtn);
    
    auto resumeAllBtn = std::make_shared<UltraCanvasToolbarButton>("resume_all");
    resumeAllBtn->SetIcon("icons/thread_resume_all.png");
    resumeAllBtn->SetTooltip("Resume All Threads");
    resumeAllBtn->SetOnClick([this]() { ResumeAllThreads(); });
    toolbar_->AddButton(resumeAllBtn);
    
    toolbar_->AddSeparator();
    
    auto refreshBtn = std::make_shared<UltraCanvasToolbarButton>("refresh");
    refreshBtn->SetIcon("icons/refresh.png");
    refreshBtn->SetTooltip("Refresh Thread List");
    refreshBtn->SetOnClick([this]() { RefreshThreads(); });
    toolbar_->AddButton(refreshBtn);
    
    AddChild(toolbar_);
    yOffset += 30;
    
    // Thread tree view
    threadTree_ = std::make_shared<UltraCanvasTreeView>(
        "thread_tree", 0, 0, yOffset, GetWidth(), GetHeight() - yOffset);
    threadTree_->SetShowExpandButtons(false);
    threadTree_->SetShowIcons(true);
    threadTree_->SetRowHeight(22);
    threadTree_->SetSelectionMode(TreeSelectionMode::Single);
    threadTree_->SetBackgroundColor(Colors::White);
    
    threadTree_->onSelectionChange = [this](const std::vector<TreeNodeId>& selected) {
        if (!selected.empty()) {
            auto it = nodeToThreadId_.find(selected[0]);
            if (it != nodeToThreadId_.end()) {
                currentThreadId_ = it->second;
                if (onThreadSelected) {
                    onThreadSelected(it->second);
                }
            }
        }
    };
    
    threadTree_->onNodeDoubleClick = [this](TreeNodeId nodeId) {
        auto it = nodeToThreadId_.find(nodeId);
        if (it != nodeToThreadId_.end()) {
            if (onThreadDoubleClicked) {
                onThreadDoubleClicked(it->second);
            }
            // Switch to this thread
            if (debugManager_) {
                debugManager_->SwitchThread(it->second);
            }
        }
    };
    
    AddChild(threadTree_);
}

void UCThreadsPanel::RefreshThreads() {
    if (!debugManager_ || !debugManager_->IsDebugging()) {
        Clear();
        return;
    }
    
    auto threads = debugManager_->GetThreads();
    SetThreads(threads);
}

void UCThreadsPanel::SetThreads(const std::vector<ThreadInfo>& threads) {
    threads_.clear();
    
    for (const auto& info : threads) {
        // Filter system threads if needed
        if (!showSystemThreads_ && info.name.find("system") != std::string::npos) {
            continue;
        }
        
        ThreadPanelItem item(info);
        
        // Get current location if stopped
        if (info.state == ThreadState::Stopped && debugManager_) {
            auto frames = debugManager_->GetCallStack(info.id);
            if (!frames.empty()) {
                const auto& topFrame = frames[0];
                if (!topFrame.functionName.empty()) {
                    item.location = topFrame.functionName;
                    if (topFrame.hasDebugInfo && topFrame.location.line > 0) {
                        item.location += " at " + topFrame.location.filePath + 
                                         ":" + std::to_string(topFrame.location.line);
                    }
                }
                item.frameCount = static_cast<int>(frames.size());
            }
        }
        
        // Check if current thread
        item.isCurrent = (info.id == currentThreadId_);
        
        threads_.push_back(item);
    }
    
    // Update current thread ID if not set
    if (currentThreadId_ < 0 && !threads_.empty()) {
        currentThreadId_ = threads_[0].threadId;
        threads_[0].isCurrent = true;
    }
    
    PopulateThreadList();
}

void UCThreadsPanel::UpdateThread(const ThreadInfo& thread) {
    for (auto& item : threads_) {
        if (item.threadId == thread.id) {
            item.state = thread.state;
            item.name = thread.name;
            break;
        }
    }
    PopulateThreadList();
}

void UCThreadsPanel::SetCurrentThread(int threadId) {
    if (currentThreadId_ == threadId) return;
    
    // Update current flags
    for (auto& item : threads_) {
        item.isCurrent = (item.threadId == threadId);
    }
    
    currentThreadId_ = threadId;
    PopulateThreadList();
    
    // Select in tree
    auto it = threadIdToNode_.find(threadId);
    if (it != threadIdToNode_.end()) {
        threadTree_->SetSelectedNode(it->second);
    }
}

void UCThreadsPanel::Clear() {
    threads_.clear();
    threadTree_->Clear();
    nodeToThreadId_.clear();
    threadIdToNode_.clear();
    nextNodeId_ = 1;
    currentThreadId_ = -1;
    
    titleLabel_->SetText("Threads");
}

void UCThreadsPanel::PopulateThreadList() {
    threadTree_->Clear();
    nodeToThreadId_.clear();
    threadIdToNode_.clear();
    nextNodeId_ = 1;
    
    // Sort threads - current first, then by ID
    std::vector<ThreadPanelItem> sorted = threads_;
    std::sort(sorted.begin(), sorted.end(), [](const ThreadPanelItem& a, const ThreadPanelItem& b) {
        if (a.isCurrent != b.isCurrent) return a.isCurrent;
        return a.threadId < b.threadId;
    });
    
    for (const auto& item : sorted) {
        AddThreadNode(item);
    }
    
    // Update title with count
    titleLabel_->SetText("Threads (" + std::to_string(threads_.size()) + ")");
}

void UCThreadsPanel::AddThreadNode(const ThreadPanelItem& item) {
    TreeNodeId nodeId = nextNodeId_++;
    
    TreeNodeData nodeData;
    nodeData.text = FormatThreadDisplay(item);
    nodeData.textColor = GetThreadColor(item);
    
    // Set icon
    nodeData.leftIcon.iconPath = GetThreadIcon(item);
    nodeData.leftIcon.width = 16;
    nodeData.leftIcon.height = 16;
    nodeData.leftIcon.visible = true;
    
    // Bold for current thread
    if (item.isCurrent) {
        nodeData.fontWeight = FontWeight::Bold;
    }
    
    threadTree_->AddNode(nodeId, nodeData);
    
    nodeToThreadId_[nodeId] = item.threadId;
    threadIdToNode_[item.threadId] = nodeId;
}

std::string UCThreadsPanel::FormatThreadDisplay(const ThreadPanelItem& item) const {
    std::ostringstream ss;
    
    // Thread ID and name
    ss << "Thread " << item.threadId;
    if (!item.name.empty()) {
        ss << " [" << item.name << "]";
    }
    
    // State
    ss << " - " << GetStateText(item.state);
    
    // Location (if showing and available)
    if (showLocation_ && !item.location.empty()) {
        ss << " in " << item.location;
    }
    
    // Frame count
    if (item.frameCount > 0) {
        ss << " (" << item.frameCount << " frames)";
    }
    
    // Current indicator
    if (item.isCurrent) {
        ss << " ←";
    }
    
    return ss.str();
}

std::string UCThreadsPanel::GetThreadIcon(const ThreadPanelItem& item) const {
    if (item.isCurrent) {
        return "icons/thread_current.png";
    }
    
    switch (item.state) {
        case ThreadState::Running:
            return "icons/thread_running.png";
        case ThreadState::Stopped:
            return "icons/thread_stopped.png";
        case ThreadState::Suspended:
            return "icons/thread_suspended.png";
        case ThreadState::Crashed:
            return "icons/thread_crashed.png";
        case ThreadState::Exited:
            return "icons/thread_exited.png";
        default:
            return "icons/thread.png";
    }
}

Color UCThreadsPanel::GetThreadColor(const ThreadPanelItem& item) const {
    if (item.isCurrent) {
        return Color(0, 100, 0);  // Dark green for current
    }
    
    switch (item.state) {
        case ThreadState::Running:
            return Color(0, 128, 0);  // Green
        case ThreadState::Stopped:
            return Color(0, 0, 0);    // Black
        case ThreadState::Suspended:
            return Color(128, 128, 0); // Olive
        case ThreadState::Crashed:
            return Color(200, 0, 0);  // Red
        case ThreadState::Exited:
            return Color(128, 128, 128); // Gray
        default:
            return Color(0, 0, 0);
    }
}

std::string UCThreadsPanel::GetStateText(ThreadState state) const {
    switch (state) {
        case ThreadState::Running: return "Running";
        case ThreadState::Stopped: return "Stopped";
        case ThreadState::Suspended: return "Suspended";
        case ThreadState::Crashed: return "Crashed";
        case ThreadState::Exited: return "Exited";
        case ThreadState::UnknownState: return "Unknown";
    }
    return "Unknown";
}

void UCThreadsPanel::SuspendThread(int threadId) {
    if (debugManager_) {
        debugManager_->SuspendThread(threadId);
    }
    if (onSuspendThread) {
        onSuspendThread(threadId);
    }
}

void UCThreadsPanel::ResumeThread(int threadId) {
    if (debugManager_) {
        debugManager_->ResumeThread(threadId);
    }
    if (onResumeThread) {
        onResumeThread(threadId);
    }
}

void UCThreadsPanel::SuspendAllThreads() {
    for (const auto& item : threads_) {
        if (item.state == ThreadState::Running) {
            SuspendThread(item.threadId);
        }
    }
}

void UCThreadsPanel::ResumeAllThreads() {
    for (const auto& item : threads_) {
        if (item.state == ThreadState::Suspended) {
            ResumeThread(item.threadId);
        }
    }
}

void UCThreadsPanel::HandleRender(IRenderContext* ctx) {
    UltraCanvasContainer::HandleRender(ctx);
}

} // namespace IDE
} // namespace UltraCanvas
