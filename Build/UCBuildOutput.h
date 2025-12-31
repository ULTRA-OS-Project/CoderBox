// Apps/IDE/Build/UCBuildOutput.h
// Compiler output parsing utilities for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "IUCCompilerPlugin.h"
#include <string>
#include <vector>
#include <regex>
#include <memory>
#include <functional>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// OUTPUT PARSER INTERFACE
// ============================================================================

/**
 * @brief Abstract interface for compiler output parsers
 */
class IOutputParser {
public:
    virtual ~IOutputParser() = default;
    
    /**
     * @brief Parse a single line of compiler output
     * @param line Raw output line
     * @return Parsed message (type is Info if line not recognized)
     */
    virtual CompilerMessage ParseLine(const std::string& line) = 0;
    
    /**
     * @brief Parse multiple lines of output
     * @param output Complete output string
     * @return Vector of parsed messages
     */
    virtual std::vector<CompilerMessage> ParseOutput(const std::string& output) = 0;
    
    /**
     * @brief Get parser name
     */
    virtual std::string GetParserName() const = 0;
};

// ============================================================================
// GCC OUTPUT PARSER
// ============================================================================

/**
 * @brief Parser for GCC/G++ compiler output
 * 
 * GCC output format examples:
 *   main.cpp:10:5: error: 'undeclared' was not declared in this scope
 *   main.cpp:15:10: warning: unused variable 'x' [-Wunused-variable]
 *   main.cpp:20:1: note: in expansion of macro 'MACRO'
 *   In file included from main.cpp:1:
 *   /path/to/header.h:5:1: error: expected ';' before '}' token
 */
class GCCOutputParser : public IOutputParser {
public:
    GCCOutputParser();
    
    CompilerMessage ParseLine(const std::string& line) override;
    std::vector<CompilerMessage> ParseOutput(const std::string& output) override;
    std::string GetParserName() const override { return "GCC Output Parser"; }
    
private:
    // Primary message pattern: file:line:column: type: message
    std::regex primaryPattern;
    
    // Secondary pattern without column: file:line: type: message
    std::regex secondaryPattern;
    
    // "In file included from" pattern
    std::regex includePattern;
    
    // Linker error pattern
    std::regex linkerPattern;
    
    // Warning flag pattern (e.g., [-Wunused-variable])
    std::regex warningFlagPattern;
    
    /**
     * @brief Convert GCC type string to CompilerMessageType
     */
    CompilerMessageType ParseMessageType(const std::string& typeStr);
    
    /**
     * @brief Extract warning code from message (e.g., "Wunused-variable")
     */
    std::string ExtractWarningCode(const std::string& message);
};

// ============================================================================
// FREEPASCAL OUTPUT PARSER
// ============================================================================

/**
 * @brief Parser for FreePascal compiler output
 * 
 * FreePascal output format examples:
 *   main.pas(10,5) Error: Identifier not found "undeclared"
 *   main.pas(15,10) Warning: Variable "x" not used
 *   main.pas(20,1) Note: Expanding macro
 *   main.pas(25,1) Hint: Parameter "param" not used
 *   main.pas(30,1) Fatal: Syntax error, ";" expected but "}" found
 */
class FreePascalOutputParser : public IOutputParser {
public:
    FreePascalOutputParser();
    
    CompilerMessage ParseLine(const std::string& line) override;
    std::vector<CompilerMessage> ParseOutput(const std::string& output) override;
    std::string GetParserName() const override { return "FreePascal Output Parser"; }
    
private:
    // Primary pattern: file(line,column) Type: message
    std::regex primaryPattern;
    
    // Alternative pattern: file(line) Type: message
    std::regex secondaryPattern;
    
    // Linker error pattern
    std::regex linkerPattern;
    
    /**
     * @brief Convert FreePascal type string to CompilerMessageType
     */
    CompilerMessageType ParseMessageType(const std::string& typeStr);
};

// ============================================================================
// GENERIC OUTPUT PARSER
// ============================================================================

/**
 * @brief Generic parser that tries multiple patterns
 */
class GenericOutputParser : public IOutputParser {
public:
    GenericOutputParser();
    
    CompilerMessage ParseLine(const std::string& line) override;
    std::vector<CompilerMessage> ParseOutput(const std::string& output) override;
    std::string GetParserName() const override { return "Generic Output Parser"; }
    
    /**
     * @brief Add a custom regex pattern
     * @param pattern Regex pattern with capture groups: (file)(line)(column?)(type)(message)
     * @param typeMap Map from captured type string to CompilerMessageType
     */
    void AddPattern(const std::string& pattern, 
                    const std::map<std::string, CompilerMessageType>& typeMap);
    
private:
    struct PatternEntry {
        std::regex regex;
        std::map<std::string, CompilerMessageType> typeMap;
    };
    
    std::vector<PatternEntry> patterns;
    
    // Common patterns to try
    std::vector<std::regex> commonPatterns;
};

// ============================================================================
// BUILD OUTPUT MANAGER
// ============================================================================

/**
 * @brief Manages build output parsing and distribution
 */
class UCBuildOutput {
public:
    UCBuildOutput();
    ~UCBuildOutput() = default;
    
    // ===== PARSER MANAGEMENT =====
    
    /**
     * @brief Set the output parser to use
     */
    void SetParser(std::shared_ptr<IOutputParser> parser);
    
    /**
     * @brief Get parser for specific compiler type
     */
    static std::shared_ptr<IOutputParser> GetParserForCompiler(CompilerType type);
    
    // ===== OUTPUT PROCESSING =====
    
    /**
     * @brief Process a single line of output
     * @param line Raw output line
     */
    void ProcessLine(const std::string& line);
    
    /**
     * @brief Process complete output
     * @param output Complete output string
     */
    void ProcessOutput(const std::string& output);
    
    /**
     * @brief Clear all collected data
     */
    void Clear();
    
    // ===== RESULTS =====
    
    /**
     * @brief Get all collected messages
     */
    const std::vector<CompilerMessage>& GetMessages() const { return messages; }
    
    /**
     * @brief Get error messages only
     */
    std::vector<CompilerMessage> GetErrors() const;
    
    /**
     * @brief Get warning messages only
     */
    std::vector<CompilerMessage> GetWarnings() const;
    
    /**
     * @brief Get raw output lines
     */
    const std::vector<std::string>& GetRawOutput() const { return rawOutput; }
    
    /**
     * @brief Get complete raw output as single string
     */
    std::string GetRawOutputString() const;
    
    /**
     * @brief Get error count
     */
    int GetErrorCount() const { return errorCount; }
    
    /**
     * @brief Get warning count
     */
    int GetWarningCount() const { return warningCount; }
    
    // ===== CALLBACKS =====
    
    /**
     * @brief Callback for each raw output line
     */
    std::function<void(const std::string&)> onRawLine;
    
    /**
     * @brief Callback for each parsed message
     */
    std::function<void(const CompilerMessage&)> onMessage;
    
    /**
     * @brief Callback for errors specifically
     */
    std::function<void(const CompilerMessage&)> onError;
    
    /**
     * @brief Callback for warnings specifically
     */
    std::function<void(const CompilerMessage&)> onWarning;

private:
    std::shared_ptr<IOutputParser> parser;
    std::vector<CompilerMessage> messages;
    std::vector<std::string> rawOutput;
    int errorCount = 0;
    int warningCount = 0;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Split string by newlines
 */
std::vector<std::string> SplitLines(const std::string& text);

/**
 * @brief Trim whitespace from string
 */
std::string Trim(const std::string& str);

/**
 * @brief Check if string contains substring (case-insensitive)
 */
bool ContainsIgnoreCase(const std::string& str, const std::string& substr);

/**
 * @brief Extract file extension from path
 */
std::string GetFileExtension(const std::string& path);

/**
 * @brief Get filename from path
 */
std::string GetFileName(const std::string& path);

/**
 * @brief Get directory from path
 */
std::string GetDirectory(const std::string& path);

/**
 * @brief Normalize path separators
 */
std::string NormalizePath(const std::string& path);

/**
 * @brief Join path components
 */
std::string JoinPath(const std::string& base, const std::string& relative);

/**
 * @brief Check if path is absolute
 */
bool IsAbsolutePath(const std::string& path);

/**
 * @brief Make path relative to base
 */
std::string MakeRelativePath(const std::string& path, const std::string& base);

} // namespace IDE
} // namespace UltraCanvas
