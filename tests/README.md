# tests/ — smoke tests da lib comum (sem hardware, sem framework)

- `test_common_smoke.cpp` — inclui os 4 headers (`pci_bar/device/device_info/pci_types`),
  instancia `NvidiaPciDevice`, `NvidiaPciBar`, `NvidiaDeviceInfo` e valida invariantes
  (`is_valid`, `bar(i)`, `looks_like_ga106`). Registrado via CTest.
- `placeholder.hpp` — marcador histórico do scaffolding; será removido quando
  a suíte real crescer (parse de `lspci`, snapshot sysfs, etc.).

Rodar: `./scripts/build.sh` (já invoca `ctest`) ou
`ctest --test-dir build --output-on-failure`.
