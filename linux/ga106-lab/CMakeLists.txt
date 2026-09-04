# ga106-lab — Fase 0, SOMENTE LEITURA.
# Target de observacao passiva: nenhum fonte aqui pode alterar estado do
# hardware. Auditoria: este target so abre sysfs/proc com leitura e nunca
# cria recurso de escrita (sem targets extras, sem definicoes de escrita).

add_executable(ga106-lab
  src/main.cpp
  src/pci_discovery.cpp
  src/drm_discovery.cpp
  src/baseline.cpp
)

target_link_libraries(ga106-lab PRIVATE nvidia_common)

target_compile_features(ga106-lab PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ga106-lab PRIVATE -Wall -Wextra -Wpedantic)
endif()
