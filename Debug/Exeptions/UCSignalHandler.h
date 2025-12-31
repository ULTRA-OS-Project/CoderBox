// Apps/IDE/Debug/Exceptions/UCSignalHandler.h
// Signal Handler for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Signal disposition (what to do when signal is received)
 */
enum class SignalDisposition {
    Stop,           // Stop the program
    NoStop,         // Don't stop
    Print,          // Print message but don't stop
    Pass,           // Pass to program
    NoPass,         // Don't pass to program
    Ignore          // Ignore completely
};

/**
 * @brief Signal information
 */
struct SignalInfo {
    int number = 0;
    std::string name;
    std::string description;
    bool stop = true;           // Stop when received
    bool print = true;          // Print when received
    bool pass = true;           // Pass to program
    bool isRealtime = false;    // Is realtime signal (SIGRTMIN+n)
    
    SignalInfo() = default;
    SignalInfo(int num, const std::string& n, const std::string& desc)
        : number(num), name(n), description(desc) {}
};

/**
 * @brief Caught signal information
 */
struct CaughtSignal {
    int signalNumber = 0;
    std::string signalName;
    std::string description;
    int threadId = 0;
    uint64_t address = 0;           // Fault address (for SIGSEGV, etc.)
    std::string additionalInfo;     // si_code description, etc.
    bool isFatal = false;
    
    CaughtSignal() = default;
};

/**
 * @brief Signal group for easy configuration
 */
enum class SignalGroup {
    All,
    Program,        // Signals typically sent by program errors
    External,       // Signals from external sources
    Job,            // Job control signals
    Timer,          // Timer signals
    IO,             // I/O signals
    User,           // User-defined signals
    Realtime        // Realtime signals
};

/**
 * @brief Unix signal handler for debug sessions
 * 
 * Configures how the debugger handles signals sent to the target process.
 */
class UCSignalHandler {
public:
    UCSignalHandler();
    ~UCSignalHandler() = default;
    
    // =========================================================================
    // SIGNAL CONFIGURATION
    // =========================================================================
    
    /**
     * @brief Get signal info by number
     */
    const SignalInfo* GetSignal(int signalNumber) const;
    
    /**
     * @brief Get signal info by name
     */
    const SignalInfo* GetSignalByName(const std::string& name) const;
    
    /**
     * @brief Get all signals
     */
    std::vector<SignalInfo> GetAllSignals() const;
    
    /**
     * @brief Get signals in a group
     */
    std::vector<SignalInfo> GetSignalsInGroup(SignalGroup group) const;
    
    // =========================================================================
    // SIGNAL HANDLING
    // =========================================================================
    
    /**
     * @brief Set whether to stop on signal
     */
    void SetStop(int signalNumber, bool stop);
    void SetStop(const std::string& signalName, bool stop);
    
    /**
     * @brief Get whether to stop on signal
     */
    bool GetStop(int signalNumber) const;
    
    /**
     * @brief Set whether to print on signal
     */
    void SetPrint(int signalNumber, bool print);
    void SetPrint(const std::string& signalName, bool print);
    
    /**
     * @brief Get whether to print on signal
     */
    bool GetPrint(int signalNumber) const;
    
    /**
     * @brief Set whether to pass signal to program
     */
    void SetPass(int signalNumber, bool pass);
    void SetPass(const std::string& signalName, bool pass);
    
    /**
     * @brief Get whether to pass signal to program
     */
    bool GetPass(int signalNumber) const;
    
    /**
     * @brief Set all dispositions at once
     */
    void SetDisposition(int signalNumber, bool stop, bool print, bool pass);
    void SetDisposition(const std::string& signalName, bool stop, bool print, bool pass);
    
    // =========================================================================
    // GROUP OPERATIONS
    // =========================================================================
    
    /**
     * @brief Apply settings to signal group
     */
    void SetGroupStop(SignalGroup group, bool stop);
    void SetGroupPrint(SignalGroup group, bool print);
    void SetGroupPass(SignalGroup group, bool pass);
    
    // =========================================================================
    // SIGNAL PROCESSING
    // =========================================================================
    
    /**
     * @brief Called when a signal is caught
     * @return true if debugger should stop
     */
    bool OnSignalCaught(const CaughtSignal& signal);
    
    /**
     * @brief Get last caught signal
     */
    const CaughtSignal& GetLastSignal() const { return lastSignal_; }
    
    /**
     * @brief Get signal history
     */
    std::vector<CaughtSignal> GetSignalHistory() const { return signalHistory_; }
    
    /**
     * @brief Clear signal history
     */
    void ClearHistory() { signalHistory_.clear(); }
    
    // =========================================================================
    // GDB COMMAND GENERATION
    // =========================================================================
    
    /**
     * @brief Generate GDB 'handle' command for a signal
     */
    std::string GenerateGDBHandleCommand(int signalNumber) const;
    
    /**
     * @brief Generate all GDB handle commands
     */
    std::vector<std::string> GenerateAllGDBCommands() const;
    
    // =========================================================================
    // PRESETS
    // =========================================================================
    
    /**
     * @brief Load default signal handling
     */
    void LoadDefaults();
    
    /**
     * @brief Set all signals to stop
     */
    void StopOnAll();
    
    /**
     * @brief Set all signals to not stop
     */
    void StopOnNone();
    
    /**
     * @brief Reset to GDB defaults
     */
    void ResetToGDBDefaults();
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const CaughtSignal&)> onSignalCaught;
    std::function<void(int signalNumber)> onSignalConfigChanged;
    
private:
    void InitializeSignals();
    SignalGroup GetSignalGroup(int signalNumber) const;
    
    std::map<int, SignalInfo> signals_;
    std::map<std::string, int> nameToNumber_;
    
    CaughtSignal lastSignal_;
    std::vector<CaughtSignal> signalHistory_;
    size_t maxHistorySize_ = 50;
};

/**
 * @brief Get signal name from number
 */
std::string SignalNumberToName(int signalNumber);

/**
 * @brief Get signal number from name
 */
int SignalNameToNumber(const std::string& name);

/**
 * @brief Get signal description
 */
std::string GetSignalDescription(int signalNumber);

/**
 * @brief Check if signal is typically fatal
 */
bool IsSignalFatal(int signalNumber);

} // namespace IDE
} // namespace UltraCanvas
