cmake_minimum_required(VERSION 3.23)

if(MSVC)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "Примечание: включение файла: ") 
endif()

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

# ── C++ standard selection ─────────────────────────────────────────────────────
# vd's own sources are compiled with the newest C++ standard the active compiler
# understands, so library-internal code can opportunistically use newer standard
# library features (e.g. the std::expected-backed monadic rules gated behind
# __cpp_lib_expected in vd_monadic_rules.hxx). This is a private per-target
# property (set via vd_use_highest_cxx_standard below), not a usage requirement --
# it never forces a higher standard onto downstream consumers. Consumers only
# need to satisfy the PUBLIC cxx_std_20 floor set on the vd target further down;
# a consumer building at C++23+ still gets __cpp_lib_expected etc. defined in
# their own translation units automatically, since the headers are header-only.
set(VD_CXX_STANDARD 20)
foreach(_vd_candidate_std 26 23 20)
    if("cxx_std_${_vd_candidate_std}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
        set(VD_CXX_STANDARD ${_vd_candidate_std})
        break()
    endif()
endforeach()
unset(_vd_candidate_std)

message(STATUS "Validate: building library/test sources as C++${VD_CXX_STANDARD} (consumers require >= C++20)")

function(vd_use_highest_cxx_standard target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD            ${VD_CXX_STANDARD}
        CXX_STANDARD_REQUIRED   ON
        CXX_EXTENSIONS          OFF
    )
endfunction()

# ── Static library (all compiled code lives here) ─────────────────────────────
add_library(vd STATIC
    src/assert/vd_assert.cxx
    src/models/vd_string_rules.cxx
    src/core/vd_result.cxx
)
add_library(Validate::vd ALIAS vd)
vd_use_highest_cxx_standard(vd)

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
