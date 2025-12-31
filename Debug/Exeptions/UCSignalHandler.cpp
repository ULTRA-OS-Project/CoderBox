// Apps/IDE/Debug/Exceptions/UCSignalHandler.cpp
// Signal Handler Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCSignalHandler.h"
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

UCSignalHandler::UCSignalHandler() {
    InitializeSignals();
    LoadDefaults();
}

void UCSignalHandler::InitializeSignals() {
    // Standard POSIX signals
    struct SignalDef {
        int number;
        const char* name;
        const char* description;
    };
    
    SignalDef standardSignals[] = {
        {1,  "SIGHUP",    "Hangup"},
        {2,  "SIGINT",    "Interrupt"},
        {3,  "SIGQUIT",   "Quit"},
        {4,  "SIGILL",    "Illegal instruction"},
        {5,  "SIGTRAP",   "Trace/breakpoint trap"},
        {6,  "SIGABRT",   "Aborted"},
        {7,  "SIGBUS",    "Bus error"},
        {8,  "SIGFPE",    "Floating point exception"},
        {9,  "SIGKILL",   "Killed"},
        {10, "SIGUSR1",   "User defined signal 1"},
        {11, "SIGSEGV",   "Segmentation fault"},
        {12, "SIGUSR2",   "User defined signal 2"},
        {13, "SIGPIPE",   "Broken pipe"},
        {14, "SIGALRM",   "Alarm clock"},
        {15, "SIGTERM",   "Terminated"},
        {16, "SIGSTKFLT", "Stack fault"},
        {17, "SIGCHLD",   "Child exited"},
        {18, "SIGCONT",   "Continued"},
        {19, "SIGSTOP",   "Stopped (signal)"},
        {20, "SIGTSTP",   "Stopped"},
        {21, "SIGTTIN",   "Stopped (tty input)"},
        {22, "SIGTTOU",   "Stopped (tty output)"},
        {23, "SIGURG",    "Urgent I/O condition"},
        {24, "SIGXCPU",   "CPU time limit exceeded"},
        {25, "SIGXFSZ",   "File size limit exceeded"},
        {26, "SIGVTALRM", "Virtual timer expired"},
        {27, "SIGPROF",   "Profiling timer expired"},
        {28, "SIGWINCH",  "Window size changed"},
        {29, "SIGIO",     "I/O possible"},
        {30, "SIGPWR",    "Power failure"},
        {31, "SIGSYS",    "Bad system call"},
    };
    
    for (const auto& sig : standardSignals) {
        SignalInfo info;
        info.number = sig.number;
        info.name = sig.name;
        info.description = sig.description;
        info.isRealtime = false;
        
        signals_[sig.number] = info;
        nameToNumber_[sig.name] = sig.number;
    }
    
    // Add realtime signals (SIGRTMIN to SIGRTMAX, typically 34-64)
    for (int i = 34; i <= 64; i++) {
        SignalInfo info;
        info.number = i;
        info.name = "SIGRT" + std::to_string(i - 34);
        info.description = "Real-time signal " + std::to_string(i - 34);
        info.isRealtime = true;
        info.stop = false;
        info.print = false;
        info.pass = true;
        
        signals_[i] = info;
        nameToNumber_[info.name] = i;
    }
}

void UCSignalHandler::LoadDefaults() {
    // Set defaults based on typical debugging behavior
    for (auto& pair : signals_) {
        SignalInfo& sig = pair.second;
        
        switch (sig.number) {
            // Fatal program errors - always stop
            case 4:  // SIGILL
            case 6:  // SIGABRT
            case 7:  // SIGBUS
            case 8:  // SIGFPE
            case 11: // SIGSEGV
            case 31: // SIGSYS
                sig.stop = true;
                sig.print = true;
                sig.pass = true;
                break;
                
            // Signals that shouldn't stop debugger
            case 5:  // SIGTRAP - handled by debugger
                sig.stop = false;
                sig.print = false;
                sig.pass = false;
                break;
                
            case 17: // SIGCHLD
            case 18: // SIGCONT
            case 28: // SIGWINCH
            case 29: // SIGIO
                sig.stop = false;
                sig.print = false;
                sig.pass = true;
                break;
                
            // Termination signals
            case 2:  // SIGINT
            case 15: // SIGTERM
                sig.stop = true;
                sig.print = true;
                sig.pass = true;
                break;
                
            // Job control
            case 19: // SIGSTOP
            case 20: // SIGTSTP
            case 21: // SIGTTIN
            case 22: // SIGTTOU
                sig.stop = false;
                sig.print = true;
                sig.pass = true;
                break;
                
            // Default for others
            default:
                sig.stop = false;
                sig.print = true;
                sig.pass = true;
                break;
        }
    }
}

const SignalInfo* UCSignalHandler::GetSignal(int signalNumber) const {
    auto it = signals_.find(signalNumber);
    return (it != signals_.end()) ? &it->second : nullptr;
}

const SignalInfo* UCSignalHandler::GetSignalByName(const std::string& name) const {
    auto it = nameToNumber_.find(name);
    if (it != nameToNumber_.end()) {
        return GetSignal(it->second);
    }
    return nullptr;
}

std::vector<SignalInfo> UCSignalHandler::GetAllSignals() const {
    std::vector<SignalInfo> result;
    for (const auto& pair : signals_) {
        result.push_back(pair.second);
    }
    
    std::sort(result.begin(), result.end(),
              [](const SignalInfo& a, const SignalInfo& b) {
                  return a.number < b.number;
              });
    
    return result;
}

std::vector<SignalInfo> UCSignalHandler::GetSignalsInGroup(SignalGroup group) const {
    std::vector<SignalInfo> result;
    
    for (const auto& pair : signals_) {
        if (group == SignalGroup::All || GetSignalGroup(pair.first) == group) {
            result.push_back(pair.second);
        }
    }
    
    return result;
}

SignalGroup UCSignalHandler::GetSignalGroup(int signalNumber) const {
    switch (signalNumber) {
        // Program error signals
        case 4:  // SIGILL
        case 6:  // SIGABRT
        case 7:  // SIGBUS
        case 8:  // SIGFPE
        case 11: // SIGSEGV
        case 31: // SIGSYS
            return SignalGroup::Program;
            
        // External signals
        case 1:  // SIGHUP
        case 2:  // SIGINT
        case 3:  // SIGQUIT
        case 9:  // SIGKILL
        case 15: // SIGTERM
        case 30: // SIGPWR
            return SignalGroup::External;
            
        // Job control
        case 17: // SIGCHLD
        case 18: // SIGCONT
        case 19: // SIGSTOP
        case 20: // SIGTSTP
        case 21: // SIGTTIN
        case 22: // SIGTTOU
            return SignalGroup::Job;
            
        // Timers
        case 14: // SIGALRM
        case 26: // SIGVTALRM
        case 27: // SIGPROF
            return SignalGroup::Timer;
            
        // I/O
        case 13: // SIGPIPE
        case 23: // SIGURG
        case 29: // SIGIO
            return SignalGroup::IO;
            
        // User defined
        case 10: // SIGUSR1
        case 12: // SIGUSR2
            return SignalGroup::User;
            
        default:
            if (signalNumber >= 34 && signalNumber <= 64) {
                return SignalGroup::Realtime;
            }
            return SignalGroup::External;
    }
}

void UCSignalHandler::SetStop(int signalNumber, bool stop) {
    auto it = signals_.find(signalNumber);
    if (it != signals_.end()) {
        it->second.stop = stop;
        if (onSignalConfigChanged) {
            onSignalConfigChanged(signalNumber);
        }
    }
}

void UCSignalHandler::SetStop(const std::string& signalName, bool stop) {
    auto it = nameToNumber_.find(signalName);
    if (it != nameToNumber_.end()) {
        SetStop(it->second, stop);
    }
}

bool UCSignalHandler::GetStop(int signalNumber) const {
    auto it = signals_.find(signalNumber);
    return (it != signals_.end()) ? it->second.stop : false;
}

void UCSignalHandler::SetPrint(int signalNumber, bool print) {
    auto it = signals_.find(signalNumber);
    if (it != signals_.end()) {
        it->second.print = print;
        if (onSignalConfigChanged) {
            onSignalConfigChanged(signalNumber);
        }
    }
}

void UCSignalHandler::SetPrint(const std::string& signalName, bool print) {
    auto it = nameToNumber_.find(signalName);
    if (it != nameToNumber_.end()) {
        SetPrint(it->second, print);
    }
}

bool UCSignalHandler::GetPrint(int signalNumber) const {
    auto it = signals_.find(signalNumber);
    return (it != signals_.end()) ? it->second.print : false;
}

void UCSignalHandler::SetPass(int signalNumber, bool pass) {
    auto it = signals_.find(signalNumber);
    if (it != signals_.end()) {
        it->second.pass = pass;
        if (onSignalConfigChanged) {
            onSignalConfigChanged(signalNumber);
        }
    }
}

void UCSignalHandler::SetPass(const std::string& signalName, bool pass) {
    auto it = nameToNumber_.find(signalName);
    if (it != nameToNumber_.end()) {
        SetPass(it->second, pass);
    }
}

bool UCSignalHandler::GetPass(int signalNumber) const {
    auto it = signals_.find(signalNumber);
    return (it != signals_.end()) ? it->second.pass : false;
}

void UCSignalHandler::SetDisposition(int signalNumber, bool stop, bool print, bool pass) {
    auto it = signals_.find(signalNumber);
    if (it != signals_.end()) {
        it->second.stop = stop;
        it->second.print = print;
        it->second.pass = pass;
        if (onSignalConfigChanged) {
            onSignalConfigChanged(signalNumber);
        }
    }
}

void UCSignalHandler::SetDisposition(const std::string& signalName, bool stop, bool print, bool pass) {
    auto it = nameToNumber_.find(signalName);
    if (it != nameToNumber_.end()) {
        SetDisposition(it->second, stop, print, pass);
    }
}

void UCSignalHandler::SetGroupStop(SignalGroup group, bool stop) {
    for (auto& pair : signals_) {
        if (group == SignalGroup::All || GetSignalGroup(pair.first) == group) {
            pair.second.stop = stop;
        }
    }
}

void UCSignalHandler::SetGroupPrint(SignalGroup group, bool print) {
    for (auto& pair : signals_) {
        if (group == SignalGroup::All || GetSignalGroup(pair.first) == group) {
            pair.second.print = print;
        }
    }
}

void UCSignalHandler::SetGroupPass(SignalGroup group, bool pass) {
    for (auto& pair : signals_) {
        if (group == SignalGroup::All || GetSignalGroup(pair.first) == group) {
            pair.second.pass = pass;
        }
    }
}

bool UCSignalHandler::OnSignalCaught(const CaughtSignal& signal) {
    lastSignal_ = signal;
    
    signalHistory_.push_back(signal);
    while (signalHistory_.size() > maxHistorySize_) {
        signalHistory_.erase(signalHistory_.begin());
    }
    
    if (onSignalCaught) {
        onSignalCaught(signal);
    }
    
    return GetStop(signal.signalNumber);
}

std::string UCSignalHandler::GenerateGDBHandleCommand(int signalNumber) const {
    auto it = signals_.find(signalNumber);
    if (it == signals_.end()) return "";
    
    const SignalInfo& sig = it->second;
    
    std::ostringstream cmd;
    cmd << "handle " << sig.name;
    cmd << (sig.stop ? " stop" : " nostop");
    cmd << (sig.print ? " print" : " noprint");
    cmd << (sig.pass ? " pass" : " nopass");
    
    return cmd.str();
}

std::vector<std::string> UCSignalHandler::GenerateAllGDBCommands() const {
    std::vector<std::string> commands;
    
    for (const auto& pair : signals_) {
        if (!pair.second.isRealtime) {  // Skip realtime signals for now
            commands.push_back(GenerateGDBHandleCommand(pair.first));
        }
    }
    
    return commands;
}

void UCSignalHandler::StopOnAll() {
    for (auto& pair : signals_) {
        pair.second.stop = true;
        pair.second.print = true;
    }
}

void UCSignalHandler::StopOnNone() {
    for (auto& pair : signals_) {
        pair.second.stop = false;
    }
}

void UCSignalHandler::ResetToGDBDefaults() {
    LoadDefaults();
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string SignalNumberToName(int signalNumber) {
    static std::map<int, std::string> signalNames = {
        {1,  "SIGHUP"},  {2,  "SIGINT"},  {3,  "SIGQUIT"}, {4,  "SIGILL"},
        {5,  "SIGTRAP"}, {6,  "SIGABRT"}, {7,  "SIGBUS"},  {8,  "SIGFPE"},
        {9,  "SIGKILL"}, {10, "SIGUSR1"}, {11, "SIGSEGV"}, {12, "SIGUSR2"},
        {13, "SIGPIPE"}, {14, "SIGALRM"}, {15, "SIGTERM"}, {17, "SIGCHLD"},
        {18, "SIGCONT"}, {19, "SIGSTOP"}, {20, "SIGTSTP"}, {21, "SIGTTIN"},
        {22, "SIGTTOU"}, {23, "SIGURG"},  {24, "SIGXCPU"}, {25, "SIGXFSZ"},
        {26, "SIGVTALRM"},{27, "SIGPROF"},{28, "SIGWINCH"},{29, "SIGIO"},
        {30, "SIGPWR"},  {31, "SIGSYS"}
    };
    
    auto it = signalNames.find(signalNumber);
    if (it != signalNames.end()) {
        return it->second;
    }
    
    if (signalNumber >= 34 && signalNumber <= 64) {
        return "SIGRT" + std::to_string(signalNumber - 34);
    }
    
    return "SIG" + std::to_string(signalNumber);
}

int SignalNameToNumber(const std::string& name) {
    static std::map<std::string, int> signalNumbers = {
        {"SIGHUP", 1},  {"SIGINT", 2},  {"SIGQUIT", 3}, {"SIGILL", 4},
        {"SIGTRAP", 5}, {"SIGABRT", 6}, {"SIGBUS", 7},  {"SIGFPE", 8},
        {"SIGKILL", 9}, {"SIGUSR1", 10},{"SIGSEGV", 11},{"SIGUSR2", 12},
        {"SIGPIPE", 13},{"SIGALRM", 14},{"SIGTERM", 15},{"SIGCHLD", 17},
        {"SIGCONT", 18},{"SIGSTOP", 19},{"SIGTSTP", 20},{"SIGTTIN", 21},
        {"SIGTTOU", 22},{"SIGURG", 23}, {"SIGXCPU", 24},{"SIGXFSZ", 25},
        {"SIGVTALRM", 26},{"SIGPROF", 27},{"SIGWINCH", 28},{"SIGIO", 29},
        {"SIGPWR", 30}, {"SIGSYS", 31}
    };
    
    auto it = signalNumbers.find(name);
    if (it != signalNumbers.end()) {
        return it->second;
    }
    
    // Check for SIGRT format
    if (name.substr(0, 5) == "SIGRT") {
        try {
            int rtNum = std::stoi(name.substr(5));
            return 34 + rtNum;
        } catch (...) {}
    }
    
    return 0;
}

std::string GetSignalDescription(int signalNumber) {
    static std::map<int, std::string> descriptions = {
        {1,  "Hangup"},
        {2,  "Interrupt from keyboard"},
        {3,  "Quit from keyboard"},
        {4,  "Illegal instruction"},
        {5,  "Trace/breakpoint trap"},
        {6,  "Abort signal"},
        {7,  "Bus error (bad memory access)"},
        {8,  "Floating point exception"},
        {9,  "Kill signal"},
        {10, "User-defined signal 1"},
        {11, "Segmentation fault (invalid memory reference)"},
        {12, "User-defined signal 2"},
        {13, "Broken pipe"},
        {14, "Timer signal from alarm"},
        {15, "Termination signal"},
        {17, "Child stopped or terminated"},
        {18, "Continue if stopped"},
        {19, "Stop process"},
        {20, "Stop typed at terminal"},
        {21, "Terminal input for background process"},
        {22, "Terminal output for background process"},
        {31, "Bad system call"}
    };
    
    auto it = descriptions.find(signalNumber);
    return (it != descriptions.end()) ? it->second : "Unknown signal";
}

bool IsSignalFatal(int signalNumber) {
    switch (signalNumber) {
        case 4:  // SIGILL
        case 6:  // SIGABRT
        case 7:  // SIGBUS
        case 8:  // SIGFPE
        case 9:  // SIGKILL
        case 11: // SIGSEGV
        case 31: // SIGSYS
            return true;
        default:
            return false;
    }
}

} // namespace IDE
} // namespace UltraCanvas
