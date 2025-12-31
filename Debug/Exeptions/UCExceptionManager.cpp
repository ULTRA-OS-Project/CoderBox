// Apps/IDE/Debug/Exceptions/UCExceptionManager.cpp
// Exception Manager Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCExceptionManager.h"
#include <regex>
#include <algorithm>
#include <sstream>

namespace UltraCanvas {
namespace IDE {

UCExceptionManager::UCExceptionManager() {
    LoadDefaults();
}

UCExceptionManager::~UCExceptionManager() = default;

// ============================================================================
// EXCEPTION TYPE CONFIGURATION
// ============================================================================

void UCExceptionManager::RegisterExceptionType(const ExceptionType& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    exceptionTypes_[type.name] = type;
    
    if (onExceptionTypeChanged) {
        onExceptionTypeChanged(type.name);
    }
}

void UCExceptionManager::UnregisterExceptionType(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    exceptionTypes_.erase(name);
}

const ExceptionType* UCExceptionManager::GetExceptionType(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = exceptionTypes_.find(name);
    if (it != exceptionTypes_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<ExceptionType> UCExceptionManager::GetAllExceptionTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ExceptionType> result;
    for (const auto& pair : exceptionTypes_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<ExceptionType> UCExceptionManager::GetExceptionTypesByCategory(ExceptionCategory category) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ExceptionType> result;
    for (const auto& pair : exceptionTypes_) {
        if (pair.second.category == category || category == ExceptionCategory::All) {
            result.push_back(pair.second);
        }
    }
    return result;
}

// ============================================================================
// BREAK BEHAVIOR
// ============================================================================

void UCExceptionManager::SetBreakBehavior(const std::string& exceptionName, 
                                           ExceptionBreakBehavior behavior) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = exceptionTypes_.find(exceptionName);
    if (it != exceptionTypes_.end()) {
        it->second.breakBehavior = behavior;
        
        if (onExceptionTypeChanged) {
            onExceptionTypeChanged(exceptionName);
        }
    }
}

ExceptionBreakBehavior UCExceptionManager::GetBreakBehavior(const std::string& exceptionName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = exceptionTypes_.find(exceptionName);
    if (it != exceptionTypes_.end()) {
        return it->second.breakBehavior;
    }
    return ExceptionBreakBehavior::Unhandled;
}

void UCExceptionManager::SetExceptionEnabled(const std::string& exceptionName, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = exceptionTypes_.find(exceptionName);
    if (it != exceptionTypes_.end()) {
        it->second.enabled = enabled;
    }
}

bool UCExceptionManager::IsExceptionEnabled(const std::string& exceptionName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = exceptionTypes_.find(exceptionName);
    if (it != exceptionTypes_.end()) {
        return it->second.enabled;
    }
    return false;
}

void UCExceptionManager::SetCategoryBehavior(ExceptionCategory category, 
                                              ExceptionBreakBehavior behavior) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : exceptionTypes_) {
        if (pair.second.category == category) {
            pair.second.breakBehavior = behavior;
        }
    }
}

void UCExceptionManager::SetCategoryEnabled(ExceptionCategory category, bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& pair : exceptionTypes_) {
        if (pair.second.category == category) {
            pair.second.enabled = enabled;
        }
    }
}

// ============================================================================
// EXCEPTION FILTERS
// ============================================================================

int UCExceptionManager::AddFilter(const ExceptionFilter& filter) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    int id = nextFilterId_++;
    filters_[id] = filter;
    return id;
}

bool UCExceptionManager::RemoveFilter(int filterId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return filters_.erase(filterId) > 0;
}

std::vector<std::pair<int, ExceptionFilter>> UCExceptionManager::GetFilters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::pair<int, ExceptionFilter>> result;
    for (const auto& pair : filters_) {
        result.push_back(pair);
    }
    
    // Sort by priority
    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) {
                  return a.second.priority > b.second.priority;
              });
    
    return result;
}

void UCExceptionManager::ClearFilters() {
    std::lock_guard<std::mutex> lock(mutex_);
    filters_.clear();
}

ExceptionBreakBehavior UCExceptionManager::EvaluateFilters(const std::string& exceptionName) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Get filters sorted by priority
    std::vector<std::pair<int, ExceptionFilter>> sortedFilters;
    for (const auto& pair : filters_) {
        if (pair.second.enabled) {
            sortedFilters.push_back(pair);
        }
    }
    
    std::sort(sortedFilters.begin(), sortedFilters.end(),
              [](const auto& a, const auto& b) {
                  return a.second.priority > b.second.priority;
              });
    
    // Find first matching filter
    for (const auto& pair : sortedFilters) {
        if (MatchesFilter(exceptionName, pair.second.pattern)) {
            return pair.second.behavior;
        }
    }
    
    // No filter matched, use default behavior from exception type
    auto it = exceptionTypes_.find(exceptionName);
    if (it != exceptionTypes_.end()) {
        return it->second.breakBehavior;
    }
    
    return ExceptionBreakBehavior::Unhandled;
}

// ============================================================================
// EXCEPTION HANDLING
// ============================================================================

bool UCExceptionManager::OnExceptionCaught(const CaughtException& exception) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        lastException_ = exception;
        exceptionHistory_.push_back(exception);
        
        // Trim history
        while (exceptionHistory_.size() > maxHistorySize_) {
            exceptionHistory_.erase(exceptionHistory_.begin());
        }
    }
    
    if (onExceptionCaught) {
        onExceptionCaught(exception);
    }
    
    bool shouldBreak = ShouldBreak(exception);
    
    if (shouldBreak && onExceptionBreak) {
        onExceptionBreak(exception);
    }
    
    return shouldBreak;
}

std::vector<CaughtException> UCExceptionManager::GetExceptionHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return exceptionHistory_;
}

void UCExceptionManager::ClearExceptionHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    exceptionHistory_.clear();
}

bool UCExceptionManager::ShouldBreak(const CaughtException& exception) const {
    // Check Just My Code
    if (justMyCodeEnabled_ && !exception.sourceFile.empty()) {
        if (!IsMyCode(exception.sourceFile)) {
            return false;  // Don't break in non-user code
        }
    }
    
    // Check filters first
    ExceptionBreakBehavior behavior = EvaluateFilters(exception.type);
    
    switch (behavior) {
        case ExceptionBreakBehavior::Never:
            return false;
            
        case ExceptionBreakBehavior::Always:
            return true;
            
        case ExceptionBreakBehavior::Unhandled:
            return !exception.isHandled;
            
        case ExceptionBreakBehavior::FirstChance:
            return exception.isFirstChance;
            
        case ExceptionBreakBehavior::SecondChance:
            return !exception.isFirstChance;
    }
    
    return false;
}

// ============================================================================
// C++ EXCEPTION SETTINGS
// ============================================================================

void UCExceptionManager::SetBreakOnAllCppExceptions(bool breakOnAll) {
    std::lock_guard<std::mutex> lock(mutex_);
    breakOnAllCpp_ = breakOnAll;
}

void UCExceptionManager::SetBreakOnUnhandledCppExceptions(bool breakOnUnhandled) {
    std::lock_guard<std::mutex> lock(mutex_);
    breakOnUnhandledCpp_ = breakOnUnhandled;
}

void UCExceptionManager::AddCppExceptionType(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(mutex_);
    cppExceptionTypes_.insert(typeName);
    
    // Also register as exception type
    ExceptionType type;
    type.name = typeName;
    type.displayName = typeName;
    type.category = ExceptionCategory::CppException;
    type.breakBehavior = ExceptionBreakBehavior::Unhandled;
    exceptionTypes_[typeName] = type;
}

void UCExceptionManager::RemoveCppExceptionType(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(mutex_);
    cppExceptionTypes_.erase(typeName);
}

std::vector<std::string> UCExceptionManager::GetCppExceptionTypes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(cppExceptionTypes_.begin(), cppExceptionTypes_.end());
}

// ============================================================================
// JUST-MY-CODE
// ============================================================================

void UCExceptionManager::AddJustMyCodeModule(const std::string& modulePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    justMyCodeModules_.insert(modulePath);
}

void UCExceptionManager::RemoveJustMyCodeModule(const std::string& modulePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    justMyCodeModules_.erase(modulePath);
}

bool UCExceptionManager::IsMyCode(const std::string& modulePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (justMyCodeModules_.empty()) {
        return true;  // No restrictions
    }
    
    // Check if path starts with any of our modules
    for (const auto& module : justMyCodeModules_) {
        if (modulePath.find(module) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// PRESETS
// ============================================================================

void UCExceptionManager::LoadDefaults() {
    LoadCppStandardExceptions();
    
#ifdef __unix__
    LoadUnixSignals();
#elif defined(_WIN32)
    LoadWindowsSEH();
#endif
}

void UCExceptionManager::LoadCppStandardExceptions() {
    std::vector<std::pair<std::string, std::string>> stdExceptions = {
        {"std::exception", "Base C++ exception"},
        {"std::bad_alloc", "Memory allocation failed"},
        {"std::bad_cast", "Invalid dynamic_cast"},
        {"std::bad_typeid", "Invalid typeid"},
        {"std::bad_exception", "Unexpected exception"},
        {"std::logic_error", "Logic error"},
        {"std::domain_error", "Domain error"},
        {"std::invalid_argument", "Invalid argument"},
        {"std::length_error", "Length error"},
        {"std::out_of_range", "Out of range access"},
        {"std::runtime_error", "Runtime error"},
        {"std::overflow_error", "Arithmetic overflow"},
        {"std::underflow_error", "Arithmetic underflow"},
        {"std::range_error", "Range error"},
        {"std::system_error", "System error"},
        {"std::ios_base::failure", "I/O error"}
    };
    
    for (const auto& pair : stdExceptions) {
        ExceptionType type;
        type.name = pair.first;
        type.displayName = pair.first;
        type.category = ExceptionCategory::CppException;
        type.description = pair.second;
        type.breakBehavior = ExceptionBreakBehavior::Unhandled;
        type.severity = ExceptionSeverity::Error;
        type.enabled = true;
        
        RegisterExceptionType(type);
        cppExceptionTypes_.insert(pair.first);
    }
}

void UCExceptionManager::LoadUnixSignals() {
    struct SignalInfo {
        int number;
        std::string name;
        std::string description;
        ExceptionSeverity severity;
        ExceptionBreakBehavior defaultBehavior;
    };
    
    std::vector<SignalInfo> signals = {
        {1,  "SIGHUP",    "Hangup",                    ExceptionSeverity::Warning,  ExceptionBreakBehavior::Never},
        {2,  "SIGINT",    "Interrupt",                 ExceptionSeverity::Warning,  ExceptionBreakBehavior::Never},
        {3,  "SIGQUIT",   "Quit",                      ExceptionSeverity::Warning,  ExceptionBreakBehavior::Always},
        {4,  "SIGILL",    "Illegal instruction",       ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {5,  "SIGTRAP",   "Trace/breakpoint trap",     ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {6,  "SIGABRT",   "Aborted",                   ExceptionSeverity::Fatal,    ExceptionBreakBehavior::Always},
        {7,  "SIGBUS",    "Bus error",                 ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {8,  "SIGFPE",    "Floating point exception",  ExceptionSeverity::Error,    ExceptionBreakBehavior::Always},
        {9,  "SIGKILL",   "Killed",                    ExceptionSeverity::Fatal,    ExceptionBreakBehavior::Never},
        {10, "SIGUSR1",   "User signal 1",             ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {11, "SIGSEGV",   "Segmentation fault",        ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {12, "SIGUSR2",   "User signal 2",             ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {13, "SIGPIPE",   "Broken pipe",               ExceptionSeverity::Warning,  ExceptionBreakBehavior::Never},
        {14, "SIGALRM",   "Alarm clock",               ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {15, "SIGTERM",   "Terminated",                ExceptionSeverity::Warning,  ExceptionBreakBehavior::Never},
        {17, "SIGCHLD",   "Child stopped/terminated",  ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {18, "SIGCONT",   "Continued",                 ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {19, "SIGSTOP",   "Stopped",                   ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {20, "SIGTSTP",   "Stopped (tty input)",       ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
    };
    
    for (const auto& sig : signals) {
        ExceptionType type;
        type.name = sig.name;
        type.displayName = sig.name + " (" + std::to_string(sig.number) + ")";
        type.category = ExceptionCategory::UnixSignal;
        type.description = sig.description;
        type.code = sig.number;
        type.severity = sig.severity;
        type.breakBehavior = sig.defaultBehavior;
        type.enabled = true;
        
        RegisterExceptionType(type);
    }
}

void UCExceptionManager::LoadWindowsSEH() {
    struct SEHInfo {
        uint32_t code;
        std::string name;
        std::string description;
        ExceptionSeverity severity;
        ExceptionBreakBehavior defaultBehavior;
    };
    
    std::vector<SEHInfo> sehExceptions = {
        {0xC0000005, "ACCESS_VIOLATION",         "Access violation",              ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {0xC0000006, "IN_PAGE_ERROR",            "In page error",                 ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {0xC0000008, "INVALID_HANDLE",           "Invalid handle",                ExceptionSeverity::Error,    ExceptionBreakBehavior::Always},
        {0xC000001D, "ILLEGAL_INSTRUCTION",      "Illegal instruction",           ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {0xC0000025, "NONCONTINUABLE_EXCEPTION", "Non-continuable exception",     ExceptionSeverity::Fatal,    ExceptionBreakBehavior::Always},
        {0xC0000026, "INVALID_DISPOSITION",      "Invalid disposition",           ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {0xC000008C, "ARRAY_BOUNDS_EXCEEDED",    "Array bounds exceeded",         ExceptionSeverity::Error,    ExceptionBreakBehavior::Always},
        {0xC000008D, "FLT_DENORMAL_OPERAND",     "Float denormal operand",        ExceptionSeverity::Warning,  ExceptionBreakBehavior::Unhandled},
        {0xC000008E, "FLT_DIVIDE_BY_ZERO",       "Float divide by zero",          ExceptionSeverity::Error,    ExceptionBreakBehavior::Always},
        {0xC000008F, "FLT_INEXACT_RESULT",       "Float inexact result",          ExceptionSeverity::Warning,  ExceptionBreakBehavior::Never},
        {0xC0000090, "FLT_INVALID_OPERATION",    "Float invalid operation",       ExceptionSeverity::Error,    ExceptionBreakBehavior::Unhandled},
        {0xC0000091, "FLT_OVERFLOW",             "Float overflow",                ExceptionSeverity::Error,    ExceptionBreakBehavior::Unhandled},
        {0xC0000092, "FLT_STACK_CHECK",          "Float stack check",             ExceptionSeverity::Error,    ExceptionBreakBehavior::Always},
        {0xC0000093, "FLT_UNDERFLOW",            "Float underflow",               ExceptionSeverity::Warning,  ExceptionBreakBehavior::Never},
        {0xC0000094, "INT_DIVIDE_BY_ZERO",       "Integer divide by zero",        ExceptionSeverity::Error,    ExceptionBreakBehavior::Always},
        {0xC0000095, "INT_OVERFLOW",             "Integer overflow",              ExceptionSeverity::Error,    ExceptionBreakBehavior::Unhandled},
        {0xC00000FD, "STACK_OVERFLOW",           "Stack overflow",                ExceptionSeverity::Critical, ExceptionBreakBehavior::Always},
        {0x80000001, "GUARD_PAGE_VIOLATION",     "Guard page violation",          ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {0x80000003, "BREAKPOINT",               "Breakpoint",                    ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
        {0x80000004, "SINGLE_STEP",              "Single step",                   ExceptionSeverity::Information, ExceptionBreakBehavior::Never},
    };
    
    for (const auto& seh : sehExceptions) {
        ExceptionType type;
        type.name = seh.name;
        
        std::ostringstream ss;
        ss << seh.name << " (0x" << std::hex << seh.code << ")";
        type.displayName = ss.str();
        
        type.category = ExceptionCategory::WindowsSEH;
        type.description = seh.description;
        type.code = static_cast<int>(seh.code);
        type.severity = seh.severity;
        type.breakBehavior = seh.defaultBehavior;
        type.enabled = true;
        
        RegisterExceptionType(type);
    }
}

// ============================================================================
// PERSISTENCE
// ============================================================================

std::string UCExceptionManager::ToJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream json;
    json << "{\n";
    json << "  \"breakOnAllCpp\": " << (breakOnAllCpp_ ? "true" : "false") << ",\n";
    json << "  \"breakOnUnhandledCpp\": " << (breakOnUnhandledCpp_ ? "true" : "false") << ",\n";
    json << "  \"justMyCodeEnabled\": " << (justMyCodeEnabled_ ? "true" : "false") << ",\n";
    
    json << "  \"exceptionTypes\": [\n";
    bool first = true;
    for (const auto& pair : exceptionTypes_) {
        if (!first) json << ",\n";
        first = false;
        
        const auto& type = pair.second;
        json << "    {\"name\": \"" << type.name << "\", ";
        json << "\"enabled\": " << (type.enabled ? "true" : "false") << ", ";
        json << "\"behavior\": \"" << ExceptionBreakBehaviorToString(type.breakBehavior) << "\"}";
    }
    json << "\n  ]\n";
    json << "}";
    
    return json.str();
}

void UCExceptionManager::FromJson(const std::string& json) {
    // TODO: Implement JSON parsing
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

bool UCExceptionManager::MatchesFilter(const std::string& name, const std::string& pattern) const {
    try {
        std::regex re(pattern, std::regex::icase);
        return std::regex_search(name, re);
    } catch (...) {
        // Simple substring match as fallback
        return name.find(pattern) != std::string::npos;
    }
}

ExceptionSeverity UCExceptionManager::DetermineSeverity(const CaughtException& exception) const {
    auto type = GetExceptionType(exception.type);
    if (type) {
        return type->severity;
    }
    
    // Default based on category
    switch (exception.category) {
        case ExceptionCategory::SystemException:
        case ExceptionCategory::WindowsSEH:
            return ExceptionSeverity::Critical;
        case ExceptionCategory::UnixSignal:
            return ExceptionSeverity::Error;
        default:
            return ExceptionSeverity::Error;
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string ExceptionCategoryToString(ExceptionCategory category) {
    switch (category) {
        case ExceptionCategory::CppException: return "cpp";
        case ExceptionCategory::SystemException: return "system";
        case ExceptionCategory::UnixSignal: return "signal";
        case ExceptionCategory::WindowsSEH: return "seh";
        case ExceptionCategory::RuntimeError: return "runtime";
        case ExceptionCategory::UserDefined: return "user";
        case ExceptionCategory::All: return "all";
    }
    return "unknown";
}

ExceptionCategory StringToExceptionCategory(const std::string& str) {
    if (str == "cpp") return ExceptionCategory::CppException;
    if (str == "system") return ExceptionCategory::SystemException;
    if (str == "signal") return ExceptionCategory::UnixSignal;
    if (str == "seh") return ExceptionCategory::WindowsSEH;
    if (str == "runtime") return ExceptionCategory::RuntimeError;
    if (str == "user") return ExceptionCategory::UserDefined;
    if (str == "all") return ExceptionCategory::All;
    return ExceptionCategory::CppException;
}

std::string ExceptionBreakBehaviorToString(ExceptionBreakBehavior behavior) {
    switch (behavior) {
        case ExceptionBreakBehavior::Never: return "never";
        case ExceptionBreakBehavior::Always: return "always";
        case ExceptionBreakBehavior::Unhandled: return "unhandled";
        case ExceptionBreakBehavior::FirstChance: return "firstChance";
        case ExceptionBreakBehavior::SecondChance: return "secondChance";
    }
    return "unhandled";
}

ExceptionBreakBehavior StringToExceptionBreakBehavior(const std::string& str) {
    if (str == "never") return ExceptionBreakBehavior::Never;
    if (str == "always") return ExceptionBreakBehavior::Always;
    if (str == "firstChance") return ExceptionBreakBehavior::FirstChance;
    if (str == "secondChance") return ExceptionBreakBehavior::SecondChance;
    return ExceptionBreakBehavior::Unhandled;
}

std::string ExceptionSeverityToString(ExceptionSeverity severity) {
    switch (severity) {
        case ExceptionSeverity::Information: return "info";
        case ExceptionSeverity::Warning: return "warning";
        case ExceptionSeverity::Error: return "error";
        case ExceptionSeverity::Critical: return "critical";
        case ExceptionSeverity::Fatal: return "fatal";
    }
    return "error";
}

ExceptionSeverity StringToExceptionSeverity(const std::string& str) {
    if (str == "info") return ExceptionSeverity::Information;
    if (str == "warning") return ExceptionSeverity::Warning;
    if (str == "critical") return ExceptionSeverity::Critical;
    if (str == "fatal") return ExceptionSeverity::Fatal;
    return ExceptionSeverity::Error;
}

} // namespace IDE
} // namespace UltraCanvas
