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
vd_use_highest_cxx_standard(test_assert)

add_executable(test_models test_models.cxx)
target_link_libraries(test_models PRIVATE Validate::vd GTest::gtest_main)
vd_use_highest_cxx_standard(test_models)

add_executable(test_string_rules test_string_rules.cxx)
target_link_libraries(test_string_rules PRIVATE Validate::vd GTest::gtest_main)
vd_use_highest_cxx_standard(test_string_rules)

add_executable(test_integration test_integration.cxx)
target_link_libraries(test_integration PRIVATE Validate::vd GTest::gtest_main)
vd_use_highest_cxx_standard(test_integration)

add_executable(test_not_null test_not_null.cxx)
target_link_libraries(test_not_null PRIVATE Validate::vd GTest::gtest_main)
vd_use_highest_cxx_standard(test_not_null)

add_executable(test_monadic_rules test_monadic_rules.cxx)
target_link_libraries(test_monadic_rules PRIVATE Validate::vd GTest::gtest_main)
vd_use_highest_cxx_standard(test_monadic_rules)

add_executable(test_static_model test_static_model.cxx)
target_link_libraries(test_static_model PRIVATE Validate::vd GTest::gtest_main)
vd_use_highest_cxx_standard(test_static_model)

include(GoogleTest)
# PRE_TEST: defer test discovery to ctest run time.
# When Qt extensions are enabled, vd.lib carries a Qt6Core dependency so the
# test binaries need Qt DLLs on PATH — which is not guaranteed during MSBuild.
gtest_discover_tests(test_assert        DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_models        DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_string_rules  DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_integration   DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_not_null      DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_static_model  DISCOVERY_MODE PRE_TEST)
gtest_discover_tests(test_monadic_rules DISCOVERY_MODE PRE_TEST)

if(VD_EXTENSION_QT_BASE)
    add_executable(test_qt_property test_qt_property.cxx)
    set_target_properties(test_qt_property PROPERTIES AUTOMOC ON)
    target_link_libraries(test_qt_property PRIVATE Validate::vd GTest::gtest_main)
    vd_use_highest_cxx_standard(test_qt_property)
    # PRE_TEST: defer test discovery to ctest run time instead of MSBuild build time.
    # Without this, MSBuild would fail because Qt DLLs are not on PATH during the build step.
    gtest_discover_tests(test_qt_property DISCOVERY_MODE PRE_TEST)

    add_executable(test_qt_qstring_rules test_qt_qstring_rules.cxx)
    set_target_properties(test_qt_qstring_rules PROPERTIES AUTOMOC ON)
    target_link_libraries(test_qt_qstring_rules PRIVATE Validate::vd GTest::gtest_main)
    vd_use_highest_cxx_standard(test_qt_qstring_rules)
    gtest_discover_tests(test_qt_qstring_rules DISCOVERY_MODE PRE_TEST)

    add_executable(test_qt_integration test_qt_integration.cxx)
    set_target_properties(test_qt_integration PROPERTIES AUTOMOC ON)
    target_link_libraries(test_qt_integration PRIVATE Validate::vd GTest::gtest_main)
    vd_use_highest_cxx_standard(test_qt_integration)
    gtest_discover_tests(test_qt_integration DISCOVERY_MODE PRE_TEST)
endif()
