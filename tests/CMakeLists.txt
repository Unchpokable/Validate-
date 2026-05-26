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
# PRE_TEST: defer test discovery to ctest run time.
# When Qt extensions are enabled, vd.lib carries a Qt6Core dependency so the
# test binaries need Qt DLLs on PATH — which is not guaranteed during MSBuild.
gtest_discover_tests(test_assert       DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_models       DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_string_rules DISCOVERY_MODE PRE_TEST)

if(VD_EXTENSION_QT_BASE)
    add_executable(test_qt_property test_qt_property.cxx)
    set_target_properties(test_qt_property PROPERTIES AUTOMOC ON)
    target_link_libraries(test_qt_property PRIVATE Validate::vd GTest::gtest_main)
    # PRE_TEST: defer test discovery to ctest run time instead of MSBuild build time.
    # Without this, MSBuild would fail because Qt DLLs are not on PATH during the build step.
    gtest_discover_tests(test_qt_property DISCOVERY_MODE PRE_TEST)

    add_executable(test_qt_qstring_rules test_qt_qstring_rules.cxx)
    set_target_properties(test_qt_qstring_rules PROPERTIES AUTOMOC ON)
    target_link_libraries(test_qt_qstring_rules PRIVATE Validate::vd GTest::gtest_main)
    gtest_discover_tests(test_qt_qstring_rules DISCOVERY_MODE PRE_TEST)
endif()
