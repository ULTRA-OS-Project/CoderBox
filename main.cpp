// Apps/CoderBox/main.cpp
// CoderBox Application Entry Point
// Version: 2.0.0
// Last Modified: 2025-12-30
// Author: UltraCanvas Framework / CoderBox

#include "CoderBox.h"                // Business logic layer
#include "UI/UCCoderBox_Core.h"      // UI layer
#include <iostream>
#include <string>

using namespace UltraCanvas;
using namespace UltraCanvas::CoderBox;

/**
 * @brief Print usage information
 */
void PrintUsage(const char* programName) {
    std::cout << "CoderBox - Integrated Development Environment\n";
    std::cout << "Built on UltraCanvas Framework\n\n";
    std::cout << "Usage: " << programName << " [options] [project.ucproj]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --help, -h       Show this help message\n";
    std::cout << "  --version, -v    Show version information\n";
    std::cout << "  --light          Use light theme\n";
    std::cout << "  --dark           Use dark theme (default)\n";
    std::cout << "  --highcontrast   Use high contrast theme\n";
    std::cout << "  --width <n>      Set window width (default: 1280)\n";
    std::cout << "  --height <n>     Set window height (default: 720)\n";
    std::cout << std::endl;
}

/**
 * @brief Print version information
 */
void PrintVersion() {
    std::cout << "CoderBox v" << CODERBOX_VERSION << "\n";
    std::cout << "Built on UltraCanvas Framework\n";
    std::cout << "Copyright (c) 2025 UltraCanvas Framework\n";
    std::cout << std::endl;
}

/**
 * @brief Parse command line arguments
 */
CoderBoxConfiguration ParseArguments(int argc, char* argv[], std::string& projectPath) {
    CoderBoxConfiguration config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            exit(0);
        }
        else if (arg == "--version" || arg == "-v") {
            PrintVersion();
            exit(0);
        }
        else if (arg == "--light") {
            config.darkTheme = false;
        }
        else if (arg == "--dark") {
            config.darkTheme = true;
        }
        else if (arg == "--highcontrast") {
            // High contrast is typically dark-based
            config.darkTheme = true;
        }
        else if (arg == "--width" && i + 1 < argc) {
            config.windowWidth = std::stoi(argv[++i]);
        }
        else if (arg == "--height" && i + 1 < argc) {
            config.windowHeight = std::stoi(argv[++i]);
        }
        else if (arg[0] != '-') {
            // Assume it's a project file
            projectPath = arg;
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            PrintUsage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "         CoderBox Starting...\n";
    std::cout << "========================================\n\n";
    
    // Parse command line arguments
    std::string projectPath;
    CoderBoxConfiguration uiConfig = ParseArguments(argc, argv, projectPath);
    
    // Step 1: Initialize business logic layer
    CoderBoxConfig businessConfig = CoderBoxConfig::Default();
    if (!CoderBox::Instance().Initialize(businessConfig)) {
        std::cerr << "Failed to initialize CoderBox business logic" << std::endl;
        return -1;
    }
    
    // Step 2: Create and initialize UI layer
    auto app = CreateCoderBox(uiConfig);
    if (!app) {
        std::cerr << "Failed to create CoderBox UI application" << std::endl;
        CoderBox::Instance().Shutdown();
        return -1;
    }
    
    // Step 3: Connect UI layer to business logic layer
    // Build callbacks
    CoderBox::Instance().onBuildOutput = [&app](const std::string& line) {
        app->WriteBuildOutput(line + "\n");
    };
    
    CoderBox::Instance().onCompilerMessage = [&app](const CompilerMessage& msg) {
        if (msg.type == MessageType::Error) {
            app->WriteError(msg.file, msg.line, msg.column, msg.message);
        } else if (msg.type == MessageType::Warning) {
            app->WriteWarning(msg.file, msg.line, msg.column, msg.message);
        }
    };
    
    CoderBox::Instance().onBuildComplete = [&app](const BuildResult& result) {
        app->SetErrorCounts(result.errorCount, result.warningCount);
        app->SetStatus(result.success ? "Build succeeded" : "Build failed");
    };
    
    CoderBox::Instance().onStateChange = [&app](CoderBoxState state) {
        app->SetStatus(CoderBoxStateToString(state));
    };
    
    // Connect UI commands to business logic
    app->onCommand = [](CoderBoxCommand cmd) {
        switch (cmd) {
            case CoderBoxCommand::BuildBuild:
                CoderBox::Instance().Build();
                break;
            case CoderBoxCommand::BuildRebuild:
                CoderBox::Instance().Rebuild();
                break;
            case CoderBoxCommand::BuildClean:
                CoderBox::Instance().Clean();
                break;
            case CoderBoxCommand::BuildRun:
                CoderBox::Instance().Run();
                break;
            case CoderBoxCommand::BuildStop:
                CoderBox::Instance().CancelBuild();
                CoderBox::Instance().Stop();
                break;
            default:
                // Other commands handled by UI layer
                break;
        }
    };
    
    // Step 4: Open project if specified
    if (!projectPath.empty()) {
        std::cout << "Opening project: " << projectPath << std::endl;
        auto project = CoderBox::Instance().OpenProject(projectPath);
        if (project) {
            app->OpenProject(projectPath);
        }
    }
    
    std::cout << "\nCoderBox is ready.\n";
    std::cout << "Press Ctrl+N for new file, Ctrl+O to open file.\n\n";
    
    // Step 5: Run the application main loop
    int result = app->Run();
    
    // Step 6: Shutdown
    CoderBox::Instance().Shutdown();
    
    std::cout << "\nCoderBox terminated with code: " << result << std::endl;
    
    return result;
}
