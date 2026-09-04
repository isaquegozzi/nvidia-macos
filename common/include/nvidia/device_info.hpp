#pragma once

// Visão agregada de um adaptador na bancada (Fase 0: identificação).

#include <cstdint>
#include <string>

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
    // IDs iniciais (expandir via docs/ga106/ após observação real).
    switch (pci.device_id) {
      case 0x2487:  // GA106M (laptop)
      case 0x2503:  // GA106 [RTX 3060 12GB]
      case 0x2504:  // GA106 [RTX 3060 12GB]
      case 0x24AA:
        return true;
      default:
        return chip_name == "GA106";
    }
  }
};

}  // namespace nvidia
