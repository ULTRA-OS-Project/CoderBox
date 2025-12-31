// Apps/IDE/Debug/Testing/UCCatch2Adapter.h
// Catch2 Adapter for ULTRA IDE
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
 * @brief Catch2 framework adapter
 * 
 * Implements test discovery, execution, and result parsing for
 * Catch2 test framework (v2 and v3).
 */
class UCCatch2Adapter : public ITestAdapter {
public:
    UCCatch2Adapter();
    ~UCCatch2Adapter() override = default;
    
    // =========================================================================
    // ITestAdapter Implementation
    // =========================================================================
    
    TestFramework GetFramework() const override { return TestFramework::Catch2; }
    std::string GetName() const override { return "Catch2"; }
    
    bool CanHandle(const std::string& executable) const override;
    std::vector<TestCase> DiscoverTests(const std::string& executable) override;
    std::vector<std::string> BuildCommandLine(const TestDebugConfig& config) override;
    void ParseOutput(const std::string& output, TestRunResult& result) override;
    TestState ParseTestResult(const std::string& testOutput) override;
    
    // =========================================================================
    // CATCH2 SPECIFIC
    // =========================================================================
    
    /**
     * @brief Parse --list-tests output
     */
    std::vector<TestCase> ParseTestList(const std::string& output);
    
    /**
     * @brief Parse --list-tags output
     */
    std::vector<std::string> ParseTagList(const std::string& output);
    
    /**
     * @brief Parse XML reporter output
     */
    TestRunResult ParseXMLOutput(const std::string& xmlContent);
    
    /**
     * @brief Parse JUnit reporter output
     */
    TestRunResult ParseJUnitOutput(const std::string& xmlPath);
    
    /**
     * @brief Check if output indicates Catch2
     */
    static bool IsCatch2Output(const std::string& output);
    
    /**
     * @brief Get Catch2 version from executable
     */
    int GetCatch2Version(const std::string& executable);
    
    /**
     * @brief Extract test location from source
     */
    bool ExtractTestLocation(const std::string& sourceFile, const std::string& testName,
                             std::string& file, int& line);
    
private:
    void ParseConsoleOutput(const std::string& output, TestRunResult& result);
    void ParseSection(const std::string& sectionOutput, TestCase& test);
    std::vector<std::string> ExtractTags(const std::string& tagString);
    
    std::string currentTestName_;
    std::string currentSectionName_;
    bool inTestCase_ = false;
    bool inSection_ = false;
    
    // Regex patterns
    std::regex testCaseStartPattern_;
    std::regex testCaseEndPattern_;
    std::regex sectionStartPattern_;
    std::regex passedPattern_;
    std::regex failedPattern_;
    std::regex assertionPattern_;
    std::regex tagPattern_;
    std::regex listTestPattern_;
    
    int catch2Version_ = 3;  // Assume v3 by default
};

} // namespace IDE
} // namespace UltraCanvas
