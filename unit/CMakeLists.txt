find_package(GTest REQUIRED)

add_executable(kv_storage_tests
    kv_storage_test.cpp
)

target_link_libraries(kv_storage_tests
    ${KV_STORAGE_LIB}
    GTest::gtest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(kv_storage_tests)