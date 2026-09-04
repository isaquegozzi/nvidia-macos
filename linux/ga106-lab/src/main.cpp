// ga106-lab info — relatorio passivo do GPU NVIDIA observado (Fase 0).
// Somente leitura: agrega sysfs/proc via pci_discovery/drm_discovery e
// imprime. Nenhuma escrita no hardware; campos ausentes viram "unknown".

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "baseline.hpp"
#include "baseline_compare.hpp"
#include "drm_discovery.hpp"
#include "lab_status.hpp"
#include "pci_discovery.hpp"

namespace {

constexpr const char* kTag = "[ga106-lab]";
constexpr const char* kVersion = "0.0.1-phase0";

int usage() {
  std::cerr << kTag << " uso: ga106-lab info\n";
  std::cerr << kTag << "   info   imprime o relatorio passivo do GPU observado\n";
  std::cerr << kTag << " uso: ga106-lab baseline [--out-dir DIR] [--stdout]\n";
  std::cerr << kTag
            << "   baseline gera snapshot reproduzivel em "
               "artifacts/baselines/<timestamp>/\n";
  std::cerr << kTag
            << " uso: ga106-lab baseline --compare OLD.json NEW.json "
               "[--strict]\n";
  std::cerr << kTag
            << "   baseline --compare diffs deterministicos por classe "
               "(--strict so falha em IDENTITY/STATIC)\n";
  std::cerr << kTag << " uso: ga106-lab lab-status\n";
  std::cerr << kTag
            << "   lab-status visao read-only de ownership (driver, boot "
               "VGA, fb, DRM, conectores, iGPU, veredito)\n";
  return 2;
}

void print_bar(const ga106lab::ObservedGpu& gpu, int index) {
  const auto bar = gpu.pci.bar(static_cast<std::uint8_t>(index));
  if (!bar || !bar->is_valid()) {
    std::cout << "  BAR" << index << ": absent\n";
    return;
  }
  const std::uint64_t start = bar->base_address;
  const std::uint64_t end = start + bar->size - 1;
  const std::uint64_t flags = gpu.extra.bar_flags[index];
  std::cout << "  BAR" << index << ": start=" << ga106lab::hex_u64(start)
            << " end=" << ga106lab::hex_u64(end) << " size="
            << ga106lab::hex_u64(bar->size) << " ("
            << ga106lab::format_size(bar->size) << ") flags="
            << ga106lab::hex_u64(flags) << " ("
            << ga106lab::bar_flags_text(flags, bar->is_io_space) << ")\n";
}

int run_info() {
  const auto gpus = ga106lab::discover_nvidia_gpus();
  if (gpus.empty()) {
    std::cerr << kTag << " nenhum dispositivo NVIDIA encontrado em "
              << "/sys/bus/pci/devices\n";
    return 1;
  }
  const ga106lab::ObservedGpu* gpu = ga106lab::pick_primary_gpu(gpus);
  if (gpu == nullptr) {
    std::cerr << kTag << " selecao do GPU primario falhou\n";
    return 1;
  }

  const ga106lab::ArchInfo arch =
      ga106lab::classify_arch(gpu->pci.vendor_id, gpu->pci.device_id);
  const std::vector<ga106lab::DrmNodeInfo> drm =
      ga106lab::discover_drm_nodes(gpu->pci.bdf);
  const std::string vram = ga106lab::query_vram_total(gpu->pci.bdf);

  std::cout << kTag << " info for " << gpu->pci.bdf << "\n";

  std::cout << "GPU:\n";
  std::cout << "  BDF: " << gpu->pci.bdf << "\n";
  std::cout << "  Vendor: " << ga106lab::hex_u16(gpu->pci.vendor_id) << "\n";
  std::cout << "  Device: " << ga106lab::hex_u16(gpu->pci.device_id) << "\n";
  std::cout << "  Subsystem: " << ga106lab::hex_u16(gpu->pci.subsystem_vendor)
            << ":" << ga106lab::hex_u16(gpu->pci.subsystem_device) << "\n";
  std::cout << "  Revision: 0x" << std::hex << std::nouppercase << std::setw(2)
            << std::setfill('0') << static_cast<unsigned>(gpu->pci.revision_id)
            << std::dec << "\n";
  std::cout << "  Class: " << gpu->extra.class_hex << "\n";
  std::cout << "  Modalias: " << gpu->extra.modalias << "\n";

  std::cout << "Architecture:\n";
  std::cout << "  Arch: " << arch.arch << "\n";
  std::cout << "  Chip: " << arch.chip << "\n";

  std::cout << "PCI:\n";
  std::cout << "  Driver: " << gpu->extra.driver << "\n";
  std::cout << "  Modules: " << gpu->extra.modules << "\n";
  for (int i = 0; i < 6; ++i) print_bar(*gpu, i);
  if (gpu->extra.rom_present) {
    const std::uint64_t size = gpu->extra.rom_end >= gpu->extra.rom_start
                                   ? (gpu->extra.rom_end - gpu->extra.rom_start + 1)
                                   : 0;
    std::cout << "  ROM: start=" << ga106lab::hex_u64(gpu->extra.rom_start)
              << " end=" << ga106lab::hex_u64(gpu->extra.rom_end) << " size="
              << ga106lab::hex_u64(size) << " (" << ga106lab::format_size(size)
              << ") flags=" << ga106lab::hex_u64(gpu->extra.rom_flags) << "\n";
  } else {
    std::cout << "  ROM: absent\n";
  }

  std::cout << "System:\n";
  std::cout << "  IOMMU group: " << gpu->extra.iommu_group << "\n";
  std::cout << "  NUMA node: " << gpu->extra.numa_node << "\n";
  std::cout << "  Kernel driver: " << gpu->extra.driver << "\n";

  std::cout << "DRM:\n";
  if (drm.empty()) {
    std::cout << "  nodes: unknown\n";
  }
  for (const auto& node : drm) {
    std::cout << "  " << node.name << ": " << node.dri_path;
    if (!node.dri_present) std::cout << " (missing)";
    std::cout << " (" << node.major_minor << ") slot=" << node.pci_slot << "\n";
  }

  std::cout << "VRAM:\n";
  std::cout << "  Total: " << vram << "\n";

  if (gpus.size() > 1) {
    std::cerr << kTag << " " << gpus.size() << " dispositivos NVIDIA observados; "
              << "exibindo o primario (" << gpu->pci.bdf << ")\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string(argv[1]) == "info") return run_info();
  if (argc == 2 && std::string(argv[1]) == "lab-status") {
    return ga106lab::run_lab_status({});
  }
  if (argc > 2 && std::string(argv[1]) == "lab-status") {
    std::vector<std::string> rest;
    for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
    return ga106lab::run_lab_status(rest);
  }
  if (argc >= 2 && std::string(argv[1]) == "baseline") {
    std::vector<std::string> rest;
    for (int i = 2; i < argc; ++i) rest.emplace_back(argv[i]);
    if (!rest.empty() && rest[0] == "--compare") {
      rest.erase(rest.begin());
      return ga106lab::run_baseline_compare(rest);
    }
    return ga106lab::run_baseline(rest);
  }
  if (argc == 2 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
    std::cout << "ga106-lab " << kVersion << " (Fase 0, somente leitura)\n";
    return usage();
  }
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "ga106-lab " << kVersion << "\n";
    return 0;
  }
  return usage();
}
