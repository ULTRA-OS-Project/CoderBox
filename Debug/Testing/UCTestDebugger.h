// Apps/IDE/Debug/Testing/UCTestDebugger.h
// Test Debugger for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../Core/UCDebugManager.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Test framework type
 */
enum class TestFramework {
    Unknown,
    GoogleTest,
    Catch2,
    Boost,
    CppUnit,
    DocTest,
    Custom
};

/**
 * @brief Test execution state
 */
enum class TestState {
    NotRun,
    Running,
    Passed,
    Failed,
    Skipped,
    Disabled,
    Error,
    Timeout
};

/**
 * @brief Single test case
 */
struct TestCase {
    std::string id;                     // Unique identifier
    std::string name;                   // Test name
    std::string suiteName;              // Suite/fixture name
    std::string fullName;               // Full qualified name
    std::string sourceFile;             // Source file path
    int sourceLine = 0;                 // Line number
    TestState state = TestState::NotRun;
    std::string errorMessage;           // Failure message
    std::string stackTrace;             // Stack trace on failure
    std::chrono::milliseconds duration{0};
    std::vector<std::string> tags;      // Test tags (Catch2)
    bool isParameterized = false;       // Parameterized test
    std::string parameterInfo;          // Parameter description
    
    TestCase() = default;
    TestCase(const std::string& n, const std::string& suite = "")
        : name(n), suiteName(suite), fullName(suite.empty() ? n : suite + "." + n) {}
};

/**
 * @brief Test suite (group of tests)
 */
struct TestSuite {
    std::string name;
    std::string sourceFile;
    int sourceLine = 0;
    std::vector<TestCase> tests;
    int passedCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    std::chrono::milliseconds duration{0};
    
    TestSuite() = default;
    TestSuite(const std::string& n) : name(n) {}
    
    void UpdateCounts();
};

/**
 * @brief Test run result
 */
struct TestRunResult {
    std::string runId;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::chrono::milliseconds totalDuration{0};
    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;
    int skippedTests = 0;
    int errorTests = 0;
    std::vector<TestSuite> suites;
    std::string outputLog;
    bool success = false;
    
    void CalculateTotals();
};

/**
 * @brief Test filter for selective execution
 */
struct TestFilter {
    std::string pattern;                // Name pattern (supports wildcards)
    std::vector<std::string> tags;      // Include tags
    std::vector<std::string> excludeTags; // Exclude tags
    std::vector<std::string> suites;    // Specific suites
    std::vector<std::string> tests;     // Specific tests
    bool includeDisabled = false;
    
    bool Matches(const TestCase& test) const;
    std::string ToGoogleTestFilter() const;
    std::string ToCatch2Filter() const;
};

/**
 * @brief Test debug configuration
 */
struct TestDebugConfig {
    TestFramework framework = TestFramework::Unknown;
    std::string executablePath;
    std::string workingDirectory;
    std::vector<std::string> arguments;
    std::map<std::string, std::string> environment;
    TestFilter filter;
    bool breakOnFailure = true;         // Break when test fails
    bool breakOnException = true;       // Break on test exception
    bool stopOnFirstFailure = false;    // Stop run on first failure
    int timeout = 60000;                // Test timeout in ms
    bool captureOutput = true;
    bool shuffleTests = false;
    int repeatCount = 1;
    int randomSeed = 0;
};

/**
 * @brief Test adapter interface
 */
class ITestAdapter {
public:
    virtual ~ITestAdapter() = default;
    
    virtual TestFramework GetFramework() const = 0;
    virtual std::string GetName() const = 0;
    
    virtual bool CanHandle(const std::string& executable) const = 0;
    virtual std::vector<TestCase> DiscoverTests(const std::string& executable) = 0;
    virtual std::vector<std::string> BuildCommandLine(const TestDebugConfig& config) = 0;
    virtual void ParseOutput(const std::string& output, TestRunResult& result) = 0;
    virtual TestState ParseTestResult(const std::string& testOutput) = 0;
};

/**
 * @brief Test debugger
 * 
 * Integrates unit test frameworks with the debugger, allowing
 * test discovery, selective execution, and debugging of failing tests.
 */
class UCTestDebugger {
public:
    UCTestDebugger();
    ~UCTestDebugger();
    
    // =========================================================================
    // INITIALIZATION
    // =========================================================================
    
    void SetDebugManager(UCDebugManager* manager);
    void RegisterAdapter(std::shared_ptr<ITestAdapter> adapter);
    void UnregisterAdapter(TestFramework framework);
    
    // =========================================================================
    // TEST DISCOVERY
    // =========================================================================
    
    /**
     * @brief Discover tests in executable
     */
    std::vector<TestCase> DiscoverTests(const std::string& executable);
    
    /**
     * @brief Detect test framework
     */
    TestFramework DetectFramework(const std::string& executable);
    
    /**
     * @brief Get discovered tests
     */
    const std::vector<TestSuite>& GetTestSuites() const { return suites_; }
    
    /**
     * @brief Find test by name
     */
    const TestCase* FindTest(const std::string& fullName) const;
    
    /**
     * @brief Clear discovered tests
     */
    void ClearTests();
    
    // =========================================================================
    // TEST EXECUTION
    // =========================================================================
    
    /**
     * @brief Run all tests
     */
    bool RunAllTests(const TestDebugConfig& config);
    
    /**
     * @brief Run specific tests
     */
    bool RunTests(const TestDebugConfig& config, const std::vector<std::string>& testNames);
    
    /**
     * @brief Run single test
     */
    bool RunTest(const TestDebugConfig& config, const std::string& testName);
    
    /**
     * @brief Run failed tests from last run
     */
    bool RunFailedTests(const TestDebugConfig& config);
    
    /**
     * @brief Debug all tests
     */
    bool DebugAllTests(const TestDebugConfig& config);
    
    /**
     * @brief Debug specific test
     */
    bool DebugTest(const TestDebugConfig& config, const std::string& testName);
    
    /**
     * @brief Stop current test run
     */
    void StopTestRun();
    
    /**
     * @brief Check if tests are running
     */
    bool IsRunning() const { return isRunning_; }
    
    // =========================================================================
    // RESULTS
    // =========================================================================
    
    /**
     * @brief Get last test run result
     */
    const TestRunResult& GetLastResult() const { return lastResult_; }
    
    /**
     * @brief Get test run history
     */
    const std::vector<TestRunResult>& GetRunHistory() const { return runHistory_; }
    
    /**
     * @brief Clear run history
     */
    void ClearHistory();
    
    /**
     * @brief Export results to file
     */
    bool ExportResults(const std::string& filePath, const std::string& format = "xml");
    
    // =========================================================================
    // BREAKPOINTS
    // =========================================================================
    
    /**
     * @brief Set breakpoint on test entry
     */
    void SetTestBreakpoint(const std::string& testName);
    
    /**
     * @brief Remove test breakpoint
     */
    void RemoveTestBreakpoint(const std::string& testName);
    
    /**
     * @brief Set breakpoint on test failure assertion
     */
    void SetFailureBreakpoint(bool enable = true);
    
    // =========================================================================
    // CALLBACKS
    // =========================================================================
    
    std::function<void(const TestCase&)> onTestStarted;
    std::function<void(const TestCase&)> onTestFinished;
    std::function<void(const TestSuite&)> onSuiteStarted;
    std::function<void(const TestSuite&)> onSuiteFinished;
    std::function<void(const TestRunResult&)> onRunStarted;
    std::function<void(const TestRunResult&)> onRunFinished;
    std::function<void(const std::string&)> onOutput;
    std::function<void(const std::string&)> onError;
    
private:
    ITestAdapter* GetAdapter(TestFramework framework);
    ITestAdapter* GetAdapterForExecutable(const std::string& executable);
    void ProcessTestOutput(const std::string& output);
    void UpdateTestState(const std::string& testName, TestState state, 
                         const std::string& message = "");
    std::string GenerateRunId();
    
    UCDebugManager* debugManager_ = nullptr;
    std::map<TestFramework, std::shared_ptr<ITestAdapter>> adapters_;
    
    std::vector<TestSuite> suites_;
    std::map<std::string, TestCase*> testMap_;  // Quick lookup
    
    TestRunResult lastResult_;
    std::vector<TestRunResult> runHistory_;
    size_t maxHistorySize_ = 50;
    
    bool isRunning_ = false;
    TestDebugConfig currentConfig_;
    
    // Test breakpoints
    std::set<std::string> testBreakpoints_;
    bool breakOnFailure_ = true;
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string TestFrameworkToString(TestFramework framework);
TestFramework StringToTestFramework(const std::string& str);

std::string TestStateToString(TestState state);
TestState StringToTestState(const std::string& str);

} // namespace IDE
} // namespace UltraCanvas
