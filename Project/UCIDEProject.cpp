// Apps/IDE/Project/UCIDEProject.cpp
// IDE Project implementation with JSON serialization
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCIDEProject.h"
#include "../Build/UCBuildOutput.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <regex>
#include <cstring>

// Platform-specific includes for directory operations
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #define PATH_SEPARATOR '\\'
#else
    #include <dirent.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #define PATH_SEPARATOR '/'
#endif

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// HELPER FUNCTIONS FOR JSON
// ============================================================================

namespace {

/**
 * @brief Get current ISO 8601 timestamp
 */
std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

/**
 * @brief Escape string for JSON
 */
std::string JsonEscape(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 10);
    
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    
    return result;
}

/**
 * @brief Unescape JSON string
 */
std::string JsonUnescape(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"':  result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                case 'b':  result += '\b'; i++; break;
                case 'f':  result += '\f'; i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 't':  result += '\t'; i++; break;
                case 'u':
                    if (i + 5 < str.size()) {
                        i += 5;
                    }
                    break;
                default:
                    result += str[i];
            }
        } else {
            result += str[i];
        }
    }
    
    return result;
}

/**
 * @brief Write indentation
 */
std::string Indent(int level) {
    return std::string(level * 4, ' ');
}

/**
 * @brief Write JSON string value
 */
std::string JsonString(const std::string& value) {
    return "\"" + JsonEscape(value) + "\"";
}

/**
 * @brief Write JSON array of strings
 */
std::string JsonStringArray(const std::vector<std::string>& arr, int indent) {
    if (arr.empty()) return "[]";
    
    std::stringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < arr.size(); ++i) {
        ss << Indent(indent + 1) << JsonString(arr[i]);
        if (i < arr.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << Indent(indent) << "]";
    return ss.str();
}

/**
 * @brief Simple JSON value extraction (string)
 */
std::string ExtractJsonString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t startQuote = json.find('"', colonPos);
    if (startQuote == std::string::npos) return "";
    
    size_t endQuote = startQuote + 1;
    while (endQuote < json.size()) {
        if (json[endQuote] == '"' && json[endQuote - 1] != '\\') {
            break;
        }
        endQuote++;
    }
    
    if (endQuote >= json.size()) return "";
    
    return JsonUnescape(json.substr(startQuote + 1, endQuote - startQuote - 1));
}

/**
 * @brief Simple JSON value extraction (int)
 */
int ExtractJsonInt(const std::string& json, const std::string& key, int defaultValue = 0) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultValue;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultValue;
    
    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t' || json[start] == '\n')) {
        start++;
    }
    
    std::string numStr;
    while (start < json.size() && (std::isdigit(json[start]) || json[start] == '-')) {
        numStr += json[start++];
    }
    
    if (numStr.empty()) return defaultValue;
    
    try {
        return std::stoi(numStr);
    } catch (...) {
        return defaultValue;
    }
}

/**
 * @brief Simple JSON value extraction (bool)
 */
bool ExtractJsonBool(const std::string& json, const std::string& key, bool defaultValue = false) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return defaultValue;
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return defaultValue;
    
    size_t start = colonPos + 1;
    while (start < json.size() && (json[start] == ' ' || json[start] == '\t' || json[start] == '\n')) {
        start++;
    }
    
    if (start + 4 <= json.size() && json.substr(start, 4) == "true") return true;
    if (start + 5 <= json.size() && json.substr(start, 5) == "false") return false;
    
    return defaultValue;
}

/**
 * @brief Extract JSON array of strings
 */
std::vector<std::string> ExtractJsonStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return result;
    
    size_t bracketStart = json.find('[', keyPos);
    if (bracketStart == std::string::npos) return result;
    
    size_t bracketEnd = bracketStart + 1;
    int depth = 1;
    while (bracketEnd < json.size() && depth > 0) {
        if (json[bracketEnd] == '[') depth++;
        else if (json[bracketEnd] == ']') depth--;
        bracketEnd++;
    }
    
    std::string arrayContent = json.substr(bracketStart + 1, bracketEnd - bracketStart - 2);
    
    size_t pos = 0;
    while (pos < arrayContent.size()) {
        size_t startQuote = arrayContent.find('"', pos);
        if (startQuote == std::string::npos) break;
        
        size_t endQuote = startQuote + 1;
        while (endQuote < arrayContent.size()) {
            if (arrayContent[endQuote] == '"' && arrayContent[endQuote - 1] != '\\') {
                break;
            }
            endQuote++;
        }
        
        if (endQuote < arrayContent.size()) {
            result.push_back(JsonUnescape(arrayContent.substr(startQuote + 1, endQuote - startQuote - 1)));
        }
        
        pos = endQuote + 1;
    }
    
    return result;
}

/**
 * @brief Extract JSON object section
 */
std::string ExtractJsonObject(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) return "";
    
    size_t braceStart = json.find('{', keyPos);
    if (braceStart == std::string::npos) return "";
    
    size_t braceEnd = braceStart + 1;
    int depth = 1;
    while (braceEnd < json.size() && depth > 0) {
        if (json[braceEnd] == '{') depth++;
        else if (json[braceEnd] == '}') depth--;
        braceEnd++;
    }
    
    return json.substr(braceStart, braceEnd - braceStart);
}

/**
 * @brief Check if file exists
 */
bool FileExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode));
#endif
}

/**
 * @brief Check if directory exists
 */
bool DirectoryExists(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

/**
 * @brief Get directory entries
 */
std::vector<std::pair<std::string, bool>> GetDirectoryEntries(const std::string& path) {
    std::vector<std::pair<std::string, bool>> entries;
    
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name = findData.cFileName;
            if (name != "." && name != "..") {
                bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                entries.push_back({name, isDir});
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name != "." && name != "..") {
                std::string fullPath = path + "/" + name;
                struct stat st;
                bool isDir = false;
                if (stat(fullPath.c_str(), &st) == 0) {
                    isDir = S_ISDIR(st.st_mode);
                }
                entries.push_back({name, isDir});
            }
        }
        closedir(dir);
    }
#endif
    
    // Sort: directories first, then alphabetically
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    
    return entries;
}

/**
 * @brief Simple glob pattern matching
 */
bool MatchesGlob(const std::string& str, const std::string& pattern) {
    size_t s = 0, p = 0;
    size_t starIdx = std::string::npos;
    size_t matchIdx = 0;
    
    while (s < str.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == str[s])) {
            s++;
            p++;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starIdx = p;
            matchIdx = s;
            p++;
        } else if (starIdx != std::string::npos) {
            p = starIdx + 1;
            matchIdx++;
            s = matchIdx;
        } else {
            return false;
        }
    }
    
    while (p < pattern.size() && pattern[p] == '*') {
        p++;
    }
    
    return p == pattern.size();
}

} // anonymous namespace

// ============================================================================
// UCIDEPROJECT IMPLEMENTATION
// ============================================================================

bool UCIDEProject::LoadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();
        file.close();
        
        if (!FromJSON(json)) {
            return false;
        }
        
        projectFilePath = path;
        
        // Extract root directory from project file path
        size_t lastSlash = path.rfind('/');
        if (lastSlash == std::string::npos) lastSlash = path.rfind('\\');
        if (lastSlash != std::string::npos) {
            rootDirectory = path.substr(0, lastSlash);
        } else {
            rootDirectory = ".";
        }
        
        RefreshFileTree();
        isModified = false;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool UCIDEProject::SaveToFile(const std::string& path) {
    try {
        std::string savePath = path.empty() ? projectFilePath : path;
        if (savePath.empty()) {
            return false;
        }
        
        Touch();
        
        std::string json = ToJSON();
        
        std::ofstream file(savePath);
        if (!file.is_open()) {
            return false;
        }
        
        file << json;
        file.close();
        
        projectFilePath = savePath;
        isModified = false;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

BuildConfiguration& UCIDEProject::GetActiveConfiguration() {
    if (activeConfigurationIndex >= 0 && 
        activeConfigurationIndex < static_cast<int>(configurations.size())) {
        return configurations[activeConfigurationIndex];
    }
    
    if (configurations.empty()) {
        configurations.push_back(CreateDebugConfiguration(name));
    }
    
    activeConfigurationIndex = 0;
    return configurations[0];
}

const BuildConfiguration& UCIDEProject::GetActiveConfiguration() const {
    if (activeConfigurationIndex >= 0 && 
        activeConfigurationIndex < static_cast<int>(configurations.size())) {
        return configurations[activeConfigurationIndex];
    }
    
    static BuildConfiguration defaultConfig;
    return defaultConfig;
}

void UCIDEProject::AddConfiguration(const BuildConfiguration& config) {
    configurations.push_back(config);
    isModified = true;
}

void UCIDEProject::RemoveConfiguration(int index) {
    if (index >= 0 && index < static_cast<int>(configurations.size())) {
        configurations.erase(configurations.begin() + index);
        
        if (activeConfigurationIndex >= static_cast<int>(configurations.size())) {
            activeConfigurationIndex = std::max(0, static_cast<int>(configurations.size()) - 1);
        }
        isModified = true;
    }
}

void UCIDEProject::SetActiveConfiguration(int index) {
    if (index >= 0 && index < static_cast<int>(configurations.size())) {
        activeConfigurationIndex = index;
        isModified = true;
    }
}

void UCIDEProject::SetActiveConfiguration(const std::string& name) {
    int index = FindConfiguration(name);
    if (index >= 0) {
        SetActiveConfiguration(index);
    }
}

int UCIDEProject::FindConfiguration(const std::string& name) const {
    for (size_t i = 0; i < configurations.size(); ++i) {
        if (configurations[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void UCIDEProject::RefreshFileTree() {
    rootFolder = ProjectFolder();
    rootFolder.name = name;
    rootFolder.absolutePath = rootDirectory;
    rootFolder.relativePath = "";
    rootFolder.isExpanded = true;
    
    if (!rootDirectory.empty() && DirectoryExists(rootDirectory)) {
        ScanDirectory(rootDirectory, rootFolder, 0);
    }
}

void UCIDEProject::ScanDirectory(const std::string& path, ProjectFolder& folder, int depth) {
    if (depth > MAX_SCAN_DEPTH) {
        return;
    }
    
    auto entries = GetDirectoryEntries(path);
    
    for (const auto& entry : entries) {
        std::string fullPath = path + "/" + entry.first;
        std::string relativePath = folder.relativePath.empty() ? 
                                   entry.first : 
                                   folder.relativePath + "/" + entry.first;
        
        // Check exclude patterns
        if (MatchesExcludePattern(relativePath) || MatchesExcludePattern(entry.first)) {
            continue;
        }
        
        // Skip hidden files/folders (starting with .)
        if (!entry.first.empty() && entry.first[0] == '.') {
            continue;
        }
        
        if (entry.second) {
            // Directory
            ProjectFolder subfolder;
            subfolder.name = entry.first;
            subfolder.absolutePath = fullPath;
            subfolder.relativePath = relativePath;
            subfolder.isExpanded = false;
            
            ScanDirectory(fullPath, subfolder, depth + 1);
            
            // Only add non-empty folders
            if (!subfolder.files.empty() || !subfolder.subfolders.empty()) {
                folder.subfolders.push_back(subfolder);
            }
        } else {
            // File
            ProjectFile file;
            file.absolutePath = fullPath;
            file.relativePath = relativePath;
            file.fileName = entry.first;
            file.DetectType();
            file.isOpen = false;
            file.isModified = false;
            
            folder.files.push_back(file);
        }
    }
}

void UCIDEProject::AddFile(const std::string& filePath) {
    // Implementation for adding file to project
    isModified = true;
    RefreshFileTree();
}

void UCIDEProject::RemoveFile(const std::string& filePath) {
    // Implementation for removing file from project
    isModified = true;
    RefreshFileTree();
}

ProjectFile* UCIDEProject::FindFile(const std::string& relativePath) {
    std::function<ProjectFile*(ProjectFolder&)> search = [&](ProjectFolder& folder) -> ProjectFile* {
        for (auto& file : folder.files) {
            if (file.relativePath == relativePath || file.absolutePath == relativePath) {
                return &file;
            }
        }
        for (auto& subfolder : folder.subfolders) {
            if (auto* found = search(subfolder)) {
                return found;
            }
        }
        return nullptr;
    };
    
    return search(rootFolder);
}

const ProjectFile* UCIDEProject::FindFile(const std::string& relativePath) const {
    return const_cast<UCIDEProject*>(this)->FindFile(relativePath);
}

std::vector<ProjectFile*> UCIDEProject::GetSourceFiles() {
    std::vector<ProjectFile*> sources;
    
    std::function<void(ProjectFolder&)> collect = [&](ProjectFolder& folder) {
        for (auto& file : folder.files) {
            if (file.IsSource()) {
                sources.push_back(&file);
            }
        }
        for (auto& subfolder : folder.subfolders) {
            collect(subfolder);
        }
    };
    
    collect(rootFolder);
    return sources;
}

std::vector<const ProjectFile*> UCIDEProject::GetSourceFiles() const {
    std::vector<const ProjectFile*> sources;
    
    std::function<void(const ProjectFolder&)> collect = [&](const ProjectFolder& folder) {
        for (const auto& file : folder.files) {
            if (file.IsSource()) {
                sources.push_back(&file);
            }
        }
        for (const auto& subfolder : folder.subfolders) {
            collect(subfolder);
        }
    };
    
    collect(rootFolder);
    return sources;
}

std::vector<ProjectFile*> UCIDEProject::GetModifiedFiles() {
    std::vector<ProjectFile*> modified;
    
    std::function<void(ProjectFolder&)> collect = [&](ProjectFolder& folder) {
        for (auto& file : folder.files) {
            if (file.isModified) {
                modified.push_back(&file);
            }
        }
        for (auto& subfolder : folder.subfolders) {
            collect(subfolder);
        }
    };
    
    collect(rootFolder);
    return modified;
}

bool UCIDEProject::MatchesExcludePattern(const std::string& path) const {
    for (const auto& pattern : excludePatterns) {
        if (MatchesGlob(path, pattern)) {
            return true;
        }
    }
    return false;
}

bool UCIDEProject::DetectCMakeProject() {
    std::string cmakePath = rootDirectory + "/CMakeLists.txt";
    if (FileExists(cmakePath)) {
        cmake.enabled = true;
        cmake.cmakeListsPath = "CMakeLists.txt";
        return true;
    }
    return false;
}

bool UCIDEProject::ParseCMakeLists() {
    // Basic CMakeLists.txt parsing
    // Full implementation would use a proper CMake parser
    
    if (!cmake.enabled || cmake.cmakeListsPath.empty()) {
        return false;
    }
    
    std::string cmakePath = GetAbsolutePath(cmake.cmakeListsPath);
    std::ifstream file(cmakePath);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    cmakeTargets.clear();
    
    // Simple regex to find add_executable and add_library
    std::regex executableRegex(R"(add_executable\s*\(\s*(\w+))", std::regex::icase);
    std::regex libraryRegex(R"(add_library\s*\(\s*(\w+))", std::regex::icase);
    std::regex projectRegex(R"(project\s*\(\s*(\w+))", std::regex::icase);
    
    std::smatch match;
    std::string::const_iterator searchStart = content.cbegin();
    
    // Extract project name
    if (std::regex_search(content, match, projectRegex)) {
        if (name.empty()) {
            name = match[1].str();
        }
    }
    
    // Find executables
    while (std::regex_search(searchStart, content.cend(), match, executableRegex)) {
        CMakeTarget target;
        target.name = match[1].str();
        target.type = CMakeTarget::Type::Executable;
        cmakeTargets.push_back(target);
        searchStart = match.suffix().first;
    }
    
    // Find libraries
    searchStart = content.cbegin();
    while (std::regex_search(searchStart, content.cend(), match, libraryRegex)) {
        CMakeTarget target;
        target.name = match[1].str();
        // Determine library type based on content (simplified)
        if (content.find("STATIC") != std::string::npos) {
            target.type = CMakeTarget::Type::StaticLibrary;
        } else if (content.find("SHARED") != std::string::npos) {
            target.type = CMakeTarget::Type::SharedLibrary;
        } else {
            target.type = CMakeTarget::Type::StaticLibrary;
        }
        cmakeTargets.push_back(target);
        searchStart = match.suffix().first;
    }
    
    // Set first target as active if none selected
    if (cmake.activeTarget.empty() && !cmakeTargets.empty()) {
        cmake.activeTarget = cmakeTargets[0].name;
    }
    
    return true;
}

std::vector<std::string> UCIDEProject::GetCMakeTargetNames() const {
    std::vector<std::string> names;
    for (const auto& target : cmakeTargets) {
        names.push_back(target.name);
    }
    return names;
}

const CMakeTarget* UCIDEProject::GetCMakeTarget(const std::string& name) const {
    for (const auto& target : cmakeTargets) {
        if (target.name == name) {
            return &target;
        }
    }
    return nullptr;
}

std::string UCIDEProject::ToJSON() const {
    std::stringstream ss;
    
    ss << "{\n";
    
    // UltraIDE section
    ss << Indent(1) << "\"ultraide\": {\n";
    ss << Indent(2) << "\"version\": " << JsonString(UCIDE_PROJECT_VERSION) << ",\n";
    ss << Indent(2) << "\"formatVersion\": " << UCIDE_FORMAT_VERSION << "\n";
    ss << Indent(1) << "},\n";
    
    // Project section
    ss << Indent(1) << "\"project\": {\n";
    ss << Indent(2) << "\"name\": " << JsonString(name) << ",\n";
    ss << Indent(2) << "\"version\": " << JsonString(version) << ",\n";
    ss << Indent(2) << "\"description\": " << JsonString(description) << ",\n";
    ss << Indent(2) << "\"author\": " << JsonString(author) << ",\n";
    ss << Indent(2) << "\"created\": " << JsonString(createdDate) << ",\n";
    ss << Indent(2) << "\"modified\": " << JsonString(modifiedDate) << "\n";
    ss << Indent(1) << "},\n";
    
    // Compiler section
    ss << Indent(1) << "\"compiler\": {\n";
    ss << Indent(2) << "\"type\": " << JsonString(CompilerTypeToString(primaryCompiler)) << ",\n";
    ss << Indent(2) << "\"path\": " << JsonString(compilerPath) << ",\n";
    ss << Indent(2) << "\"defaultConfiguration\": " << JsonString(
        activeConfigurationIndex >= 0 && activeConfigurationIndex < static_cast<int>(configurations.size()) ?
        configurations[activeConfigurationIndex].name : "Debug") << "\n";
    ss << Indent(1) << "},\n";
    
    // Configurations section
    ss << Indent(1) << "\"configurations\": [\n";
    for (size_t i = 0; i < configurations.size(); ++i) {
        const auto& cfg = configurations[i];
        ss << Indent(2) << "{\n";
        ss << Indent(3) << "\"name\": " << JsonString(cfg.name) << ",\n";
        ss << Indent(3) << "\"outputDirectory\": " << JsonString(cfg.outputDirectory) << ",\n";
        ss << Indent(3) << "\"outputName\": " << JsonString(cfg.outputName) << ",\n";
        ss << Indent(3) << "\"outputType\": " << JsonString(BuildOutputTypeToString(cfg.outputType)) << ",\n";
        ss << Indent(3) << "\"optimizationLevel\": " << cfg.optimizationLevel << ",\n";
        ss << Indent(3) << "\"debugSymbols\": " << (cfg.debugSymbols ? "true" : "false") << ",\n";
        ss << Indent(3) << "\"enableWarnings\": " << (cfg.enableWarnings ? "true" : "false") << ",\n";
        ss << Indent(3) << "\"treatWarningsAsErrors\": " << (cfg.treatWarningsAsErrors ? "true" : "false") << ",\n";
        ss << Indent(3) << "\"defines\": " << JsonStringArray(cfg.defines, 3) << ",\n";
        ss << Indent(3) << "\"includePaths\": " << JsonStringArray(cfg.includePaths, 3) << ",\n";
        ss << Indent(3) << "\"libraryPaths\": " << JsonStringArray(cfg.libraryPaths, 3) << ",\n";
        ss << Indent(3) << "\"libraries\": " << JsonStringArray(cfg.libraries, 3) << ",\n";
        ss << Indent(3) << "\"compilerFlags\": " << JsonStringArray(cfg.compilerFlags, 3) << ",\n";
        ss << Indent(3) << "\"linkerFlags\": " << JsonStringArray(cfg.linkerFlags, 3) << "\n";
        ss << Indent(2) << "}";
        if (i < configurations.size() - 1) ss << ",";
        ss << "\n";
    }
    ss << Indent(1) << "],\n";
    
    // CMake section
    ss << Indent(1) << "\"cmake\": {\n";
    ss << Indent(2) << "\"enabled\": " << (cmake.enabled ? "true" : "false") << ",\n";
    ss << Indent(2) << "\"cmakeListsPath\": " << JsonString(cmake.cmakeListsPath) << ",\n";
    ss << Indent(2) << "\"activeTarget\": " << JsonString(cmake.activeTarget) << ",\n";
    ss << Indent(2) << "\"buildDirectory\": " << JsonString(cmake.buildDirectory) << ",\n";
    ss << Indent(2) << "\"generator\": " << JsonString(cmake.generator) << "\n";
    ss << Indent(1) << "},\n";
    
    // Files section
    ss << Indent(1) << "\"files\": {\n";
    ss << Indent(2) << "\"excludePatterns\": " << JsonStringArray(excludePatterns, 2) << "\n";
    ss << Indent(1) << "},\n";
    
    // Editor section
    ss << Indent(1) << "\"editor\": {\n";
    ss << Indent(2) << "\"tabSize\": " << editor.tabSize << ",\n";
    ss << Indent(2) << "\"insertSpaces\": " << (editor.insertSpaces ? "true" : "false") << ",\n";
    ss << Indent(2) << "\"trimTrailingWhitespace\": " << (editor.trimTrailingWhitespace ? "true" : "false") << ",\n";
    ss << Indent(2) << "\"defaultEncoding\": " << JsonString(editor.defaultEncoding) << "\n";
    ss << Indent(1) << "}\n";
    
    ss << "}\n";
    
    return ss.str();
}

bool UCIDEProject::FromJSON(const std::string& json) {
    try {
        // Parse project section
        std::string projectSection = ExtractJsonObject(json, "project");
        if (!projectSection.empty()) {
            name = ExtractJsonString(projectSection, "name");
            version = ExtractJsonString(projectSection, "version");
            description = ExtractJsonString(projectSection, "description");
            author = ExtractJsonString(projectSection, "author");
            createdDate = ExtractJsonString(projectSection, "created");
            modifiedDate = ExtractJsonString(projectSection, "modified");
        }
        
        // Parse compiler section
        std::string compilerSection = ExtractJsonObject(json, "compiler");
        if (!compilerSection.empty()) {
            primaryCompiler = StringToCompilerType(ExtractJsonString(compilerSection, "type"));
            compilerPath = ExtractJsonString(compilerSection, "path");
            std::string defaultConfig = ExtractJsonString(compilerSection, "defaultConfiguration");
            // Will set active config after parsing configurations
        }
        
        // Parse configurations - simplified parsing
        // Full implementation would parse the configurations array properly
        
        // Parse cmake section
        std::string cmakeSection = ExtractJsonObject(json, "cmake");
        if (!cmakeSection.empty()) {
            cmake.enabled = ExtractJsonBool(cmakeSection, "enabled", false);
            cmake.cmakeListsPath = ExtractJsonString(cmakeSection, "cmakeListsPath");
            cmake.activeTarget = ExtractJsonString(cmakeSection, "activeTarget");
            cmake.buildDirectory = ExtractJsonString(cmakeSection, "buildDirectory");
            cmake.generator = ExtractJsonString(cmakeSection, "generator");
        }
        
        // Parse files section
        std::string filesSection = ExtractJsonObject(json, "files");
        if (!filesSection.empty()) {
            excludePatterns = ExtractJsonStringArray(filesSection, "excludePatterns");
        }
        
        // Parse editor section
        std::string editorSection = ExtractJsonObject(json, "editor");
        if (!editorSection.empty()) {
            editor.tabSize = ExtractJsonInt(editorSection, "tabSize", 4);
            editor.insertSpaces = ExtractJsonBool(editorSection, "insertSpaces", true);
            editor.trimTrailingWhitespace = ExtractJsonBool(editorSection, "trimTrailingWhitespace", true);
            editor.defaultEncoding = ExtractJsonString(editorSection, "defaultEncoding");
            if (editor.defaultEncoding.empty()) editor.defaultEncoding = "UTF-8";
        }
        
        // Create default configurations if none were parsed
        if (configurations.empty()) {
            configurations.push_back(CreateDebugConfiguration(name));
            configurations.push_back(CreateReleaseConfiguration(name));
        }
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string UCIDEProject::GetRelativePath(const std::string& absolutePath) const {
    return MakeRelativePath(absolutePath, rootDirectory);
}

std::string UCIDEProject::GetAbsolutePath(const std::string& relativePath) const {
    return JoinPath(rootDirectory, relativePath);
}

void UCIDEProject::Touch() {
    modifiedDate = GetCurrentTimestamp();
    isModified = true;
}

bool UCIDEProject::HasUnsavedChanges() const {
    return isModified;
}

void UCIDEProject::SetModified(bool modified) {
    isModified = modified;
}

std::shared_ptr<UCIDEProject> UCIDEProject::Create(
    const std::string& projectName,
    const std::string& rootPath,
    CompilerType compiler
) {
    auto project = std::make_shared<UCIDEProject>();
    
    project->name = projectName;
    project->version = "1.0.0";
    project->rootDirectory = rootPath;
    project->projectFilePath = rootPath + "/" + projectName + UCIDE_PROJECT_EXTENSION;
    project->createdDate = GetCurrentTimestamp();
    project->modifiedDate = project->createdDate;
    project->primaryCompiler = compiler;
    
    // Add default configurations
    project->configurations.push_back(CreateDebugConfiguration(projectName));
    project->configurations.push_back(CreateReleaseConfiguration(projectName));
    project->activeConfigurationIndex = 0;
    
    // Detect CMake if present
    project->DetectCMakeProject();
    if (project->cmake.enabled) {
        project->ParseCMakeLists();
    }
    
    // Scan file tree
    project->RefreshFileTree();
    
    return project;
}

std::shared_ptr<UCIDEProject> UCIDEProject::CreateFromCMake(const std::string& cmakeListsPath) {
    // Get directory containing CMakeLists.txt
    size_t lastSlash = cmakeListsPath.rfind('/');
    if (lastSlash == std::string::npos) lastSlash = cmakeListsPath.rfind('\\');
    
    std::string rootPath = (lastSlash != std::string::npos) ? 
                           cmakeListsPath.substr(0, lastSlash) : ".";
    
    auto project = std::make_shared<UCIDEProject>();
    
    project->rootDirectory = rootPath;
    project->cmake.enabled = true;
    project->cmake.cmakeListsPath = "CMakeLists.txt";
    project->createdDate = GetCurrentTimestamp();
    project->modifiedDate = project->createdDate;
    project->primaryCompiler = CompilerType::GPlusPlus;
    
    // Parse CMakeLists.txt to get project name
    project->ParseCMakeLists();
    
    if (project->name.empty()) {
        // Use directory name as project name
        size_t nameStart = rootPath.rfind('/');
        if (nameStart == std::string::npos) nameStart = rootPath.rfind('\\');
        project->name = (nameStart != std::string::npos) ? 
                        rootPath.substr(nameStart + 1) : rootPath;
    }
    
    project->projectFilePath = rootPath + "/" + project->name + UCIDE_PROJECT_EXTENSION;
    
    // Add configurations
    project->configurations.push_back(CreateDebugConfiguration(project->name));
    project->configurations.push_back(CreateReleaseConfiguration(project->name));
    
    // Scan file tree
    project->RefreshFileTree();
    
    return project;
}

BuildConfiguration UCIDEProject::CreateDebugConfiguration(const std::string& outputName) {
    BuildConfiguration config;
    config.name = "Debug";
    config.outputDirectory = "build/debug";
    config.outputName = outputName;
    config.outputType = BuildOutputType::Executable;
    config.optimizationLevel = 0;
    config.debugSymbols = true;
    config.enableWarnings = true;
    config.treatWarningsAsErrors = false;
    config.defines = {"DEBUG", "_DEBUG"};
    config.compilerFlags = {"-Wall", "-Wextra", "-g"};
    return config;
}

BuildConfiguration UCIDEProject::CreateReleaseConfiguration(const std::string& outputName) {
    BuildConfiguration config;
    config.name = "Release";
    config.outputDirectory = "build/release";
    config.outputName = outputName;
    config.outputType = BuildOutputType::Executable;
    config.optimizationLevel = 3;
    config.debugSymbols = false;
    config.enableWarnings = true;
    config.treatWarningsAsErrors = false;
    config.defines = {"NDEBUG"};
    config.compilerFlags = {"-Wall", "-O3"};
    config.linkerFlags = {"-s"};
    return config;
}

// ============================================================================
// UCIDEPROJECTMANAGER IMPLEMENTATION
// ============================================================================

UCIDEProjectManager& UCIDEProjectManager::Instance() {
    static UCIDEProjectManager instance;
    return instance;
}

std::shared_ptr<UCIDEProject> UCIDEProjectManager::CreateProject(
    const std::string& name,
    const std::string& path,
    CompilerType compiler
) {
    auto project = UCIDEProject::Create(name, path, compiler);
    
    if (project) {
        openProjects.push_back(project);
        SetActiveProject(project);
        AddToRecentProjects(project->projectFilePath);
        
        if (onProjectOpen) {
            onProjectOpen(project);
        }
    }
    
    return project;
}

std::shared_ptr<UCIDEProject> UCIDEProjectManager::OpenProject(const std::string& projectFilePath) {
    // Check if already open
    for (auto& proj : openProjects) {
        if (proj->projectFilePath == projectFilePath) {
            SetActiveProject(proj);
            return proj;
        }
    }
    
    auto project = std::make_shared<UCIDEProject>();
    if (!project->LoadFromFile(projectFilePath)) {
        if (onProjectError) {
            onProjectError("Failed to open project: " + projectFilePath);
        }
        return nullptr;
    }
    
    openProjects.push_back(project);
    SetActiveProject(project);
    AddToRecentProjects(projectFilePath);
    
    if (onProjectOpen) {
        onProjectOpen(project);
    }
    
    return project;
}

std::shared_ptr<UCIDEProject> UCIDEProjectManager::ImportCMakeProject(const std::string& cmakeListsPath) {
    auto project = UCIDEProject::CreateFromCMake(cmakeListsPath);
    
    if (project) {
        openProjects.push_back(project);
        SetActiveProject(project);
        
        if (onProjectOpen) {
            onProjectOpen(project);
        }
    }
    
    return project;
}

bool UCIDEProjectManager::SaveProject(std::shared_ptr<UCIDEProject> project) {
    if (!project) return false;
    
    if (project->SaveToFile()) {
        if (onProjectSave) {
            onProjectSave(project);
        }
        return true;
    }
    
    if (onProjectError) {
        onProjectError("Failed to save project: " + project->name);
    }
    return false;
}

bool UCIDEProjectManager::CloseProject(std::shared_ptr<UCIDEProject> project) {
    if (!project) return false;
    
    auto it = std::find(openProjects.begin(), openProjects.end(), project);
    if (it != openProjects.end()) {
        if (onProjectClose) {
            onProjectClose(project);
        }
        
        openProjects.erase(it);
        
        if (activeProject == project) {
            activeProject = openProjects.empty() ? nullptr : openProjects.back();
            if (onActiveProjectChange) {
                onActiveProjectChange(activeProject);
            }
        }
        
        return true;
    }
    
    return false;
}

void UCIDEProjectManager::CloseAllProjects() {
    while (!openProjects.empty()) {
        CloseProject(openProjects.back());
    }
}

void UCIDEProjectManager::SetActiveProject(std::shared_ptr<UCIDEProject> project) {
    if (activeProject != project) {
        activeProject = project;
        if (onActiveProjectChange) {
            onActiveProjectChange(project);
        }
    }
}

void UCIDEProjectManager::AddToRecentProjects(const std::string& path) {
    // Remove if already exists
    RemoveFromRecentProjects(path);
    
    // Add to front
    recentProjects.insert(recentProjects.begin(), path);
    
    // Limit size
    if (recentProjects.size() > MAX_RECENT_PROJECTS) {
        recentProjects.resize(MAX_RECENT_PROJECTS);
    }
    
    SaveRecentProjects();
}

void UCIDEProjectManager::RemoveFromRecentProjects(const std::string& path) {
    auto it = std::find(recentProjects.begin(), recentProjects.end(), path);
    if (it != recentProjects.end()) {
        recentProjects.erase(it);
    }
}

void UCIDEProjectManager::ClearRecentProjects() {
    recentProjects.clear();
    SaveRecentProjects();
}

void UCIDEProjectManager::LoadRecentProjects() {
    // Implementation would load from a settings file
    // For now, just initialize empty
    recentProjects.clear();
}

void UCIDEProjectManager::SaveRecentProjects() {
    // Implementation would save to a settings file
}

} // namespace IDE
} // namespace UltraCanvas
