// Apps/IDE/Debug/UI/UCThreadsPanel.h
// Threads Panel for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasContainer.h"
#include "UltraCanvasTreeView.h"
#include "UltraCanvasToolbar.h"
#include "UltraCanvasLabel.h"
#include "../Core/UCDebugManager.h"
#include <functional>
#include <memory>
#include <map>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Thread display state
 */
enum class ThreadDisplayState {
    Running,
    Stopped,
    Suspended,
    Waiting,
    Crashed
};

/**
 * @brief Thread panel item data
 */
struct ThreadPanelItem {
    int threadId = 0;
    std::string name;
    ThreadState state = ThreadState::Stopped;
    std::string location;           // Current function/file:line
    int frameCount = 0;
    bool isCurrent = false;
    
    ThreadPanelItem() = default;
    ThreadPanelItem(const ThreadInfo& info) 
        : threadId(info.id), name(info.name), state(info.state) {}
};

/**
 * @brief Threads panel using UltraCanvas components
 * 
 * Displays all threads in the debugged process with their state,
 * current location, and allows switching between threads.
 */
class UCThreadsPanel : public UltraCanvasContainer {
public:
    UCThreadsPanel(const std::string& id, int x, int y, int width, int height);
    ~UCThreadsPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // THREAD MANAGEMENT
    // =========================================================================
    
    void RefreshThreads();
    void SetThreads(const std::vector<ThreadInfo>& threads);
    void UpdateThread(const ThreadInfo& thread);
    void SetCurrentThread(int threadId);
    int GetCurrentThreadId() const { return currentThreadId_; }
    
    void Clear();
    
    // =========================================================================
    // THREAD OPERATIONS
    // =========================================================================
    
    void SuspendThread(int threadId);
    void ResumeThread(int threadId);
    void SuspendAllThreads();
    void ResumeAllThreads();
    
    // =========================================================================
    // DISPLAY OPTIONS
    // =========================================================================
    
    void SetShowSystemThreads(bool show) { showSystemThreads_ = show; RefreshThreads(); }
    bool GetShowSystemThreads() const { return showSystemThreads_; }
    
    void SetShowThreadLocation(bool show) { showLocation_ = show; RefreshThreads(); }
    bool GetShowThreadLocation() const { return showLocation_; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(int threadId)> onThreadSelected;
    std::function<void(int threadId)> onThreadDoubleClicked;
    std::function<void(int threadId)> onSuspendThread;
    std::function<void(int threadId)> onResumeThread;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateThreadList();
    void AddThreadNode(const ThreadPanelItem& item);
    std::string FormatThreadDisplay(const ThreadPanelItem& item) const;
    std::string GetThreadIcon(const ThreadPanelItem& item) const;
    Color GetThreadColor(const ThreadPanelItem& item) const;
    std::string GetStateText(ThreadState state) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    // UI Components
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasToolbar> toolbar_;
    std::shared_ptr<UltraCanvasTreeView> threadTree_;
    
    // Thread data
    std::vector<ThreadPanelItem> threads_;
    int currentThreadId_ = -1;
    
    // Node mapping
    std::map<TreeNodeId, int> nodeToThreadId_;
    std::map<int, TreeNodeId> threadIdToNode_;
    TreeNodeId nextNodeId_ = 1;
    
    // Display options
    bool showSystemThreads_ = false;
    bool showLocation_ = true;
};

} // namespace IDE
} // namespace UltraCanvas
