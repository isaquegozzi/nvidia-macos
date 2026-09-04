> Status: Fase 0 — análise somente leitura de `common/`, sem código novo ou alterado.
> Sem dependências de OS introduzidas. Complementa
> `docs/macos/driver-architecture-gate.md` (decisão PCIDriverKit vs KEXT),
> `docs/macos/ADR-0001-driver-entrypoint.md` (decisão: `IOKit KEXT first`,
> PCIDriverKit diferido — o reuse abaixo vale para ambos, sem reabrir o ADR) e
> `docs/ga106/first-mmio-read.md` §1 (candidato BOOT_0). Data: 2026-09-04.

# `common/` no macOS — veredito de reuse

## 0. Conclusão (1 parágrafo)

`NvidiaDeviceInfo` / `NvidiaPciDevice` / `NvidiaPciBar` + tabelas de identificação
GA106 em `common/` são **reutilizáveis no macOS sem dependências Linux**:
só usam C++20 portável (`<cstdint> <optional> <string> <vector>`), compilam como
`STATIC lib` sob `GNU|Clang`, e não incluem nenhum header/APIs de OS.
O reuse direto vale para **helper userspace macOS e para lógica de identificação
dentro do fluxo DriverKit**; uso *dentro* do objeto dext/kernel exige camada de
adaptação (sem `std::string`/`std::vector` no hot path do driver). Nenhum código
foi alterado para afirmar isso.

## 1. O que já é reutilizável (por arquivo)

| Arquivo | Contém | Includes | Reuse macOS |
|---|---|---|---|
| `common/include/nvidia/pci_bar.hpp` | `NvidiaPciBar` (`index`, `base_address`, `size`, `is_64bit`, `is_prefetchable`, `is_io_space`, `is_valid()`, `is_memory()`) | só `<cstdint>` | SIM — `constexpr`, sem heap/OS; mapeia 1:1 para `memoryIndex`/BAR em `IOPCIDevice` (ver §3). |
| `common/include/nvidia/pci_device.hpp` | `NvidiaPciDevice` (`bdf`, `vendor/device/subsystem`, `class/subclass`, `revision`, `bars`, `is_nvidia()`, `bar(i)`) | `<cstdint> <optional> <string> <vector>` + `pci_bar.hpp` | SIM — valor puro; `bdf`/`bars` vêm de qualquer fonte (Linux `sysfs`, macOS `IOPCIDevice`, ou log). |
| `common/include/nvidia/device_info.hpp` | `NvidiaDeviceInfo` (`pci`, `marketing_name`, `chip_name`, `kernel_driver`, `driver_bound`, `notes`, `looks_like_ga106()`) | `<cstdint> <string>` + `pci_device.hpp` | SIM, com ressalva de tabela (§2). |
| `common/include/nvidia/pci_types.hpp` | umbrella (re-exporta os 3) | só os 3 acima | SIM. |
| `common/src/common.cpp` | `detail::phase0_version()`, `format_vendor_device()`, `count_valid_bars()` | os 4 headers + `<cstdint> <optional> <string> <vector>` | SIM — sem estado global, sem IO; âncora de link p/ smoke tests. |
| `common/CMakeLists.txt` | `add_library(nvidia_common STATIC)`, `cxx_std_20`, `-Wall -Wextra -Wpedantic` | — | SIM — sem `find_package` de OS; `macos/` futuro pode `target_link_libraries(... nvidia_common)`. |
| `tests/test_common_smoke.cpp` | prova link + comportamento (`is_valid`, `bar(i)`, `looks_like_ga106`) | mesmos headers | SIM — sem IO/hardware; roda em macOS host sem GPU. |

## 2. Acoplamento sysfs/Linux — veredito "sem acoplamento de código"

Busca `sysfs|linux|unistd|fcntl|mmap|/sys|/proc|/dev|libpci|IOKit|DriverKit|sys/`
em `common/` → 4 matches, **todos inócuos**:

- `common/include/nvidia/pci_device.hpp:3` — comentário:
  `// Dispositivo PCI identificado passivamente (lspci / sysfs, O_RDONLY).`
- `common/include/nvidia/pci_bar.hpp:11` — comentário de campo:
  `// bus address lido via sysfs (somente leitura)`
- `common/src/common.cpp:5`, `common/include/nvidia/pci_types.hpp:6` — falsos
  positivos de `#include "nvidia/device_info.hpp"` (casa `/dev/`).

**Declaração: sem acoplamento.** Nenhum `#include <sys/...>`, `<unistd.h>`,
`<libpci>`, `<IOKit/...>`, `<DriverKit/...>`, nenhum `open/mmap/ioctl`.
Comentários descrevem *uma* fonte possível (Linux lab); o tipo aceita qualquer
fonte que preencha os campos. Não introduzir `#ifdef __linux__/__APPLE__` aqui —
a portabilidade atual depende de não ter nenhum.

### 2.1 Ressalva de tabela GA106 (único ponto de atenção)

`device_info.hpp:20-31` (`looks_like_ga106()`):

- lista `0x2487` como `GA106M` — mas `docs/ga106/device-ids.md` §2 classifica
  `0x2487` como **GA104 (armadilha, nome comercial RTX 3060)**;
- lista `0x24AA` — ausente na lista verificada (`2501/2503/2504/2505/2507/2508/
  2509/2520/2521/2523/252f/2531/2544/2571` + HDA `228e`);
- omite demais IDs verificados (caem no fallback `chip_name == "GA106"`).

Ação: alinhar o `switch` com `device-ids.md` em commit de docs/código futuro,
**antes** de usar a heurística como gate de MMIO. Não feito aqui (Fase 0 doc-only).
O problema é de *dados*, não de *plataforma* — não afeta o veredito de reuse.

## 3. DriverKit C++ vs IOKit C++ vs helper userspace

Ver decisão de entrypoint em `docs/macos/driver-architecture-gate.md`
(PCIDriverKit dext vs KEXT; Tahoe Intel/Hackintosh; `built-in`; produção `0x10DE`).
Este doc só responde "onde `common/` cabe em cada caminho":

- **Helper userspace macOS (recomendado para Fase 0+): 100% reuse.**
  Ferramenta CLI que parseia/loga `bdf`, `vendor:device`, BARs e `looks_like_ga106()`
  a partir de dados já coletados (ex. ioreg dump, log Linux). `std::string`/
  `std::vector`/`std::optional` totalmente disponíveis; `common/` linka direto.
- **DriverKit dext (PCIDriverKit, `IOPCIDevice` provider): reuse da lógica, com adaptação.**
  Dext usa `ConfigurationRead32`/`MemoryRead32(memoryIndex, offset)` (não ponteiro
  cru) e containers `OSDictionary`/`OSArray`/`OSString` no provider; `std::vector`
  /`std::string` com exceção não pertencem ao hot path do driver. Padrão: manter
  `common/` como *value types* fora do `IOService` (parse/validação/teste) e converter
  na borda (`NvidiaPciBar` ↔ `memoryIndex`/descritor `IOMemoryDescriptor`).
  Não colocar `std::vector<NvidiaPciBar>` como membro de `IOService` sem checar
  o subset C++ do DriverKit SDK do Xcode alvo.
- **IOKit KEXT legado (`mapDeviceMemoryWithRegister` → `IOMemoryMap` →
  `getVirtualAddress()`): mesmo padrão do dext.** `common/` descreve *o que* foi
  encontrado; o KEXT detém *como* mapear. Reuse idêntico ao dext, sem vantagem
  adicional — a escolha dext-vs-KEXT segue o gate, não este doc.

Em nenhum caminho `common/` acessa hardware: ele é **modelo de dados +
heurística pura**. O acesso (Linux `sysfs O_RDONLY` hoje; `IOPCIDevice::MemoryRead32`
ou `mapDeviceMemoryWithRegister` amanhã) mora fora da lib, nos callers
(`linux/ga106-lab` hoje; futuro helper/dext documentado em `docs/research/`).

## 4. O que NÃO fazer (para preservar o reuse)

- Não incluir headers de OS em `common/include/` (`sys/`, `IOKit/`, `DriverKit/`,
  `libpci`, `pci/pci.h`).
- Não trocar `std::string`/`std::vector` por `OSString`/`OSArray` dentro da lib —
  isso quebraria o helper userspace e os smoke tests; adaptar na borda do driver.
- Não embutir leitura (`open/mmap/IOMemoryMap`) nos structs; manter `is_valid()`,
  `is_nvidia()`, `bar(i)`, `looks_like_ga106()` puros e testáveis sem hardware.
- Não "corrigir" a tabela GA106 aqui sem bancada + revisão (ver §2.1).

## 5. Referências

- `common/include/nvidia/*.hpp`, `common/src/*.cpp`, `common/CMakeLists.txt`
  (lidos em 2026-09-04; sem SHA — working tree Fase 0).
- `docs/ga106/device-ids.md` (tabela verificada + armadilhas).
- `docs/macos/driver-architecture-gate.md` (PCIDriverKit vs KEXT, Tahoe).
- `docs/ga106/first-mmio-read.md` §1 (mesmo veredito, resumido) + §2 (BOOT_0).
- `macos/README.md` (placeholder; sem `.kext`/`.dext` nesta fase).
