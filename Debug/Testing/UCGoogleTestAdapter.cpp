// Apps/IDE/Debug/Testing/UCGoogleTestAdapter.cpp
// Google Test Adapter Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCGoogleTestAdapter.h"
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace UltraCanvas {
namespace IDE {

UCGoogleTestAdapter::UCGoogleTestAdapter() {
    // Initialize regex patterns for Google Test output
    runningPattern_ = std::regex(R"(\[ RUN      \] (.+)\.(.+))");
    passedPattern_ = std::regex(R"(\[       OK \] (.+)\.(.+) \((\d+) ms\))");
    failedPattern_ = std::regex(R"(\[  FAILED  \] (.+)\.(.+) \((\d+) ms\))");
    suiteStartPattern_ = std::regex(R"(\[----------\] \d+ tests? from (.+))");
    suiteEndPattern_ = std::regex(R"(\[----------\] \d+ tests? from (.+) \((\d+) ms total\))");
    summaryPattern_ = std::regex(R"(\[==========\] (\d+) tests? from (\d+) test suites? ran\.)");
    failureLocationPattern_ = std::regex(R"((.+):(\d+): Failure)");
}

bool UCGoogleTestAdapter::CanHandle(const std::string& executable) const {
    // Run executable with --gtest_list_tests to check if it's a Google Test executable
    std::string command = "\"" + executable + "\" --gtest_list_tests 2>&1";
    
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    
    if (!pipe) return false;
    
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
        // Check early to avoid reading too much
        if (output.length() > 1000) break;
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    // Google Test list output starts with test suite names followed by dots
    // or contains "Running main()"
    return IsGoogleTestOutput(output);
}

bool UCGoogleTestAdapter::IsGoogleTestOutput(const std::string& output) {
    // Check for Google Test markers
    if (output.find("Running main()") != std::string::npos) return true;
    if (output.find("[==========]") != std::string::npos) return true;
    if (output.find("[----------]") != std::string::npos) return true;
    if (output.find("[ RUN      ]") != std::string::npos) return true;
    
    // Check for test list format (SuiteName.\n  TestName\n)
    std::regex listPattern(R"(\w+\.\s*\n\s+\w+)");
    return std::regex_search(output, listPattern);
}

std::vector<TestCase> UCGoogleTestAdapter::DiscoverTests(const std::string& executable) {
    std::vector<TestCase> tests;
    
    // Run with --gtest_list_tests
    std::string command = "\"" + executable + "\" --gtest_list_tests 2>&1";
    
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    
    if (!pipe) return tests;
    
    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    return ParseTestList(output);
}

std::vector<TestCase> UCGoogleTestAdapter::ParseTestList(const std::string& output) {
    std::vector<TestCase> tests;
    std::istringstream stream(output);
    std::string line;
    std::string currentSuite;
    
    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Check if it's a suite name (ends with '.')
        if (!line.empty() && line[0] != ' ' && line.back() == '.') {
            currentSuite = line.substr(0, line.length() - 1);
            continue;
        }
        
        // Check if it's a test name (starts with spaces)
        if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
            // Trim leading whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start != std::string::npos) {
                std::string testName = line.substr(start);
                
                // Remove parameter info if present (e.g., "  # GetParam() = ...")
                size_t commentPos = testName.find("  #");
                std::string paramInfo;
                if (commentPos != std::string::npos) {
                    paramInfo = testName.substr(commentPos + 3);
                    testName = testName.substr(0, commentPos);
                }
                
                // Trim any remaining whitespace
                while (!testName.empty() && std::isspace(testName.back())) {
                    testName.pop_back();
                }
                
                if (!testName.empty()) {
                    TestCase test;
                    test.name = testName;
                    test.suiteName = currentSuite;
                    test.fullName = currentSuite + "." + testName;
                    test.state = TestState::NotRun;
                    
                    // Check for disabled test
                    if (testName.substr(0, 9) == "DISABLED_" || 
                        currentSuite.substr(0, 9) == "DISABLED_") {
                        test.state = TestState::Disabled;
                    }
                    
                    // Check for parameterized test
                    if (testName.find('/') != std::string::npos) {
                        test.isParameterized = true;
                        test.parameterInfo = paramInfo;
                    }
                    
                    tests.push_back(test);
                }
            }
        }
    }
    
    return tests;
}

std::vector<std::string> UCGoogleTestAdapter::BuildCommandLine(const TestDebugConfig& config) {
    std::vector<std::string> args;
    
    // Test filter
    std::string filter = config.filter.ToGoogleTestFilter();
    if (!filter.empty() && filter != "*") {
        args.push_back("--gtest_filter=" + filter);
    }
    
    // Output format
    args.push_back("--gtest_output=xml:" + config.workingDirectory + "/test_results.xml");
    
    // Color output (disable for parsing)
    args.push_back("--gtest_color=no");
    
    // Print time
    args.push_back("--gtest_print_time=1");
    
    // Repeat count
    if (config.repeatCount > 1) {
        args.push_back("--gtest_repeat=" + std::to_string(config.repeatCount));
    }
    
    // Shuffle
    if (config.shuffleTests) {
        args.push_back("--gtest_shuffle");
        if (config.randomSeed > 0) {
            args.push_back("--gtest_random_seed=" + std::to_string(config.randomSeed));
        }
    }
    
    // Break on failure (for debugging)
    if (config.breakOnFailure) {
        args.push_back("--gtest_break_on_failure");
    }
    
    // Include disabled tests
    if (config.filter.includeDisabled) {
        args.push_back("--gtest_also_run_disabled_tests");
    }
    
    // Stop on first failure
    if (config.stopOnFirstFailure) {
        args.push_back("--gtest_fail_fast");
    }
    
    // Add any custom arguments
    for (const auto& arg : config.arguments) {
        args.push_back(arg);
    }
    
    return args;
}

std::vector<std::string> UCGoogleTestAdapter::GetGTestArguments(const TestDebugConfig& config) {
    return BuildCommandLine(config);
}

void UCGoogleTestAdapter::ParseOutput(const std::string& output, TestRunResult& result) {
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        ParseRunningLine(line, result);
        ParseResultLine(line, result);
        ParseFailureLine(line, result);
    }
}

void UCGoogleTestAdapter::ParseRunningLine(const std::string& line, TestRunResult& result) {
    std::smatch match;
    
    // Check for suite start
    if (std::regex_search(line, match, suiteStartPattern_)) {
        currentSuiteName_ = match[1].str();
        
        // Find or create suite
        bool found = false;
        for (auto& suite : result.suites) {
            if (suite.name == currentSuiteName_) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.suites.push_back(TestSuite(currentSuiteName_));
        }
        return;
    }
    
    // Check for test start
    if (std::regex_search(line, match, runningPattern_)) {
        currentSuiteName_ = match[1].str();
        currentTestName_ = match[2].str();
        currentFailureMessage_.clear();
        currentStackTrace_.clear();
        
        // Update test state
        for (auto& suite : result.suites) {
            if (suite.name == currentSuiteName_) {
                for (auto& test : suite.tests) {
                    if (test.name == currentTestName_) {
                        test.state = TestState::Running;
                        break;
                    }
                }
                break;
            }
        }
    }
}

void UCGoogleTestAdapter::ParseResultLine(const std::string& line, TestRunResult& result) {
    std::smatch match;
    
    // Check for passed test
    if (std::regex_search(line, match, passedPattern_)) {
        std::string suiteName = match[1].str();
        std::string testName = match[2].str();
        int duration = std::stoi(match[3].str());
        
        for (auto& suite : result.suites) {
            if (suite.name == suiteName) {
                for (auto& test : suite.tests) {
                    if (test.name == testName) {
                        test.state = TestState::Passed;
                        test.duration = std::chrono::milliseconds(duration);
                        break;
                    }
                }
                break;
            }
        }
        return;
    }
    
    // Check for failed test
    if (std::regex_search(line, match, failedPattern_)) {
        std::string suiteName = match[1].str();
        std::string testName = match[2].str();
        int duration = std::stoi(match[3].str());
        
        for (auto& suite : result.suites) {
            if (suite.name == suiteName) {
                for (auto& test : suite.tests) {
                    if (test.name == testName) {
                        test.state = TestState::Failed;
                        test.duration = std::chrono::milliseconds(duration);
                        test.errorMessage = currentFailureMessage_;
                        
                        std::ostringstream stack;
                        for (const auto& frame : currentStackTrace_) {
                            stack << frame << "\n";
                        }
                        test.stackTrace = stack.str();
                        break;
                    }
                }
                break;
            }
        }
    }
}

void UCGoogleTestAdapter::ParseFailureLine(const std::string& line, TestRunResult& result) {
    std::smatch match;
    
    // Check for failure location
    if (std::regex_search(line, match, failureLocationPattern_)) {
        std::string file = match[1].str();
        int lineNum = std::stoi(match[2].str());
        currentStackTrace_.push_back(file + ":" + std::to_string(lineNum));
    }
    
    // Collect failure message (lines after location)
    if (!currentStackTrace_.empty() && line.find("Failure") == std::string::npos) {
        if (!line.empty() && line[0] != '[') {
            if (!currentFailureMessage_.empty()) {
                currentFailureMessage_ += "\n";
            }
            currentFailureMessage_ += line;
        }
    }
}

TestState UCGoogleTestAdapter::ParseTestResult(const std::string& testOutput) {
    if (testOutput.find("[       OK ]") != std::string::npos) {
        return TestState::Passed;
    }
    if (testOutput.find("[  FAILED  ]") != std::string::npos) {
        return TestState::Failed;
    }
    if (testOutput.find("[  SKIPPED ]") != std::string::npos) {
        return TestState::Skipped;
    }
    return TestState::NotRun;
}

TestRunResult UCGoogleTestAdapter::ParseXMLOutput(const std::string& xmlPath) {
    TestRunResult result;
    
    std::ifstream file(xmlPath);
    if (!file.is_open()) {
        return result;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // Simple XML parsing for test results
    // In production, use a proper XML parser
    
    // Extract testsuite elements
    std::regex suitePattern(R"(<testsuite name="([^"]+)" tests="(\d+)" failures="(\d+)" disabled="(\d+)" errors="(\d+)" time="([^"]+)")");
    std::regex testPattern(R"(<testcase name="([^"]+)" status="([^"]+)" time="([^"]+)" classname="([^"]+)")");
    std::regex failurePattern(R"(<failure[^>]*>([\s\S]*?)</failure>)");
    
    std::smatch match;
    std::string::const_iterator searchStart = content.cbegin();
    
    while (std::regex_search(searchStart, content.cend(), match, suitePattern)) {
        TestSuite suite;
        suite.name = match[1].str();
        
        // Parse tests within this suite
        // ... (simplified for brevity)
        
        result.suites.push_back(suite);
        searchStart = match.suffix().first;
    }
    
    result.CalculateTotals();
    return result;
}

bool UCGoogleTestAdapter::ExtractTestLocation(const std::string& sourceFile, 
                                               const std::string& testName,
                                               std::string& file, int& line) {
    std::ifstream source(sourceFile);
    if (!source.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(source)),
                         std::istreambuf_iterator<char>());
    
    // Look for TEST, TEST_F, TEST_P macros
    std::vector<std::string> patterns = {
        "TEST\\s*\\(\\s*\\w+\\s*,\\s*" + testName + "\\s*\\)",
        "TEST_F\\s*\\(\\s*\\w+\\s*,\\s*" + testName + "\\s*\\)",
        "TEST_P\\s*\\(\\s*\\w+\\s*,\\s*" + testName + "\\s*\\)"
    };
    
    for (const auto& pattern : patterns) {
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(content, match, re)) {
            // Count lines to find line number
            size_t pos = match.position();
            int lineNum = 1;
            for (size_t i = 0; i < pos; i++) {
                if (content[i] == '\n') lineNum++;
            }
            
            file = sourceFile;
            line = lineNum;
            return true;
        }
    }
    
    return false;
}

} // namespace IDE
} // namespace UltraCanvas
