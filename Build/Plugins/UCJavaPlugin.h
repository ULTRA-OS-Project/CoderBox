// Apps/IDE/Build/Plugins/UCJavaPlugin.h
// Java Compiler Plugin for ULTRA IDE (JDK/OpenJDK)
// Version: 1.0.0
// Last Modified: 2025-12-27
// Author: UltraCanvas Framework / ULTRA IDE
#pragma once

#include "../IUCCompilerPlugin.h"
#include "../UCBuildOutput.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>
#include <functional>

namespace UltraCanvas {
namespace IDE {

// ============================================================================
// JAVA VERSION / JDK TYPE
// ============================================================================

/**
 * @brief JDK distribution type
 */
enum class JDKType {
    OracleJDK,          // Oracle JDK
    OpenJDK,            // OpenJDK (various vendors)
    AdoptOpenJDK,       // Eclipse Adoptium (formerly AdoptOpenJDK)
    AmazonCorretto,     // Amazon Corretto
    Azul,               // Azul Zulu
    GraalVM,            // GraalVM (with native-image)
    IBMJDK,             // IBM Semeru
    Microsoft,          // Microsoft Build of OpenJDK
    Unknown
};

/**
 * @brief Java language level
 */
enum class JavaVersion {
    Java8  = 8,         // 1.8 (LTS)
    Java11 = 11,        // LTS
    Java17 = 17,        // LTS
    Java21 = 21,        // LTS (current)
    Java22 = 22,        // Latest
    Java23 = 23,        // Preview
    Unknown = 0
};

// ============================================================================
// JAVA PLUGIN CONFIGURATION
// ============================================================================

/**
 * @brief Java-specific configuration options
 */
struct JavaPluginConfig {
    // JDK paths
    std::string javaHome;               // JAVA_HOME
    std::string javacPath;              // Path to javac
    std::string javaPath;               // Path to java
    std::string jarPath;                // Path to jar
    std::string javadocPath;            // Path to javadoc
    std::string jdepsPath;              // Path to jdeps
    
    // Compilation options
    JavaVersion sourceVersion = JavaVersion::Java21;    // -source
    JavaVersion targetVersion = JavaVersion::Java21;    // -target
    bool enablePreview = false;         // --enable-preview
    std::string encoding = "UTF-8";     // -encoding
    
    // Warnings
    bool showAllWarnings = true;        // -Xlint:all
    bool showDeprecation = true;        // -deprecation
    bool showUnchecked = true;          // -Xlint:unchecked
    bool warningsAsErrors = false;      // -Werror
    std::vector<std::string> lintOptions; // Specific -Xlint options
    
    // Debug
    bool debugInfo = true;              // -g (generate debug info)
    bool debugLines = true;             // -g:lines
    bool debugVars = true;              // -g:vars
    bool debugSource = true;            // -g:source
    
    // Output
    std::string outputDirectory;        // -d (class output directory)
    std::string sourceOutputDir;        // -s (generated source output)
    std::string headerOutputDir;        // -h (native header output)
    
    // Classpath
    std::vector<std::string> classpath; // -cp / -classpath
    std::vector<std::string> modulePath; // --module-path
    std::string bootClasspath;          // -bootclasspath (legacy)
    
    // Modules (Java 9+)
    std::string moduleName;             // --module
    std::vector<std::string> addModules; // --add-modules
    std::vector<std::string> addExports; // --add-exports
    std::vector<std::string> addOpens;   // --add-opens
    std::vector<std::string> addReads;   // --add-reads
    
    // Annotation processing
    bool processAnnotations = true;     // Enable annotation processing
    std::vector<std::string> annotationProcessors; // -processor
    std::string processorPath;          // -processorpath
    std::map<std::string, std::string> processorOptions; // -A options
    
    // Runtime options (for java command)
    int heapSizeInitialMB = 256;        // -Xms
    int heapSizeMaxMB = 1024;           // -Xmx
    int stackSizeMB = 1;                // -Xss
    bool enableAssertions = false;      // -ea
    bool enableSystemAssertions = false; // -esa
    std::vector<std::string> systemProperties; // -D properties
    std::vector<std::string> jvmArgs;   // Additional JVM arguments
    
    // Verbose output
    bool verbose = false;               // -verbose
    bool verboseClass = false;          // -verbose:class
    bool verboseGC = false;             // -verbose:gc
    bool verboseJNI = false;            // -verbose:jni
    
    /**
     * @brief Get default configuration
     */
    static JavaPluginConfig Default();
};

// ============================================================================
// JAVA ANALYSIS STRUCTURES
// ============================================================================

/**
 * @brief Java class information
 */
struct JavaClassInfo {
    std::string name;                   // Simple class name
    std::string fullName;               // Fully qualified name
    std::string packageName;            // Package
    std::string sourceFile;             // Source .java file
    std::string classFile;              // Compiled .class file
    
    bool isPublic = false;
    bool isAbstract = false;
    bool isFinal = false;
    bool isInterface = false;
    bool isEnum = false;
    bool isAnnotation = false;
    bool isRecord = false;              // Java 16+
    bool isSealed = false;              // Java 17+
    
    std::string superClass;
    std::vector<std::string> interfaces;
    std::vector<std::string> innerClasses;
    std::vector<std::string> permits;   // Permitted subclasses (sealed)
};

/**
 * @brief Java method information
 */
struct JavaMethodInfo {
    std::string name;
    std::string returnType;
    std::vector<std::string> parameters;
    std::vector<std::string> exceptions;
    std::string className;
    int startLine = 0;
    int endLine = 0;
    
    bool isPublic = false;
    bool isPrivate = false;
    bool isProtected = false;
    bool isStatic = false;
    bool isFinal = false;
    bool isAbstract = false;
    bool isSynchronized = false;
    bool isNative = false;
    bool isDefault = false;             // Interface default method
};

/**
 * @brief Java field information
 */
struct JavaFieldInfo {
    std::string name;
    std::string type;
    std::string className;
    int line = 0;
    
    bool isPublic = false;
    bool isPrivate = false;
    bool isProtected = false;
    bool isStatic = false;
    bool isFinal = false;
    bool isVolatile = false;
    bool isTransient = false;
};

/**
 * @brief Java package information
 */
struct JavaPackageInfo {
    std::string name;
    std::string sourceRoot;
    std::vector<std::string> classes;
    std::vector<std::string> subpackages;
};

/**
 * @brief Java module information (Java 9+)
 */
struct JavaModuleInfo {
    std::string name;
    std::string version;
    std::vector<std::string> requires;
    std::vector<std::string> requiresTransitive;
    std::vector<std::string> exports;
    std::vector<std::string> opens;
    std::vector<std::string> uses;
    std::vector<std::string> provides;
    bool isOpen = false;
};

/**
 * @brief JAR file information
 */
struct JarInfo {
    std::string path;
    std::string mainClass;
    std::string manifestVersion;
    std::vector<std::string> classEntries;
    std::vector<std::string> resourceEntries;
    size_t fileSize = 0;
    bool isMultiRelease = false;        // Multi-release JAR
    bool isModular = false;             // Contains module-info
};

/**
 * @brief Java dependency information
 */
struct JavaDependency {
    std::string className;
    std::string dependsOn;
    std::string type;                   // "class", "interface", "annotation"
    bool isRequired = true;
};

// ============================================================================
// JAVA COMPILER PLUGIN
// ============================================================================

/**
 * @brief Java Compiler Plugin Implementation
 * 
 * Supports:
 * - JDK 8-23 (all LTS and recent versions)
 * - Multiple JDK distributions (Oracle, OpenJDK, GraalVM, etc.)
 * - Compilation, execution, packaging
 * - Module system (Java 9+)
 * - Annotation processing
 * - Javadoc generation
 * - JAR creation and management
 * - Dependency analysis (jdeps)
 * - JUnit test execution
 * 
 * Error Format Parsed:
 *   MyClass.java:10: error: ';' expected
 *       int x = 5
 *               ^
 */
class UCJavaPlugin : public IUCCompilerPlugin {
public:
    /**
     * @brief Constructor
     */
    UCJavaPlugin();
    
    /**
     * @brief Constructor with configuration
     */
    explicit UCJavaPlugin(const JavaPluginConfig& config);
    
    ~UCJavaPlugin() override;
    
    // ===== PLUGIN IDENTIFICATION =====
    
    std::string GetPluginName() const override;
    std::string GetPluginVersion() const override;
    CompilerType GetCompilerType() const override;
    std::string GetCompilerName() const override;
    
    // ===== COMPILER DETECTION =====
    
    bool IsAvailable() override;
    std::string GetCompilerPath() override;
    void SetCompilerPath(const std::string& path) override;
    std::string GetCompilerVersion() override;
    std::vector<std::string> GetSupportedExtensions() const override;
    bool CanCompile(const std::string& filePath) const override;
    
    // ===== COMPILATION =====
    
    void CompileAsync(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config,
        std::function<void(const BuildResult&)> onComplete,
        std::function<void(const std::string&)> onOutputLine,
        std::function<void(float)> onProgress
    ) override;
    
    BuildResult CompileSync(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) override;
    
    // ===== BUILD CONTROL =====
    
    void Cancel() override;
    bool IsBuildInProgress() const override;
    
    // ===== OUTPUT PARSING =====
    
    CompilerMessage ParseOutputLine(const std::string& line) override;
    
    // ===== COMMAND LINE GENERATION =====
    
    std::vector<std::string> GenerateCommandLine(
        const std::vector<std::string>& sourceFiles,
        const BuildConfiguration& config
    ) override;
    
    // ===== JAVA-SPECIFIC METHODS =====
    
    /**
     * @brief Get/set plugin configuration
     */
    const JavaPluginConfig& GetConfig() const { return pluginConfig; }
    void SetConfig(const JavaPluginConfig& config);
    
    /**
     * @brief Detect JDK installation
     */
    bool DetectInstallation();
    
    /**
     * @brief Get JDK type
     */
    JDKType GetJDKType() const { return detectedJDKType; }
    
    /**
     * @brief Get Java version
     */
    JavaVersion GetJavaVersion() const { return detectedVersion; }
    
    /**
     * @brief List installed JDKs
     */
    std::vector<std::string> ListInstalledJDKs();
    
    /**
     * @brief Switch to different JDK
     */
    bool SetJavaHome(const std::string& javaHome);
    
    // ===== EXECUTION =====
    
    /**
     * @brief Run a Java class
     */
    BuildResult RunClass(const std::string& className,
                         const std::vector<std::string>& args = {},
                         std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run a JAR file
     */
    BuildResult RunJar(const std::string& jarFile,
                       const std::vector<std::string>& args = {},
                       std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run a Java module
     */
    BuildResult RunModule(const std::string& moduleName,
                          const std::string& mainClass,
                          const std::vector<std::string>& args = {},
                          std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run with custom JVM options
     */
    BuildResult RunWithOptions(const std::string& className,
                               const std::vector<std::string>& jvmArgs,
                               const std::vector<std::string>& programArgs,
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== JAR OPERATIONS =====
    
    /**
     * @brief Create a JAR file
     */
    bool CreateJar(const std::string& jarFile,
                   const std::string& classesDir,
                   const std::string& mainClass = "",
                   std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Create a JAR with manifest
     */
    bool CreateJarWithManifest(const std::string& jarFile,
                               const std::string& classesDir,
                               const std::string& manifestFile,
                               std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Update a JAR file
     */
    bool UpdateJar(const std::string& jarFile,
                   const std::vector<std::string>& files,
                   std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Extract a JAR file
     */
    bool ExtractJar(const std::string& jarFile,
                    const std::string& outputDir,
                    std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief List JAR contents
     */
    std::vector<std::string> ListJarContents(const std::string& jarFile);
    
    /**
     * @brief Get JAR info
     */
    JarInfo GetJarInfo(const std::string& jarFile);
    
    // ===== JAVADOC =====
    
    /**
     * @brief Generate Javadoc
     */
    BuildResult GenerateJavadoc(const std::vector<std::string>& sourceFiles,
                                const std::string& outputDir,
                                std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Generate Javadoc for package
     */
    BuildResult GenerateJavadocForPackage(const std::string& packageName,
                                          const std::string& sourceDir,
                                          const std::string& outputDir,
                                          std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== MODULE SYSTEM (Java 9+) =====
    
    /**
     * @brief Compile module
     */
    BuildResult CompileModule(const std::string& moduleSourceDir,
                              const std::string& outputDir,
                              std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Get module info
     */
    JavaModuleInfo GetModuleInfo(const std::string& moduleInfoPath);
    
    /**
     * @brief Create modular JAR
     */
    bool CreateModularJar(const std::string& jarFile,
                          const std::string& classesDir,
                          const std::string& mainClass,
                          const std::string& moduleVersion = "",
                          std::function<void(const std::string&)> onOutput = nullptr);
    
    // ===== SOURCE ANALYSIS =====
    
    /**
     * @brief Parse class information from source
     */
    JavaClassInfo ParseClassInfo(const std::string& sourceFile);
    
    /**
     * @brief Scan for classes in directory
     */
    std::vector<JavaClassInfo> ScanClasses(const std::string& sourceDir);
    
    /**
     * @brief Get methods in a class
     */
    std::vector<JavaMethodInfo> GetMethods(const std::string& sourceFile);
    
    /**
     * @brief Get fields in a class
     */
    std::vector<JavaFieldInfo> GetFields(const std::string& sourceFile);
    
    /**
     * @brief Get imports from source file
     */
    std::vector<std::string> GetImports(const std::string& sourceFile);
    
    /**
     * @brief Get package from source file
     */
    std::string GetPackage(const std::string& sourceFile);
    
    // ===== DEPENDENCY ANALYSIS =====
    
    /**
     * @brief Analyze dependencies (jdeps)
     */
    std::vector<JavaDependency> AnalyzeDependencies(const std::string& classOrJar);
    
    /**
     * @brief Get module dependencies
     */
    std::vector<std::string> GetModuleDependencies(const std::string& classOrJar);
    
    /**
     * @brief Generate module-info from classpath
     */
    std::string GenerateModuleInfo(const std::string& jarFile);
    
    // ===== TESTING =====
    
    /**
     * @brief Run JUnit tests
     */
    BuildResult RunJUnitTests(const std::string& testClass,
                              std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Run all tests in directory
     */
    BuildResult RunAllTests(const std::string& testDir,
                            std::function<void(const std::string&)> onOutput = nullptr);
    
    /**
     * @brief Find test classes
     */
    std::vector<std::string> FindTestClasses(const std::string& sourceDir);
    
    // ===== CALLBACKS =====
    
    std::function<void(const JavaClassInfo&)> onClassCompiled;
    std::function<void(const std::string&, float)> onFileProgress;
    std::function<void(const std::string&)> onRuntimeOutput;
    std::function<void(const std::string&)> onRuntimeError;

private:
    // ===== INTERNAL METHODS =====
    
    std::string GetJavacExecutable() const;
    std::string GetJavaExecutable() const;
    std::string GetJarExecutable() const;
    std::string GetJavadocExecutable() const;
    std::string GetJdepsExecutable() const;
    
    bool DetectJDKType(const std::string& versionOutput);
    JavaVersion ParseJavaVersion(const std::string& versionOutput);
    
    std::vector<std::string> GetClasspathArgs() const;
    std::vector<std::string> GetModuleArgs() const;
    std::vector<std::string> GetWarningArgs() const;
    std::vector<std::string> GetDebugArgs() const;
    std::vector<std::string> GetJVMArgs() const;
    
    std::string BuildClasspath() const;
    std::string BuildModulePath() const;
    
    void ParseClassFromSource(const std::string& source,
                              const std::string& file,
                              JavaClassInfo& info);
    
    bool ExecuteCommand(const std::vector<std::string>& args,
                        std::string& output,
                        std::string& error,
                        int& exitCode,
                        const std::string& workingDir = "");
    
    // ===== STATE =====
    
    JavaPluginConfig pluginConfig;
    
    JDKType detectedJDKType = JDKType::Unknown;
    JavaVersion detectedVersion = JavaVersion::Unknown;
    std::string detectedVersionString;
    
    std::atomic<bool> buildInProgress{false};
    std::atomic<bool> cancelRequested{false};
    
    std::thread buildThread;
    void* currentProcess = nullptr;
    
    // Cached data
    bool installationDetected = false;
    std::map<std::string, JavaClassInfo> classCache;
    
    // Multi-line error parsing
    std::string pendingFile;
    int pendingLine = 0;
    CompilerMessageType pendingType = CompilerMessageType::Info;
    std::string pendingMessage;
    bool inErrorBlock = false;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

inline std::string JavaVersionToString(JavaVersion ver) {
    switch (ver) {
        case JavaVersion::Java8:  return "1.8";
        case JavaVersion::Java11: return "11";
        case JavaVersion::Java17: return "17";
        case JavaVersion::Java21: return "21";
        case JavaVersion::Java22: return "22";
        case JavaVersion::Java23: return "23";
        default: return "unknown";
    }
}

inline std::string JavaVersionToSourceString(JavaVersion ver) {
    int v = static_cast<int>(ver);
    return (v <= 8) ? "1." + std::to_string(v) : std::to_string(v);
}

inline std::string JDKTypeToString(JDKType type) {
    switch (type) {
        case JDKType::OracleJDK:      return "Oracle JDK";
        case JDKType::OpenJDK:        return "OpenJDK";
        case JDKType::AdoptOpenJDK:   return "Eclipse Adoptium";
        case JDKType::AmazonCorretto: return "Amazon Corretto";
        case JDKType::Azul:           return "Azul Zulu";
        case JDKType::GraalVM:        return "GraalVM";
        case JDKType::IBMJDK:         return "IBM Semeru";
        case JDKType::Microsoft:      return "Microsoft OpenJDK";
        default: return "Unknown JDK";
    }
}

} // namespace IDE
} // namespace UltraCanvas
