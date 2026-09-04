#pragma once

// ga106-lab: descoberta passiva de GPUs NVIDIA via sysfs (somente leitura).
// Todo acesso a arquivo usa abertura read-only; nenhum estado do dispositivo
// e alterado por estas funcoes. Tipos de BAR/dispositivo reutilizados de
// nvidia_common (portavel, sem dependencias de OS).

#include <cstdint>
#include <string>
#include <vector>

#include "nvidia/pci_device.hpp"

namespace ga106lab {

// Classificacao arquitetura/chip a partir do hardware real (vendor/device
// lidos do sysfs). Nunca assume modelo pela marca; IDs fora da tabela
// retornam "NVIDIA GPU" / "unknown".
struct ArchInfo {
  std::string arch;  // ex. "Ampere", "NVIDIA GPU" ou "unknown"
  std::string chip;  // ex. "GA106", "GA104 (not GA106)" ou "unknown"
};

[[nodiscard]] ArchInfo classify_arch(std::uint16_t vendor_id,
                                     std::uint16_t device_id);

// Metadados extras (display) coletados junto ao dispositivo. "unknown"
// quando a fonte nao existe ou nao pode ser lida.
struct PciExtra {
  std::string driver = "unknown";       // basename do symlink .../driver
  std::string modules = "unknown";      // entradas relacionadas em /proc/modules
  std::string modalias = "unknown";     // .../modalias
  std::string iommu_group = "unknown";  // basename de .../iommu_group
  std::string numa_node = "unknown";    // conteudo de .../numa_node
  std::string class_hex = "unknown";    // classe PCI como "0x030000"
  std::uint64_t bar_flags[6] = {};      // flags brutas de resource por BAR
  std::uint64_t rom_start = 0;          // janela ROM (linha 6 de resource)
  std::uint64_t rom_end = 0;
  std::uint64_t rom_flags = 0;
  bool rom_present = false;
};

struct ObservedGpu {
  nvidia::NvidiaPciDevice pci;
  PciExtra extra;
};

// Varre /sys/bus/pci/devices e retorna todos os dispositivos com
// vendor 0x10DE, em ordem de BDF. Retorna vazio se nenhum for encontrado.
[[nodiscard]] std::vector<ObservedGpu> discover_nvidia_gpus();

// Escolhe o GPU primario: primeiro controlador de video (classe 0x03),
// senao o primeiro NVIDIA observado. Retorna nullptr se vazio.
[[nodiscard]] const ObservedGpu* pick_primary_gpu(
    const std::vector<ObservedGpu>& gpus) noexcept;

// Memoria total de video. Sem contador padrao no sysfs deste driver, usa
// consulta opcional somente-leitura; "unknown" se indisponivel.
[[nodiscard]] std::string query_vram_total(const std::string& bdf);

// Formata bytes como "16M", "512K", "16G" ou "<n> B" quando nao alinhado.
[[nodiscard]] std::string format_size(std::uint64_t bytes);

// "0x" + hex minusculo sem zeros a esquerda (ex. 0xfb000000).
[[nodiscard]] std::string hex_u64(std::uint64_t v);

// "0x" + 4 digitos hex minusculos (ex. 0x10de).
[[nodiscard]] std::string hex_u16(std::uint16_t v);

// Texto de decodificacao das flags brutas de resource (leitura).
[[nodiscard]] std::string bar_flags_text(std::uint64_t flags, bool is_io);

}  // namespace ga106lab
