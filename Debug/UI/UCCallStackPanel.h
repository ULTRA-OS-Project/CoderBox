// Apps/IDE/Debug/UI/UCCallStackPanel.h
// Call Stack Panel for ULTRA IDE using UltraCanvas TreeView
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UltraCanvasTreeView.h"
#include "UltraCanvasContainer.h"
#include "UltraCanvasLabel.h"
#include "UltraCanvasDropDown.h"
#include "../Core/UCDebugManager.h"
#include "../Stack/UCStackFrame.h"
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Call stack panel using UltraCanvasTreeView
 * 
 * Displays the current call stack with thread selection.
 * Uses UltraCanvasTreeView to show stack frames.
 */
class UCCallStackPanel : public UltraCanvasContainer {
public:
    UCCallStackPanel(const std::string& id, int x, int y, int width, int height);
    ~UCCallStackPanel() override = default;
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void Initialize();
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // DATA UPDATES
    // =========================================================================
    
    void SetCallStack(const std::vector<StackFrame>& frames);
    void SetThreads(const std::vector<ThreadInfo>& threads);
    void Clear();
    
    void RefreshFromDebugger();
    void SetCurrentFrame(int level);
    
    // =========================================================================
    // SELECTION
    // =========================================================================
    
    int GetSelectedFrameLevel() const { return selectedFrameLevel_; }
    int GetSelectedThreadId() const { return selectedThreadId_; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(int frameLevel)> onFrameSelected;
    std::function<void(int threadId)> onThreadSelected;
    std::function<void(const StackFrame&)> onFrameDoubleClicked;
    
protected:
    void HandleRender(IRenderContext* ctx) override;
    
private:
    void CreateUI();
    void PopulateCallStack(const std::vector<StackFrame>& frames);
    void PopulateThreadDropdown(const std::vector<ThreadInfo>& threads);
    std::string FormatFrame(const StackFrame& frame) const;
    
    UCDebugManager* debugManager_ = nullptr;
    
    // UI Components (UltraCanvas elements)
    std::shared_ptr<UltraCanvasLabel> titleLabel_;
    std::shared_ptr<UltraCanvasDropDown> threadDropdown_;
    std::shared_ptr<UltraCanvasTreeView> callStackTree_;
    
    // State
    std::vector<StackFrame> currentFrames_;
    std::vector<ThreadInfo> currentThreads_;
    int selectedFrameLevel_ = 0;
    int selectedThreadId_ = 1;
};

} // namespace IDE
} // namespace UltraCanvas
