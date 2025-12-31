// Apps/IDE/Debug/Testing/UCTestDebugger.cpp
// Test Debugger Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCTestDebugger.h"
#include <sstream>
#include <algorithm>
#include <random>
#include <ctime>
#include <fstream>
#include <regex>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// TestSuite
// ============================================================================

void TestSuite::UpdateCounts() {
    passedCount = 0;
    failedCount = 0;
    skippedCount = 0;
    duration = std::chrono::milliseconds(0);
    
    for (const auto& test : tests) {
        switch (test.state) {
            case TestState::Passed:
                passedCount++;
                break;
            case TestState::Failed:
            case TestState::Error:
                failedCount++;
                break;
            case TestState::Skipped:
            case TestState::Disabled:
                skippedCount++;
                break;
            default:
                break;
        }
        duration += test.duration;
    }
}

// ============================================================================
// TestRunResult
// ============================================================================

void TestRunResult::CalculateTotals() {
    totalTests = 0;
    passedTests = 0;
    failedTests = 0;
    skippedTests = 0;
    errorTests = 0;
    totalDuration = std::chrono::milliseconds(0);
    
    for (auto& suite : suites) {
        suite.UpdateCounts();
        totalTests += static_cast<int>(suite.tests.size());
        passedTests += suite.passedCount;
        failedTests += suite.failedCount;
        skippedTests += suite.skippedCount;
        totalDuration += suite.duration;
    }
    
    success = (failedTests == 0 && errorTests == 0);
}

// ============================================================================
// TestFilter
// ============================================================================

bool TestFilter::Matches(const TestCase& test) const {
    // Check pattern
    if (!pattern.empty()) {
        std::regex re(pattern, std::regex::icase);
        if (!std::regex_search(test.fullName, re)) {
            return false;
        }
    }
    
    // Check suites
    if (!suites.empty()) {
        bool found = false;
        for (const auto& suite : suites) {
            if (test.suiteName == suite) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Check specific tests
    if (!tests.empty()) {
        bool found = false;
        for (const auto& t : tests) {
            if (test.fullName == t || test.name == t) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    
    // Check tags (for Catch2)
    if (!tags.empty()) {
        bool hasTag = false;
        for (const auto& tag : tags) {
            if (std::find(test.tags.begin(), test.tags.end(), tag) != test.tags.end()) {
                hasTag = true;
                break;
            }
        }
        if (!hasTag) return false;
    }
    
    // Check exclude tags
    for (const auto& tag : excludeTags) {
        if (std::find(test.tags.begin(), test.tags.end(), tag) != test.tags.end()) {
            return false;
        }
    }
    
    // Check disabled
    if (!includeDisabled && test.state == TestState::Disabled) {
        return false;
    }
    
    return true;
}

std::string TestFilter::ToGoogleTestFilter() const {
    if (pattern.empty() && tests.empty() && suites.empty()) {
        return "*";
    }
    
    std::ostringstream filter;
    
    if (!tests.empty()) {
        for (size_t i = 0; i < tests.size(); i++) {
            if (i > 0) filter << ":";
            filter << tests[i];
        }
    } else if (!suites.empty()) {
        for (size_t i = 0; i < suites.size(); i++) {
            if (i > 0) filter << ":";
            filter << suites[i] << ".*";
        }
    } else if (!pattern.empty()) {
        filter << pattern;
    } else {
        filter << "*";
    }
    
    return filter.str();
}

std::string TestFilter::ToCatch2Filter() const {
    std::ostringstream filter;
    
    if (!tests.empty()) {
        for (const auto& test : tests) {
            filter << "\"" << test << "\",";
        }
    }
    
    if (!tags.empty()) {
        for (const auto& tag : tags) {
            filter << "[" << tag << "]";
        }
    }
    
    if (!excludeTags.empty()) {
        for (const auto& tag : excludeTags) {
            filter << "~[" << tag << "]";
        }
    }
    
    return filter.str();
}

// ============================================================================
// UCTestDebugger
// ============================================================================

UCTestDebugger::UCTestDebugger() = default;

UCTestDebugger::~UCTestDebugger() {
    if (isRunning_) {
        StopTestRun();
    }
}

void UCTestDebugger::SetDebugManager(UCDebugManager* manager) {
    debugManager_ = manager;
    
    if (debugManager_) {
        // Listen for debug output to parse test results
        debugManager_->onOutput = [this](const std::string& output) {
            ProcessTestOutput(output);
            if (onOutput) onOutput(output);
        };
    }
}

void UCTestDebugger::RegisterAdapter(std::shared_ptr<ITestAdapter> adapter) {
    if (adapter) {
        adapters_[adapter->GetFramework()] = adapter;
    }
}

void UCTestDebugger::UnregisterAdapter(TestFramework framework) {
    adapters_.erase(framework);
}

ITestAdapter* UCTestDebugger::GetAdapter(TestFramework framework) {
    auto it = adapters_.find(framework);
    return (it != adapters_.end()) ? it->second.get() : nullptr;
}

ITestAdapter* UCTestDebugger::GetAdapterForExecutable(const std::string& executable) {
    for (auto& pair : adapters_) {
        if (pair.second->CanHandle(executable)) {
            return pair.second.get();
        }
    }
    return nullptr;
}

std::vector<TestCase> UCTestDebugger::DiscoverTests(const std::string& executable) {
    std::vector<TestCase> allTests;
    
    ITestAdapter* adapter = GetAdapterForExecutable(executable);
    if (!adapter) {
        // Try to detect framework
        TestFramework framework = DetectFramework(executable);
        adapter = GetAdapter(framework);
    }
    
    if (adapter) {
        allTests = adapter->DiscoverTests(executable);
    }
    
    // Organize into suites
    suites_.clear();
    testMap_.clear();
    
    std::map<std::string, TestSuite*> suiteMap;
    
    for (auto& test : allTests) {
        std::string suiteName = test.suiteName.empty() ? "Default" : test.suiteName;
        
        if (suiteMap.find(suiteName) == suiteMap.end()) {
            suites_.push_back(TestSuite(suiteName));
            suiteMap[suiteName] = &suites_.back();
        }
        
        suiteMap[suiteName]->tests.push_back(test);
        testMap_[test.fullName] = &suiteMap[suiteName]->tests.back();
    }
    
    return allTests;
}

TestFramework UCTestDebugger::DetectFramework(const std::string& executable) {
    // Try each adapter
    for (auto& pair : adapters_) {
        if (pair.second->CanHandle(executable)) {
            return pair.first;
        }
    }
    
    return TestFramework::Unknown;
}

const TestCase* UCTestDebugger::FindTest(const std::string& fullName) const {
    auto it = testMap_.find(fullName);
    return (it != testMap_.end()) ? it->second : nullptr;
}

void UCTestDebugger::ClearTests() {
    suites_.clear();
    testMap_.clear();
}

bool UCTestDebugger::RunAllTests(const TestDebugConfig& config) {
    if (isRunning_) return false;
    
    TestDebugConfig cfg = config;
    cfg.filter = TestFilter();  // No filter - run all
    
    return RunTests(cfg, {});
}

bool UCTestDebugger::RunTests(const TestDebugConfig& config, 
                               const std::vector<std::string>& testNames) {
    if (isRunning_ || !debugManager_) return false;
    
    isRunning_ = true;
    currentConfig_ = config;
    
    // Initialize result
    lastResult_ = TestRunResult();
    lastResult_.runId = GenerateRunId();
    lastResult_.startTime = std::chrono::system_clock::now();
    
    if (onRunStarted) {
        onRunStarted(lastResult_);
    }
    
    // Get adapter
    ITestAdapter* adapter = GetAdapter(config.framework);
    if (!adapter) {
        adapter = GetAdapterForExecutable(config.executablePath);
    }
    
    if (!adapter) {
        if (onError) onError("No suitable test adapter found");
        isRunning_ = false;
        return false;
    }
    
    // Build command line
    TestDebugConfig runConfig = config;
    if (!testNames.empty()) {
        runConfig.filter.tests = testNames;
    }
    
    std::vector<std::string> args = adapter->BuildCommandLine(runConfig);
    
    // Launch without debugging (just run)
    DebugLaunchConfig launchConfig;
    launchConfig.program = config.executablePath;
    launchConfig.arguments = args;
    launchConfig.workingDirectory = config.workingDirectory;
    launchConfig.environment = config.environment;
    
    // For simple run, we could use a process runner instead of debugger
    // But for now, launch with debugger but don't stop
    
    bool success = debugManager_->Launch(launchConfig);
    
    if (!success) {
        if (onError) onError("Failed to launch test executable");
        isRunning_ = false;
        return false;
    }
    
    // Continue immediately (don't stop at entry)
    debugManager_->Continue();
    
    return true;
}

bool UCTestDebugger::RunTest(const TestDebugConfig& config, const std::string& testName) {
    return RunTests(config, {testName});
}

bool UCTestDebugger::RunFailedTests(const TestDebugConfig& config) {
    std::vector<std::string> failedTests;
    
    for (const auto& suite : lastResult_.suites) {
        for (const auto& test : suite.tests) {
            if (test.state == TestState::Failed || test.state == TestState::Error) {
                failedTests.push_back(test.fullName);
            }
        }
    }
    
    if (failedTests.empty()) {
        return true;  // Nothing to run
    }
    
    return RunTests(config, failedTests);
}

bool UCTestDebugger::DebugAllTests(const TestDebugConfig& config) {
    if (isRunning_ || !debugManager_) return false;
    
    isRunning_ = true;
    currentConfig_ = config;
    
    // Initialize result
    lastResult_ = TestRunResult();
    lastResult_.runId = GenerateRunId();
    lastResult_.startTime = std::chrono::system_clock::now();
    
    if (onRunStarted) {
        onRunStarted(lastResult_);
    }
    
    ITestAdapter* adapter = GetAdapter(config.framework);
    if (!adapter) {
        adapter = GetAdapterForExecutable(config.executablePath);
    }
    
    if (!adapter) {
        if (onError) onError("No suitable test adapter found");
        isRunning_ = false;
        return false;
    }
    
    std::vector<std::string> args = adapter->BuildCommandLine(config);
    
    // Set up debug launch
    DebugLaunchConfig launchConfig;
    launchConfig.program = config.executablePath;
    launchConfig.arguments = args;
    launchConfig.workingDirectory = config.workingDirectory;
    launchConfig.environment = config.environment;
    launchConfig.stopAtEntry = false;
    
    // Set breakpoints for failure assertions if configured
    if (config.breakOnFailure) {
        SetFailureBreakpoint(true);
    }
    
    // Set breakpoints on specific tests
    for (const auto& testBp : testBreakpoints_) {
        const TestCase* test = FindTest(testBp);
        if (test && !test->sourceFile.empty() && test->sourceLine > 0) {
            debugManager_->SetBreakpoint(test->sourceFile, test->sourceLine);
        }
    }
    
    bool success = debugManager_->Launch(launchConfig);
    
    if (!success) {
        if (onError) onError("Failed to launch test executable for debugging");
        isRunning_ = false;
        return false;
    }
    
    return true;
}

bool UCTestDebugger::DebugTest(const TestDebugConfig& config, const std::string& testName) {
    TestDebugConfig cfg = config;
    cfg.filter.tests = {testName};
    
    // Set breakpoint on this specific test
    SetTestBreakpoint(testName);
    
    return DebugAllTests(cfg);
}

void UCTestDebugger::StopTestRun() {
    if (!isRunning_) return;
    
    if (debugManager_) {
        debugManager_->Terminate();
    }
    
    isRunning_ = false;
    
    // Finalize result
    lastResult_.endTime = std::chrono::system_clock::now();
    lastResult_.CalculateTotals();
    
    if (onRunFinished) {
        onRunFinished(lastResult_);
    }
    
    // Add to history
    runHistory_.push_back(lastResult_);
    while (runHistory_.size() > maxHistorySize_) {
        runHistory_.erase(runHistory_.begin());
    }
}

void UCTestDebugger::ClearHistory() {
    runHistory_.clear();
}

bool UCTestDebugger::ExportResults(const std::string& filePath, const std::string& format) {
    std::ofstream file(filePath);
    if (!file.is_open()) return false;
    
    if (format == "xml" || format == "junit") {
        // JUnit XML format
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<testsuites tests=\"" << lastResult_.totalTests 
             << "\" failures=\"" << lastResult_.failedTests
             << "\" errors=\"" << lastResult_.errorTests
             << "\" time=\"" << lastResult_.totalDuration.count() / 1000.0 << "\">\n";
        
        for (const auto& suite : lastResult_.suites) {
            file << "  <testsuite name=\"" << suite.name 
                 << "\" tests=\"" << suite.tests.size()
                 << "\" failures=\"" << suite.failedCount
                 << "\" time=\"" << suite.duration.count() / 1000.0 << "\">\n";
            
            for (const auto& test : suite.tests) {
                file << "    <testcase name=\"" << test.name 
                     << "\" classname=\"" << test.suiteName
                     << "\" time=\"" << test.duration.count() / 1000.0 << "\"";
                
                if (test.state == TestState::Failed) {
                    file << ">\n      <failure message=\"" << test.errorMessage << "\">"
                         << test.stackTrace << "</failure>\n    </testcase>\n";
                } else if (test.state == TestState::Skipped) {
                    file << ">\n      <skipped/>\n    </testcase>\n";
                } else {
                    file << "/>\n";
                }
            }
            
            file << "  </testsuite>\n";
        }
        
        file << "</testsuites>\n";
    } else if (format == "json") {
        // JSON format
        file << "{\n";
        file << "  \"totalTests\": " << lastResult_.totalTests << ",\n";
        file << "  \"passed\": " << lastResult_.passedTests << ",\n";
        file << "  \"failed\": " << lastResult_.failedTests << ",\n";
        file << "  \"duration\": " << lastResult_.totalDuration.count() << ",\n";
        file << "  \"suites\": [\n";
        
        for (size_t i = 0; i < lastResult_.suites.size(); i++) {
            const auto& suite = lastResult_.suites[i];
            if (i > 0) file << ",\n";
            file << "    {\"name\": \"" << suite.name << "\", \"tests\": " << suite.tests.size() << "}";
        }
        
        file << "\n  ]\n}\n";
    }
    
    return true;
}

void UCTestDebugger::SetTestBreakpoint(const std::string& testName) {
    testBreakpoints_.insert(testName);
    
    // If debugging, set actual breakpoint
    if (debugManager_ && debugManager_->IsDebugging()) {
        const TestCase* test = FindTest(testName);
        if (test && !test->sourceFile.empty() && test->sourceLine > 0) {
            debugManager_->SetBreakpoint(test->sourceFile, test->sourceLine);
        }
    }
}

void UCTestDebugger::RemoveTestBreakpoint(const std::string& testName) {
    testBreakpoints_.erase(testName);
}

void UCTestDebugger::SetFailureBreakpoint(bool enable) {
    breakOnFailure_ = enable;
    
    if (debugManager_ && enable) {
        // Set breakpoint on common assertion failure functions
        // Google Test
        debugManager_->SetFunctionBreakpoint("testing::internal::AssertHelper::operator=");
        debugManager_->SetFunctionBreakpoint("testing::internal::GTestLog");
        
        // Catch2
        debugManager_->SetFunctionBreakpoint("Catch::AssertionHandler::handleExpr");
    }
}

void UCTestDebugger::ProcessTestOutput(const std::string& output) {
    if (!isRunning_) return;
    
    ITestAdapter* adapter = GetAdapter(currentConfig_.framework);
    if (adapter) {
        adapter->ParseOutput(output, lastResult_);
    }
    
    lastResult_.outputLog += output;
}

void UCTestDebugger::UpdateTestState(const std::string& testName, TestState state,
                                      const std::string& message) {
    auto it = testMap_.find(testName);
    if (it != testMap_.end()) {
        it->second->state = state;
        if (!message.empty()) {
            it->second->errorMessage = message;
        }
        
        if (onTestFinished) {
            onTestFinished(*it->second);
        }
    }
}

std::string UCTestDebugger::GenerateRunId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::ostringstream ss;
    ss << "run_" << timestamp;
    return ss.str();
}

// ============================================================================
// Helper Functions
// ============================================================================

std::string TestFrameworkToString(TestFramework framework) {
    switch (framework) {
        case TestFramework::GoogleTest: return "googletest";
        case TestFramework::Catch2: return "catch2";
        case TestFramework::Boost: return "boost";
        case TestFramework::CppUnit: return "cppunit";
        case TestFramework::DocTest: return "doctest";
        case TestFramework::Custom: return "custom";
        default: return "unknown";
    }
}

TestFramework StringToTestFramework(const std::string& str) {
    if (str == "googletest" || str == "gtest") return TestFramework::GoogleTest;
    if (str == "catch2" || str == "catch") return TestFramework::Catch2;
    if (str == "boost") return TestFramework::Boost;
    if (str == "cppunit") return TestFramework::CppUnit;
    if (str == "doctest") return TestFramework::DocTest;
    if (str == "custom") return TestFramework::Custom;
    return TestFramework::Unknown;
}

std::string TestStateToString(TestState state) {
    switch (state) {
        case TestState::NotRun: return "not_run";
        case TestState::Running: return "running";
        case TestState::Passed: return "passed";
        case TestState::Failed: return "failed";
        case TestState::Skipped: return "skipped";
        case TestState::Disabled: return "disabled";
        case TestState::Error: return "error";
        case TestState::Timeout: return "timeout";
    }
    return "unknown";
}

TestState StringToTestState(const std::string& str) {
    if (str == "running") return TestState::Running;
    if (str == "passed") return TestState::Passed;
    if (str == "failed") return TestState::Failed;
    if (str == "skipped") return TestState::Skipped;
    if (str == "disabled") return TestState::Disabled;
    if (str == "error") return TestState::Error;
    if (str == "timeout") return TestState::Timeout;
    return TestState::NotRun;
}

} // namespace IDE
} // namespace UltraCanvas
