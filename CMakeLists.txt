cmake_minimum_required(VERSION 3.23)

project(Validate
    VERSION 0.1.0
    DESCRIPTION "C++20 validation and assertion library"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler version guards ────────────────────────────────────────────────────
# Required for C++20 <format> and <source_location>
if(MSVC)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.29")
        message(FATAL_ERROR
            "MSVC 19.29 (Visual Studio 2019 16.10) or later is required.\n"
            "Current version: ${CMAKE_CXX_COMPILER_VERSION}"
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
        message(FATAL_ERROR
            "GCC 13 or later is required for <format> support.\n"
            "Current version: ${CMAKE_CXX_COMPILER_VERSION}"
        )
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
        message(FATAL_ERROR
            "Clang 14 or later is required for <format> support.\n"
            "Current version: ${CMAKE_CXX_COMPILER_VERSION}"
        )
    endif()
endif()

# ── Static library (all compiled code lives here) ─────────────────────────────
add_library(vd STATIC
    src/assert/vd_assert.cxx
    src/models/vd_string_rules.cxx
)
add_library(Validate::vd ALIAS vd)

target_include_directories(vd PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(vd PUBLIC cxx_std_20)

# ── Dependencies (may add Qt sources / link libs to vd) ───────────────────────
add_subdirectory(deps)

# ── Tests (only when this is the top-level project) ───────────────────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    add_subdirectory(tests)
endif()

# ── Install & packaging (skipped when consumed via FetchContent) ───────────────
if(NOT CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    return()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS     vd
    EXPORT      ValidateTargets
    ARCHIVE     DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(
    DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/src/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hxx"
)

install(
    EXPORT      ValidateTargets
    FILE        ValidateTargets.cmake
    NAMESPACE   Validate::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Validate"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ValidateConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ValidateConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Validate"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ValidateConfigVersion.cmake"
    VERSION         ${PROJECT_VERSION}
    COMPATIBILITY   SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ValidateConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ValidateConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/Validate"
)
