// Apps/IDE/Debug/DAP/UCDAPServer.h
// Debug Adapter Protocol Server for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCDAPTypes.h"
#include "../Core/UCDebugManager.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>

namespace UltraCanvas {
namespace IDE {
namespace DAP {

/**
 * @brief DAP server transport type
 */
enum class DAPTransport {
    Stdio,              // Standard input/output
    Socket,             // TCP socket
    Pipe                // Named pipe
};

/**
 * @brief DAP server configuration
 */
struct DAPServerConfig {
    DAPTransport transport = DAPTransport::Stdio;
    int port = 4711;                    // For socket transport
    std::string pipeName;               // For pipe transport
    bool logMessages = false;
    std::string logFile;
};

/**
 * @brief DAP Server
 * 
 * Implements the Debug Adapter Protocol server that can communicate
 * with VS Code and other DAP clients. Bridges between DAP messages
 * and the UCDebugManager.
 */
class UCDAPServer {
public:
    UCDAPServer();
    ~UCDAPServer();
    
    // =========================================================================
    // SERVER LIFECYCLE
    // =========================================================================
    
    /**
     * @brief Initialize the server
     */
    bool Initialize(const DAPServerConfig& config);
    
    /**
     * @brief Start the server
     */
    bool Start();
    
    /**
     * @brief Stop the server
     */
    void Stop();
    
    /**
     * @brief Check if server is running
     */
    bool IsRunning() const { return running_; }
    
    /**
     * @brief Set the debug manager
     */
    void SetDebugManager(UCDebugManager* manager);
    
    // =========================================================================
    // MESSAGE HANDLING
    // =========================================================================
    
    /**
     * @brief Process a single message (for testing)
     */
    Response ProcessMessage(const std::string& messageJson);
    
    /**
     * @brief Send an event to the client
     */
    void SendEvent(const Event& event);
    
    // =========================================================================
    // CAPABILITIES
    // =========================================================================
    
    /**
     * @brief Get server capabilities
     */
    const Capabilities& GetCapabilities() const { return capabilities_; }
    
    /**
     * @brief Set custom capabilities
     */
    void SetCapabilities(const Capabilities& caps) { capabilities_ = caps; }
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const std::string&)> onMessageReceived;
    std::function<void(const std::string&)> onMessageSent;
    std::function<void(const std::string&)> onError;
    std::function<void()> onClientConnected;
    std::function<void()> onClientDisconnected;
    
private:
    // Message processing
    void MessageLoop();
    void ProcessRequest(const Request& request);
    Response CreateErrorResponse(const Request& request, const std::string& message);
    void SendResponse(const Response& response);
    void SendMessage(const std::string& json);
    
    // Request handlers
    Response HandleInitialize(const Request& request);
    Response HandleLaunch(const Request& request);
    Response HandleAttach(const Request& request);
    Response HandleDisconnect(const Request& request);
    Response HandleConfigurationDone(const Request& request);
    Response HandleSetBreakpoints(const Request& request);
    Response HandleSetFunctionBreakpoints(const Request& request);
    Response HandleSetExceptionBreakpoints(const Request& request);
    Response HandleContinue(const Request& request);
    Response HandleNext(const Request& request);
    Response HandleStepIn(const Request& request);
    Response HandleStepOut(const Request& request);
    Response HandlePause(const Request& request);
    Response HandleTerminate(const Request& request);
    Response HandleRestart(const Request& request);
    Response HandleThreads(const Request& request);
    Response HandleStackTrace(const Request& request);
    Response HandleScopes(const Request& request);
    Response HandleVariables(const Request& request);
    Response HandleSetVariable(const Request& request);
    Response HandleSource(const Request& request);
    Response HandleEvaluate(const Request& request);
    Response HandleCompletions(const Request& request);
    Response HandleModules(const Request& request);
    Response HandleLoadedSources(const Request& request);
    Response HandleReadMemory(const Request& request);
    Response HandleWriteMemory(const Request& request);
    Response HandleDisassemble(const Request& request);
    Response HandleExceptionInfo(const Request& request);
    Response HandleDataBreakpointInfo(const Request& request);
    Response HandleSetDataBreakpoints(const Request& request);
    Response HandleSetInstructionBreakpoints(const Request& request);
    Response HandleBreakpointLocations(const Request& request);
    
    // Event helpers
    void SendInitializedEvent();
    void SendStoppedEvent(DAPStopReason reason, int threadId, const std::string& text = "");
    void SendContinuedEvent(int threadId, bool allThreadsContinued = true);
    void SendExitedEvent(int exitCode);
    void SendTerminatedEvent(bool restart = false);
    void SendThreadEvent(const std::string& reason, int threadId);
    void SendOutputEvent(const std::string& output, OutputCategory category = OutputCategory::Console);
    void SendBreakpointEvent(const std::string& reason, const Breakpoint& bp);
    void SendModuleEvent(const std::string& reason, const Module& module);
    void SendLoadedSourceEvent(const std::string& reason, const Source& source);
    void SendProcessEvent(const std::string& name, int processId, bool isLocalProcess = true);
    void SendCapabilitiesEvent();
    void SendProgressEvent(const std::string& progressId, const std::string& title, 
                           const std::string& message = "", int percentage = -1);
    void SendInvalidatedEvent(InvalidatedAreas areas = InvalidatedAreas::All, int threadId = 0);
    void SendMemoryEvent(const std::string& memoryReference, int offset, int count);
    
    // Debug manager integration
    void SetupDebugManagerCallbacks();
    void OnDebugStateChange(DebugSessionState oldState, DebugSessionState newState);
    void OnTargetStop(StopReason reason, const SourceLocation& location);
    void OnBreakpointHit(int breakpointId);
    void OnOutput(const std::string& text, bool isError);
    
    // I/O
    bool ReadMessage(std::string& message);
    bool WriteMessage(const std::string& message);
    std::string ReadLine();
    
    // Transport-specific
    bool InitializeStdio();
    bool InitializeSocket();
    bool InitializePipe();
    
    // Configuration
    DAPServerConfig config_;
    Capabilities capabilities_;
    UCDebugManager* debugManager_ = nullptr;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> configurationDone_{false};
    int nextSeq_ = 1;
    
    // Threading
    std::thread messageThread_;
    std::mutex sendMutex_;
    std::queue<std::string> messageQueue_;
    std::mutex queueMutex_;
    
    // Variable references (for scopes/variables)
    struct VariableReference {
        enum Type { Scope, Variable, Register };
        Type type;
        int frameId = 0;
        int threadId = 0;
        std::string path;               // Variable path for expansion
    };
    std::map<int, VariableReference> variableReferences_;
    int nextVariableRef_ = 1;
    
    // Frame ID mapping
    std::map<int, std::pair<int, int>> frameIdToThreadFrame_;  // frameId -> (threadId, frameIndex)
    int nextFrameId_ = 1;
    
    // Socket (if using socket transport)
    int serverSocket_ = -1;
    int clientSocket_ = -1;
};

} // namespace DAP
} // namespace IDE
} // namespace UltraCanvas
