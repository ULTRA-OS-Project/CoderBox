// Apps/IDE/Debug/Exceptions/UCExceptionManager.h
// Exception Manager for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <mutex>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Exception category
 */
enum class ExceptionCategory {
    CppException,       // C++ exceptions (std::exception, etc.)
    SystemException,    // System/OS exceptions (access violation, etc.)
    UnixSignal,         // Unix signals (SIGSEGV, SIGFPE, etc.)
    WindowsSEH,         // Windows Structured Exception Handling
    RuntimeError,       // Runtime errors (assertion, abort)
    UserDefined,        // User-defined exception types
    All                 // All exceptions
};

/**
 * @brief Exception break behavior
 */
enum class ExceptionBreakBehavior {
    Never,              // Never break on this exception
    Always,             // Always break when thrown
    Unhandled,          // Break only if unhandled
    FirstChance,        // Break on first chance (before handlers)
    SecondChance        // Break on second chance (after handlers fail)
};

/**
 * @brief Exception severity level
 */
enum class ExceptionSeverity {
    Information,
    Warning,
    Error,
    Critical,
    Fatal
};

/**
 * @brief Single exception type configuration
 */
struct ExceptionType {
    std::string name;                   // Exception name/type
    std::string displayName;            // User-friendly name
    ExceptionCategory category = ExceptionCategory::CppException;
    ExceptionBreakBehavior breakBehavior = ExceptionBreakBehavior::Unhandled;
    ExceptionSeverity severity = ExceptionSeverity::Error;
    bool enabled = true;
    std::string description;
    int code = 0;                       // Exception code (for system exceptions)
    
    ExceptionType() = default;
    ExceptionType(const std::string& n, ExceptionCategory cat = ExceptionCategory::CppException)
        : name(n), displayName(n), category(cat) {}
};

/**
 * @brief Caught exception information
 */
struct CaughtException {
    std::string type;                   // Exception type name
    std::string message;                // Exception message
    ExceptionCategory category = ExceptionCategory::CppException;
    ExceptionSeverity severity = ExceptionSeverity::Error;
    int code = 0;                       // Exception/signal code
    uint64_t address = 0;               // Address where exception occurred
    std::string functionName;           // Function where thrown
    std::string sourceFile;             // Source file
    int sourceLine = 0;                 // Source line
    std::string stackTrace;             // Stack trace as string
    bool isFirstChance = true;          // First vs second chance
    bool isHandled = false;             // Whether a handler exists
    std::string additionalInfo;         // Platform-specific info
    
    CaughtException() = default;
};

/**
 * @brief Exception filter rule
 */
struct ExceptionFilter {
    std::string pattern;                // Regex pattern for exception name
    ExceptionBreakBehavior behavior = ExceptionBreakBehavior::Always;
    bool enabled = true;
    int priority = 0;                   // Higher priority rules match first
    
    ExceptionFilter() = default;
    ExceptionFilter(const std::string& p, ExceptionBreakBehavior b)
        : pattern(p), behavior(b) {}
};

/**
 * @brief Exception manager
 * 
 * Manages exception breakpoints, filters, and caught exception handling.
 * Supports C++ exceptions, system exceptions, and Unix signals.
 */
class UCExceptionManager {
public:
    UCExceptionManager();
    ~UCExceptionManager();
    
    // =========================================================================
    // EXCEPTION TYPE CONFIGURATION
    // =========================================================================
    
    /**
     * @brief Register an exception type
     */
    void RegisterExceptionType(const ExceptionType& type);
    
    /**
     * @brief Unregister an exception type
     */
    void UnregisterExceptionType(const std::string& name);
    
    /**
     * @brief Get exception type by name
     */
    const ExceptionType* GetExceptionType(const std::string& name) const;
    
    /**
     * @brief Get all registered exception types
     */
    std::vector<ExceptionType> GetAllExceptionTypes() const;
    
    /**
     * @brief Get exception types by category
     */
    std::vector<ExceptionType> GetExceptionTypesByCategory(ExceptionCategory category) const;
    
    // =========================================================================
    // BREAK BEHAVIOR
    // =========================================================================
    
    /**
     * @brief Set break behavior for an exception type
     */
    void SetBreakBehavior(const std::string& exceptionName, ExceptionBreakBehavior behavior);
    
    /**
     * @brief Get break behavior for an exception type
     */
    ExceptionBreakBehavior GetBreakBehavior(const std::string& exceptionName) const;
    
    /**
     * @brief Enable/disable breaking on an exception type
     */
    void SetExceptionEnabled(const std::string& exceptionName, bool enabled);
    
    /**
     * @brief Check if exception type is enabled
     */
    bool IsExceptionEnabled(const std::string& exceptionName) const;
    
    /**
     * @brief Set behavior for all exceptions in a category
     */
    void SetCategoryBehavior(ExceptionCategory category, ExceptionBreakBehavior behavior);
    
    /**
     * @brief Enable/disable entire category
     */
    void SetCategoryEnabled(ExceptionCategory category, bool enabled);
    
    // =========================================================================
    // EXCEPTION FILTERS
    // =========================================================================
    
    /**
     * @brief Add an exception filter
     */
    int AddFilter(const ExceptionFilter& filter);
    
    /**
     * @brief Remove a filter by ID
     */
    bool RemoveFilter(int filterId);
    
    /**
     * @brief Get all filters
     */
    std::vector<std::pair<int, ExceptionFilter>> GetFilters() const;
    
    /**
     * @brief Clear all filters
     */
    void ClearFilters();
    
    /**
     * @brief Evaluate filters for an exception
     */
    ExceptionBreakBehavior EvaluateFilters(const std::string& exceptionName) const;
    
    // =========================================================================
    // EXCEPTION HANDLING
    // =========================================================================
    
    /**
     * @brief Called when an exception is caught
     * @return true if debugger should break
     */
    bool OnExceptionCaught(const CaughtException& exception);
    
    /**
     * @brief Get the last caught exception
     */
    const CaughtException& GetLastException() const { return lastException_; }
    
    /**
     * @brief Get exception history
     */
    std::vector<CaughtException> GetExceptionHistory() const;
    
    /**
     * @brief Clear exception history
     */
    void ClearExceptionHistory();
    
    /**
     * @brief Check if should break on this exception
     */
    bool ShouldBreak(const CaughtException& exception) const;
    
    // =========================================================================
    // C++ EXCEPTION SETTINGS
    // =========================================================================
    
    void SetBreakOnAllCppExceptions(bool breakOnAll);
    bool GetBreakOnAllCppExceptions() const { return breakOnAllCpp_; }
    
    void SetBreakOnUnhandledCppExceptions(bool breakOnUnhandled);
    bool GetBreakOnUnhandledCppExceptions() const { return breakOnUnhandledCpp_; }
    
    void AddCppExceptionType(const std::string& typeName);
    void RemoveCppExceptionType(const std::string& typeName);
    std::vector<std::string> GetCppExceptionTypes() const;
    
    // =========================================================================
    // JUST-MY-CODE
    // =========================================================================
    
    void SetJustMyCodeEnabled(bool enabled) { justMyCodeEnabled_ = enabled; }
    bool IsJustMyCodeEnabled() const { return justMyCodeEnabled_; }
    
    void AddJustMyCodeModule(const std::string& modulePath);
    void RemoveJustMyCodeModule(const std::string& modulePath);
    bool IsMyCode(const std::string& modulePath) const;
    
    // =========================================================================
    // PRESETS
    // =========================================================================
    
    /**
     * @brief Load default exception configurations
     */
    void LoadDefaults();
    
    /**
     * @brief Load C++ standard exception types
     */
    void LoadCppStandardExceptions();
    
    /**
     * @brief Load Unix signals
     */
    void LoadUnixSignals();
    
    /**
     * @brief Load Windows SEH exceptions
     */
    void LoadWindowsSEH();
    
    // =========================================================================
    // PERSISTENCE
    // =========================================================================
    
    std::string ToJson() const;
    void FromJson(const std::string& json);
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const CaughtException&)> onExceptionCaught;
    std::function<void(const CaughtException&)> onExceptionBreak;
    std::function<void(const std::string&)> onExceptionTypeChanged;
    
private:
    bool MatchesFilter(const std::string& name, const std::string& pattern) const;
    ExceptionSeverity DetermineSeverity(const CaughtException& exception) const;
    
    std::map<std::string, ExceptionType> exceptionTypes_;
    std::map<int, ExceptionFilter> filters_;
    int nextFilterId_ = 1;
    
    std::vector<CaughtException> exceptionHistory_;
    CaughtException lastException_;
    size_t maxHistorySize_ = 100;
    
    bool breakOnAllCpp_ = false;
    bool breakOnUnhandledCpp_ = true;
    std::set<std::string> cppExceptionTypes_;
    
    bool justMyCodeEnabled_ = true;
    std::set<std::string> justMyCodeModules_;
    
    mutable std::mutex mutex_;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string ExceptionCategoryToString(ExceptionCategory category);
ExceptionCategory StringToExceptionCategory(const std::string& str);

std::string ExceptionBreakBehaviorToString(ExceptionBreakBehavior behavior);
ExceptionBreakBehavior StringToExceptionBreakBehavior(const std::string& str);

std::string ExceptionSeverityToString(ExceptionSeverity severity);
ExceptionSeverity StringToExceptionSeverity(const std::string& str);

} // namespace IDE
} // namespace UltraCanvas
