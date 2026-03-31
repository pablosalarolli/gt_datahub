# ==================================================================
# Exemplos
# ==================================================================

add_executable(example_minimal
               example_minimal.cpp)

target_include_directories(example_minimal
                           PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(example_minimal
                      PUBLIC gtfw::gt_datahub::gt_datahub)

add_executable(example_interactive
               example_interactive.cpp)

target_include_directories(example_interactive
                           PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_link_libraries(example_interactive
                      PUBLIC gtfw::gt_datahub::gt_datahub)

gtfw_example_test_target_setup(TARGETS example_minimal example_interactive)
