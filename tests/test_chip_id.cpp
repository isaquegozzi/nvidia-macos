// test_chip_id — tabela canônica PCI ID → die (docs/ga106/device-ids.md).
// Sem IO, sem hardware. Falha via return != 0; sem dependências externas.
//
// Regressão: looks_like_ga106() tratava 0x2487 (GA104!) e 0x24AA (UNKNOWN)
// como GA106. O PCI ID conhecido agora decide; nome comercial não prova die.

#include "nvidia/chip.hpp"
#include "nvidia/device_info.hpp"

#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

void check(bool cond, const char* name) {
  if (!cond) {
    ++failures;
    std::cerr << "FAIL: " << name << "\n";
  }
}

nvidia::NvidiaDeviceInfo info_for(std::uint16_t device_id,
                                 const char* chip_name = "") {
  nvidia::NvidiaDeviceInfo info{};
  info.pci.vendor_id = 0x10DE;
  info.pci.device_id = device_id;
  info.chip_name = chip_name;
  return info;
}

}  // namespace

int main() {
  using nvidia::NvidiaChip;
  using nvidia::chip_from_device_id;

  // Casos exigidos.
  check(chip_from_device_id(0x2504) == NvidiaChip::GA106, "10de:2504 -> GA106");
  check(chip_from_device_id(0x2487) == NvidiaChip::GA104, "10de:2487 -> GA104");
  check(chip_from_device_id(0x2503) == NvidiaChip::GA106, "2503 -> GA106");
  check(chip_from_device_id(0x2414) == NvidiaChip::GA103, "2414 -> GA103");
  check(chip_from_device_id(0xFFFF) == NvidiaChip::Unknown, "0xFFFF -> Unknown");
  check(!info_for(0x2487).looks_like_ga106(),
        "looks_like_ga106(2487) == false");

  // GA106 completos (§1 da doc).
  for (std::uint16_t id : {0x2501, 0x2503, 0x2504, 0x2505, 0x2507, 0x2508,
                           0x2509, 0x2544, 0x2520, 0x2521, 0x2523, 0x252F,
                           0x2531, 0x2571}) {
    check(chip_from_device_id(id) == NvidiaChip::GA106, "GA106 id da tabela");
    check(info_for(id).looks_like_ga106(), "looks_like_ga106(GA106) == true");
  }

  // Armadilhas NÃO-GA106 (§2 da doc): GA104 / GA103 / GA107.
  for (std::uint16_t id : {0x2487, 0x2488, 0x2489, 0x24C7, 0x24C9}) {
    check(chip_from_device_id(id) == NvidiaChip::GA104, "GA104 id da tabela");
    check(!info_for(id).looks_like_ga106(),
          "looks_like_ga106(GA104) == false");
  }
  check(chip_from_device_id(0x2414) == NvidiaChip::GA103, "GA103 id da tabela");
  check(!info_for(0x2414).looks_like_ga106(),
        "looks_like_ga106(GA103) == false");
  for (std::uint16_t id : {0x2582, 0x2583, 0x2584}) {
    check(chip_from_device_id(id) == NvidiaChip::GA107, "GA107 id da tabela");
    check(!info_for(id).looks_like_ga106(),
          "looks_like_ga106(GA107) == false");
  }

  // 0x24AA: ausente da lista verificada → Unknown, NÃO GA106.
  check(chip_from_device_id(0x24AA) == NvidiaChip::Unknown,
        "0x24AA -> Unknown");
  check(!info_for(0x24AA).looks_like_ga106(),
        "looks_like_ga106(24AA) == false");

  // Marketing name não sobrepõe PCI ID conhecido (0x2487 se diz "3060").
  check(!info_for(0x2487, "GA106").looks_like_ga106(),
        "2487+chip_name GA106 continua false");

  // Fallback preservado só para ID desconhecido (futura revisão GA106).
  check(info_for(0xFFFF, "GA106").looks_like_ga106(),
        "Unknown+chip_name GA106 == true");
  check(!info_for(0xFFFF, "UNKNOWN").looks_like_ga106(),
        "Unknown+chip_name UNKNOWN == false");

  if (failures == 0) {
    std::cout << "chip_id: OK\n";
    return 0;
  }
  std::cerr << "chip_id: " << failures << " falha(s)\n";
  return 1;
}
