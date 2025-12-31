// Apps/IDE/Build/UCBuildOutput.cpp
// Compiler output parsing implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCBuildOutput.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// GCC OUTPUT PARSER IMPLEMENTATION
// ============================================================================

GCCOutputParser::GCCOutputParser() {
    // Primary pattern: file:line:column: type: message
    // Example: main.cpp:10:5: error: 'x' was not declared in this scope
    primaryPattern = std::regex(
        R"(^(.+):(\d+):(\d+):\s*(error|warning|note|fatal error):\s*(.+)$)",
        std::regex::icase
    );
    
    // Secondary pattern without column: file:line: type: message
    // Example: main.cpp:10: error: message
    secondaryPattern = std::regex(
        R"(^(.+):(\d+):\s*(error|warning|note|fatal error):\s*(.+)$)",
        std::regex::icase
    );
    
    // "In file included from" pattern
    // Example: In file included from main.cpp:1:
    includePattern = std::regex(
        R"(^In file included from (.+):(\d+)[:,]?.*$)",
        std::regex::icase
    );
    
    // Linker error pattern
    // Example: /usr/bin/ld: cannot find -lsomelib
    // Example: undefined reference to `symbol'
    linkerPattern = std::regex(
        R"(^(?:/usr/bin/ld:|.*:\s*)?(.*)(?:undefined reference to|cannot find|multiple definition)(.*)$)",
        std::regex::icase
    );
    
    // Warning flag pattern to extract code
    // Example: [-Wunused-variable]
    warningFlagPattern = std::regex(R"(\[-W([^\]]+)\])");
}

CompilerMessage GCCOutputParser::ParseLine(const std::string& line) {
    CompilerMessage msg;
    msg.rawLine = line;
    msg.type = CompilerMessageType::Info;
    
    std::string trimmedLine = Trim(line);
    if (trimmedLine.empty()) {
        return msg;
    }
    
    std::smatch match;
    
    // Try primary pattern (with column)
    if (std::regex_match(trimmedLine, match, primaryPattern)) {
        msg.filePath = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = std::stoi(match[3].str());
        msg.type = ParseMessageType(match[4].str());
        msg.message = match[5].str();
        msg.code = ExtractWarningCode(msg.message);
        return msg;
    }
    
    // Try secondary pattern (without column)
    if (std::regex_match(trimmedLine, match, secondaryPattern)) {
        msg.filePath = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = 0;
        msg.type = ParseMessageType(match[3].str());
        msg.message = match[4].str();
        msg.code = ExtractWarningCode(msg.message);
        return msg;
    }
    
    // Try "In file included from" pattern
    if (std::regex_match(trimmedLine, match, includePattern)) {
        msg.filePath = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.type = CompilerMessageType::Note;
        msg.message = "In file included from here";
        return msg;
    }
    
    // Try linker error pattern
    if (std::regex_search(trimmedLine, match, linkerPattern)) {
        msg.type = CompilerMessageType::Error;
        msg.message = trimmedLine;
        return msg;
    }
    
    // Check for common error indicators
    std::string lowerLine = trimmedLine;
    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
    
    if (lowerLine.find("error") != std::string::npos) {
        msg.type = CompilerMessageType::Error;
        msg.message = trimmedLine;
    } else if (lowerLine.find("warning") != std::string::npos) {
        msg.type = CompilerMessageType::Warning;
        msg.message = trimmedLine;
    } else if (lowerLine.find("undefined reference") != std::string::npos ||
               lowerLine.find("cannot find") != std::string::npos) {
        msg.type = CompilerMessageType::Error;
        msg.message = trimmedLine;
    } else {
        msg.message = trimmedLine;
    }
    
    return msg;
}

std::vector<CompilerMessage> GCCOutputParser::ParseOutput(const std::string& output) {
    std::vector<CompilerMessage> messages;
    auto lines = SplitLines(output);
    
    for (const auto& line : lines) {
        auto msg = ParseLine(line);
        // Only add messages that are not just empty info messages
        if (!msg.message.empty() || msg.IsError() || msg.IsWarning()) {
            messages.push_back(msg);
        }
    }
    
    return messages;
}

CompilerMessageType GCCOutputParser::ParseMessageType(const std::string& typeStr) {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "error") return CompilerMessageType::Error;
    if (lower == "fatal error") return CompilerMessageType::FatalError;
    if (lower == "warning") return CompilerMessageType::Warning;
    if (lower == "note") return CompilerMessageType::Note;
    
    return CompilerMessageType::Info;
}

std::string GCCOutputParser::ExtractWarningCode(const std::string& message) {
    std::smatch match;
    if (std::regex_search(message, match, warningFlagPattern)) {
        return "W" + match[1].str();
    }
    return "";
}

// ============================================================================
// FREEPASCAL OUTPUT PARSER IMPLEMENTATION
// ============================================================================

FreePascalOutputParser::FreePascalOutputParser() {
    // Primary pattern: file(line,column) Type: message
    // Example: main.pas(10,5) Error: Identifier not found "undeclared"
    primaryPattern = std::regex(
        R"(^(.+)\((\d+),(\d+)\)\s*(Error|Warning|Note|Hint|Fatal):\s*(.+)$)",
        std::regex::icase
    );
    
    // Secondary pattern: file(line) Type: message
    // Example: main.pas(10) Error: message
    secondaryPattern = std::regex(
        R"(^(.+)\((\d+)\)\s*(Error|Warning|Note|Hint|Fatal):\s*(.+)$)",
        std::regex::icase
    );
    
    // Linker error pattern
    linkerPattern = std::regex(
        R"(^(?:Linking|ld[.:]|.*\.o:)(.*)$)",
        std::regex::icase
    );
}

CompilerMessage FreePascalOutputParser::ParseLine(const std::string& line) {
    CompilerMessage msg;
    msg.rawLine = line;
    msg.type = CompilerMessageType::Info;
    
    std::string trimmedLine = Trim(line);
    if (trimmedLine.empty()) {
        return msg;
    }
    
    std::smatch match;
    
    // Try primary pattern (with column)
    if (std::regex_match(trimmedLine, match, primaryPattern)) {
        msg.filePath = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = std::stoi(match[3].str());
        msg.type = ParseMessageType(match[4].str());
        msg.message = match[5].str();
        return msg;
    }
    
    // Try secondary pattern (without column)
    if (std::regex_match(trimmedLine, match, secondaryPattern)) {
        msg.filePath = match[1].str();
        msg.line = std::stoi(match[2].str());
        msg.column = 0;
        msg.type = ParseMessageType(match[3].str());
        msg.message = match[4].str();
        return msg;
    }
    
    // Check for common error indicators in FreePascal
    std::string lowerLine = trimmedLine;
    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
    
    if (lowerLine.find("fatal:") != std::string::npos) {
        msg.type = CompilerMessageType::FatalError;
        msg.message = trimmedLine;
    } else if (lowerLine.find("error:") != std::string::npos) {
        msg.type = CompilerMessageType::Error;
        msg.message = trimmedLine;
    } else if (lowerLine.find("warning:") != std::string::npos) {
        msg.type = CompilerMessageType::Warning;
        msg.message = trimmedLine;
    } else if (lowerLine.find("note:") != std::string::npos ||
               lowerLine.find("hint:") != std::string::npos) {
        msg.type = CompilerMessageType::Note;
        msg.message = trimmedLine;
    } else {
        msg.message = trimmedLine;
    }
    
    return msg;
}

std::vector<CompilerMessage> FreePascalOutputParser::ParseOutput(const std::string& output) {
    std::vector<CompilerMessage> messages;
    auto lines = SplitLines(output);
    
    for (const auto& line : lines) {
        auto msg = ParseLine(line);
        if (!msg.message.empty() || msg.IsError() || msg.IsWarning()) {
            messages.push_back(msg);
        }
    }
    
    return messages;
}

CompilerMessageType FreePascalOutputParser::ParseMessageType(const std::string& typeStr) {
    std::string lower = typeStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "error") return CompilerMessageType::Error;
    if (lower == "fatal") return CompilerMessageType::FatalError;
    if (lower == "warning") return CompilerMessageType::Warning;
    if (lower == "note" || lower == "hint") return CompilerMessageType::Note;
    
    return CompilerMessageType::Info;
}

// ============================================================================
// GENERIC OUTPUT PARSER IMPLEMENTATION
// ============================================================================

GenericOutputParser::GenericOutputParser() {
    // Add some common patterns
    
    // Pattern 1: file:line:column: type: message (GCC-style)
    commonPatterns.push_back(std::regex(
        R"(^(.+):(\d+):(\d+):\s*(error|warning|note):\s*(.+)$)",
        std::regex::icase
    ));
    
    // Pattern 2: file(line,column): type: message (MSVC-style)
    commonPatterns.push_back(std::regex(
        R"(^(.+)\((\d+),(\d+)\):\s*(error|warning):\s*(.+)$)",
        std::regex::icase
    ));
    
    // Pattern 3: file:line: type: message
    commonPatterns.push_back(std::regex(
        R"(^(.+):(\d+):\s*(error|warning|note):\s*(.+)$)",
        std::regex::icase
    ));
}

CompilerMessage GenericOutputParser::ParseLine(const std::string& line) {
    CompilerMessage msg;
    msg.rawLine = line;
    msg.type = CompilerMessageType::Info;
    
    std::string trimmedLine = Trim(line);
    if (trimmedLine.empty()) {
        return msg;
    }
    
    std::smatch match;
    
    // Try custom patterns first
    for (const auto& entry : patterns) {
        if (std::regex_match(trimmedLine, match, entry.regex)) {
            if (match.size() >= 5) {
                msg.filePath = match[1].str();
                msg.line = std::stoi(match[2].str());
                if (match.size() >= 6) {
                    msg.column = std::stoi(match[3].str());
                    std::string typeStr = match[4].str();
                    msg.message = match[5].str();
                    
                    auto it = entry.typeMap.find(typeStr);
                    if (it != entry.typeMap.end()) {
                        msg.type = it->second;
                    }
                } else {
                    std::string typeStr = match[3].str();
                    msg.message = match[4].str();
                    
                    auto it = entry.typeMap.find(typeStr);
                    if (it != entry.typeMap.end()) {
                        msg.type = it->second;
                    }
                }
                return msg;
            }
        }
    }
    
    // Try common patterns
    for (const auto& pattern : commonPatterns) {
        if (std::regex_match(trimmedLine, match, pattern)) {
            msg.filePath = match[1].str();
            msg.line = std::stoi(match[2].str());
            
            if (match.size() == 6) {
                // Pattern with column
                msg.column = std::stoi(match[3].str());
                std::string typeStr = match[4].str();
                std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::tolower);
                
                if (typeStr == "error") msg.type = CompilerMessageType::Error;
                else if (typeStr == "warning") msg.type = CompilerMessageType::Warning;
                else if (typeStr == "note") msg.type = CompilerMessageType::Note;
                
                msg.message = match[5].str();
            } else if (match.size() == 5) {
                // Pattern without column
                std::string typeStr = match[3].str();
                std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::tolower);
                
                if (typeStr == "error") msg.type = CompilerMessageType::Error;
                else if (typeStr == "warning") msg.type = CompilerMessageType::Warning;
                else if (typeStr == "note") msg.type = CompilerMessageType::Note;
                
                msg.message = match[4].str();
            }
            
            return msg;
        }
    }
    
    // Fallback: just store the line
    msg.message = trimmedLine;
    
    // Check for error/warning keywords
    std::string lowerLine = trimmedLine;
    std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
    
    if (lowerLine.find("error") != std::string::npos) {
        msg.type = CompilerMessageType::Error;
    } else if (lowerLine.find("warning") != std::string::npos) {
        msg.type = CompilerMessageType::Warning;
    }
    
    return msg;
}

std::vector<CompilerMessage> GenericOutputParser::ParseOutput(const std::string& output) {
    std::vector<CompilerMessage> messages;
    auto lines = SplitLines(output);
    
    for (const auto& line : lines) {
        auto msg = ParseLine(line);
        if (!msg.message.empty()) {
            messages.push_back(msg);
        }
    }
    
    return messages;
}

void GenericOutputParser::AddPattern(const std::string& pattern,
                                     const std::map<std::string, CompilerMessageType>& typeMap) {
    PatternEntry entry;
    entry.regex = std::regex(pattern, std::regex::icase);
    entry.typeMap = typeMap;
    patterns.push_back(entry);
}

// ============================================================================
// BUILD OUTPUT MANAGER IMPLEMENTATION
// ============================================================================

UCBuildOutput::UCBuildOutput() {
    // Default to generic parser
    parser = std::make_shared<GenericOutputParser>();
}

void UCBuildOutput::SetParser(std::shared_ptr<IOutputParser> newParser) {
    if (newParser) {
        parser = newParser;
    }
}

std::shared_ptr<IOutputParser> UCBuildOutput::GetParserForCompiler(CompilerType type) {
    switch (type) {
        case CompilerType::GCC:
        case CompilerType::GPlusPlus:
        case CompilerType::Clang:
            return std::make_shared<GCCOutputParser>();
            
        case CompilerType::FreePascal:
            return std::make_shared<FreePascalOutputParser>();
            
        default:
            return std::make_shared<GenericOutputParser>();
    }
}

void UCBuildOutput::ProcessLine(const std::string& line) {
    rawOutput.push_back(line);
    
    if (onRawLine) {
        onRawLine(line);
    }
    
    if (parser) {
        auto msg = parser->ParseLine(line);
        
        if (!msg.message.empty() || msg.IsError() || msg.IsWarning()) {
            messages.push_back(msg);
            
            if (msg.IsError()) {
                errorCount++;
                if (onError) {
                    onError(msg);
                }
            } else if (msg.IsWarning()) {
                warningCount++;
                if (onWarning) {
                    onWarning(msg);
                }
            }
            
            if (onMessage) {
                onMessage(msg);
            }
        }
    }
}

void UCBuildOutput::ProcessOutput(const std::string& output) {
    auto lines = SplitLines(output);
    for (const auto& line : lines) {
        ProcessLine(line);
    }
}

void UCBuildOutput::Clear() {
    messages.clear();
    rawOutput.clear();
    errorCount = 0;
    warningCount = 0;
}

std::vector<CompilerMessage> UCBuildOutput::GetErrors() const {
    std::vector<CompilerMessage> errors;
    for (const auto& msg : messages) {
        if (msg.IsError()) {
            errors.push_back(msg);
        }
    }
    return errors;
}

std::vector<CompilerMessage> UCBuildOutput::GetWarnings() const {
    std::vector<CompilerMessage> warnings;
    for (const auto& msg : messages) {
        if (msg.IsWarning()) {
            warnings.push_back(msg);
        }
    }
    return warnings;
}

std::string UCBuildOutput::GetRawOutputString() const {
    std::string result;
    for (const auto& line : rawOutput) {
        result += line + "\n";
    }
    return result;
}

// ============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// ============================================================================

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Handle Windows line endings
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    
    return lines;
}

std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool ContainsIgnoreCase(const std::string& str, const std::string& substr) {
    std::string lowerStr = str;
    std::string lowerSubstr = substr;
    
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    std::transform(lowerSubstr.begin(), lowerSubstr.end(), lowerSubstr.begin(), ::tolower);
    
    return lowerStr.find(lowerSubstr) != std::string::npos;
}

std::string GetFileExtension(const std::string& path) {
    size_t dotPos = path.rfind('.');
    size_t slashPos = path.rfind('/');
    if (slashPos == std::string::npos) slashPos = path.rfind('\\');
    
    // Ensure dot is after last slash (part of filename, not directory)
    if (dotPos != std::string::npos && 
        (slashPos == std::string::npos || dotPos > slashPos)) {
        return path.substr(dotPos + 1);
    }
    
    return "";
}

std::string GetFileName(const std::string& path) {
    size_t slashPos = path.rfind('/');
    if (slashPos == std::string::npos) slashPos = path.rfind('\\');
    
    if (slashPos != std::string::npos) {
        return path.substr(slashPos + 1);
    }
    
    return path;
}

std::string GetDirectory(const std::string& path) {
    size_t slashPos = path.rfind('/');
    if (slashPos == std::string::npos) slashPos = path.rfind('\\');
    
    if (slashPos != std::string::npos) {
        return path.substr(0, slashPos);
    }
    
    return "";
}

std::string NormalizePath(const std::string& path) {
    std::string result = path;
    
    // Convert backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');
    
    // Remove trailing slash
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }
    
    // Remove double slashes
    size_t pos = 0;
    while ((pos = result.find("//", pos)) != std::string::npos) {
        result.erase(pos, 1);
    }
    
    return result;
}

std::string JoinPath(const std::string& base, const std::string& relative) {
    if (base.empty()) return relative;
    if (relative.empty()) return base;
    
    std::string normalizedBase = NormalizePath(base);
    std::string normalizedRelative = NormalizePath(relative);
    
    // If relative is absolute, return it directly
    if (IsAbsolutePath(normalizedRelative)) {
        return normalizedRelative;
    }
    
    // Join with separator
    if (!normalizedBase.empty() && normalizedBase.back() != '/') {
        normalizedBase += '/';
    }
    
    return normalizedBase + normalizedRelative;
}

bool IsAbsolutePath(const std::string& path) {
    if (path.empty()) return false;
    
    // Unix absolute path
    if (path[0] == '/') return true;
    
    // Windows absolute path (drive letter)
    if (path.length() >= 2 && std::isalpha(path[0]) && path[1] == ':') {
        return true;
    }
    
    // UNC path
    if (path.length() >= 2 && path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    
    return false;
}

std::string MakeRelativePath(const std::string& path, const std::string& base) {
    std::string normalizedPath = NormalizePath(path);
    std::string normalizedBase = NormalizePath(base);
    
    // Ensure base ends with separator for proper prefix matching
    if (!normalizedBase.empty() && normalizedBase.back() != '/') {
        normalizedBase += '/';
    }
    
    // Check if path starts with base
    if (normalizedPath.find(normalizedBase) == 0) {
        return normalizedPath.substr(normalizedBase.length());
    }
    
    // Path is not under base, return as-is
    return path;
}

} // namespace IDE
} // namespace UltraCanvas
