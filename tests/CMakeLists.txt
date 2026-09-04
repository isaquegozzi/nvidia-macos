# Smoke tests Fase 0 — sem framework externo, sem acesso a hardware.
# Cada .cpp vira um executável CTest mínimo (return != 0 = falha).

add_executable(test_common_smoke test_common_smoke.cpp)
target_link_libraries(test_common_smoke PRIVATE nvidia_common)

add_test(NAME common_smoke COMMAND test_common_smoke)

add_executable(test_compare_logic test_compare_logic.cpp
  ../linux/ga106-lab/src/json_flat.cpp
  ../linux/ga106-lab/src/baseline_compare.cpp
)
target_include_directories(test_compare_logic PRIVATE
  ../linux/ga106-lab/src
)

add_test(NAME compare_logic COMMAND test_compare_logic)

add_executable(test_chip_id test_chip_id.cpp)
target_link_libraries(test_chip_id PRIVATE nvidia_common)

add_test(NAME chip_id COMMAND test_chip_id)
