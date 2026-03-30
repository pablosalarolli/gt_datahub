# ==================================================================
# Instala o gtest
# ==================================================================

# configure build of googletest

find_package(GTest CONFIG REQUIRED)

# Mantem a arvore de testes espelhando `include/gt_datahub`, com excecao do
# entrypoint da suite, que permanece em `test/main_test.cpp`.
add_executable(unit_tests
               main_test.cpp
               core/compiled_catalog_test.cpp
               core/state_store_test.cpp
               core/state_store_quality_test.cpp
               gt_datahub/public_types_test.cpp
               gt_datahub/data_type_header_test.cpp
               gt_datahub/quality_header_test.cpp
               gt_datahub/variable_role_header_test.cpp
               gt_datahub/timestamp_header_test.cpp
               gt_datahub/errors_header_test.cpp
               gt_datahub/value_test.cpp
               gt_datahub/variable_definition_test.cpp
               gt_datahub/variable_state_test.cpp
               gt_datahub/update_request_test.cpp
               gt_datahub/public_model_smoke_test.cpp
               gt_datahub/value_header_test.cpp
               gt_datahub/variable_definition_header_test.cpp
               gt_datahub/variable_state_header_test.cpp
               gt_datahub/update_request_header_test.cpp
               gt_datahub/i_datahub_header_test.cpp
               gt_datahub/i_internal_producer_header_test.cpp
               gt_datahub/i_datahub_runtime_header_test.cpp
               gt_datahub/interface_mocks_test.cpp
               gt_datahub/public_api_smoke_test.cpp
)

target_include_directories(unit_tests BEFORE
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fakes
                ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_tests 
        PUBLIC gtfw::gt_datahub::gt_datahub
        PRIVATE GTest::gtest
)

gtfw_example_test_target_setup(TARGETS unit_tests)

include(GoogleTest)
gtest_discover_tests(unit_tests)
