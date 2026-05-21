cmake_minimum_required(VERSION 3.21)

project(Validate
    VERSION 0.1.0
    DESCRIPTION "C++20 header-only validation and assertion library"
    LANGUAGES CXX
)

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

# ── Library target (header-only INTERFACE) ─────────────────────────────────────
add_library(vd INTERFACE)
add_library(Validate::vd ALIAS vd)

target_include_directories(vd INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(vd INTERFACE cxx_std_20)

# ── Dependencies ───────────────────────────────────────────────────────────────
add_subdirectory(deps)

# ── Tests (only when this is the top-level project) ───────────────────────────
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
    add_subdirectory(tests)
endif()

# ── Install & packaging ────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS vd EXPORT ValidateTargets)

foreach(_ext qt_base qt_widgets qt_qml)
    if(TARGET vd_${_ext})
        install(TARGETS vd_${_ext} EXPORT ValidateTargets)
    endif()
endforeach()

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
