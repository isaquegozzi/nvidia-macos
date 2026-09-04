#pragma once

// ga106-lab: descoberta passiva de nos DRM associados ao GPU (somente
// leitura). Varre /sys/class/drm e cruza o symlink de dispositivo com o BDF
// observado. Nenhum no /dev/dri e aberto; apenas listagem e metadados.

#include <string>
#include <vector>

namespace ga106lab {

struct DrmNodeInfo {
  std::string name;         // ex. "card1", "renderD128"
  std::string dri_path;     // ex. "/dev/dri/card1"
  std::string major_minor;  // ex. "226:1" ou "unknown"
  std::string pci_slot;     // ex. "0000:01:00.0" ou "unknown"
  bool dri_present = false;
};

// Retorna os nos card*/renderD* cujo dispositivo e o BDF informado.
// Vetor vazio quando nao ha no associado (DRM indisponivel para o BDF).
[[nodiscard]] std::vector<DrmNodeInfo> discover_drm_nodes(
    const std::string& bdf);

}  // namespace ga106lab
