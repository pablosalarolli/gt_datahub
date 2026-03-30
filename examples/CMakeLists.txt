# ==================================================================
# Exemplos
# ==================================================================

add_executable(example_minimal
               example_minimal.cpp)

target_link_libraries(example_minimal
                      PUBLIC gtfw::gt_datahub::gt_datahub)

gtfw_example_test_target_setup(TARGETS example_minimal)
