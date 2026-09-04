#pragma once

// Visão agregada de um adaptador na bancada (Fase 0: identificação).

#include <cstdint>
#include <string>

#include "nvidia/chip.hpp"
#include "nvidia/pci_device.hpp"

namespace nvidia {

struct NvidiaDeviceInfo {
  NvidiaPciDevice pci;
  std::string marketing_name;  // ex. "GeForce RTX 3060 (GA106)"
  std::string chip_name;       // ex. "GA106"
  std::string kernel_driver;   // ex. "nouveau" / "" = nenhum
  bool driver_bound = false;
  std::string notes;

  [[nodiscard]] bool looks_like_ga106() const noexcept {
    // Tabela canônica (nvidia/chip.hpp ← docs/ga106/device-ids.md).
    // O PCI ID conhecido decide: marketing name ("RTX 3060") NÃO prova GA106
    // (0x2487 é GA104!). O fallback chip_name só vale para IDs ainda
    // desconhecidos (futura revisão GA106 fora da tabela).
    const NvidiaChip chip = chip_from_device_id(pci.device_id);
    if (chip == NvidiaChip::GA106) return true;
    if (chip != NvidiaChip::Unknown) return false;
    return chip_name == "GA106";
  }
};

}  // namespace nvidia
