// Apps/IDE/Debug/Testing/UCGoogleTestAdapter.h
// Google Test Adapter for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "UCTestDebugger.h"
#include <string>
#include <vector>
#include <regex>

namespace UltraCanvas {
namespace IDE {

/**
 * @brief Google Test framework adapter
 * 
 * Implements test discovery, execution, and result parsing for
 * Google Test (gtest) framework.
 */
class UCGoogleTestAdapter : public ITestAdapter {
public:
    UCGoogleTestAdapter();
    ~UCGoogleTestAdapter() override = default;
    
    // =========================================================================
    // ITestAdapter Implementation
    // =========================================================================
    
    TestFramework GetFramework() const override { return TestFramework::GoogleTest; }
    std::string GetName() const override { return "Google Test"; }
    
    bool CanHandle(const std::string& executable) const override;
    std::vector<TestCase> DiscoverTests(const std::string& executable) override;
    std::vector<std::string> BuildCommandLine(const TestDebugConfig& config) override;
    void ParseOutput(const std::string& output, TestRunResult& result) override;
    TestState ParseTestResult(const std::string& testOutput) override;
    
    // =========================================================================
    // GOOGLE TEST SPECIFIC
    // =========================================================================
    
    /**
     * @brief Parse --gtest_list_tests output
     */
    std::vector<TestCase> ParseTestList(const std::string& output);
    
    /**
     * @brief Parse XML output file
     */
    TestRunResult ParseXMLOutput(const std::string& xmlPath);
    
    /**
     * @brief Get Google Test specific arguments
     */
    std::vector<std::string> GetGTestArguments(const TestDebugConfig& config);
    
    /**
     * @brief Check if output indicates Google Test
     */
    static bool IsGoogleTestOutput(const std::string& output);
    
    /**
     * @brief Extract test location from source
     */
    bool ExtractTestLocation(const std::string& sourceFile, const std::string& testName,
                             std::string& file, int& line);
    
private:
    void ParseRunningLine(const std::string& line, TestRunResult& result);
    void ParseResultLine(const std::string& line, TestRunResult& result);
    void ParseFailureLine(const std::string& line, TestRunResult& result);
    
    std::string currentTestName_;
    std::string currentSuiteName_;
    std::string currentFailureMessage_;
    std::vector<std::string> currentStackTrace_;
    
    // Regex patterns for output parsing
    std::regex runningPattern_;
    std::regex passedPattern_;
    std::regex failedPattern_;
    std::regex suiteStartPattern_;
    std::regex suiteEndPattern_;
    std::regex summaryPattern_;
    std::regex failureLocationPattern_;
};

} // namespace IDE
} // namespace UltraCanvas
