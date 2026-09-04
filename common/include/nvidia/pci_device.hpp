#pragma once

// Dispositivo PCI identificado passivamente (lspci / sysfs, O_RDONLY).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "nvidia/pci_bar.hpp"

namespace nvidia {

struct NvidiaPciDevice {
  std::string bdf;  // ex. "0000:01:00.0"
  std::uint16_t vendor_id = 0x10DE;
  std::uint16_t device_id = 0x0000;
  std::uint16_t subsystem_vendor = 0x0000;
  std::uint16_t subsystem_device = 0x0000;
  std::uint8_t pci_class = 0x00;
  std::uint8_t pci_subclass = 0x00;
  std::uint8_t revision_id = 0x00;
  std::vector<NvidiaPciBar> bars;

  [[nodiscard]] bool is_nvidia() const noexcept { return vendor_id == 0x10DE; }

  [[nodiscard]] std::optional<NvidiaPciBar> bar(std::uint8_t index) const {
    for (const auto& b : bars) {
      if (b.index == index) return b;
    }
    return std::nullopt;
  }
};

}  // namespace nvidia
