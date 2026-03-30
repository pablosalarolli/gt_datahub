# ==================================================================
# Instala o gtest
# ==================================================================

# configure build of googletest

find_package(GTest CONFIG REQUIRED)

# Mantem a arvore de testes espelhando `include/gt_datahub`, com excecao do
# entrypoint da suite, que permanece em `test/main_test.cpp`.
add_executable(unit_tests
               main_test.cpp
)

target_include_directories(unit_tests BEFORE
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes
)

target_link_libraries(unit_tests 
        PUBLIC gtfw::gt_datahub::gt_datahub
        PRIVATE GTest::gtest
)

gtfw_example_test_target_setup(TARGETS unit_tests)

include(GoogleTest)
gtest_discover_tests(unit_tests)
