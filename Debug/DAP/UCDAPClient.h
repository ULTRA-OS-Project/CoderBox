// Apps/IDE/Debug/DAP/UCDAPClient.h
// Debug Adapter Protocol Client for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCDAPTypes.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <queue>

namespace UltraCanvas {
namespace IDE {
namespace DAP {

/**
 * @brief DAP client connection state
 */
enum class DAPClientState {
    Disconnected,
    Connecting,
    Connected,
    Initializing,
    Ready,
    Debugging,
    Error
};

/**
 * @brief Pending request tracker
 */
struct PendingRequest {
    int seq;
    std::string command;
    std::promise<Response> promise;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief DAP Client
 * 
 * Connects to external debug adapters (e.g., VS Code debug adapters)
 * and translates DAP messages to/from the IDE's debug interface.
 */
class UCDAPClient {
public:
    UCDAPClient();
    ~UCDAPClient();
    
    // =========================================================================
    // CONNECTION
    // =========================================================================
    
    /**
     * @brief Connect to adapter via stdio
     */
    bool ConnectStdio(const std::string& adapterPath, 
                      const std::vector<std::string>& args = {});
    
    /**
     * @brief Connect to adapter via socket
     */
    bool ConnectSocket(const std::string& host, int port);
    
    /**
     * @brief Disconnect from adapter
     */
    void Disconnect();
    
    /**
     * @brief Get connection state
     */
    DAPClientState GetState() const { return state_; }
    
    /**
     * @brief Check if connected
     */
    bool IsConnected() const { return state_ >= DAPClientState::Connected; }
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    /**
     * @brief Initialize the debug adapter
     */
    std::future<Response> Initialize(const std::string& adapterID = "ultra-ide",
                                      const std::string& clientName = "ULTRA IDE");
    
    /**
     * @brief Signal configuration complete
     */
    std::future<Response> ConfigurationDone();
    
    /**
     * @brief Get adapter capabilities
     */
    const Capabilities& GetCapabilities() const { return capabilities_; }
    
    // =========================================================================
    // LAUNCH/ATTACH
    // =========================================================================
    
    /**
     * @brief Launch a program
     */
    std::future<Response> Launch(const JsonObject& args);
    
    /**
     * @brief Attach to a process
     */
    std::future<Response> Attach(const JsonObject& args);
    
    /**
     * @brief Restart the debug session
     */
    std::future<Response> Restart(const JsonObject& args = {});
    
    /**
     * @brief Terminate the debuggee
     */
    std::future<Response> Terminate(bool restart = false);
    
    /**
     * @brief Disconnect from debuggee
     */
    std::future<Response> DisconnectRequest(bool terminateDebuggee = false,
                                             bool suspendDebuggee = false);
    
    // =========================================================================
    // BREAKPOINTS
    // =========================================================================
    
    /**
     * @brief Set source breakpoints
     */
    std::future<Response> SetBreakpoints(const Source& source,
                                          const std::vector<SourceBreakpoint>& breakpoints);
    
    /**
     * @brief Set function breakpoints
     */
    std::future<Response> SetFunctionBreakpoints(const std::vector<FunctionBreakpoint>& breakpoints);
    
    /**
     * @brief Set exception breakpoints
     */
    std::future<Response> SetExceptionBreakpoints(const std::vector<std::string>& filters,
                                                   const std::vector<ExceptionFilterOptions>& filterOptions = {},
                                                   const std::vector<ExceptionOptions>& exceptionOptions = {});
    
    /**
     * @brief Set data breakpoints
     */
    std::future<Response> SetDataBreakpoints(const std::vector<DataBreakpoint>& breakpoints);
    
    /**
     * @brief Set instruction breakpoints
     */
    std::future<Response> SetInstructionBreakpoints(const std::vector<InstructionBreakpoint>& breakpoints);
    
    /**
     * @brief Get possible breakpoint locations
     */
    std::future<Response> BreakpointLocations(const Source& source, int line,
                                               int column = 0, int endLine = 0, int endColumn = 0);
    
    /**
     * @brief Get data breakpoint info
     */
    std::future<Response> DataBreakpointInfo(int variablesReference, const std::string& name,
                                              int frameId = 0);
    
    // =========================================================================
    // EXECUTION CONTROL
    // =========================================================================
    
    /**
     * @brief Continue execution
     */
    std::future<Response> Continue(int threadId = 0, bool singleThread = false);
    
    /**
     * @brief Step over (next)
     */
    std::future<Response> Next(int threadId, SteppingGranularity granularity = SteppingGranularity::Statement,
                               bool singleThread = false);
    
    /**
     * @brief Step into
     */
    std::future<Response> StepIn(int threadId, int targetId = 0,
                                  SteppingGranularity granularity = SteppingGranularity::Statement,
                                  bool singleThread = false);
    
    /**
     * @brief Step out
     */
    std::future<Response> StepOut(int threadId, SteppingGranularity granularity = SteppingGranularity::Statement,
                                   bool singleThread = false);
    
    /**
     * @brief Pause execution
     */
    std::future<Response> Pause(int threadId = 0);
    
    /**
     * @brief Reverse continue (step back)
     */
    std::future<Response> ReverseContinue(int threadId);
    
    /**
     * @brief Step back
     */
    std::future<Response> StepBack(int threadId, SteppingGranularity granularity = SteppingGranularity::Statement);
    
    // =========================================================================
    // STATE INSPECTION
    // =========================================================================
    
    /**
     * @brief Get threads
     */
    std::future<Response> Threads();
    
    /**
     * @brief Get stack trace
     */
    std::future<Response> StackTrace(int threadId, int startFrame = 0, int levels = 20,
                                      const ValueFormat& format = {});
    
    /**
     * @brief Get scopes for a frame
     */
    std::future<Response> Scopes(int frameId);
    
    /**
     * @brief Get variables
     */
    std::future<Response> Variables(int variablesReference, const std::string& filter = "",
                                     int start = 0, int count = 0, const ValueFormat& format = {});
    
    /**
     * @brief Set variable value
     */
    std::future<Response> SetVariable(int variablesReference, const std::string& name,
                                       const std::string& value, const ValueFormat& format = {});
    
    /**
     * @brief Evaluate expression
     */
    std::future<Response> Evaluate(const std::string& expression, int frameId = 0,
                                    EvaluateContext context = EvaluateContext::Watch,
                                    const ValueFormat& format = {});
    
    /**
     * @brief Set expression value
     */
    std::future<Response> SetExpression(int frameId, const std::string& expression,
                                         const std::string& value, const ValueFormat& format = {});
    
    /**
     * @brief Get step-in targets
     */
    std::future<Response> StepInTargets(int frameId);
    
    /**
     * @brief Get goto targets
     */
    std::future<Response> GotoTargets(const Source& source, int line, int column = 0);
    
    /**
     * @brief Goto location
     */
    std::future<Response> Goto(int threadId, int targetId);
    
    // =========================================================================
    // SOURCE & MODULES
    // =========================================================================
    
    /**
     * @brief Get source content
     */
    std::future<Response> GetSource(const Source& source);
    
    /**
     * @brief Get loaded sources
     */
    std::future<Response> LoadedSources();
    
    /**
     * @brief Get modules
     */
    std::future<Response> Modules(int startModule = 0, int moduleCount = 0);
    
    // =========================================================================
    // MEMORY & DISASSEMBLY
    // =========================================================================
    
    /**
     * @brief Read memory
     */
    std::future<Response> ReadMemory(const std::string& memoryReference, int offset = 0, int count = 256);
    
    /**
     * @brief Write memory
     */
    std::future<Response> WriteMemory(const std::string& memoryReference, int offset, const std::string& data,
                                       bool allowPartial = false);
    
    /**
     * @brief Disassemble
     */
    std::future<Response> Disassemble(const std::string& memoryReference, int offset = 0,
                                       int instructionOffset = 0, int instructionCount = 50,
                                       bool resolveSymbols = true);
    
    // =========================================================================
    // EXCEPTIONS
    // =========================================================================
    
    /**
     * @brief Get exception info
     */
    std::future<Response> ExceptionInfo(int threadId);
    
    // =========================================================================
    // COMPLETIONS
    // =========================================================================
    
    /**
     * @brief Get completions
     */
    std::future<Response> Completions(int frameId, const std::string& text, int column, int line = 0);
    
    // =========================================================================
    // EVENTS
    // =========================================================================
    
    std::function<void()> onInitialized;
    std::function<void(DAPStopReason reason, int threadId, const std::string& text)> onStopped;
    std::function<void(int threadId, bool allThreadsContinued)> onContinued;
    std::function<void(int exitCode)> onExited;
    std::function<void(bool restart)> onTerminated;
    std::function<void(const std::string& reason, int threadId)> onThread;
    std::function<void(const std::string& output, OutputCategory category, 
                       const Source& source, int line)> onOutput;
    std::function<void(const std::string& reason, const Breakpoint& breakpoint)> onBreakpoint;
    std::function<void(const std::string& reason, const Module& module)> onModule;
    std::function<void(const std::string& reason, const Source& source)> onLoadedSource;
    std::function<void(const std::string& name, int processId)> onProcess;
    std::function<void(const Capabilities& capabilities)> onCapabilities;
    std::function<void(InvalidatedAreas areas, int threadId, int stackFrameId)> onInvalidated;
    std::function<void(const std::string& memoryReference, int offset, int count)> onMemory;
    
    std::function<void(const std::string& error)> onError;
    std::function<void()> onDisconnected;
    
private:
    // Message handling
    void ReaderThread();
    void ProcessEvent(const Event& event);
    void ProcessResponse(const Response& response);
    
    std::future<Response> SendRequest(const std::string& command, const JsonObject& args = {});
    void SendMessage(const std::string& json);
    bool ReadMessage(std::string& message);
    
    // State
    std::atomic<DAPClientState> state_{DAPClientState::Disconnected};
    Capabilities capabilities_;
    int nextSeq_ = 1;
    
    // Pending requests
    std::map<int, PendingRequest> pendingRequests_;
    std::mutex requestMutex_;
    int requestTimeout_ = 30000;  // 30 seconds
    
    // Threading
    std::thread readerThread_;
    std::atomic<bool> running_{false};
    std::mutex sendMutex_;
    
    // Process handles (for stdio transport)
#ifdef _WIN32
    void* processHandle_ = nullptr;
    void* stdinWrite_ = nullptr;
    void* stdoutRead_ = nullptr;
#else
    int processPid_ = -1;
    int stdinFd_ = -1;
    int stdoutFd_ = -1;
#endif
    
    // Socket (for socket transport)
    int socket_ = -1;
};

} // namespace DAP
} // namespace IDE
} // namespace UltraCanvas
