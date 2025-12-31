// Apps/IDE/Debug/Testing/UCCatch2Adapter.cpp
// Catch2 Adapter Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-29
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCCatch2Adapter.h"
#include <sstream>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace UltraCanvas {
namespace IDE {

UCCatch2Adapter::UCCatch2Adapter() {
    // Initialize regex patterns for Catch2 output
    // Catch2 v3 patterns
    testCaseStartPattern_ = std::regex(R"(^([^\n]+)\n-{50,})");
    testCaseEndPattern_ = std::regex(R"(^-{50,}\n)");
    sectionStartPattern_ = std::regex(R"(^\s+Section: (.+))");
    passedPattern_ = std::regex(R"(All tests passed \((\d+) assertions? in (\d+) test cases?\))");
    failedPattern_ = std::regex(R"(test cases?: (\d+) \| (\d+) passed \| (\d+) failed)");
    assertionPattern_ = std::regex(R"((.+):(\d+): FAILED:)");
    tagPattern_ = std::regex(R"(\[([^\]]+)\])");
    listTestPattern_ = std::regex(R"(^\s*(.+?)(?:\s+(\[[^\]]+\](?:\s*\[[^\]]+\])*))?$)");
}

bool UCCatch2Adapter::CanHandle(const std::string& executable) const {
    // Run executable with --list-tests to check if it's a Catch2 executable
    std::string command = "\"" + executable + "\" --list-tests 2>&1";
    
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
        if (output.length() > 1000) break;
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    return IsCatch2Output(output);
}

bool UCCatch2Adapter::IsCatch2Output(const std::string& output) {
    // Check for Catch2 markers
    if (output.find("Catch v") != std::string::npos) return true;
    if (output.find("CATCH_VERSION") != std::string::npos) return true;
    if (output.find("All available test cases:") != std::string::npos) return true;
    if (output.find("test cases:") != std::string::npos && 
        output.find("assertions:") != std::string::npos) return true;
    
    // Check for Catch2 style test listing (indented with tags)
    std::regex catch2ListPattern(R"(\s+\w+.*\[[\w\-]+\])");
    return std::regex_search(output, catch2ListPattern);
}

int UCCatch2Adapter::GetCatch2Version(const std::string& executable) {
    std::string command = "\"" + executable + "\" --version 2>&1";
    
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    
    if (!pipe) return 3;  // Assume v3
    
    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    
    // Parse version from "Catch v2.x.x" or "Catch2 v3.x.x"
    std::regex versionPattern(R"(Catch2?\s+v(\d+))");
    std::smatch match;
    if (std::regex_search(output, match, versionPattern)) {
        return std::stoi(match[1].str());
    }
    
    return 3;
}

std::vector<TestCase> UCCatch2Adapter::DiscoverTests(const std::string& executable) {
    std::vector<TestCase> tests;
    
    catch2Version_ = GetCatch2Version(executable);
    
    // Use appropriate list command based on version
    std::string listArg = (catch2Version_ >= 3) ? "--list-tests" : "-l";
    std::string command = "\"" + executable + "\" " + listArg + " 2>&1";
    
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

std::vector<TestCase> UCCatch2Adapter::ParseTestList(const std::string& output) {
    std::vector<TestCase> tests;
    std::istringstream stream(output);
    std::string line;
    
    // Skip header lines
    bool headerDone = false;
    
    while (std::getline(stream, line)) {
        // Skip empty lines and header
        if (line.empty()) continue;
        if (line.find("All available") != std::string::npos) {
            headerDone = true;
            continue;
        }
        if (!headerDone && line.find("test case") == std::string::npos) continue;
        headerDone = true;
        
        // Skip summary lines
        if (line.find("test cases") != std::string::npos && 
            line.find("Matching") != std::string::npos) continue;
        
        // Parse test line
        // Format: "  Test Name [tag1][tag2]" or "Test Name"
        
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        
        std::string testLine = line.substr(start);
        
        // Extract tags
        std::vector<std::string> tags = ExtractTags(testLine);
        
        // Remove tags from name
        std::string testName = testLine;
        size_t tagStart = testLine.find('[');
        if (tagStart != std::string::npos) {
            testName = testLine.substr(0, tagStart);
            // Trim trailing whitespace
            while (!testName.empty() && std::isspace(testName.back())) {
                testName.pop_back();
            }
        }
        
        if (!testName.empty()) {
            TestCase test;
            test.name = testName;
            test.fullName = testName;
            test.tags = tags;
            test.state = TestState::NotRun;
            
            // Determine suite from tags or use "Default"
            for (const auto& tag : tags) {
                if (tag != "." && tag.find("!") != 0) {
                    test.suiteName = tag;
                    break;
                }
            }
            if (test.suiteName.empty()) {
                test.suiteName = "Default";
            }
            
            // Check for hidden/skip tags
            for (const auto& tag : tags) {
                if (tag == "." || tag == "!hide" || tag == "!skip") {
                    test.state = TestState::Skipped;
                    break;
                }
                if (tag == "!mayfail") {
                    // May fail tag - still run but don't count failure
                }
            }
            
            tests.push_back(test);
        }
    }
    
    return tests;
}

std::vector<std::string> UCCatch2Adapter::ExtractTags(const std::string& testLine) {
    std::vector<std::string> tags;
    
    std::sregex_iterator it(testLine.begin(), testLine.end(), tagPattern_);
    std::sregex_iterator end;
    
    while (it != end) {
        tags.push_back((*it)[1].str());
        ++it;
    }
    
    return tags;
}

std::vector<std::string> UCCatch2Adapter::ParseTagList(const std::string& output) {
    std::vector<std::string> tags;
    std::istringstream stream(output);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Format: "[tag] - count tests"
        std::smatch match;
        if (std::regex_search(line, match, tagPattern_)) {
            tags.push_back(match[1].str());
        }
    }
    
    return tags;
}

std::vector<std::string> UCCatch2Adapter::BuildCommandLine(const TestDebugConfig& config) {
    std::vector<std::string> args;
    
    // Test filter
    std::string filter = config.filter.ToCatch2Filter();
    if (!filter.empty()) {
        args.push_back(filter);
    }
    
    // Reporter
    if (catch2Version_ >= 3) {
        args.push_back("--reporter");
        args.push_back("xml");
        args.push_back("--out");
        args.push_back(config.workingDirectory + "/test_results.xml");
    } else {
        args.push_back("-r");
        args.push_back("xml");
        args.push_back("-o");
        args.push_back(config.workingDirectory + "/test_results.xml");
    }
    
    // Also output to console for real-time updates
    if (catch2Version_ >= 3) {
        args.push_back("--reporter");
        args.push_back("console::out=-");
    }
    
    // Verbosity
    args.push_back("-s");  // Show successful tests too
    
    // Duration
    args.push_back("-d");
    args.push_back("yes");
    
    // Abort on failure
    if (config.stopOnFirstFailure) {
        args.push_back("-a");
    }
    
    // Break into debugger on failure
    if (config.breakOnFailure) {
        args.push_back("-b");
    }
    
    // Seed for randomization
    if (config.shuffleTests) {
        args.push_back("--order");
        args.push_back("rand");
        if (config.randomSeed > 0) {
            args.push_back("--rng-seed");
            args.push_back(std::to_string(config.randomSeed));
        }
    }
    
    // Add any custom arguments
    for (const auto& arg : config.arguments) {
        args.push_back(arg);
    }
    
    return args;
}

void UCCatch2Adapter::ParseOutput(const std::string& output, TestRunResult& result) {
    ParseConsoleOutput(output, result);
}

void UCCatch2Adapter::ParseConsoleOutput(const std::string& output, TestRunResult& result) {
    std::istringstream stream(output);
    std::string line;
    
    TestSuite* currentSuite = nullptr;
    TestCase* currentTest = nullptr;
    std::string currentFailure;
    
    while (std::getline(stream, line)) {
        // Check for test case start (line of dashes followed by test name)
        if (line.find("---") == 0 && line.length() > 50) {
            // Next line should be test name
            std::string nextLine;
            if (std::getline(stream, nextLine)) {
                // Find or create test
                std::string testName = nextLine;
                // Trim
                size_t start = testName.find_first_not_of(" \t");
                size_t end = testName.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    testName = testName.substr(start, end - start + 1);
                }
                
                // Find in suites
                for (auto& suite : result.suites) {
                    for (auto& test : suite.tests) {
                        if (test.name == testName || test.fullName == testName) {
                            currentTest = &test;
                            currentTest->state = TestState::Running;
                            currentSuite = &suite;
                            break;
                        }
                    }
                    if (currentTest) break;
                }
            }
            continue;
        }
        
        // Check for assertion failure
        std::smatch match;
        if (std::regex_search(line, match, assertionPattern_)) {
            if (currentTest) {
                currentTest->sourceFile = match[1].str();
                currentTest->sourceLine = std::stoi(match[2].str());
                currentFailure = line;
            }
            continue;
        }
        
        // Check for passed summary
        if (std::regex_search(line, match, passedPattern_)) {
            int assertions = std::stoi(match[1].str());
            int testCases = std::stoi(match[2].str());
            result.passedTests = testCases;
            continue;
        }
        
        // Check for failed summary
        if (std::regex_search(line, match, failedPattern_)) {
            int total = std::stoi(match[1].str());
            int passed = std::stoi(match[2].str());
            int failed = std::stoi(match[3].str());
            result.totalTests = total;
            result.passedTests = passed;
            result.failedTests = failed;
            continue;
        }
        
        // Check for PASSED/FAILED marker
        if (line.find("PASSED") != std::string::npos && currentTest) {
            currentTest->state = TestState::Passed;
            currentTest = nullptr;
        } else if (line.find("FAILED") != std::string::npos && currentTest) {
            currentTest->state = TestState::Failed;
            currentTest->errorMessage = currentFailure;
            currentTest = nullptr;
            currentFailure.clear();
        }
    }
}

void UCCatch2Adapter::ParseSection(const std::string& sectionOutput, TestCase& test) {
    // Sections in Catch2 are nested test scopes
    // Parse section-specific failures
}

TestState UCCatch2Adapter::ParseTestResult(const std::string& testOutput) {
    if (testOutput.find("PASSED") != std::string::npos) {
        return TestState::Passed;
    }
    if (testOutput.find("FAILED") != std::string::npos) {
        return TestState::Failed;
    }
    if (testOutput.find("SKIPPED") != std::string::npos) {
        return TestState::Skipped;
    }
    return TestState::NotRun;
}

TestRunResult UCCatch2Adapter::ParseXMLOutput(const std::string& xmlContent) {
    TestRunResult result;
    
    // Parse Catch2 XML format
    // <Catch2TestRun> or <Catch> root element
    // <TestCase name="..."> elements
    
    // Simple regex-based parsing (use proper XML parser in production)
    std::regex testCasePattern(R"(<TestCase name="([^"]+)"[^>]*>)");
    std::regex resultPattern(R"(<OverallResult success="([^"]+)")");
    std::regex durationPattern(R"(durationInSeconds="([^"]+)")");
    std::regex failurePattern(R"(<Failure[^>]*>([\s\S]*?)</Failure>)");
    
    std::sregex_iterator it(xmlContent.begin(), xmlContent.end(), testCasePattern);
    std::sregex_iterator end;
    
    TestSuite defaultSuite("Default");
    
    while (it != end) {
        TestCase test;
        test.name = (*it)[1].str();
        test.fullName = test.name;
        
        // Find closing tag and extract details
        // ... simplified for brevity
        
        defaultSuite.tests.push_back(test);
        ++it;
    }
    
    if (!defaultSuite.tests.empty()) {
        result.suites.push_back(defaultSuite);
    }
    
    result.CalculateTotals();
    return result;
}

TestRunResult UCCatch2Adapter::ParseJUnitOutput(const std::string& xmlPath) {
    TestRunResult result;
    
    std::ifstream file(xmlPath);
    if (!file.is_open()) return result;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // JUnit format parsing similar to Google Test
    std::regex testsuitePattern(R"(<testsuite[^>]+name="([^"]+)"[^>]+tests="(\d+)"[^>]+failures="(\d+)")");
    std::regex testcasePattern(R"(<testcase[^>]+name="([^"]+)"[^>]+classname="([^"]+)"[^>]+time="([^"]+)")");
    
    std::smatch match;
    std::string::const_iterator searchStart = content.cbegin();
    
    while (std::regex_search(searchStart, content.cend(), match, testsuitePattern)) {
        TestSuite suite;
        suite.name = match[1].str();
        
        result.suites.push_back(suite);
        searchStart = match.suffix().first;
    }
    
    result.CalculateTotals();
    return result;
}

bool UCCatch2Adapter::ExtractTestLocation(const std::string& sourceFile,
                                           const std::string& testName,
                                           std::string& file, int& line) {
    std::ifstream source(sourceFile);
    if (!source.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(source)),
                         std::istreambuf_iterator<char>());
    
    // Look for TEST_CASE, SCENARIO, SECTION macros
    std::vector<std::string> patterns = {
        R"(TEST_CASE\s*\(\s*")" + testName + R"("\s*[,)])",
        R"(SCENARIO\s*\(\s*")" + testName + R"("\s*[,)])",
        R"(SECTION\s*\(\s*")" + testName + R"("\s*\))"
    };
    
    for (const auto& pattern : patterns) {
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(content, match, re)) {
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
