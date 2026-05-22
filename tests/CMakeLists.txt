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
