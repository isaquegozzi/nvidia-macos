#pragma once

// Tabela canônica PCI device ID → die (Fase 0: identificação).
//
// Quatro conceitos distintos que NÃO devem ser confundidos:
//
//   - marketing model: nome comercial ("GeForce RTX 3060", "RTX 3060 Ti").
//     A mesma placa "RTX 3060" foi vendida com dies diferentes (GA106 E GA104).
//   - PCI device ID: o campo `device_id` (ex. 0x2504, 0x2487) do vendor 0x10DE.
//     É o que esta tabela mapeia — nada mais.
//   - die: o chip físico (GA100/GA102/GA103/GA104/GA106/GA107). É o que
//     `chip_from_device_id()` retorna.
//   - arch: a arquitetura (ex. Ampere para GA10x). Não modelada aqui;
//     ver `classify_arch()` em linux/ga106-lab.
//
// Fonte: docs/ga106/device-ids.md (pci-ids.ucw.cz, verificado 2026-09-04).
// Qualquer ID fora da lista abaixo → NvidiaChip::Unknown (nunca adivinhar).

#include <cstdint>

namespace nvidia {

enum class NvidiaChip {
  Unknown,  // ID ausente da tabela canônica (ex. 0x24AA, 0xFFFF).
  GA100,
  GA102,
  GA103,
  GA104,
  GA106,
  GA107,
};

// Nome curto do die para logs/diagnóstico. Puro, sem IO.
[[nodiscard]] constexpr const char* chip_to_string(NvidiaChip chip) noexcept {
  switch (chip) {
    case NvidiaChip::GA100:
      return "GA100";
    case NvidiaChip::GA102:
      return "GA102";
    case NvidiaChip::GA103:
      return "GA103";
    case NvidiaChip::GA104:
      return "GA104";
    case NvidiaChip::GA106:
      return "GA106";
    case NvidiaChip::GA107:
      return "GA107";
    case NvidiaChip::Unknown:
    default:
      return "UNKNOWN";
  }
}

// Mapeia o PCI device ID (vendor 10de implícito) para o die.
// Fonte: docs/ga106/device-ids.md §1 (GA106) e §2 (armadilhas NÃO-GA106).
// GA100/GA102 não têm IDs na doc → nenhum ID os retorna (ainda Unknown).
[[nodiscard]] constexpr NvidiaChip chip_from_device_id(
    std::uint16_t device_id) noexcept {
  switch (device_id) {
    // GA106 verificados (incl. variantes M GA106M mobile).
    case 0x2501:  // GA106 [RTX 3060]
    case 0x2503:  // GA106 [RTX 3060]
    case 0x2504:  // GA106 [RTX 3060 LHR]
    case 0x2505:  // GA106
    case 0x2507:  // GA106 [3050]
    case 0x2508:  // GA106 [3050 OEM]
    case 0x2509:  // GA106 [3060 12GB Rev.2]
    case 0x2520:  // GA106M [3060 Mobile / Max-Q]
    case 0x2521:  // GA106M [3060 Laptop]
    case 0x2523:  // GA106M [3050 Ti Mobile]
    case 0x252F:  // GA106 [3060 ES] (0x252f)
    case 0x2531:  // GA106 [RTX A2000]
    case 0x2544:  // GA106 [RTX 3060]
    case 0x2571:  // GA106 [A2000 12GB]
    case 0x228E:  // GA106 HDA (áudio HDMI/DP na placa GA106, não GPU 3D)
      return NvidiaChip::GA106;

    // Armadilhas NÃO-GA106: nome comercial parecido, die real diferente.
    case 0x2414:  // GA103 [3060 Ti]
      return NvidiaChip::GA103;

    case 0x2487:  // GA104 [RTX 3060] — NÃO é GA106 apesar do nome comercial!
    case 0x2488:  // GA104 [3070 / 3060 Ti LHR]
    case 0x2489:  // GA104 [3070 / 3060 Ti LHR]
    case 0x24C7:  // GA104 [3060 8GB]
    case 0x24C9:  // GA104 [3060 Ti GDDR6X]
      return NvidiaChip::GA104;

    case 0x2582:  // GA107 [3050]
    case 0x2583:  // GA107 [3050]
    case 0x2584:  // GA107 [3050]
      return NvidiaChip::GA107;

    default:
      return NvidiaChip::Unknown;
  }
}

}  // namespace nvidia
