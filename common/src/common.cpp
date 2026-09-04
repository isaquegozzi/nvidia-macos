// nvidia_common — translation unit mínima para garantir que a static lib
// compila em qualquer OS. Sem includes de sistema além do portável.
// Fase 0: nenhuma lógica de hardware aqui, apenas helpers puros.

#include "nvidia/device_info.hpp"
#include "nvidia/pci_bar.hpp"
#include "nvidia/pci_device.hpp"
#include "nvidia/pci_types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nvidia {
namespace detail {

// Âncora de link: referenciada pelos smoke tests para provar que a lib linkou.
// Retorna versão da Fase 0; sem estado global, sem IO.
[[nodiscard]] const char* phase0_version() noexcept { return "0.0.1-phase0"; }

// Formata "vvvv:dddd" (vendor:device) como hex minúsculo — puro, sem OS.
[[nodiscard]] std::string format_vendor_device(std::uint16_t vendor,
                                               std::uint16_t device) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(9);
  for (int shift = 12; shift >= 0; shift -= 4) {
    out.push_back(kHex[(vendor >> shift) & 0xF]);
  }
  out.push_back(':');
  for (int shift = 12; shift >= 0; shift -= 4) {
    out.push_back(kHex[(device >> shift) & 0xF]);
  }
  return out;
}

// Conta BARs válidas (size != 0). Puro, sem acesso a hardware.
[[nodiscard]] std::size_t count_valid_bars(const NvidiaPciDevice& dev) noexcept {
  std::size_t n = 0;
  for (const auto& b : dev.bars) {
    if (b.is_valid()) ++n;
  }
  return n;
}

}  // namespace detail
}  // namespace nvidia
