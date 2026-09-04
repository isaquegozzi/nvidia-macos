// test_common_smoke — prova que nvidia_common compila, linka e os tipos
// portáveis se comportam como documentado. Sem IO, sem hardware.
// Falha via return != 0; sem dependências externas.

#include "nvidia/device_info.hpp"
#include "nvidia/pci_bar.hpp"
#include "nvidia/pci_device.hpp"
#include "nvidia/pci_types.hpp"

#include <cstdint>

namespace {

int check(bool cond) { return cond ? 0 : 1; }

}  // namespace

int main() {
  int failures = 0;

  // NvidiaPciBar: default inválida, válida quando size != 0.
  nvidia::NvidiaPciBar empty{};
  failures += check(!empty.is_valid());

  nvidia::NvidiaPciBar bar0{};
  bar0.index = 1;
  bar0.base_address = 0xFB000000ULL;
  bar0.size = 0x1000000ULL;
  failures += check(bar0.is_valid());
  failures += check(bar0.is_memory());

  // NvidiaPciDevice: lookup de BAR + vendor check.
  nvidia::NvidiaPciDevice dev{};
  dev.bdf = "0000:01:00.0";
  dev.vendor_id = 0x10DE;
  dev.device_id = 0x2503;
  dev.bars.push_back(bar0);
  failures += check(dev.is_nvidia());
  failures += check(dev.bar(1).has_value());
  failures += check(!dev.bar(0).has_value());
  failures += check(dev.bar(1)->size == 0x1000000ULL);

  // NvidiaDeviceInfo: heurística GA106 (ID conhecido + fallback chip_name).
  nvidia::NvidiaDeviceInfo info{};
  info.pci = dev;
  info.marketing_name = "GeForce RTX 3060 (GA106)";
  info.chip_name = "GA106";
  failures += check(info.looks_like_ga106());

  nvidia::NvidiaPciDevice other{};
  other.device_id = 0x1234;
  nvidia::NvidiaDeviceInfo other_info{};
  other_info.pci = other;
  other_info.chip_name = "UNKNOWN";
  failures += check(!other_info.looks_like_ga106());

  // Umbrella header compila junto (pci_types.hpp incluído no topo).
  const auto ref = dev.bar(1).value();
  failures += check(ref.base_address == 0xFB000000ULL);

  return failures == 0 ? 0 : 1;
}
