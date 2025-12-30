// Apps/IDE/main.cpp
// ULTRA IDE Entry Point
// Version: 1.0.0
// Last Modified: 2025-12-28
// Author: UltraCanvas Framework / ULTRA IDE

#include "UltraIDE.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

// Platform-specific
#ifdef _WIN32
    #include <windows.h>
#else
    #include <signal.h>
#endif

using namespace UltraCanvas::IDE;

// ============================================================================
// SIGNAL HANDLING
// ============================================================================

#ifndef _WIN32
void SignalHandler(int signal) {
    std::cout << "\n[ULTRA IDE] Received signal " << signal << ", shutting down..." << std::endl;
    UltraIDE::Instance().Shutdown();
    exit(0);
}
#endif

// ============================================================================
// COMMAND LINE PARSING
// ============================================================================

struct CommandLineArgs {
    std::string projectPath;
    std::string filePath;
    bool showHelp = false;
    bool showVersion = false;
    bool listPlugins = false;
    bool verbose = false;
    bool buildOnly = false;
    std::string configFile;
};

void PrintUsage(const char* programName) {
    std::cout << "\nUsage: " << programName << " [options] [project.ucproj | file]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help           Show this help message\n";
    std::cout << "  -v, --version        Show version information\n";
    std::cout << "  -p, --plugins        List available compiler plugins\n";
    std::cout << "  -c, --config <file>  Use specified configuration file\n";
    std::cout << "  -b, --build          Build project and exit\n";
    std::cout << "  --verbose            Enable verbose output\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << "                     # Start IDE\n";
    std::cout << "  " << programName << " MyProject.ucproj   # Open project\n";
    std::cout << "  " << programName << " main.cpp           # Open file\n";
    std::cout << "  " << programName << " -b MyProject.ucproj # Build project\n";
    std::cout << "  " << programName << " -p                  # List plugins\n";
    std::cout << "\n";
}

CommandLineArgs ParseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            args.showHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            args.showVersion = true;
        } else if (arg == "-p" || arg == "--plugins") {
            args.listPlugins = true;
        } else if (arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "-b" || arg == "--build") {
            args.buildOnly = true;
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            args.configFile = argv[++i];
        } else if (arg[0] != '-') {
            // Check if it's a project file or source file
            if (arg.find(".ucproj") != std::string::npos) {
                args.projectPath = arg;
            } else {
                args.filePath = arg;
            }
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            args.showHelp = true;
        }
    }
    
    return args;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, char* argv[]) {
    // Parse command line arguments
    CommandLineArgs args = ParseCommandLine(argc, argv);
    
    // Show help
    if (args.showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }
    
    // Show version
    if (args.showVersion) {
        std::cout << UltraIDE::Instance().GetBuildInfo() << std::endl;
        return 0;
    }
    
    // Set up signal handlers (Unix)
#ifndef _WIN32
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
#endif
    
    // Initialize IDE configuration
    UltraIDEConfig config = UltraIDEConfig::Default();
    
    // Load custom config if specified
    if (!args.configFile.empty()) {
        if (!config.LoadFromFile(args.configFile)) {
            std::cerr << "Warning: Could not load config file: " << args.configFile << std::endl;
        }
    }
    
    // Initialize IDE
    if (!UltraIDE::Instance().Initialize(config)) {
        std::cerr << "Failed to initialize ULTRA IDE" << std::endl;
        return 1;
    }
    
    // List plugins only
    if (args.listPlugins) {
        UltraIDE::Instance().PrintPluginStatus();
        return 0;
    }
    
    // Open project if specified
    if (!args.projectPath.empty()) {
        auto project = UltraIDE::Instance().OpenProject(args.projectPath);
        if (!project) {
            std::cerr << "Failed to open project: " << args.projectPath << std::endl;
            return 1;
        }
        
        std::cout << "[ULTRA IDE] Opened project: " << project->name << std::endl;
        
        // Build only mode
        if (args.buildOnly) {
            std::cout << "[ULTRA IDE] Building project..." << std::endl;
            UltraIDE::Instance().Build();
            
            // Wait for build to complete
            while (UltraIDE::Instance().IsBuildInProgress()) {
                // Small sleep
#ifdef _WIN32
                Sleep(100);
#else
                usleep(100000);
#endif
            }
            
            const auto& result = UltraIDE::Instance().GetLastBuildResult();
            std::cout << "[ULTRA IDE] " << result.GetSummary() << std::endl;
            
            return result.success ? 0 : 1;
        }
    }
    
    // Open file if specified
    if (!args.filePath.empty()) {
        UltraIDE::Instance().OpenFile(args.filePath);
        std::cout << "[ULTRA IDE] Opened file: " << args.filePath << std::endl;
    }
    
    // Run the IDE
    int exitCode = UltraIDE::Instance().Run();
    
    // Shutdown
    UltraIDE::Instance().Shutdown();
    
    return exitCode;
}

// ============================================================================
// WINDOWS ENTRY POINT
// ============================================================================

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // Convert command line to argc/argv format
    int argc;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    
    std::vector<std::string> argStrings(argc);
    std::vector<char*> argv(argc);
    
    for (int i = 0; i < argc; i++) {
        // Convert wide string to narrow string
        int size = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        argStrings[i].resize(size);
        WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &argStrings[i][0], size, NULL, NULL);
        argv[i] = &argStrings[i][0];
    }
    
    LocalFree(argvW);
    
    // Allocate console for debug output
#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif
    
    return main(argc, argv.data());
}
#endif
