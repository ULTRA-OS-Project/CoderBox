// Apps/IDE/Build/CMakeIntegration/UCCMakeParser.cpp
// Enhanced CMake Parser Implementation for ULTRA IDE
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE

#include "UCCMakeParser.h"
#include "../UCBuildManager.h"
#include "../UCBuildOutput.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UCCMakeParser::UCCMakeParser() {
    // Initialize command type mapping
    commandTypes = {
        {"project", CMakeCommandType::Project},
        {"cmake_minimum_required", CMakeCommandType::CMakeMinimumRequired},
        {"add_executable", CMakeCommandType::AddExecutable},
        {"add_library", CMakeCommandType::AddLibrary},
        {"add_custom_target", CMakeCommandType::AddCustomTarget},
        {"target_sources", CMakeCommandType::TargetSources},
        {"target_include_directories", CMakeCommandType::TargetIncludeDirectories},
        {"target_compile_definitions", CMakeCommandType::TargetCompileDefinitions},
        {"target_compile_options", CMakeCommandType::TargetCompileOptions},
        {"target_link_libraries", CMakeCommandType::TargetLinkLibraries},
        {"target_link_options", CMakeCommandType::TargetLinkOptions},
        {"set_target_properties", CMakeCommandType::SetTargetProperties},
        {"include_directories", CMakeCommandType::IncludeDirectories},
        {"add_definitions", CMakeCommandType::AddDefinitions},
        {"link_libraries", CMakeCommandType::LinkLibraries},
        {"link_directories", CMakeCommandType::LinkDirectories},
        {"set", CMakeCommandType::Set},
        {"option", CMakeCommandType::Option},
        {"list", CMakeCommandType::List},
        {"add_subdirectory", CMakeCommandType::AddSubdirectory},
        {"include", CMakeCommandType::Include},
        {"if", CMakeCommandType::If},
        {"elseif", CMakeCommandType::ElseIf},
        {"else", CMakeCommandType::Else},
        {"endif", CMakeCommandType::EndIf},
        {"foreach", CMakeCommandType::ForEach},
        {"endforeach", CMakeCommandType::EndForEach},
        {"while", CMakeCommandType::While},
        {"endwhile", CMakeCommandType::EndWhile},
        {"function", CMakeCommandType::Function},
        {"endfunction", CMakeCommandType::EndFunction},
        {"macro", CMakeCommandType::Macro},
        {"endmacro", CMakeCommandType::EndMacro},
        {"message", CMakeCommandType::Message},
        {"find_package", CMakeCommandType::FindPackage},
        {"find_library", CMakeCommandType::FindLibrary},
        {"configure_file", CMakeCommandType::Configure},
        {"install", CMakeCommandType::Install}
    };
    
    // Initialize regex patterns
    commandPattern = std::regex(R"((\w+)\s*\(([^)]*)\))", std::regex::icase);
    variablePattern = std::regex(R"(\$\{([^}]+)\})");
    generatorExprPattern = std::regex(R"(\$<([^>]+)>)");
    
    // Initialize built-in variables
    builtInVariables = {
        {"CMAKE_SOURCE_DIR", ""},
        {"CMAKE_BINARY_DIR", ""},
        {"CMAKE_CURRENT_SOURCE_DIR", ""},
        {"CMAKE_CURRENT_BINARY_DIR", ""},
        {"PROJECT_SOURCE_DIR", ""},
        {"PROJECT_BINARY_DIR", ""},
        {"CMAKE_PROJECT_NAME", ""},
        {"PROJECT_NAME", ""},
        {"CMAKE_BUILD_TYPE", "Debug"},
        {"CMAKE_CXX_STANDARD", "17"},
        {"CMAKE_C_STANDARD", "11"}
    };
}

// ============================================================================
// PARSING
// ============================================================================

CMakeParsedProject UCCMakeParser::ParseFile(const std::string& filePath) {
    CMakeParsedProject project;
    
    // Read file content
    std::string content = ReadFile(filePath);
    if (content.empty()) {
        project.parseErrors.push_back("Failed to read file: " + filePath);
        return project;
    }
    
    // Get directory from file path
    std::string workingDir = GetDirectory(filePath);
    project.rootDirectory = workingDir;
    project.mainFile = filePath;
    
    currentFile = filePath;
    
    if (onParseStart) {
        onParseStart(filePath);
    }
    
    // Set built-in variables
    builtInVariables["CMAKE_SOURCE_DIR"] = workingDir;
    builtInVariables["CMAKE_CURRENT_SOURCE_DIR"] = workingDir;
    builtInVariables["PROJECT_SOURCE_DIR"] = workingDir;
    
    // Parse content
    return ParseString(content, workingDir);
}

CMakeParsedProject UCCMakeParser::ParseString(const std::string& content, 
                                               const std::string& workingDir) {
    CMakeParsedProject project;
    project.rootDirectory = workingDir;
    
    // Parse all commands
    auto commands = ParseCommands(content);
    project.commands = commands;
    
    // Process each command
    for (const auto& cmd : commands) {
        if (onCommand) {
            onCommand(cmd);
        }
        ProcessCommand(cmd, project);
    }
    
    return project;
}

CMakeParsedCommand UCCMakeParser::ParseCommand(const std::string& commandText) {
    CMakeParsedCommand cmd;
    cmd.rawText = commandText;
    
    // Find command name and arguments
    size_t parenStart = commandText.find('(');
    if (parenStart == std::string::npos) {
        return cmd;
    }
    
    cmd.name = Trim(commandText.substr(0, parenStart));
    
    // Convert name to lowercase for comparison
    std::string lowerName = cmd.name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    cmd.type = GetCommandType(lowerName);
    
    // Extract arguments
    size_t parenEnd = commandText.rfind(')');
    if (parenEnd != std::string::npos && parenEnd > parenStart) {
        std::string argsText = commandText.substr(parenStart + 1, parenEnd - parenStart - 1);
        cmd.arguments = ParseArguments(argsText);
    }
    
    return cmd;
}

std::vector<CMakeParsedCommand> UCCMakeParser::ParseCommands(const std::string& content) {
    std::vector<CMakeParsedCommand> commands;
    
    std::string cleanContent;
    cleanContent.reserve(content.size());
    
    // Remove comments and track line numbers
    bool inBlockComment = false;
    int lineNumber = 1;
    
    for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        
        if (c == '\n') {
            lineNumber++;
            cleanContent += c;
            continue;
        }
        
        // Line comment
        if (c == '#' && !inBlockComment) {
            // Skip to end of line
            while (i < content.size() && content[i] != '\n') {
                i++;
            }
            if (i < content.size()) {
                cleanContent += '\n';
                lineNumber++;
            }
            continue;
        }
        
        // Block comment start: #[[
        if (c == '#' && i + 2 < content.size() && 
            content[i+1] == '[' && content[i+2] == '[') {
            inBlockComment = true;
            i += 2;
            continue;
        }
        
        // Block comment end: ]]
        if (inBlockComment && c == ']' && i + 1 < content.size() && 
            content[i+1] == ']') {
            inBlockComment = false;
            i += 1;
            continue;
        }
        
        if (!inBlockComment) {
            cleanContent += c;
        }
    }
    
    // Parse commands from clean content
    size_t pos = 0;
    lineNumber = 1;
    
    while (pos < cleanContent.size()) {
        // Skip whitespace
        while (pos < cleanContent.size() && 
               (cleanContent[pos] == ' ' || cleanContent[pos] == '\t' || 
                cleanContent[pos] == '\n' || cleanContent[pos] == '\r')) {
            if (cleanContent[pos] == '\n') lineNumber++;
            pos++;
        }
        
        if (pos >= cleanContent.size()) break;
        
        // Find command name
        size_t nameStart = pos;
        while (pos < cleanContent.size() && 
               (std::isalnum(cleanContent[pos]) || cleanContent[pos] == '_')) {
            pos++;
        }
        
        if (pos == nameStart) {
            pos++;
            continue;
        }
        
        // Skip whitespace before (
        while (pos < cleanContent.size() && 
               (cleanContent[pos] == ' ' || cleanContent[pos] == '\t' ||
                cleanContent[pos] == '\n' || cleanContent[pos] == '\r')) {
            if (cleanContent[pos] == '\n') lineNumber++;
            pos++;
        }
        
        if (pos >= cleanContent.size() || cleanContent[pos] != '(') {
            continue;
        }
        
        // Find matching )
        int depth = 1;
        size_t argsStart = pos;
        pos++;
        
        while (pos < cleanContent.size() && depth > 0) {
            if (cleanContent[pos] == '\n') lineNumber++;
            if (cleanContent[pos] == '(') depth++;
            if (cleanContent[pos] == ')') depth--;
            pos++;
        }
        
        // Extract command
        std::string commandText = cleanContent.substr(nameStart, pos - nameStart);
        CMakeParsedCommand cmd = ParseCommand(commandText);
        cmd.lineNumber = lineNumber;
        
        if (cmd.type != CMakeCommandType::Unknown || !cmd.name.empty()) {
            commands.push_back(cmd);
        }
    }
    
    return commands;
}

std::vector<std::string> UCCMakeParser::ParseArguments(const std::string& argsText) {
    std::vector<std::string> args;
    std::string currentArg;
    bool inQuotes = false;
    char quoteChar = 0;
    int bracketDepth = 0;
    
    for (size_t i = 0; i < argsText.size(); ++i) {
        char c = argsText[i];
        
        // Handle quotes
        if ((c == '"' || c == '\'') && bracketDepth == 0) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            } else if (c == quoteChar) {
                inQuotes = false;
            } else {
                currentArg += c;
            }
            continue;
        }
        
        // Handle brackets for generator expressions
        if (c == '<' && i + 1 < argsText.size() && argsText[i+1] == '$') {
            bracketDepth++;
        }
        if (c == '>' && bracketDepth > 0) {
            bracketDepth--;
        }
        
        // Separator (whitespace or semicolon)
        if (!inQuotes && bracketDepth == 0 && 
            (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ';')) {
            if (!currentArg.empty()) {
                args.push_back(Trim(currentArg));
                currentArg.clear();
            }
            continue;
        }
        
        currentArg += c;
    }
    
    // Add last argument
    if (!currentArg.empty()) {
        args.push_back(Trim(currentArg));
    }
    
    return args;
}

CMakeCommandType UCCMakeParser::GetCommandType(const std::string& name) const {
    auto it = commandTypes.find(name);
    if (it != commandTypes.end()) {
        return it->second;
    }
    return CMakeCommandType::Unknown;
}

// ============================================================================
// COMMAND PROCESSING
// ============================================================================

void UCCMakeParser::ProcessCommand(const CMakeParsedCommand& cmd, CMakeParsedProject& project) {
    currentLine = cmd.lineNumber;
    
    switch (cmd.type) {
        case CMakeCommandType::Project:
            if (!cmd.arguments.empty()) {
                project.name = cmd.GetArg(0);
                
                // Check for VERSION
                auto versions = cmd.GetArgsAfter("VERSION");
                if (!versions.empty()) {
                    project.version = versions[0];
                }
                
                // Check for LANGUAGES
                auto languages = cmd.GetArgsAfter("LANGUAGES");
                if (!languages.empty()) {
                    project.languages = languages;
                } else {
                    // Default languages
                    project.languages = {"C", "CXX"};
                }
                
                // Check for DESCRIPTION
                auto descriptions = cmd.GetArgsAfter("DESCRIPTION");
                if (!descriptions.empty()) {
                    project.description = descriptions[0];
                }
                
                // Update variables
                variables["PROJECT_NAME"] = project.name;
                variables["CMAKE_PROJECT_NAME"] = project.name;
            }
            break;
            
        case CMakeCommandType::CMakeMinimumRequired:
            {
                auto versions = cmd.GetArgsAfter("VERSION");
                if (!versions.empty()) {
                    project.cmakeMinVersion = versions[0];
                }
            }
            break;
            
        case CMakeCommandType::AddExecutable:
        case CMakeCommandType::AddLibrary:
            ProcessTargetCommand(cmd, project);
            break;
            
        case CMakeCommandType::TargetSources:
        case CMakeCommandType::TargetIncludeDirectories:
        case CMakeCommandType::TargetCompileDefinitions:
        case CMakeCommandType::TargetCompileOptions:
        case CMakeCommandType::TargetLinkLibraries:
        case CMakeCommandType::TargetLinkOptions:
            // These modify existing targets
            if (!cmd.arguments.empty()) {
                std::string targetName = cmd.GetArg(0);
                
                // Find target
                for (auto& target : project.targets) {
                    if (target.name == targetName) {
                        // Get sources/settings after scope (PUBLIC, PRIVATE, INTERFACE)
                        std::vector<std::string> items;
                        bool foundScope = false;
                        
                        for (size_t i = 1; i < cmd.arguments.size(); ++i) {
                            const auto& arg = cmd.arguments[i];
                            if (arg == "PUBLIC" || arg == "PRIVATE" || arg == "INTERFACE") {
                                foundScope = true;
                                continue;
                            }
                            if (foundScope || i > 0) {
                                items.push_back(ExpandVariables(arg, project));
                            }
                        }
                        
                        // Add to appropriate list
                        switch (cmd.type) {
                            case CMakeCommandType::TargetSources:
                                target.sources.insert(target.sources.end(), 
                                                      items.begin(), items.end());
                                break;
                            case CMakeCommandType::TargetIncludeDirectories:
                                target.includeDirectories.insert(target.includeDirectories.end(),
                                                                  items.begin(), items.end());
                                break;
                            case CMakeCommandType::TargetCompileDefinitions:
                                target.compileDefinitions.insert(target.compileDefinitions.end(),
                                                                  items.begin(), items.end());
                                break;
                            case CMakeCommandType::TargetCompileOptions:
                                target.compileOptions.insert(target.compileOptions.end(),
                                                              items.begin(), items.end());
                                break;
                            case CMakeCommandType::TargetLinkLibraries:
                                target.linkLibraries.insert(target.linkLibraries.end(),
                                                            items.begin(), items.end());
                                break;
                            case CMakeCommandType::TargetLinkOptions:
                                target.linkOptions.insert(target.linkOptions.end(),
                                                          items.begin(), items.end());
                                break;
                            default:
                                break;
                        }
                        break;
                    }
                }
            }
            break;
            
        case CMakeCommandType::IncludeDirectories:
            for (const auto& arg : cmd.arguments) {
                std::string expanded = ExpandVariables(arg, project);
                if (expanded != "SYSTEM" && expanded != "BEFORE" && expanded != "AFTER") {
                    project.includeDirectories.push_back(expanded);
                }
            }
            break;
            
        case CMakeCommandType::LinkDirectories:
            for (const auto& arg : cmd.arguments) {
                project.linkDirectories.push_back(ExpandVariables(arg, project));
            }
            break;
            
        case CMakeCommandType::AddDefinitions:
            for (const auto& arg : cmd.arguments) {
                std::string def = arg;
                if (def.substr(0, 2) == "-D") {
                    def = def.substr(2);
                }
                project.globalDefinitions.push_back(def);
            }
            break;
            
        case CMakeCommandType::LinkLibraries:
            for (const auto& arg : cmd.arguments) {
                project.globalLinkLibraries.push_back(ExpandVariables(arg, project));
            }
            break;
            
        case CMakeCommandType::Set:
        case CMakeCommandType::Option:
            ProcessVariableCommand(cmd, project);
            break;
            
        case CMakeCommandType::AddSubdirectory:
            if (!cmd.arguments.empty()) {
                std::string subdir = ExpandVariables(cmd.GetArg(0), project);
                project.subdirectories.push_back(subdir);
                ProcessSubdirectory(subdir, project.rootDirectory, project);
            }
            break;
            
        case CMakeCommandType::Include:
            if (!cmd.arguments.empty()) {
                std::string includePath = ExpandVariables(cmd.GetArg(0), project);
                project.includedFiles.push_back(includePath);
                ProcessInclude(includePath, project.rootDirectory, project);
            }
            break;
            
        case CMakeCommandType::FindPackage:
            if (!cmd.arguments.empty()) {
                std::string packageName = cmd.GetArg(0);
                if (cmd.HasArg("REQUIRED")) {
                    project.requiredPackages.push_back(packageName);
                } else {
                    project.optionalPackages.push_back(packageName);
                }
            }
            break;
            
        default:
            break;
    }
}

void UCCMakeParser::ProcessTargetCommand(const CMakeParsedCommand& cmd, CMakeParsedProject& project) {
    if (cmd.arguments.empty()) return;
    
    CMakeParsedTarget target;
    target.name = cmd.GetArg(0);
    target.definedAtLine = cmd.lineNumber;
    target.definedInFile = currentFile;
    
    // Determine target type
    if (cmd.type == CMakeCommandType::AddExecutable) {
        target.type = CMakeTarget::Type::Executable;
    } else {
        // Check for library type
        if (cmd.HasArg("STATIC")) {
            target.type = CMakeTarget::Type::StaticLibrary;
        } else if (cmd.HasArg("SHARED")) {
            target.type = CMakeTarget::Type::SharedLibrary;
        } else if (cmd.HasArg("INTERFACE")) {
            target.type = CMakeTarget::Type::Interface;
        } else {
            target.type = CMakeTarget::Type::StaticLibrary;  // Default
        }
    }
    
    // Get source files (skip keywords)
    for (size_t i = 1; i < cmd.arguments.size(); ++i) {
        const auto& arg = cmd.arguments[i];
        
        // Skip keywords
        if (arg == "STATIC" || arg == "SHARED" || arg == "MODULE" ||
            arg == "INTERFACE" || arg == "OBJECT" || arg == "IMPORTED" ||
            arg == "ALIAS" || arg == "WIN32" || arg == "MACOSX_BUNDLE" ||
            arg == "EXCLUDE_FROM_ALL") {
            continue;
        }
        
        std::string source = ExpandVariables(arg, project);
        
        // Check if it's a header or source
        std::string ext = GetFileExtension(source);
        for (char& c : ext) c = std::tolower(c);
        
        if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "hh") {
            target.headers.push_back(source);
        } else {
            target.sources.push_back(source);
        }
    }
    
    project.targets.push_back(target);
    
    if (onTarget) {
        onTarget(target);
    }
}

void UCCMakeParser::ProcessVariableCommand(const CMakeParsedCommand& cmd, CMakeParsedProject& project) {
    if (cmd.arguments.empty()) return;
    
    std::string varName = cmd.GetArg(0);
    
    if (cmd.type == CMakeCommandType::Option) {
        // option(VAR "description" value)
        std::string value = "OFF";
        if (cmd.arguments.size() > 2) {
            value = cmd.GetArg(2);
        }
        project.options[varName] = value;
        variables[varName] = value;
    } else {
        // set(VAR value ...)
        bool isCache = cmd.HasArg("CACHE");
        bool isParentScope = cmd.HasArg("PARENT_SCOPE");
        
        std::string value;
        for (size_t i = 1; i < cmd.arguments.size(); ++i) {
            const auto& arg = cmd.arguments[i];
            if (arg == "CACHE" || arg == "PARENT_SCOPE" || 
                arg == "STRING" || arg == "BOOL" || arg == "FILEPATH" ||
                arg == "PATH" || arg == "INTERNAL" || arg == "FORCE") {
                break;
            }
            if (!value.empty()) value += ";";
            value += arg;
        }
        
        value = ExpandVariables(value, project);
        
        if (isCache) {
            project.cacheVariables[varName] = value;
        } else {
            project.variables[varName] = value;
        }
        
        variables[varName] = value;
    }
}

// ============================================================================
// VARIABLE EXPANSION
// ============================================================================

std::string UCCMakeParser::ExpandVariables(const std::string& input, 
                                            const CMakeParsedProject& project) {
    std::string result = input;
    
    // Keep expanding until no more variables
    size_t maxIterations = 10;
    while (maxIterations-- > 0) {
        std::smatch match;
        if (!std::regex_search(result, match, variablePattern)) {
            break;
        }
        
        std::string varName = match[1].str();
        std::string varValue;
        
        // Check local variables
        auto it = variables.find(varName);
        if (it != variables.end()) {
            varValue = it->second;
        } else {
            // Check project variables
            auto pit = project.variables.find(varName);
            if (pit != project.variables.end()) {
                varValue = pit->second;
            } else {
                // Check built-in
                auto bit = builtInVariables.find(varName);
                if (bit != builtInVariables.end()) {
                    varValue = bit->second;
                }
            }
        }
        
        result = result.replace(match.position(), match.length(), varValue);
    }
    
    // Handle generator expressions (simplified - just extract content)
    result = HandleGeneratorExpression(result);
    
    return result;
}

std::string UCCMakeParser::HandleGeneratorExpression(const std::string& expr) {
    // Simplified handling - just remove generator expression syntax
    std::string result = expr;
    std::smatch match;
    
    while (std::regex_search(result, match, generatorExprPattern)) {
        // Extract content between $< and >
        std::string genExpr = match[1].str();
        
        // Try to extract meaningful value
        // Format: $<CONDITION:value> or $<TARGET_PROPERTY:target,prop>
        size_t colonPos = genExpr.find(':');
        if (colonPos != std::string::npos) {
            genExpr = genExpr.substr(colonPos + 1);
        }
        
        result = result.replace(match.position(), match.length(), genExpr);
    }
    
    return result;
}

void UCCMakeParser::SetVariable(const std::string& name, const std::string& value) {
    variables[name] = value;
}

std::string UCCMakeParser::GetVariable(const std::string& name) const {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    return "";
}

// ============================================================================
// FILE OPERATIONS
// ============================================================================

std::string UCCMakeParser::ReadFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void UCCMakeParser::ProcessInclude(const std::string& includePath,
                                    const std::string& workingDir,
                                    CMakeParsedProject& project) {
    std::string fullPath = JoinPath(workingDir, includePath);
    
    // Check for .cmake extension
    if (includePath.find(".cmake") == std::string::npos) {
        fullPath += ".cmake";
    }
    
    std::string content = ReadFile(fullPath);
    if (!content.empty()) {
        auto commands = ParseCommands(content);
        for (const auto& cmd : commands) {
            ProcessCommand(cmd, project);
        }
    }
}

void UCCMakeParser::ProcessSubdirectory(const std::string& subdir,
                                         const std::string& workingDir,
                                         CMakeParsedProject& project) {
    std::string subdirPath = JoinPath(workingDir, subdir);
    std::string cmakePath = JoinPath(subdirPath, "CMakeLists.txt");
    
    // Update current source dir
    std::string oldSourceDir = variables["CMAKE_CURRENT_SOURCE_DIR"];
    variables["CMAKE_CURRENT_SOURCE_DIR"] = subdirPath;
    
    std::string content = ReadFile(cmakePath);
    if (!content.empty()) {
        auto commands = ParseCommands(content);
        for (const auto& cmd : commands) {
            ProcessCommand(cmd, project);
        }
    }
    
    // Restore
    variables["CMAKE_CURRENT_SOURCE_DIR"] = oldSourceDir;
}

// ============================================================================
// PROJECT CONVERSION
// ============================================================================

std::shared_ptr<UCIDEProject> UCCMakeParser::ToIDEProject(const CMakeParsedProject& parsed) {
    auto project = std::make_shared<UCIDEProject>();
    
    project->name = parsed.name;
    project->version = parsed.version;
    project->description = parsed.description;
    project->rootDirectory = parsed.rootDirectory;
    
    // CMake settings
    project->cmake.enabled = true;
    project->cmake.cmakeListsPath = "CMakeLists.txt";
    
    // Convert targets
    for (const auto& target : parsed.targets) {
        CMakeTarget cmakeTarget;
        cmakeTarget.name = target.name;
        cmakeTarget.type = target.type;
        cmakeTarget.sources = target.sources;
        cmakeTarget.includeDirs = target.includeDirectories;
        cmakeTarget.defines = target.compileDefinitions;
        cmakeTarget.linkLibraries = target.linkLibraries;
        project->cmakeTargets.push_back(cmakeTarget);
    }
    
    if (!project->cmakeTargets.empty()) {
        project->cmake.activeTarget = project->cmakeTargets[0].name;
    }
    
    // Create configurations
    project->configurations.push_back(UCIDEProject::CreateDebugConfiguration(parsed.name));
    project->configurations.push_back(UCIDEProject::CreateReleaseConfiguration(parsed.name));
    
    return project;
}

void UCCMakeParser::UpdateIDEProject(std::shared_ptr<UCIDEProject> project,
                                      const CMakeParsedProject& parsed) {
    if (!project) return;
    
    project->name = parsed.name;
    project->version = parsed.version;
    project->description = parsed.description;
    
    // Update targets
    project->cmakeTargets.clear();
    for (const auto& target : parsed.targets) {
        CMakeTarget cmakeTarget;
        cmakeTarget.name = target.name;
        cmakeTarget.type = target.type;
        cmakeTarget.sources = target.sources;
        cmakeTarget.includeDirs = target.includeDirectories;
        cmakeTarget.defines = target.compileDefinitions;
        cmakeTarget.linkLibraries = target.linkLibraries;
        project->cmakeTargets.push_back(cmakeTarget);
    }
}

// ============================================================================
// CMAKE BUILD RUNNER
// ============================================================================

UCCMakeBuildRunner::UCCMakeBuildRunner() {
    cmakePath = FindCommand("cmake");
}

int UCCMakeBuildRunner::Configure(const std::string& sourceDir,
                                   const std::string& buildDir,
                                   const std::string& generator,
                                   const std::map<std::string, std::string>& options) {
    std::vector<std::string> args;
    args.push_back("-S");
    args.push_back(sourceDir);
    args.push_back("-B");
    args.push_back(buildDir);
    
    if (!generator.empty()) {
        args.push_back("-G");
        args.push_back(generator);
    }
    
    for (const auto& opt : options) {
        args.push_back("-D" + opt.first + "=" + opt.second);
    }
    
    return ExecuteCMake(args, sourceDir);
}

int UCCMakeBuildRunner::Build(const std::string& buildDir,
                               const std::string& target,
                               const std::string& config,
                               int jobs) {
    std::vector<std::string> args;
    args.push_back("--build");
    args.push_back(buildDir);
    
    if (!target.empty()) {
        args.push_back("--target");
        args.push_back(target);
    }
    
    if (!config.empty()) {
        args.push_back("--config");
        args.push_back(config);
    }
    
    if (jobs > 0) {
        args.push_back("-j");
        args.push_back(std::to_string(jobs));
    } else {
        args.push_back("-j");
    }
    
    return ExecuteCMake(args, buildDir);
}

int UCCMakeBuildRunner::Clean(const std::string& buildDir) {
    std::vector<std::string> args;
    args.push_back("--build");
    args.push_back(buildDir);
    args.push_back("--target");
    args.push_back("clean");
    
    return ExecuteCMake(args, buildDir);
}

int UCCMakeBuildRunner::Install(const std::string& buildDir,
                                 const std::string& prefix) {
    std::vector<std::string> args;
    args.push_back("--install");
    args.push_back(buildDir);
    
    if (!prefix.empty()) {
        args.push_back("--prefix");
        args.push_back(prefix);
    }
    
    return ExecuteCMake(args, buildDir);
}

std::vector<std::string> UCCMakeBuildRunner::GetAvailableGenerators() {
    std::vector<std::string> generators;
    
    ExecuteProcess(
        cmakePath.empty() ? "cmake" : cmakePath,
        {"--help"},
        "",
        [&generators](const std::string& line) {
            // Parse generator list from cmake --help output
            if (line.find("*") == 0 || line.find("  ") == 0) {
                std::string gen = Trim(line);
                if (gen[0] == '*') gen = gen.substr(1);
                gen = Trim(gen);
                
                // Extract generator name (before "=")
                size_t eqPos = gen.find('=');
                if (eqPos != std::string::npos) {
                    gen = Trim(gen.substr(0, eqPos));
                }
                
                if (!gen.empty() && gen[0] != '[') {
                    generators.push_back(gen);
                }
            }
        }
    );
    
    return generators;
}

bool UCCMakeBuildRunner::IsCMakeAvailable() {
    return CommandExists(cmakePath.empty() ? "cmake" : cmakePath);
}

std::string UCCMakeBuildRunner::GetCMakeVersion() {
    std::string version;
    
    ExecuteProcess(
        cmakePath.empty() ? "cmake" : cmakePath,
        {"--version"},
        "",
        [&version](const std::string& line) {
            if (version.empty() && line.find("cmake version") != std::string::npos) {
                version = line;
            }
        }
    );
    
    // Extract version number
    std::regex versionRegex(R"((\d+\.\d+\.\d+))");
    std::smatch match;
    if (std::regex_search(version, match, versionRegex)) {
        return match[1].str();
    }
    
    return version;
}

int UCCMakeBuildRunner::ExecuteCMake(const std::vector<std::string>& args,
                                      const std::string& workDir) {
    return ExecuteProcess(
        cmakePath.empty() ? "cmake" : cmakePath,
        args,
        workDir,
        onOutput,
        onError
    );
}

} // namespace IDE
} // namespace UltraCanvas
