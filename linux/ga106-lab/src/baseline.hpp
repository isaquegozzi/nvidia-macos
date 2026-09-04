#pragma once

// ga106-lab baseline: snapshot passivo reproduzivel (Fase 0).
// Agrega pci_discovery/drm_discovery + leituras O_RDONLY de
// sysfs/proc + saida de ferramentas de inspeccao via popen.
// Nenhum estado do dispositivo e alterado; ausencias viram "unknown".

#include <string>
#include <vector>

namespace ga106lab {

// Executa o verbo baseline. args sao os tokens apos "baseline":
// [--out-dir DIR] [--stdout] | [-h|--help].
// Retorna 0 em sucesso, 1 em falha de IO, 2 em uso invalido.
int run_baseline(const std::vector<std::string>& args);

}  // namespace ga106lab
