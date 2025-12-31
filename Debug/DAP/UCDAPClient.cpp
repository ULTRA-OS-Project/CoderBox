// Apps/IDE/Debug/DAP/UCDAPClient.cpp
// Debug Adapter Protocol Client Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCDAPClient.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

namespace UltraCanvas {
namespace IDE {
namespace DAP {

UCDAPClient::UCDAPClient() = default;

UCDAPClient::~UCDAPClient() {
    Disconnect();
}

bool UCDAPClient::ConnectStdio(const std::string& adapterPath, 
                                const std::vector<std::string>& args) {
    state_ = DAPClientState::Connecting;
    
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    HANDLE stdinReadPipe, stdinWritePipe;
    HANDLE stdoutReadPipe, stdoutWritePipe;
    
    if (!CreatePipe(&stdinReadPipe, &stdinWritePipe, &sa, 0) ||
        !CreatePipe(&stdoutReadPipe, &stdoutWritePipe, &sa, 0)) {
        state_ = DAPClientState::Error;
        return false;
    }
    
    SetHandleInformation(stdinWritePipe, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdInput = stdinReadPipe;
    si.hStdOutput = stdoutWritePipe;
    si.hStdError = stdoutWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    
    std::string cmdLine = adapterPath;
    for (const auto& arg : args) {
        cmdLine += " " + arg;
    }
    
    if (!CreateProcessA(nullptr, const_cast<char*>(cmdLine.c_str()), 
                        nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        state_ = DAPClientState::Error;
        return false;
    }
    
    CloseHandle(stdinReadPipe);
    CloseHandle(stdoutWritePipe);
    
    processHandle_ = pi.hProcess;
    stdinWrite_ = stdinWritePipe;
    stdoutRead_ = stdoutReadPipe;
    CloseHandle(pi.hThread);
    
#else
    int stdinPipe[2], stdoutPipe[2];
    if (pipe(stdinPipe) < 0 || pipe(stdoutPipe) < 0) {
        state_ = DAPClientState::Error;
        return false;
    }
    
    processPid_ = fork();
    if (processPid_ < 0) {
        state_ = DAPClientState::Error;
        return false;
    }
    
    if (processPid_ == 0) {
        // Child process
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);
        
        close(stdinPipe[0]);
        close(stdoutPipe[1]);
        
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(adapterPath.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        execvp(adapterPath.c_str(), argv.data());
        _exit(1);
    }
    
    // Parent process
    close(stdinPipe[0]);
    close(stdoutPipe[1]);
    
    stdinFd_ = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    
    // Set non-blocking
    fcntl(stdoutFd_, F_SETFL, O_NONBLOCK);
#endif
    
    state_ = DAPClientState::Connected;
    running_ = true;
    readerThread_ = std::thread(&UCDAPClient::ReaderThread, this);
    
    return true;
}

bool UCDAPClient::ConnectSocket(const std::string& host, int port) {
    state_ = DAPClientState::Connecting;
    
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        state_ = DAPClientState::Error;
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    
    if (connect(socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = -1;
        state_ = DAPClientState::Error;
        return false;
    }
    
    state_ = DAPClientState::Connected;
    running_ = true;
    readerThread_ = std::thread(&UCDAPClient::ReaderThread, this);
    
    return true;
}

void UCDAPClient::Disconnect() {
    running_ = false;
    
    if (readerThread_.joinable()) {
        readerThread_.join();
    }
    
#ifdef _WIN32
    if (stdinWrite_) {
        CloseHandle(stdinWrite_);
        stdinWrite_ = nullptr;
    }
    if (stdoutRead_) {
        CloseHandle(stdoutRead_);
        stdoutRead_ = nullptr;
    }
    if (processHandle_) {
        TerminateProcess(processHandle_, 0);
        CloseHandle(processHandle_);
        processHandle_ = nullptr;
    }
#else
    if (stdinFd_ >= 0) {
        close(stdinFd_);
        stdinFd_ = -1;
    }
    if (stdoutFd_ >= 0) {
        close(stdoutFd_);
        stdoutFd_ = -1;
    }
    if (processPid_ > 0) {
        kill(processPid_, SIGTERM);
        waitpid(processPid_, nullptr, 0);
        processPid_ = -1;
    }
#endif
    
    if (socket_ >= 0) {
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = -1;
    }
    
    state_ = DAPClientState::Disconnected;
    
    if (onDisconnected) {
        onDisconnected();
    }
}

void UCDAPClient::ReaderThread() {
    while (running_) {
        std::string message;
        if (!ReadMessage(message)) {
            if (running_) {
                state_ = DAPClientState::Error;
                if (onError) onError("Connection lost");
            }
            break;
        }
        
        // Parse message type
        size_t typePos = message.find("\"type\":\"");
        if (typePos == std::string::npos) continue;
        
        size_t typeStart = typePos + 8;
        size_t typeEnd = message.find("\"", typeStart);
        std::string msgType = message.substr(typeStart, typeEnd - typeStart);
        
        if (msgType == "response") {
            Response response;
            
            // Parse seq
            size_t seqPos = message.find("\"request_seq\":");
            if (seqPos != std::string::npos) {
                size_t start = seqPos + 14;
                size_t end = message.find_first_of(",}", start);
                response.request_seq = std::stoi(message.substr(start, end - start));
            }
            
            // Parse success
            response.success = message.find("\"success\":true") != std::string::npos;
            
            // Parse command
            size_t cmdPos = message.find("\"command\":\"");
            if (cmdPos != std::string::npos) {
                size_t start = cmdPos + 11;
                size_t end = message.find("\"", start);
                response.command = message.substr(start, end - start);
            }
            
            ProcessResponse(response);
            
        } else if (msgType == "event") {
            Event event;
            
            // Parse event name
            size_t eventPos = message.find("\"event\":\"");
            if (eventPos != std::string::npos) {
                size_t start = eventPos + 9;
                size_t end = message.find("\"", start);
                event.event = message.substr(start, end - start);
            }
            
            ProcessEvent(event);
        }
    }
}

bool UCDAPClient::ReadMessage(std::string& message) {
    // Read Content-Length header
    std::string header;
    int contentLength = 0;
    
    while (true) {
        char c;
        bool readOk = false;
        
        if (socket_ >= 0) {
            readOk = recv(socket_, &c, 1, 0) == 1;
        }
#ifdef _WIN32
        else if (stdoutRead_) {
            DWORD bytesRead;
            readOk = ReadFile(stdoutRead_, &c, 1, &bytesRead, nullptr) && bytesRead == 1;
        }
#else
        else if (stdoutFd_ >= 0) {
            readOk = read(stdoutFd_, &c, 1) == 1;
        }
#endif
        
        if (!readOk) return false;
        
        header += c;
        
        if (header.length() >= 4 && header.substr(header.length() - 4) == "\r\n\r\n") {
            break;
        }
    }
    
    // Parse Content-Length
    size_t clPos = header.find("Content-Length:");
    if (clPos != std::string::npos) {
        size_t start = clPos + 15;
        while (start < header.length() && header[start] == ' ') start++;
        size_t end = header.find("\r\n", start);
        contentLength = std::stoi(header.substr(start, end - start));
    }
    
    if (contentLength <= 0) return false;
    
    // Read body
    message.resize(contentLength);
    int totalRead = 0;
    
    while (totalRead < contentLength) {
        int bytesRead = 0;
        
        if (socket_ >= 0) {
            bytesRead = recv(socket_, &message[totalRead], contentLength - totalRead, 0);
        }
#ifdef _WIN32
        else if (stdoutRead_) {
            DWORD br;
            if (ReadFile(stdoutRead_, &message[totalRead], contentLength - totalRead, &br, nullptr)) {
                bytesRead = br;
            }
        }
#else
        else if (stdoutFd_ >= 0) {
            bytesRead = read(stdoutFd_, &message[totalRead], contentLength - totalRead);
        }
#endif
        
        if (bytesRead <= 0) return false;
        totalRead += bytesRead;
    }
    
    return true;
}

void UCDAPClient::SendMessage(const std::string& json) {
    std::lock_guard<std::mutex> lock(sendMutex_);
    
    std::ostringstream header;
    header << "Content-Length: " << json.length() << "\r\n\r\n";
    
    std::string message = header.str() + json;
    
    if (socket_ >= 0) {
        send(socket_, message.c_str(), message.length(), 0);
    }
#ifdef _WIN32
    else if (stdinWrite_) {
        DWORD written;
        WriteFile(stdinWrite_, message.c_str(), message.length(), &written, nullptr);
    }
#else
    else if (stdinFd_ >= 0) {
        write(stdinFd_, message.c_str(), message.length());
    }
#endif
}

std::future<Response> UCDAPClient::SendRequest(const std::string& command, const JsonObject& args) {
    Request request;
    request.seq = nextSeq_++;
    request.command = command;
    request.arguments = args;
    
    PendingRequest pending;
    pending.seq = request.seq;
    pending.command = command;
    pending.timestamp = std::chrono::steady_clock::now();
    
    std::future<Response> future = pending.promise.get_future();
    
    {
        std::lock_guard<std::mutex> lock(requestMutex_);
        pendingRequests_[request.seq] = std::move(pending);
    }
    
    SendMessage(request.ToJson());
    
    return future;
}

void UCDAPClient::ProcessResponse(const Response& response) {
    std::lock_guard<std::mutex> lock(requestMutex_);
    
    auto it = pendingRequests_.find(response.request_seq);
    if (it != pendingRequests_.end()) {
        it->second.promise.set_value(response);
        pendingRequests_.erase(it);
    }
}

void UCDAPClient::ProcessEvent(const Event& event) {
    if (event.event == "initialized") {
        state_ = DAPClientState::Ready;
        if (onInitialized) onInitialized();
    }
    else if (event.event == "stopped") {
        if (onStopped) {
            DAPStopReason reason = DAPStopReason::Pause;
            int threadId = 1;
            onStopped(reason, threadId, "");
        }
    }
    else if (event.event == "continued") {
        if (onContinued) onContinued(1, true);
    }
    else if (event.event == "exited") {
        if (onExited) onExited(0);
    }
    else if (event.event == "terminated") {
        if (onTerminated) onTerminated(false);
    }
    else if (event.event == "thread") {
        if (onThread) onThread("started", 1);
    }
    else if (event.event == "output") {
        if (onOutput) {
            Source src;
            onOutput("", OutputCategory::Console, src, 0);
        }
    }
    else if (event.event == "breakpoint") {
        if (onBreakpoint) {
            Breakpoint bp;
            onBreakpoint("changed", bp);
        }
    }
    else if (event.event == "module") {
        if (onModule) {
            Module mod;
            onModule("new", mod);
        }
    }
    else if (event.event == "process") {
        if (onProcess) onProcess("", 0);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

std::future<Response> UCDAPClient::Initialize(const std::string& adapterID,
                                               const std::string& clientName) {
    state_ = DAPClientState::Initializing;
    
    JsonObject args;
    args.Set("clientID", adapterID);
    args.Set("clientName", clientName);
    args.Set("adapterID", adapterID);
    args.Set("locale", std::string("en-US"));
    args.Set("linesStartAt1", true);
    args.Set("columnsStartAt1", true);
    args.Set("pathFormat", std::string("path"));
    args.Set("supportsVariableType", true);
    args.Set("supportsVariablePaging", true);
    args.Set("supportsRunInTerminalRequest", false);
    args.Set("supportsMemoryReferences", true);
    args.Set("supportsProgressReporting", true);
    args.Set("supportsInvalidatedEvent", true);
    args.Set("supportsMemoryEvent", true);
    
    return SendRequest("initialize", args);
}

std::future<Response> UCDAPClient::ConfigurationDone() {
    return SendRequest("configurationDone");
}

// ============================================================================
// LAUNCH/ATTACH
// ============================================================================

std::future<Response> UCDAPClient::Launch(const JsonObject& args) {
    state_ = DAPClientState::Debugging;
    return SendRequest("launch", args);
}

std::future<Response> UCDAPClient::Attach(const JsonObject& args) {
    state_ = DAPClientState::Debugging;
    return SendRequest("attach", args);
}

std::future<Response> UCDAPClient::Restart(const JsonObject& args) {
    return SendRequest("restart", args);
}

std::future<Response> UCDAPClient::Terminate(bool restart) {
    JsonObject args;
    if (restart) args.Set("restart", true);
    return SendRequest("terminate", args);
}

std::future<Response> UCDAPClient::DisconnectRequest(bool terminateDebuggee, bool suspendDebuggee) {
    JsonObject args;
    args.Set("terminateDebuggee", terminateDebuggee);
    args.Set("suspendDebuggee", suspendDebuggee);
    return SendRequest("disconnect", args);
}

// ============================================================================
// BREAKPOINTS
// ============================================================================

std::future<Response> UCDAPClient::SetBreakpoints(const Source& source,
                                                   const std::vector<SourceBreakpoint>& breakpoints) {
    JsonObject args;
    args.Set("source", source.ToJson().ToJson());
    return SendRequest("setBreakpoints", args);
}

std::future<Response> UCDAPClient::SetFunctionBreakpoints(const std::vector<FunctionBreakpoint>& breakpoints) {
    JsonObject args;
    return SendRequest("setFunctionBreakpoints", args);
}

std::future<Response> UCDAPClient::SetExceptionBreakpoints(const std::vector<std::string>& filters,
                                                            const std::vector<ExceptionFilterOptions>& filterOptions,
                                                            const std::vector<ExceptionOptions>& exceptionOptions) {
    JsonObject args;
    return SendRequest("setExceptionBreakpoints", args);
}

std::future<Response> UCDAPClient::SetDataBreakpoints(const std::vector<DataBreakpoint>& breakpoints) {
    JsonObject args;
    return SendRequest("setDataBreakpoints", args);
}

std::future<Response> UCDAPClient::SetInstructionBreakpoints(const std::vector<InstructionBreakpoint>& breakpoints) {
    JsonObject args;
    return SendRequest("setInstructionBreakpoints", args);
}

std::future<Response> UCDAPClient::BreakpointLocations(const Source& source, int line,
                                                        int column, int endLine, int endColumn) {
    JsonObject args;
    args.Set("line", static_cast<int64_t>(line));
    if (column > 0) args.Set("column", static_cast<int64_t>(column));
    if (endLine > 0) args.Set("endLine", static_cast<int64_t>(endLine));
    if (endColumn > 0) args.Set("endColumn", static_cast<int64_t>(endColumn));
    return SendRequest("breakpointLocations", args);
}

std::future<Response> UCDAPClient::DataBreakpointInfo(int variablesReference, const std::string& name,
                                                       int frameId) {
    JsonObject args;
    args.Set("variablesReference", static_cast<int64_t>(variablesReference));
    args.Set("name", name);
    if (frameId > 0) args.Set("frameId", static_cast<int64_t>(frameId));
    return SendRequest("dataBreakpointInfo", args);
}

// ============================================================================
// EXECUTION CONTROL
// ============================================================================

std::future<Response> UCDAPClient::Continue(int threadId, bool singleThread) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    args.Set("singleThread", singleThread);
    return SendRequest("continue", args);
}

std::future<Response> UCDAPClient::Next(int threadId, SteppingGranularity granularity, bool singleThread) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    args.Set("granularity", SteppingGranularityToString(granularity));
    args.Set("singleThread", singleThread);
    return SendRequest("next", args);
}

std::future<Response> UCDAPClient::StepIn(int threadId, int targetId, SteppingGranularity granularity, bool singleThread) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    if (targetId > 0) args.Set("targetId", static_cast<int64_t>(targetId));
    args.Set("granularity", SteppingGranularityToString(granularity));
    args.Set("singleThread", singleThread);
    return SendRequest("stepIn", args);
}

std::future<Response> UCDAPClient::StepOut(int threadId, SteppingGranularity granularity, bool singleThread) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    args.Set("granularity", SteppingGranularityToString(granularity));
    args.Set("singleThread", singleThread);
    return SendRequest("stepOut", args);
}

std::future<Response> UCDAPClient::Pause(int threadId) {
    JsonObject args;
    if (threadId > 0) args.Set("threadId", static_cast<int64_t>(threadId));
    return SendRequest("pause", args);
}

std::future<Response> UCDAPClient::ReverseContinue(int threadId) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    return SendRequest("reverseContinue", args);
}

std::future<Response> UCDAPClient::StepBack(int threadId, SteppingGranularity granularity) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    args.Set("granularity", SteppingGranularityToString(granularity));
    return SendRequest("stepBack", args);
}

// ============================================================================
// STATE INSPECTION
// ============================================================================

std::future<Response> UCDAPClient::Threads() {
    return SendRequest("threads");
}

std::future<Response> UCDAPClient::StackTrace(int threadId, int startFrame, int levels, const ValueFormat& format) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    args.Set("startFrame", static_cast<int64_t>(startFrame));
    args.Set("levels", static_cast<int64_t>(levels));
    return SendRequest("stackTrace", args);
}

std::future<Response> UCDAPClient::Scopes(int frameId) {
    JsonObject args;
    args.Set("frameId", static_cast<int64_t>(frameId));
    return SendRequest("scopes", args);
}

std::future<Response> UCDAPClient::Variables(int variablesReference, const std::string& filter,
                                              int start, int count, const ValueFormat& format) {
    JsonObject args;
    args.Set("variablesReference", static_cast<int64_t>(variablesReference));
    if (!filter.empty()) args.Set("filter", filter);
    if (start > 0) args.Set("start", static_cast<int64_t>(start));
    if (count > 0) args.Set("count", static_cast<int64_t>(count));
    return SendRequest("variables", args);
}

std::future<Response> UCDAPClient::SetVariable(int variablesReference, const std::string& name,
                                                const std::string& value, const ValueFormat& format) {
    JsonObject args;
    args.Set("variablesReference", static_cast<int64_t>(variablesReference));
    args.Set("name", name);
    args.Set("value", value);
    return SendRequest("setVariable", args);
}

std::future<Response> UCDAPClient::Evaluate(const std::string& expression, int frameId,
                                             EvaluateContext context, const ValueFormat& format) {
    JsonObject args;
    args.Set("expression", expression);
    if (frameId > 0) args.Set("frameId", static_cast<int64_t>(frameId));
    
    std::string ctxStr;
    switch (context) {
        case EvaluateContext::Watch: ctxStr = "watch"; break;
        case EvaluateContext::Repl: ctxStr = "repl"; break;
        case EvaluateContext::Hover: ctxStr = "hover"; break;
        case EvaluateContext::Clipboard: ctxStr = "clipboard"; break;
        case EvaluateContext::Variables: ctxStr = "variables"; break;
    }
    args.Set("context", ctxStr);
    
    return SendRequest("evaluate", args);
}

std::future<Response> UCDAPClient::SetExpression(int frameId, const std::string& expression,
                                                  const std::string& value, const ValueFormat& format) {
    JsonObject args;
    args.Set("frameId", static_cast<int64_t>(frameId));
    args.Set("expression", expression);
    args.Set("value", value);
    return SendRequest("setExpression", args);
}

std::future<Response> UCDAPClient::StepInTargets(int frameId) {
    JsonObject args;
    args.Set("frameId", static_cast<int64_t>(frameId));
    return SendRequest("stepInTargets", args);
}

std::future<Response> UCDAPClient::GotoTargets(const Source& source, int line, int column) {
    JsonObject args;
    args.Set("line", static_cast<int64_t>(line));
    if (column > 0) args.Set("column", static_cast<int64_t>(column));
    return SendRequest("gotoTargets", args);
}

std::future<Response> UCDAPClient::Goto(int threadId, int targetId) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    args.Set("targetId", static_cast<int64_t>(targetId));
    return SendRequest("goto", args);
}

// ============================================================================
// SOURCE & MODULES
// ============================================================================

std::future<Response> UCDAPClient::GetSource(const Source& source) {
    JsonObject args;
    return SendRequest("source", args);
}

std::future<Response> UCDAPClient::LoadedSources() {
    return SendRequest("loadedSources");
}

std::future<Response> UCDAPClient::Modules(int startModule, int moduleCount) {
    JsonObject args;
    if (startModule > 0) args.Set("startModule", static_cast<int64_t>(startModule));
    if (moduleCount > 0) args.Set("moduleCount", static_cast<int64_t>(moduleCount));
    return SendRequest("modules", args);
}

// ============================================================================
// MEMORY & DISASSEMBLY
// ============================================================================

std::future<Response> UCDAPClient::ReadMemory(const std::string& memoryReference, int offset, int count) {
    JsonObject args;
    args.Set("memoryReference", memoryReference);
    if (offset != 0) args.Set("offset", static_cast<int64_t>(offset));
    args.Set("count", static_cast<int64_t>(count));
    return SendRequest("readMemory", args);
}

std::future<Response> UCDAPClient::WriteMemory(const std::string& memoryReference, int offset,
                                                const std::string& data, bool allowPartial) {
    JsonObject args;
    args.Set("memoryReference", memoryReference);
    if (offset != 0) args.Set("offset", static_cast<int64_t>(offset));
    args.Set("data", data);
    args.Set("allowPartial", allowPartial);
    return SendRequest("writeMemory", args);
}

std::future<Response> UCDAPClient::Disassemble(const std::string& memoryReference, int offset,
                                                int instructionOffset, int instructionCount,
                                                bool resolveSymbols) {
    JsonObject args;
    args.Set("memoryReference", memoryReference);
    if (offset != 0) args.Set("offset", static_cast<int64_t>(offset));
    if (instructionOffset != 0) args.Set("instructionOffset", static_cast<int64_t>(instructionOffset));
    args.Set("instructionCount", static_cast<int64_t>(instructionCount));
    args.Set("resolveSymbols", resolveSymbols);
    return SendRequest("disassemble", args);
}

// ============================================================================
// EXCEPTIONS
// ============================================================================

std::future<Response> UCDAPClient::ExceptionInfo(int threadId) {
    JsonObject args;
    args.Set("threadId", static_cast<int64_t>(threadId));
    return SendRequest("exceptionInfo", args);
}

// ============================================================================
// COMPLETIONS
// ============================================================================

std::future<Response> UCDAPClient::Completions(int frameId, const std::string& text, int column, int line) {
    JsonObject args;
    if (frameId > 0) args.Set("frameId", static_cast<int64_t>(frameId));
    args.Set("text", text);
    args.Set("column", static_cast<int64_t>(column));
    if (line > 0) args.Set("line", static_cast<int64_t>(line));
    return SendRequest("completions", args);
}

} // namespace DAP
} // namespace IDE
} // namespace UltraCanvas
