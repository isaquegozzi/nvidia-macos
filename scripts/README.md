# scripts/ — helpers reproduzíveis

- `build.sh` — configura (CMake), compila e roda `ctest`. Aceita `--clean` e `--no-test`.
  Respeita `BUILD_DIR` e `CMAKE_GENERATOR` se exportados.

Nenhum script aqui toca em hardware. Scripts futuros que lerem sysfs devem usar
apenas leitura (`O_RDONLY`, sem `setpci` com escrita, sem `dd of=`).
