#pragma once

// ga106-lab lab-status: visao somente-leitura de ownership do lab.
// GPU alvo fixa: 0000:01:00.0 (RTX 3060 10de:2504 GA106). Fontes, todas
// read-only: sysfs via O_RDONLY (vendor/device/boot_vga/fb/connectors),
// readlink (driver, device), varredura /proc (nomes de processos + fds
// /dev/dri), e saidas de inspecao via popen (nvidia-smi compute-apps
// opcional, dmesg filtrado). Nenhum estado e alterado; campos ausentes
// viram "unknown"/null — nunca inventados.
// Veredito: BLOCKED por padrao; READY_FOR_NEXT_PHASE somente se TODAS as
// condicoes valerem (iGPU presente E sessao grafica na iGPU E RTX sem
// conector ativo usado E sem clientes desktop no alvo). A mera existencia
// de outra GPU nunca implica READY.

#include <string>
#include <vector>

namespace ga106lab {

// Alvo fixo da Fase 0 + alternativa procurada (Cezanne 1002:1638).
inline constexpr const char* kLabTargetBdf = "0000:01:00.0";
inline constexpr const char* kLabTargetVendor = "0x10de";
inline constexpr const char* kLabTargetDevice = "0x2504";
inline constexpr const char* kLabAltVendor = "0x1002";
inline constexpr const char* kLabAltDevice = "0x1638";

struct ConnectorStatus {
  std::string name = "unknown";
  std::string status = "unknown";   // connected/disconnected/unknown
  std::string enabled = "unknown";  // enabled/disabled/unknown
  bool active() const { return status == "connected" && enabled == "enabled"; }
};

struct AltGpuInfo {
  bool present = false;
  std::string bdf = "unknown";
  std::string vendor = "unknown";
  std::string device = "unknown";
  std::string pci_class = "unknown";
  std::string driver = "unknown";
  std::string boot_vga = "unknown";
  std::vector<std::string> drm_nodes;
  std::vector<ConnectorStatus> connectors;
};

struct LabStatus {
  // Alvo.
  bool target_present = false;
  std::string vendor = "unknown";
  std::string device = "unknown";
  std::string subsys = "unknown";
  std::string revision = "unknown";
  bool is_expected_ga106 = false;
  std::string kernel_driver = "unknown";
  std::string boot_vga = "unknown";
  std::string fb_owner = "unknown";     // nome do fb (ex. nvidia-drmdrmfb)
  std::string fbcon_hint = "unknown";   // dmesg filtrado por fbcon (opcional)
  std::vector<std::string> drm_nodes;   // "card1 /dev/dri/card1 226:1"
  std::string drm_primary = "unknown";  // heuristica via holders; senao unknown
  std::string drm_primary_note;
  std::vector<ConnectorStatus> connectors;
  std::vector<std::string> desktop_procs;   // "pid comm" (scan /proc)
  std::vector<std::string> target_holders;  // procs com fd em /dev/dri do alvo
  bool compute_queried = false;
  std::string compute_raw = "unknown";
  std::vector<std::string> compute_desktop;  // apps desktop via nvidia-smi
  // Alternativa + sintese.
  AltGpuInfo alt;
  std::string desktop_gpu = "unknown";  // BDF que hospeda o desktop ou unknown
  std::string desktop_render_note;  // massa renderD* do compositor por GPU
  bool target_used_known = false;
  bool target_used_by_desktop = false;
  bool session_on_alt = false;
  std::string verdict = "unknown";  // BLOCKED | PARTIALLY_READY | READY_FOR_NEXT_PHASE | unknown
  std::vector<std::string> verdict_reasons;
};

// Bloco agregado ao baseline.json (tipos JSON proprios: bool/int/null).
struct LabReadiness {
  bool alt_present = false;
  bool has_desktop_gpu = false;
  std::string desktop_gpu;  // vazio => null
  bool used_known = false;
  bool target_used = false;
  bool has_active = false;
  int active_count = 0;
  std::string status = "unknown";  // BLOCKED | READY_FOR_NEXT_PHASE | unknown
};

// Coleta o estado atual (somente leitura). Nunca falha por ausencia de
// ferramenta: ausencias viram unknown.
LabStatus collect_lab_status();

// Deriva o bloco lab_readiness do estado coletado.
LabReadiness lab_readiness_from(const LabStatus& st);

// Renderiza o relatorio humano do `lab-status`.
std::string render_lab_status(const LabStatus& st);

// Executa o verbo lab-status. Retorna 0 em sucesso (relato, nao gate),
// 2 em uso invalido.
int run_lab_status(const std::vector<std::string>& args);

}  // namespace ga106lab
