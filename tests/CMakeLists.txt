# Smoke tests Fase 0 — sem framework externo, sem acesso a hardware.
# Cada .cpp vira um executável CTest mínimo (return != 0 = falha).

add_executable(test_common_smoke test_common_smoke.cpp)
target_link_libraries(test_common_smoke PRIVATE nvidia_common)

add_test(NAME common_smoke COMMAND test_common_smoke)
