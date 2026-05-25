include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY  https://github.com/google/googletest.git
    GIT_TAG         v1.14.0
)

# On Windows: prevent GTest from overriding MSVC runtime library settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_executable(test_assert test_assert.cxx)
target_link_libraries(test_assert PRIVATE Validate::vd GTest::gtest_main)

add_executable(test_models test_models.cxx)
target_link_libraries(test_models PRIVATE Validate::vd GTest::gtest_main)

add_executable(test_string_rules test_string_rules.cxx)
target_link_libraries(test_string_rules PRIVATE Validate::vd GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_assert)
gtest_discover_tests(test_models)
gtest_discover_tests(test_string_rules)

if(TARGET Validate::vd_qt_base)
    add_executable(test_qt_property test_qt_property.cxx)
    set_target_properties(test_qt_property PROPERTIES AUTOMOC ON)
    # Qt6::Core comes transitively through Validate::vd_qt_base — no need to link it directly.
    target_link_libraries(test_qt_property PRIVATE Validate::vd_qt_base GTest::gtest_main)
    # PRE_TEST: defer test discovery to ctest run time instead of MSBuild build time.
    # Without this, MSBuild would fail because Qt DLLs are not on PATH during the build step.
    gtest_discover_tests(test_qt_property DISCOVERY_MODE PRE_TEST)

    add_executable(test_qt_qstring_rules test_qt_qstring_rules.cxx)
    set_target_properties(test_qt_qstring_rules PROPERTIES AUTOMOC ON)
    target_link_libraries(test_qt_qstring_rules PRIVATE Validate::vd_qt_base GTest::gtest_main)
    gtest_discover_tests(test_qt_qstring_rules DISCOVERY_MODE PRE_TEST)
endif()
