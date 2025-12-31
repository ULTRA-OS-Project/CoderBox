// Apps/IDE/Debug/DAP/UCDAPServer.cpp
// Debug Adapter Protocol Server Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDAPServer.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

namespace UltraCanvas {
namespace IDE {
namespace DAP {

UCDAPServer::UCDAPServer() = default;

UCDAPServer::~UCDAPServer() {
    Stop();
}

bool UCDAPServer::Initialize(const DAPServerConfig& config) {
    config_ = config;
    
    switch (config_.transport) {
        case DAPTransport::Stdio:
            return InitializeStdio();
        case DAPTransport::Socket:
            return InitializeSocket();
        case DAPTransport::Pipe:
            return InitializePipe();
    }
    
    return false;
}

bool UCDAPServer::InitializeStdio() {
    // Stdio requires no special initialization
    return true;
}

bool UCDAPServer::InitializeSocket() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
#endif
    
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ < 0) {
        return false;
    }
    
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config_.port);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return false;
    }
    
    if (listen(serverSocket_, 1) < 0) {
        return false;
    }
    
    return true;
}

bool UCDAPServer::InitializePipe() {
    // Platform-specific pipe implementation
    return false;
}

bool UCDAPServer::Start() {
    if (running_) return true;
    
    running_ = true;
    
    if (config_.transport == DAPTransport::Socket) {
        // Wait for client connection
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        clientSocket_ = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket_ < 0) {
            running_ = false;
            return false;
        }
        
        if (onClientConnected) {
            onClientConnected();
        }
    }
    
    messageThread_ = std::thread(&UCDAPServer::MessageLoop, this);
    return true;
}

void UCDAPServer::Stop() {
    running_ = false;
    
    if (messageThread_.joinable()) {
        messageThread_.join();
    }
    
    if (clientSocket_ >= 0) {
#ifdef _WIN32
        closesocket(clientSocket_);
#else
        close(clientSocket_);
#endif
        clientSocket_ = -1;
    }
    
    if (serverSocket_ >= 0) {
#ifdef _WIN32
        closesocket(serverSocket_);
        WSACleanup();
#else
        close(serverSocket_);
#endif
        serverSocket_ = -1;
    }
    
    if (onClientDisconnected) {
        onClientDisconnected();
    }
}

void UCDAPServer::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    SetupDebugManagerCallbacks();
}

void UCDAPServer::SetupDebugManagerCallbacks() {
    if (!debugManager_) return;
    
    debugManager_->onStateChange = [this](DebugSessionState oldState, DebugSessionState newState) {
        OnDebugStateChange(oldState, newState);
    };
    
    debugManager_->onTargetStop = [this](StopReason reason, const SourceLocation& loc) {
        OnTargetStop(reason, loc);
    };
    
    debugManager_->onBreakpointHit = [this](int bpId) {
        OnBreakpointHit(bpId);
    };
    
    debugManager_->onOutput = [this](const std::string& text) {
        OnOutput(text, false);
    };
    
    debugManager_->onError = [this](const std::string& text) {
        OnOutput(text, true);
    };
}

void UCDAPServer::MessageLoop() {
    while (running_) {
        std::string message;
        if (!ReadMessage(message)) {
            break;
        }
        
        if (onMessageReceived) {
            onMessageReceived(message);
        }
        
        // Parse and process
        Response response = ProcessMessage(message);
        SendResponse(response);
    }
}

Response UCDAPServer::ProcessMessage(const std::string& messageJson) {
    // Simple JSON parsing for command extraction
    Request request;
    
    // Extract seq
    size_t seqPos = messageJson.find("\"seq\":");
    if (seqPos != std::string::npos) {
        size_t start = seqPos + 6;
        size_t end = messageJson.find_first_of(",}", start);
        request.seq = std::stoi(messageJson.substr(start, end - start));
    }
    
    // Extract command
    size_t cmdPos = messageJson.find("\"command\":\"");
    if (cmdPos != std::string::npos) {
        size_t start = cmdPos + 11;
        size_t end = messageJson.find("\"", start);
        request.command = messageJson.substr(start, end - start);
    }
    
    ProcessRequest(request);
    
    // Dispatch to handler
    if (request.command == "initialize") return HandleInitialize(request);
    if (request.command == "launch") return HandleLaunch(request);
    if (request.command == "attach") return HandleAttach(request);
    if (request.command == "disconnect") return HandleDisconnect(request);
    if (request.command == "configurationDone") return HandleConfigurationDone(request);
    if (request.command == "setBreakpoints") return HandleSetBreakpoints(request);
    if (request.command == "setFunctionBreakpoints") return HandleSetFunctionBreakpoints(request);
    if (request.command == "setExceptionBreakpoints") return HandleSetExceptionBreakpoints(request);
    if (request.command == "continue") return HandleContinue(request);
    if (request.command == "next") return HandleNext(request);
    if (request.command == "stepIn") return HandleStepIn(request);
    if (request.command == "stepOut") return HandleStepOut(request);
    if (request.command == "pause") return HandlePause(request);
    if (request.command == "terminate") return HandleTerminate(request);
    if (request.command == "restart") return HandleRestart(request);
    if (request.command == "threads") return HandleThreads(request);
    if (request.command == "stackTrace") return HandleStackTrace(request);
    if (request.command == "scopes") return HandleScopes(request);
    if (request.command == "variables") return HandleVariables(request);
    if (request.command == "setVariable") return HandleSetVariable(request);
    if (request.command == "source") return HandleSource(request);
    if (request.command == "evaluate") return HandleEvaluate(request);
    if (request.command == "completions") return HandleCompletions(request);
    if (request.command == "modules") return HandleModules(request);
    if (request.command == "loadedSources") return HandleLoadedSources(request);
    if (request.command == "readMemory") return HandleReadMemory(request);
    if (request.command == "writeMemory") return HandleWriteMemory(request);
    if (request.command == "disassemble") return HandleDisassemble(request);
    if (request.command == "exceptionInfo") return HandleExceptionInfo(request);
    if (request.command == "dataBreakpointInfo") return HandleDataBreakpointInfo(request);
    if (request.command == "setDataBreakpoints") return HandleSetDataBreakpoints(request);
    if (request.command == "setInstructionBreakpoints") return HandleSetInstructionBreakpoints(request);
    if (request.command == "breakpointLocations") return HandleBreakpointLocations(request);
    
    return CreateErrorResponse(request, "Unknown command: " + request.command);
}

Response UCDAPServer::CreateErrorResponse(const Request& request, const std::string& message) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = request.command;
    response.success = false;
    response.message = message;
    return response;
}

void UCDAPServer::SendResponse(const Response& response) {
    SendMessage(response.ToJson());
}

void UCDAPServer::SendEvent(const Event& event) {
    Event e = event;
    e.seq = nextSeq_++;
    SendMessage(e.ToJson());
}

void UCDAPServer::SendMessage(const std::string& json) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    std::ostringstream header;
    header << "Content-Length: " << json.length() << "\r\n\r\n";
    
    std::string message = header.str() + json;
    
    if (onMessageSent) {
        onMessageSent(json);
    }
    
    WriteMessage(message);
}

bool UCDAPServer::ReadMessage(std::string& message) {
    // Read headers
    std::string line;
    int contentLength = 0;
    
    while (true) {
        line = ReadLine();
        if (line.empty() || line == "\r\n" || line == "\n") {
            break;
        }
        
        if (line.find("Content-Length:") == 0) {
            contentLength = std::stoi(line.substr(15));
        }
    }
    
    if (contentLength <= 0) {
        return false;
    }
    
    // Read body
    message.resize(contentLength);
    
    if (config_.transport == DAPTransport::Stdio) {
        std::cin.read(&message[0], contentLength);
        return std::cin.good();
    } else if (config_.transport == DAPTransport::Socket && clientSocket_ >= 0) {
        int received = 0;
        while (received < contentLength) {
            int n = recv(clientSocket_, &message[received], contentLength - received, 0);
            if (n <= 0) return false;
            received += n;
        }
        return true;
    }
    
    return false;
}

bool UCDAPServer::WriteMessage(const std::string& message) {
    if (config_.transport == DAPTransport::Stdio) {
        std::cout << message;
        std::cout.flush();
        return true;
    } else if (config_.transport == DAPTransport::Socket && clientSocket_ >= 0) {
        int sent = 0;
        while (sent < static_cast<int>(message.length())) {
            int n = send(clientSocket_, message.c_str() + sent, message.length() - sent, 0);
            if (n <= 0) return false;
            sent += n;
        }
        return true;
    }
    return false;
}

std::string UCDAPServer::ReadLine() {
    std::string line;
    
    if (config_.transport == DAPTransport::Stdio) {
        std::getline(std::cin, line);
        line += "\n";
    } else if (config_.transport == DAPTransport::Socket && clientSocket_ >= 0) {
        char c;
        while (recv(clientSocket_, &c, 1, 0) == 1) {
            line += c;
            if (c == '\n') break;
        }
    }
    
    return line;
}

void UCDAPServer::ProcessRequest(const Request& request) {
    // Logging if enabled
    if (config_.logMessages) {
        // Log to file
    }
}

// ============================================================================
// REQUEST HANDLERS
// ============================================================================

Response UCDAPServer::HandleInitialize(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "initialize";
    response.success = true;
    response.body = capabilities_.ToJson();
    
    initialized_ = true;
    
    // Send initialized event after response
    SendInitializedEvent();
    
    return response;
}

Response UCDAPServer::HandleLaunch(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "launch";
    
    if (!debugManager_) {
        response.success = false;
        response.message = "Debug manager not initialized";
        return response;
    }
    
    // Extract launch arguments from request.arguments
    // program, args, cwd, env, stopAtEntry, etc.
    
    DebugLaunchConfig config;
    // Parse config from request.arguments
    
    if (debugManager_->Launch(config)) {
        response.success = true;
        SendProcessEvent(config.program, debugManager_->GetProcessId());
    } else {
        response.success = false;
        response.message = "Failed to launch program";
    }
    
    return response;
}

Response UCDAPServer::HandleAttach(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "attach";
    
    if (!debugManager_) {
        response.success = false;
        response.message = "Debug manager not initialized";
        return response;
    }
    
    // Extract processId from request.arguments
    int processId = 0;  // Parse from arguments
    
    if (debugManager_->Attach(processId)) {
        response.success = true;
    } else {
        response.success = false;
        response.message = "Failed to attach to process";
    }
    
    return response;
}

Response UCDAPServer::HandleDisconnect(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "disconnect";
    response.success = true;
    
    if (debugManager_) {
        debugManager_->Terminate();
    }
    
    running_ = false;
    return response;
}

Response UCDAPServer::HandleConfigurationDone(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "configurationDone";
    response.success = true;
    
    configurationDone_ = true;
    
    // Continue if stopped at entry
    if (debugManager_ && debugManager_->GetState() == DebugSessionState::Paused) {
        // Client configured, can now continue
    }
    
    return response;
}

Response UCDAPServer::HandleSetBreakpoints(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "setBreakpoints";
    
    if (!debugManager_) {
        response.success = false;
        response.message = "Debug manager not initialized";
        return response;
    }
    
    // Parse source and breakpoints from request.arguments
    // Clear existing breakpoints for this source
    // Set new breakpoints
    
    response.success = true;
    // response.body contains "breakpoints" array
    
    return response;
}

Response UCDAPServer::HandleSetFunctionBreakpoints(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "setFunctionBreakpoints";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleSetExceptionBreakpoints(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "setExceptionBreakpoints";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleContinue(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "continue";
    
    if (debugManager_) {
        debugManager_->Continue();
        response.success = true;
        response.body.Set("allThreadsContinued", true);
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleNext(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "next";
    
    if (debugManager_) {
        debugManager_->StepOver();
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleStepIn(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "stepIn";
    
    if (debugManager_) {
        debugManager_->StepInto();
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleStepOut(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "stepOut";
    
    if (debugManager_) {
        debugManager_->StepOut();
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandlePause(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "pause";
    
    if (debugManager_) {
        debugManager_->Pause();
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleTerminate(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "terminate";
    
    if (debugManager_) {
        debugManager_->Terminate();
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleRestart(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "restart";
    
    if (debugManager_) {
        debugManager_->Restart();
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleThreads(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "threads";
    
    if (!debugManager_) {
        response.success = false;
        return response;
    }
    
    auto threads = debugManager_->GetThreads();
    
    std::ostringstream threadsJson;
    threadsJson << "[";
    for (size_t i = 0; i < threads.size(); i++) {
        if (i > 0) threadsJson << ",";
        Thread t;
        t.id = threads[i].id;
        t.name = threads[i].name.empty() ? "Thread " + std::to_string(threads[i].id) : threads[i].name;
        threadsJson << t.ToJson().ToJson();
    }
    threadsJson << "]";
    
    response.success = true;
    response.body.Set("threads", threadsJson.str());
    
    return response;
}

Response UCDAPServer::HandleStackTrace(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "stackTrace";
    
    if (!debugManager_) {
        response.success = false;
        return response;
    }
    
    // Parse threadId from arguments
    int threadId = 0;  // Default
    
    auto frames = debugManager_->GetCallStack(threadId);
    
    // Build stack frames array
    response.success = true;
    response.body.Set("totalFrames", static_cast<int64_t>(frames.size()));
    
    return response;
}

Response UCDAPServer::HandleScopes(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "scopes";
    response.success = true;
    
    // Build scopes for frame (Locals, Arguments, Registers)
    
    return response;
}

Response UCDAPServer::HandleVariables(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "variables";
    response.success = true;
    
    // Look up variable reference and return children
    
    return response;
}

Response UCDAPServer::HandleSetVariable(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "setVariable";
    
    if (debugManager_) {
        // Parse variablesReference, name, value
        response.success = true;
    } else {
        response.success = false;
    }
    
    return response;
}

Response UCDAPServer::HandleSource(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "source";
    response.success = true;
    
    // Return source content for sourceReference
    
    return response;
}

Response UCDAPServer::HandleEvaluate(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "evaluate";
    
    if (!debugManager_) {
        response.success = false;
        return response;
    }
    
    // Parse expression, context, frameId
    std::string expression;  // From arguments
    
    auto result = debugManager_->Evaluate(expression);
    
    if (result.success) {
        response.success = true;
        response.body.Set("result", result.variable.value);
        response.body.Set("type", result.variable.type);
        response.body.Set("variablesReference", static_cast<int64_t>(0));
    } else {
        response.success = false;
        response.message = result.error;
    }
    
    return response;
}

Response UCDAPServer::HandleCompletions(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "completions";
    response.success = true;
    
    // Return completion items
    
    return response;
}

Response UCDAPServer::HandleModules(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "modules";
    response.success = true;
    
    // Return loaded modules
    
    return response;
}

Response UCDAPServer::HandleLoadedSources(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "loadedSources";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleReadMemory(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "readMemory";
    
    if (!debugManager_) {
        response.success = false;
        return response;
    }
    
    // Parse memoryReference, offset, count
    
    response.success = true;
    return response;
}

Response UCDAPServer::HandleWriteMemory(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "writeMemory";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleDisassemble(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "disassemble";
    
    if (!debugManager_) {
        response.success = false;
        return response;
    }
    
    response.success = true;
    return response;
}

Response UCDAPServer::HandleExceptionInfo(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "exceptionInfo";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleDataBreakpointInfo(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "dataBreakpointInfo";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleSetDataBreakpoints(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "setDataBreakpoints";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleSetInstructionBreakpoints(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "setInstructionBreakpoints";
    response.success = true;
    return response;
}

Response UCDAPServer::HandleBreakpointLocations(const Request& request) {
    Response response;
    response.seq = nextSeq_++;
    response.request_seq = request.seq;
    response.command = "breakpointLocations";
    response.success = true;
    return response;
}

// ============================================================================
// EVENT HELPERS
// ============================================================================

void UCDAPServer::SendInitializedEvent() {
    Event event("initialized");
    SendEvent(event);
}

void UCDAPServer::SendStoppedEvent(DAPStopReason reason, int threadId, const std::string& text) {
    Event event("stopped");
    event.body.Set("reason", DAPStopReasonToString(reason));
    event.body.Set("threadId", static_cast<int64_t>(threadId));
    event.body.Set("allThreadsStopped", true);
    if (!text.empty()) {
        event.body.Set("text", text);
    }
    SendEvent(event);
}

void UCDAPServer::SendContinuedEvent(int threadId, bool allThreadsContinued) {
    Event event("continued");
    event.body.Set("threadId", static_cast<int64_t>(threadId));
    event.body.Set("allThreadsContinued", allThreadsContinued);
    SendEvent(event);
}

void UCDAPServer::SendExitedEvent(int exitCode) {
    Event event("exited");
    event.body.Set("exitCode", static_cast<int64_t>(exitCode));
    SendEvent(event);
}

void UCDAPServer::SendTerminatedEvent(bool restart) {
    Event event("terminated");
    if (restart) {
        event.body.Set("restart", true);
    }
    SendEvent(event);
}

void UCDAPServer::SendThreadEvent(const std::string& reason, int threadId) {
    Event event("thread");
    event.body.Set("reason", reason);
    event.body.Set("threadId", static_cast<int64_t>(threadId));
    SendEvent(event);
}

void UCDAPServer::SendOutputEvent(const std::string& output, OutputCategory category) {
    Event event("output");
    event.body.Set("category", OutputCategoryToString(category));
    event.body.Set("output", output);
    SendEvent(event);
}

void UCDAPServer::SendBreakpointEvent(const std::string& reason, const Breakpoint& bp) {
    Event event("breakpoint");
    event.body.Set("reason", reason);
    // Add breakpoint data
    SendEvent(event);
}

void UCDAPServer::SendModuleEvent(const std::string& reason, const Module& module) {
    Event event("module");
    event.body.Set("reason", reason);
    SendEvent(event);
}

void UCDAPServer::SendLoadedSourceEvent(const std::string& reason, const Source& source) {
    Event event("loadedSource");
    event.body.Set("reason", reason);
    SendEvent(event);
}

void UCDAPServer::SendProcessEvent(const std::string& name, int processId, bool isLocalProcess) {
    Event event("process");
    event.body.Set("name", name);
    event.body.Set("systemProcessId", static_cast<int64_t>(processId));
    event.body.Set("isLocalProcess", isLocalProcess);
    event.body.Set("startMethod", std::string("launch"));
    SendEvent(event);
}

void UCDAPServer::SendCapabilitiesEvent() {
    Event event("capabilities");
    event.body = capabilities_.ToJson();
    SendEvent(event);
}

void UCDAPServer::SendProgressEvent(const std::string& progressId, const std::string& title,
                                     const std::string& message, int percentage) {
    Event event("progressStart");
    event.body.Set("progressId", progressId);
    event.body.Set("title", title);
    if (!message.empty()) event.body.Set("message", message);
    if (percentage >= 0) event.body.Set("percentage", static_cast<int64_t>(percentage));
    SendEvent(event);
}

void UCDAPServer::SendInvalidatedEvent(InvalidatedAreas areas, int threadId) {
    Event event("invalidated");
    event.body.Set("areas", InvalidatedAreasToString(areas));
    if (threadId > 0) event.body.Set("threadId", static_cast<int64_t>(threadId));
    SendEvent(event);
}

void UCDAPServer::SendMemoryEvent(const std::string& memoryReference, int offset, int count) {
    Event event("memory");
    event.body.Set("memoryReference", memoryReference);
    event.body.Set("offset", static_cast<int64_t>(offset));
    event.body.Set("count", static_cast<int64_t>(count));
    SendEvent(event);
}

// ============================================================================
// DEBUG MANAGER CALLBACKS
// ============================================================================

void UCDAPServer::OnDebugStateChange(DebugSessionState oldState, DebugSessionState newState) {
    switch (newState) {
        case DebugSessionState::Running:
            SendContinuedEvent(1, true);
            break;
        case DebugSessionState::Terminated:
            SendTerminatedEvent();
            break;
        default:
            break;
    }
}

void UCDAPServer::OnTargetStop(StopReason reason, const SourceLocation& location) {
    DAPStopReason dapReason = DAPStopReason::Pause;
    
    switch (reason) {
        case StopReason::Breakpoint:
            dapReason = DAPStopReason::Breakpoint;
            break;
        case StopReason::Step:
            dapReason = DAPStopReason::Step;
            break;
        case StopReason::Exception:
            dapReason = DAPStopReason::Exception;
            break;
        case StopReason::Entry:
            dapReason = DAPStopReason::Entry;
            break;
        default:
            dapReason = DAPStopReason::Pause;
            break;
    }
    
    SendStoppedEvent(dapReason, 1);  // Thread 1 by default
}

void UCDAPServer::OnBreakpointHit(int breakpointId) {
    SendStoppedEvent(DAPStopReason::Breakpoint, 1, "Breakpoint " + std::to_string(breakpointId));
}

void UCDAPServer::OnOutput(const std::string& text, bool isError) {
    SendOutputEvent(text, isError ? OutputCategory::Stderr : OutputCategory::Stdout);
}

} // namespace DAP
} // namespace IDE
} // namespace UltraCanvas
