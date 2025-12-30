# Apps/IDE/CMakeLists.txt
# ULTRA IDE - CMake Build Configuration
# Version: 1.0.0
# Last Modified: 2025-12-28
# Author: UltraCanvas Framework / ULTRA IDE

cmake_minimum_required(VERSION 3.16)

# ============================================================================
# PROJECT DEFINITION
# ============================================================================

project(UltraIDE
    VERSION 1.0.0
    DESCRIPTION "ULTRA IDE - Integrated Development Environment for ULTRA OS"
    LANGUAGES CXX
)

# ============================================================================
# BUILD OPTIONS
# ============================================================================

option(ULTRAIDE_BUILD_TESTS      "Build unit tests"                    OFF)
option(ULTRAIDE_BUILD_DOCS       "Build documentation"                 OFF)
option(ULTRAIDE_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)
option(ULTRAIDE_USE_SYSTEM_JSON  "Use system-installed JSON library"   OFF)

# Compiler plugin options - enable/disable specific plugins
option(ULTRAIDE_PLUGIN_GCC       "Build GCC/G++ compiler plugin"       ON)
option(ULTRAIDE_PLUGIN_FREEPASCAL "Build FreePascal compiler plugin"   ON)
option(ULTRAIDE_PLUGIN_RUST      "Build Rust/Cargo compiler plugin"    ON)
option(ULTRAIDE_PLUGIN_PYTHON    "Build Python compiler plugin"        ON)
option(ULTRAIDE_PLUGIN_JAVA      "Build Java compiler plugin"          ON)
option(ULTRAIDE_PLUGIN_GO        "Build Go compiler plugin"            ON)
option(ULTRAIDE_PLUGIN_CSHARP    "Build C# compiler plugin"            ON)
option(ULTRAIDE_PLUGIN_FORTRAN   "Build Fortran compiler plugin"       ON)
option(ULTRAIDE_PLUGIN_LUA       "Build Lua compiler plugin"           ON)

# ============================================================================
# C++ STANDARD AND COMPILER SETTINGS
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Position independent code (required for shared libraries)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# COMPILER FLAGS
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Common flags for GCC and Clang
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    
    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
        add_compile_definitions(DEBUG _DEBUG ULTRAIDE_DEBUG=1)
    endif()
    
    # Release flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG)
    endif()
    
    # RelWithDebInfo flags
    if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(-O2 -g -DNDEBUG)
    endif()
    
    # Sanitizers
    if(ULTRAIDE_ENABLE_SANITIZERS)
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC flags
    add_compile_options(
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
    )
    
    # Debug flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(/Od /Zi)
        add_compile_definitions(DEBUG _DEBUG ULTRAIDE_DEBUG=1)
    endif()
    
    # Release flags
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /DNDEBUG)
    endif()
endif()

# ============================================================================
# FIND DEPENDENCIES
# ============================================================================

# Threading support
find_package(Threads REQUIRED)

# UltraCanvas Framework (parent project)
# Assuming UltraCanvas is built as part of parent project or installed
if(NOT TARGET UltraCanvas::Core)
    # Try to find installed UltraCanvas
    find_package(UltraCanvas QUIET)
    
    if(NOT UltraCanvas_FOUND)
        message(STATUS "UltraCanvas not found, will use relative paths")
        set(ULTRACANVAS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
        set(ULTRACANVAS_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build")
    endif()
endif()

# Platform-specific dependencies
if(UNIX AND NOT APPLE)
    # Linux
    find_package(X11 QUIET)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CAIRO cairo)
        pkg_check_modules(PANGO pango pangocairo)
    endif()
elseif(WIN32)
    # Windows - no additional dependencies needed
elseif(APPLE)
    # macOS
    find_library(COCOA_LIBRARY Cocoa)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics)
endif()

# ============================================================================
# SOURCE FILES
# ============================================================================

# Core IDE sources
set(ULTRAIDE_CORE_SOURCES
    UltraIDE.cpp
    main.cpp
)

set(ULTRAIDE_CORE_HEADERS
    UltraIDE.h
)

# Project management sources
set(ULTRAIDE_PROJECT_SOURCES
    Project/UCIDEProject.cpp
)

set(ULTRAIDE_PROJECT_HEADERS
    Project/UCIDEProject.h
)

# Build system sources
set(ULTRAIDE_BUILD_SOURCES
    Build/UCBuildManager.cpp
    Build/UCBuildOutput.cpp
)

set(ULTRAIDE_BUILD_HEADERS
    Build/IUCCompilerPlugin.h
    Build/UCBuildManager.h
    Build/UCBuildOutput.h
)

# Compiler plugin sources - conditionally included
set(ULTRAIDE_PLUGIN_SOURCES "")
set(ULTRAIDE_PLUGIN_HEADERS "")

if(ULTRAIDE_PLUGIN_GCC)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCGCCPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCGCCPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_GCC_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_FREEPASCAL)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCFreePascalPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCFreePascalPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_FREEPASCAL_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_RUST)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES 
        Build/Plugins/UCRustPlugin.cpp
        Build/Plugins/UCRustPlugin_Cargo.cpp
    )
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCRustPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_RUST_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_PYTHON)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCPythonPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCPythonPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_PYTHON_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_JAVA)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCJavaPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCJavaPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_JAVA_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_GO)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCGoPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCGoPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_GO_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_CSHARP)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCCSharpPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCCSharpPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_CSHARP_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_FORTRAN)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCFortranPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCFortranPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_FORTRAN_ENABLED)
endif()

if(ULTRAIDE_PLUGIN_LUA)
    list(APPEND ULTRAIDE_PLUGIN_SOURCES Build/Plugins/UCLuaPlugin.cpp)
    list(APPEND ULTRAIDE_PLUGIN_HEADERS Build/Plugins/UCLuaPlugin.h)
    add_compile_definitions(ULTRAIDE_PLUGIN_LUA_ENABLED)
endif()

# CMake integration sources (optional future)
set(ULTRAIDE_CMAKE_SOURCES
    # Build/CMakeIntegration/UCCMakeParser.cpp
)

set(ULTRAIDE_CMAKE_HEADERS
    # Build/CMakeIntegration/UCCMakeParser.h
)

# UI sources (Phase 4 - future)
set(ULTRAIDE_UI_SOURCES
    # UI/UCIDELayout.cpp
    # UI/UCIDEMenuBar.cpp
    # UI/UCIDEToolbar.cpp
    # UI/UCIDEStatusBar.cpp
    # UI/UCIDEOutputConsole.cpp
)

set(ULTRAIDE_UI_HEADERS
    # UI/UCIDELayout.h
    # UI/UCIDEMenuBar.h
    # UI/UCIDEToolbar.h
    # UI/UCIDEStatusBar.h
    # UI/UCIDEOutputConsole.h
)

# Editor sources (Phase 4 - future)
set(ULTRAIDE_EDITOR_SOURCES
    # Editor/UCIDEEditor.cpp
)

set(ULTRAIDE_EDITOR_HEADERS
    # Editor/UCIDEEditor.h
)

# ============================================================================
# COMBINE ALL SOURCES
# ============================================================================

set(ULTRAIDE_ALL_SOURCES
    ${ULTRAIDE_CORE_SOURCES}
    ${ULTRAIDE_PROJECT_SOURCES}
    ${ULTRAIDE_BUILD_SOURCES}
    ${ULTRAIDE_PLUGIN_SOURCES}
    ${ULTRAIDE_CMAKE_SOURCES}
    ${ULTRAIDE_UI_SOURCES}
    ${ULTRAIDE_EDITOR_SOURCES}
)

set(ULTRAIDE_ALL_HEADERS
    ${ULTRAIDE_CORE_HEADERS}
    ${ULTRAIDE_PROJECT_HEADERS}
    ${ULTRAIDE_BUILD_HEADERS}
    ${ULTRAIDE_PLUGIN_HEADERS}
    ${ULTRAIDE_CMAKE_HEADERS}
    ${ULTRAIDE_UI_HEADERS}
    ${ULTRAIDE_EDITOR_HEADERS}
)

# ============================================================================
# BUILD TARGETS
# ============================================================================

# Main executable
add_executable(UltraIDE
    ${ULTRAIDE_ALL_SOURCES}
    ${ULTRAIDE_ALL_HEADERS}
)

# Include directories
target_include_directories(UltraIDE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

# Add UltraCanvas include if not found as package
if(DEFINED ULTRACANVAS_INCLUDE_DIR)
    target_include_directories(UltraIDE PRIVATE ${ULTRACANVAS_INCLUDE_DIR})
endif()

# ============================================================================
# LINK LIBRARIES
# ============================================================================

target_link_libraries(UltraIDE PRIVATE
    Threads::Threads
)

# Link UltraCanvas if found
if(TARGET UltraCanvas::Core)
    target_link_libraries(UltraIDE PRIVATE UltraCanvas::Core)
elseif(DEFINED ULTRACANVAS_LIBRARY_DIR)
    target_link_directories(UltraIDE PRIVATE ${ULTRACANVAS_LIBRARY_DIR})
    target_link_libraries(UltraIDE PRIVATE ultracanvas)
endif()

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    # Linux
    if(X11_FOUND)
        target_link_libraries(UltraIDE PRIVATE ${X11_LIBRARIES})
        target_include_directories(UltraIDE PRIVATE ${X11_INCLUDE_DIR})
    endif()
    if(CAIRO_FOUND)
        target_link_libraries(UltraIDE PRIVATE ${CAIRO_LIBRARIES})
        target_include_directories(UltraIDE PRIVATE ${CAIRO_INCLUDE_DIRS})
    endif()
    if(PANGO_FOUND)
        target_link_libraries(UltraIDE PRIVATE ${PANGO_LIBRARIES})
        target_include_directories(UltraIDE PRIVATE ${PANGO_INCLUDE_DIRS})
    endif()
    target_link_libraries(UltraIDE PRIVATE dl)
    
elseif(WIN32)
    # Windows
    target_link_libraries(UltraIDE PRIVATE
        user32
        gdi32
        shell32
        ole32
        comdlg32
        advapi32
    )
    
    # Set Windows subsystem (GUI application)
    set_target_properties(UltraIDE PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
elseif(APPLE)
    # macOS
    target_link_libraries(UltraIDE PRIVATE
        ${COCOA_LIBRARY}
        ${COREGRAPHICS_LIBRARY}
    )
    
    # Set macOS bundle properties
    set_target_properties(UltraIDE PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "ULTRA IDE"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.ultracanvas.ultraide"
    )
endif()

# ============================================================================
# STATIC LIBRARY TARGET (for embedding/testing)
# ============================================================================

add_library(UltraIDELib STATIC
    ${ULTRAIDE_PROJECT_SOURCES}
    ${ULTRAIDE_BUILD_SOURCES}
    ${ULTRAIDE_PLUGIN_SOURCES}
    ${ULTRAIDE_ALL_HEADERS}
)

target_include_directories(UltraIDELib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(DEFINED ULTRACANVAS_INCLUDE_DIR)
    target_include_directories(UltraIDELib PUBLIC ${ULTRACANVAS_INCLUDE_DIR})
endif()

target_link_libraries(UltraIDELib PUBLIC
    Threads::Threads
)

# ============================================================================
# INSTALLATION
# ============================================================================

include(GNUInstallDirs)

# Install executable
install(TARGETS UltraIDE
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install library
install(TARGETS UltraIDELib
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install headers
install(FILES ${ULTRAIDE_ALL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/UltraIDE
)

# Install resources
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
    install(DIRECTORY Resources/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/UltraIDE
        FILES_MATCHING 
            PATTERN "*.png"
            PATTERN "*.svg"
            PATTERN "*.ico"
            PATTERN "*.theme"
    )
endif()

# ============================================================================
# TESTING
# ============================================================================

if(ULTRAIDE_BUILD_TESTS)
    enable_testing()
    
    # Find or fetch Google Test
    find_package(GTest QUIET)
    
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
        )
        FetchContent_MakeAvailable(googletest)
    endif()
    
    # Test executable
    add_executable(UltraIDETests
        Tests/test_main.cpp
        Tests/test_project.cpp
        Tests/test_build_manager.cpp
        Tests/test_plugins.cpp
    )
    
    target_link_libraries(UltraIDETests PRIVATE
        UltraIDELib
        GTest::gtest
        GTest::gtest_main
    )
    
    target_include_directories(UltraIDETests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Register tests
    include(GoogleTest)
    gtest_discover_tests(UltraIDETests)
endif()

# ============================================================================
# DOCUMENTATION
# ============================================================================

if(ULTRAIDE_BUILD_DOCS)
    find_package(Doxygen QUIET)
    
    if(Doxygen_FOUND)
        set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
        set(DOXYGEN_PROJECT_NAME "ULTRA IDE")
        set(DOXYGEN_PROJECT_BRIEF "Integrated Development Environment for ULTRA OS")
        set(DOXYGEN_EXTRACT_ALL YES)
        set(DOXYGEN_EXTRACT_PRIVATE YES)
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_LATEX NO)
        
        doxygen_add_docs(docs
            ${ULTRAIDE_ALL_HEADERS}
            COMMENT "Generating API documentation with Doxygen"
        )
    else()
        message(STATUS "Doxygen not found, documentation will not be built")
    endif()
endif()

# ============================================================================
# PACKAGING
# ============================================================================

set(CPACK_PACKAGE_NAME "UltraIDE")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "UltraCanvas Framework")
set(CPACK_PACKAGE_CONTACT "stefan@ultracanvas.dev")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "ULTRA IDE")
    set(CPACK_NSIS_PACKAGE_NAME "ULTRA IDE")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "ULTRA IDE")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libx11-6")
    set(CPACK_RPM_PACKAGE_REQUIRES "glibc, libstdc++, libX11")
endif()

include(CPack)

# ============================================================================
# SUMMARY
# ============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "  ULTRA IDE Build Configuration")
message(STATUS "========================================")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "  Plugins Enabled:")
message(STATUS "    GCC/G++:      ${ULTRAIDE_PLUGIN_GCC}")
message(STATUS "    FreePascal:   ${ULTRAIDE_PLUGIN_FREEPASCAL}")
message(STATUS "    Rust:         ${ULTRAIDE_PLUGIN_RUST}")
message(STATUS "    Python:       ${ULTRAIDE_PLUGIN_PYTHON}")
message(STATUS "    Java:         ${ULTRAIDE_PLUGIN_JAVA}")
message(STATUS "    Go:           ${ULTRAIDE_PLUGIN_GO}")
message(STATUS "    C#:           ${ULTRAIDE_PLUGIN_CSHARP}")
message(STATUS "    Fortran:      ${ULTRAIDE_PLUGIN_FORTRAN}")
message(STATUS "    Lua:          ${ULTRAIDE_PLUGIN_LUA}")
message(STATUS "")
message(STATUS "  Options:")
message(STATUS "    Build Tests:  ${ULTRAIDE_BUILD_TESTS}")
message(STATUS "    Build Docs:   ${ULTRAIDE_BUILD_DOCS}")
message(STATUS "    Sanitizers:   ${ULTRAIDE_ENABLE_SANITIZERS}")
message(STATUS "========================================")
message(STATUS "")
