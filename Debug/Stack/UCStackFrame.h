// Apps/IDE/Debug/Stack/UCStackFrame.h
// Stack Frame Representation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Types/UCDebugStructs.h"
#include <string>
#include <vector>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Enhanced stack frame class
 */
class UCStackFrame {
public:
    UCStackFrame() = default;
    explicit UCStackFrame(const StackFrame& frame);
    
    // Basic accessors
    int GetLevel() const { return data_.level; }
    int GetThreadId() const { return data_.threadId; }
    const std::string& GetFunctionName() const { return data_.functionName; }
    const std::string& GetFullName() const { return data_.fullName; }
    const SourceLocation& GetLocation() const { return data_.location; }
    uint64_t GetAddress() const { return data_.address; }
    uint64_t GetFrameBase() const { return data_.frameBase; }
    const std::string& GetModuleName() const { return data_.moduleName; }
    const std::string& GetArguments() const { return data_.args; }
    
    // State
    bool HasDebugInfo() const { return data_.hasDebugInfo; }
    bool HasSource() const { return data_.HasSource(); }
    bool IsInlined() const { return data_.isInlined; }
    bool IsArtificial() const { return data_.isArtificial; }
    bool IsCurrent() const { return isCurrent_; }
    void SetCurrent(bool current) { isCurrent_ = current; }
    
    // Display
    std::string GetDisplayText() const;
    std::string GetShortDisplayText() const;
    std::string GetAddressText() const;
    std::string GetTooltip() const;
    
    // Icon
    std::string GetIconName() const;
    
    // Conversion
    const StackFrame& GetData() const { return data_; }
    StackFrame& GetData() { return data_; }
    operator StackFrame() const { return data_; }
    
private:
    StackFrame data_;
    bool isCurrent_ = false;
};

/**
 * @brief Call stack for a thread
 */
class UCCallStack {
public:
    UCCallStack() = default;
    
    // Thread info
    int GetThreadId() const { return threadId_; }
    void SetThreadId(int id) { threadId_ = id; }
    const std::string& GetThreadName() const { return threadName_; }
    void SetThreadName(const std::string& name) { threadName_ = name; }
    
    // Frames
    void SetFrames(const std::vector<StackFrame>& frames);
    void Clear();
    
    size_t GetFrameCount() const { return frames_.size(); }
    const UCStackFrame* GetFrame(int level) const;
    UCStackFrame* GetFrame(int level);
    const std::vector<UCStackFrame>& GetAllFrames() const { return frames_; }
    
    // Current frame
    int GetCurrentLevel() const { return currentLevel_; }
    bool SetCurrentLevel(int level);
    const UCStackFrame* GetCurrentFrame() const;
    
    // Helpers
    bool IsEmpty() const { return frames_.empty(); }
    const UCStackFrame* GetTopFrame() const;
    
private:
    int threadId_ = 0;
    std::string threadName_;
    std::vector<UCStackFrame> frames_;
    int currentLevel_ = 0;
};

/**
 * @brief Thread with its call stack
 */
class UCDebugThread {
public:
    UCDebugThread() = default;
    explicit UCDebugThread(const ThreadInfo& info);
    
    // Thread info
    int GetId() const { return info_.id; }
    int GetSystemId() const { return info_.systemId; }
    const std::string& GetName() const { return info_.name; }
    DebugSessionState GetState() const { return info_.state; }
    StopReason GetStopReason() const { return info_.stopReason; }
    
    bool IsCurrent() const { return info_.isCurrent; }
    void SetCurrent(bool current) { info_.isCurrent = current; }
    bool IsMain() const { return info_.isMain; }
    bool IsStopped() const { return info_.IsStopped(); }
    
    // Call stack
    UCCallStack& GetCallStack() { return callStack_; }
    const UCCallStack& GetCallStack() const { return callStack_; }
    
    // Display
    std::string GetDisplayText() const;
    std::string GetTooltip() const;
    std::string GetIconName() const;
    
    // Conversion
    const ThreadInfo& GetInfo() const { return info_; }
    void UpdateInfo(const ThreadInfo& info) { info_ = info; }
    
private:
    ThreadInfo info_;
    UCCallStack callStack_;
};

} // namespace IDE
} // namespace UltraCanvas
