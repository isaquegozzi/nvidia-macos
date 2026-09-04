// ga106-lab baseline — snapshot passivo reproduzivel (Fase 0).
// Somente leitura: agrega pci_discovery/drm_discovery + leituras com
// abertura read-only de sysfs/proc + saida de ferramentas de inspeccao
// via popen (lspci, nvidia-smi, modinfo, vulkaninfo, journalctl).
// Nenhum estado do dispositivo e alterado; ausencias viram "unknown".
// A escrita feita aqui e restrita aos arquivos de saida
// (baseline.txt/baseline.json); nunca ao dispositivo.

#include "baseline.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "drm_discovery.hpp"
#include "lab_status.hpp"
#include "pci_discovery.hpp"

namespace ga106lab {
namespace {

constexpr const char* kTag = "[ga106-lab]";
constexpr const char* kVersion = "0.0.1-phase0";
constexpr const char* kBarNote =
    "BAR1 size != VRAM size nao implica erro "
    "(abertura PCI vs memoria dedicada)";

std::string trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  std::size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

bool starts_with(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_all_digits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

bool valid_bdf_chars(const std::string& bdf) {
  if (bdf.empty() || bdf.size() > 32) return false;
  for (char c : bdf) {
    const bool ok = std::isalnum(static_cast<unsigned char>(c)) != 0 ||
                    c == ':' || c == '.' || c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

// Le a primeira linha de um arquivo texto via abertura read-only.
std::string read_first_line(const std::string& path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) return {};
  std::array<char, 4096> buf{};
  const ssize_t n = ::read(fd, buf.data(), buf.size() - 1);
  ::close(fd);
  if (n <= 0) return {};
  std::string s(buf.data(), static_cast<std::size_t>(n));
  const auto nl = s.find('\n');
  if (nl != std::string::npos) s.resize(nl);
  return trim(s);
}

// Le ate max_bytes de um arquivo texto via abertura read-only.
std::string read_file_capped(const std::string& path, std::size_t max_bytes) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) return {};
  std::string out;
  std::array<char, 4096> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    out.append(buf.data(), static_cast<std::size_t>(n));
    if (out.size() >= max_bytes) {
      out.resize(max_bytes);
      break;
    }
  }
  ::close(fd);
  return out;
}

std::string readlink_target(const std::string& path) {
  std::array<char, 4096> buf{};
  const ssize_t n = ::readlink(path.c_str(), buf.data(), buf.size() - 1);
  if (n < 0) return {};
  return std::string(buf.data(), static_cast<std::size_t>(n));
}

std::string basename_of(const std::string& path) {
  const auto pos = path.find_last_of('/');
  return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::vector<std::string> list_dir_names(const std::string& dir) {
  std::vector<std::string> out;
  std::error_code ec;
  std::filesystem::directory_iterator it(dir, ec);
  if (ec) return out;
  for (const auto& entry : it) {
    out.push_back(entry.path().filename().string());
  }
  std::sort(out.begin(), out.end());
  return out;
}

// Roda uma linha de inspeccao e captura a saida (leitura).
// Corta em max_bytes / max_lines para manter o snapshot pequeno.
std::string shell_capture(const std::string& line, std::size_t max_bytes,
                          std::size_t max_lines) {
  FILE* pipe = ::popen(line.c_str(), "r");
  if (pipe == nullptr) return {};
  std::string out;
  std::array<char, 4096> buf{};
  std::size_t lines = 0;
  while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) !=
         nullptr) {
    out.append(buf.data());
    ++lines;
    if (lines >= max_lines || out.size() >= max_bytes) break;
  }
  ::pclose(pipe);
  if (out.size() > max_bytes) out.resize(max_bytes);
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
    out.pop_back();
  }
  return out;
}

std::string utc_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm tm_utc{};
  if (::gmtime_r(&now, &tm_utc) == nullptr) return "unknown";
  std::array<char, 32> buf{};
  if (::strftime(buf.data(), buf.size(), "%Y%m%d-%H%M%S", &tm_utc) == 0) {
    return "unknown";
  }
  return std::string(buf.data());
}

std::string host_name() {
  std::array<char, 256> buf{};
  if (::gethostname(buf.data(), buf.size()) != 0) return "unknown";
  buf.back() = '\0';
  const std::string s(buf.data());
  const std::string t = trim(s);
  return t.empty() ? "unknown" : t;
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '\"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (c < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out += kHex[(c >> 4) & 0xF];
          out += kHex[c & 0xF];
        } else {
          out += ch;
        }
        break;
    }
  }
  return out;
}

bool write_text_file(const std::string& path, const std::string& content) {
  const int fd =
      ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return false;
  std::size_t off = 0;
  while (off < content.size()) {
    const ssize_t n =
        ::write(fd, content.data() + off, content.size() - off);
    if (n <= 0) {
      ::close(fd);
      return false;
    }
    off += static_cast<std::size_t>(n);
  }
  ::close(fd);
  return true;
}

bool ensure_dir(const std::string& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  return !ec;
}

std::string first_non_empty_line(const std::string& text) {
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    const std::string t = trim(line);
    if (!t.empty()) return t;
  }
  return {};
}

std::vector<std::string> split_lines_capped(const std::string& text,
                                            std::size_t max_lines) {
  std::vector<std::string> out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line) && out.size() < max_lines) {
    out.push_back(line);
  }
  return out;
}

std::string join_with(const std::vector<std::string>& parts,
                      const std::string& sep) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) out += sep;
    out += parts[i];
  }
  return out;
}

// --- visoes parciais do snapshot -----------------------------------

struct LinkView {
  std::string cur_width = "unknown";
  std::string cur_speed = "unknown";
  std::string max_width = "unknown";
  std::string max_speed = "unknown";
};

struct ResizeView {
  std::string name;
  std::string value;
};

struct ConnectorView {
  std::string name;
  std::string status = "unknown";
  std::string modes = "unknown";
};

struct NvInfoView {
  std::string model = "unknown";
  std::string uuid = "unknown";
  std::string vbios = "unknown";
  std::string bus_location = "unknown";
  std::string gpu_firmware = "unknown";
  std::string raw;
};

LinkView read_link_view(const std::string& bdf) {
  LinkView v;
  if (!valid_bdf_chars(bdf)) return v;
  const std::string base = "/sys/bus/pci/devices/" + bdf + "/";
  std::string s = read_first_line(base + "current_link_width");
  if (!s.empty()) v.cur_width = s;
  s = read_first_line(base + "current_link_speed");
  if (!s.empty()) v.cur_speed = s;
  s = read_first_line(base + "max_link_width");
  if (!s.empty()) v.max_width = s;
  s = read_first_line(base + "max_link_speed");
  if (!s.empty()) v.max_speed = s;
  return v;
}

std::vector<ResizeView> read_resize_view(const std::string& bdf) {
  std::vector<ResizeView> out;
  if (!valid_bdf_chars(bdf)) return out;
  const std::string base = "/sys/bus/pci/devices/" + bdf;
  for (const auto& name : list_dir_names(base)) {
    if (!starts_with(name, "resource")) continue;
    if (!ends_with(name, "_resize")) continue;
    const std::string val = read_first_line(base + "/" + name);
    out.push_back({name, val.empty() ? "unknown" : val});
    if (out.size() >= 16) break;
  }
  return out;
}

NvInfoView read_nv_information(const std::string& bdf) {
  NvInfoView view;
  const std::string root = "/proc/driver/nvidia/gpus";
  const std::vector<std::string> entries = list_dir_names(root);
  std::string chosen;
  // Prefere o diretorio do BDF observado; senao o primeiro disponivel.
  for (const auto& e : entries) {
    if (e == bdf) {
      chosen = e;
      break;
    }
  }
  if (chosen.empty()) {
    for (const auto& e : entries) {
      const std::string info_path = root + "/" + e + "/information";
      std::error_code ec;
      if (std::filesystem::exists(info_path, ec) && !ec) {
        chosen = e;
        break;
      }
    }
  }
  if (chosen.empty()) return view;
  const std::string raw =
      read_file_capped(root + "/" + chosen + "/information", 8192);
  if (raw.empty()) return view;
  view.raw = raw;
  for (const auto& line : split_lines_capped(raw, 64)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    const std::string key = trim(line.substr(0, colon));
    const std::string val = trim(line.substr(colon + 1));
    if (val.empty()) continue;
    if (key == "Model") view.model = val;
    if (key == "GPU UUID") view.uuid = val;
    if (key == "Video BIOS") view.vbios = val;
    if (key == "Bus Location") view.bus_location = val;
    if (key == "GPU Firmware") view.gpu_firmware = val;
  }
  return view;
}

std::string read_version_text() {
  const std::string raw =
      read_file_capped("/proc/driver/nvidia/version", 4096);
  if (raw.empty()) return "unknown";
  const auto lines = split_lines_capped(raw, 6);
  const std::string joined = join_with(lines, "\n");
  return joined.empty() ? "unknown" : joined;
}

std::string open_flavor_of(const std::string& version_text) {
  if (version_text.empty() || version_text == "unknown") return "unknown";
  if (version_text.find("Open") != std::string::npos) return "open";
  if (version_text.find("NVIDIA") != std::string::npos) return "proprietary";
  return "unknown";
}

// Parametros expostos em /sys/module/nvidia*/parameters (leitura).
std::vector<std::string> read_exposed_params() {
  std::vector<std::string> out;
  for (const auto& mod : list_dir_names("/sys/module")) {
    if (!starts_with(mod, "nvidia")) continue;
    const std::string pdir = "/sys/module/" + mod + "/parameters";
    std::error_code ec;
    if (!std::filesystem::is_directory(pdir, ec) || ec) continue;
    std::size_t per_mod = 0;
    for (const auto& pname : list_dir_names(pdir)) {
      if (per_mod >= 64 || out.size() >= 128) break;
      const std::string val = read_first_line(pdir + "/" + pname);
      out.push_back(mod + "." + pname + "=" +
                    (val.empty() ? "unknown" : val));
      ++per_mod;
    }
    if (out.size() >= 128) break;
  }
  return out;
}

std::string lspci_view(const std::string& bdf) {
  if (!valid_bdf_chars(bdf)) return "unknown (BDF invalido)";
  const std::string out =
      shell_capture("lspci -vv -s " + bdf + " 2>/dev/null", 8192, 80);
  return out.empty() ? "unknown (lspci ausente ou vazio)" : out;
}

void split_vram_query(const std::string& bdf, std::string* total,
                      std::string* used) {
  *total = "unknown";
  *used = "unknown";
  if (!valid_bdf_chars(bdf)) return;
  const std::string out = shell_capture(
      "nvidia-smi --query-gpu=pci.bus_id,memory.total,memory.used "
      "--format=csv,noheader,nounits 2>/dev/null",
      4096, 64);
  if (out.empty()) return;
  std::string want = bdf;
  for (auto& c : want) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (const auto& line : split_lines_capped(out, 64)) {
    std::istringstream cols(line);
    std::string bus, tot, usd;
    if (!std::getline(cols, bus, ',')) continue;
    if (!std::getline(cols, tot, ',')) continue;
    if (!std::getline(cols, usd, ',')) continue;
    std::string bl = trim(bus);
    for (auto& c : bl) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (bl != want && !ends_with(bl, want)) continue;
    std::string t = trim(tot);
    std::string u = trim(usd);
    // Normaliza sufixo MiB quando presente.
    auto strip_mib = [](std::string v) {
      std::string lv = v;
      for (auto& c : lv) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (ends_with(lv, "mib")) v = trim(v.substr(0, v.size() - 3));
      return trim(v);
    };
    t = strip_mib(t);
    u = strip_mib(u);
    if (!t.empty()) *total = t + " MiB";
    if (!u.empty()) *used = u + " MiB";
    return;
  }
}

std::string drm_driver_view(const std::string& node_name) {
  const std::string target =
      readlink_target("/sys/class/drm/" + node_name + "/device/driver");
  if (target.empty()) return "unknown";
  return basename_of(target);
}

// Conectores do tipo cardN-<saida> presos ao BDF observado.
std::vector<ConnectorView> read_connector_views(
    const std::string& bdf, const std::vector<DrmNodeInfo>& nodes) {
  std::vector<ConnectorView> out;
  for (const auto& name : list_dir_names("/sys/class/drm")) {
    if (name.find('-') == std::string::npos) continue;
    const std::string dash = name.substr(0, name.find('-'));
    bool owned = false;
    for (const auto& n : nodes) {
      if (n.name == dash) {
        owned = true;
        break;
      }
    }
    if (!owned) {
      // Confere pelo symlink de dispositivo ou pelo slot PCI.
      const std::string target =
          readlink_target("/sys/class/drm/" + name + "/device");
      if (!target.empty() && target.find(bdf) != std::string::npos) {
        owned = true;
      } else {
        const std::string slot = read_first_line(
            "/sys/class/drm/" + name + "/device/uevent");
        if (!slot.empty() && slot.find(bdf) != std::string::npos) {
          owned = true;
        } else {
          const std::string uevent = read_file_capped(
              "/sys/class/drm/" + name + "/device/uevent", 4096);
          if (uevent.find("PCI_SLOT_NAME=" + bdf) != std::string::npos) {
            owned = true;
          }
        }
      }
    }
    if (!owned) continue;
    ConnectorView cv;
    cv.name = name;
    const std::string st =
        read_first_line("/sys/class/drm/" + name + "/status");
    if (!st.empty()) cv.status = st;
    const std::string modes_raw =
        read_file_capped("/sys/class/drm/" + name + "/modes", 4096);
    if (!modes_raw.empty()) {
      std::vector<std::string> modes;
      for (const auto& ln : split_lines_capped(modes_raw, 32)) {
        const std::string t = trim(ln);
        if (t.empty()) continue;
        modes.push_back(t);
        if (modes.size() >= 5) break;
      }
      if (!modes.empty()) cv.modes = join_with(modes, ", ");
    }
    out.push_back(cv);
    if (out.size() >= 32) break;
  }
  return out;
}

std::string vulkan_view() {
  const std::string out = shell_capture(
      "timeout 10 vulkaninfo --summary 2>/dev/null", 8192, 80);
  if (out.empty()) return "unknown (vulkaninfo ausente ou vazio)";
  return out;
}

bool line_matches_log_filter(const std::string& line) {
  static const char* kTokens[] = {"NVRM",      "nvidia", "nvidia-drm", "GSP",
                                  "PCIe",      "IOMMU",  "Xid",        "GPU",
                                  "HDA NVidia"};
  for (const char* tok : kTokens) {
    if (line.find(tok) != std::string::npos) return true;
  }
  return false;
}

std::vector<std::string> filtered_log_view() {
  std::vector<std::string> out;
  const std::string out_raw = shell_capture(
      "journalctl -k --no-pager -n 400 2>/dev/null", 262144, 400);
  if (out_raw.empty()) return out;
  for (const auto& line : split_lines_capped(out_raw, 400)) {
    if (!line_matches_log_filter(line)) continue;
    std::string v = line;
    if (v.size() > 500) v.resize(500);
    out.push_back(v);
    if (out.size() >= 100) break;
  }
  return out;
}

std::string compute_apps_view() {
  const std::string out = shell_capture(
      "nvidia-smi --query-compute-apps=pid,process_name,used_memory "
      "--format=csv,noheader 2>/dev/null",
      8192, 64);
  if (out.empty()) return "unknown (sem apps visiveis ou nvidia-smi ausente)";
  return out;
}

// Processos com /dev/dri aberto, so via /proc (leitura).
std::vector<std::string> drm_holder_view(
    const std::vector<DrmNodeInfo>& nodes) {
  std::vector<std::string> out;
  if (nodes.empty()) return out;
  std::vector<std::string> wanted;
  for (const auto& n : nodes) wanted.push_back(n.name);
  std::size_t scanned = 0;
  for (const auto& pid : list_dir_names("/proc")) {
    if (!is_all_digits(pid)) continue;
    if (++scanned > 4096) break;
    const std::string comm =
        read_first_line("/proc/" + pid + "/comm");
    const std::string fddir = "/proc/" + pid + "/fd";
    std::error_code ec;
    std::filesystem::directory_iterator it(fddir, ec);
    if (ec) continue;
    std::size_t fds = 0;
    for (const auto& entry : it) {
      if (++fds > 256) break;
      const std::string link =
          readlink_target(entry.path().string());
      if (!starts_with(link, "/dev/dri/")) continue;
      const std::string base = basename_of(link);
      bool hit = false;
      for (const auto& w : wanted) {
        if (base == w) {
          hit = true;
          break;
        }
      }
      if (!hit) continue;
      out.push_back(pid + " " + (comm.empty() ? "unknown" : comm) + " " +
                    link);
      if (out.size() >= 64) return out;
      break;
    }
  }
  return out;
}

// --- snapshot -----------------------------------------------------

struct Snapshot {
  std::string ts;
  std::string hostname;
  std::string kernel;
  std::size_t gpu_count = 0;
  // Identidade (primario ou unknown).
  std::string bdf = "unknown";
  std::string vendor = "unknown";
  std::string device = "unknown";
  std::string subsys = "unknown";
  std::string revision = "unknown";
  std::string arch = "unknown";
  std::string chip = "unknown";
  std::string kernel_driver = "unknown";
  std::string uuid = "unknown";
  std::string model = "unknown";
  std::string vbios = "unknown";
  // PCI.
  std::vector<std::string> bar_lines;
  std::string bar1_size = "unknown";
  LinkView link;
  std::vector<ResizeView> resize;
  std::string iommu = "unknown";
  std::string numa = "unknown";
  std::string lspci;
  // Driver.
  std::string version_text = "unknown";
  std::string flavor = "unknown";
  std::string modinfo = "unknown";
  std::string modules = "unknown";
  std::vector<std::string> params;
  std::string userspace = "unknown";
  std::string gpu_firmware = "unknown";
  // DRM.
  std::vector<DrmNodeInfo> nodes;
  std::string drm_driver = "unknown";
  std::vector<ConnectorView> connectors;
  // Memoria.
  std::string vram_total = "unknown";
  std::string vram_used = "unknown";
  // Vulkan / logs / uso.
  std::string vulkan;
  std::vector<std::string> logs;
  std::string compute_apps;
  std::vector<std::string> holders;
  // Prontidao do lab (reusa a coleta read-only do lab-status).
  LabReadiness readiness;
};

void describe_bars(const ObservedGpu* gpu, Snapshot* snap) {
  if (gpu == nullptr) return;
  for (int i = 0; i < 6; ++i) {
    const auto bar = gpu->pci.bar(static_cast<std::uint8_t>(i));
    std::ostringstream os;
    if (!bar || !bar->is_valid()) {
      os << "BAR" << i << ": absent";
    } else {
      const std::uint64_t start = bar->base_address;
      const std::uint64_t end = start + bar->size - 1;
      const std::uint64_t flags = gpu->extra.bar_flags[i];
      os << "BAR" << i << ": start=" << hex_u64(start) << " end="
         << hex_u64(end) << " size=" << hex_u64(bar->size) << " ("
         << format_size(bar->size) << ") flags=" << hex_u64(flags) << " ("
         << bar_flags_text(flags, bar->is_io_space) << ")";
      if (i == 1) snap->bar1_size = hex_u64(bar->size) + " (" +
                                    format_size(bar->size) + ")";
    }
    snap->bar_lines.push_back(os.str());
  }
  {
    std::ostringstream os;
    if (gpu->extra.rom_present) {
      const std::uint64_t size =
          gpu->extra.rom_end >= gpu->extra.rom_start
              ? (gpu->extra.rom_end - gpu->extra.rom_start + 1)
              : 0;
      os << "ROM: start=" << hex_u64(gpu->extra.rom_start)
         << " end=" << hex_u64(gpu->extra.rom_end) << " size="
         << hex_u64(size) << " (" << format_size(size) << ") flags="
         << hex_u64(gpu->extra.rom_flags);
    } else {
      os << "ROM: absent";
    }
    snap->bar_lines.push_back(os.str());
  }
}

Snapshot collect_snapshot() {
  Snapshot s;
  s.ts = utc_timestamp();
  s.hostname = host_name();
  {
    const std::string kv = read_first_line("/proc/version");
    s.kernel = kv.empty() ? "unknown" : kv;
  }
  const auto gpus = discover_nvidia_gpus();
  s.gpu_count = gpus.size();
  const ObservedGpu* gpu = pick_primary_gpu(gpus);
  if (gpu != nullptr) {
    s.bdf = gpu->pci.bdf;
    s.vendor = hex_u16(gpu->pci.vendor_id);
    s.device = hex_u16(gpu->pci.device_id);
    {
      std::ostringstream os;
      os << hex_u16(gpu->pci.subsystem_vendor) << ":"
         << hex_u16(gpu->pci.subsystem_device);
      s.subsys = os.str();
    }
    {
      std::ostringstream os;
      os << "0x" << std::hex << std::nouppercase << std::setw(2)
         << std::setfill('0') << static_cast<unsigned>(gpu->pci.revision_id);
      s.revision = os.str();
    }
    const ArchInfo arch =
        classify_arch(gpu->pci.vendor_id, gpu->pci.device_id);
    s.arch = arch.arch.empty() ? "unknown" : arch.arch;
    s.chip = arch.chip.empty() ? "unknown" : arch.chip;
    s.kernel_driver =
        gpu->extra.driver.empty() ? "unknown" : gpu->extra.driver;
    s.modules =
        gpu->extra.modules.empty() ? "unknown" : gpu->extra.modules;
    s.iommu = gpu->extra.iommu_group.empty() ? "unknown"
                                             : gpu->extra.iommu_group;
    s.numa =
        gpu->extra.numa_node.empty() ? "unknown" : gpu->extra.numa_node;
    describe_bars(gpu, &s);
    s.link = read_link_view(s.bdf);
    s.resize = read_resize_view(s.bdf);
    s.lspci = lspci_view(s.bdf);
    s.nodes = discover_drm_nodes(s.bdf);
    s.connectors = read_connector_views(s.bdf, s.nodes);
    std::string tot, usd;
    split_vram_query(s.bdf, &tot, &usd);
    if (tot != "unknown") {
      s.vram_total = tot;
    } else {
      s.vram_total = query_vram_total(s.bdf);
    }
    s.vram_used = usd;
  } else {
    s.lspci = "unknown (sem dispositivo NVIDIA em /sys/bus/pci/devices)";
  }
  {
    const NvInfoView info = read_nv_information(s.bdf);
    if (info.model != "unknown") s.model = info.model;
    if (info.uuid != "unknown") s.uuid = info.uuid;
    if (info.vbios != "unknown") s.vbios = info.vbios;
    if (info.gpu_firmware != "unknown") s.gpu_firmware = info.gpu_firmware;
  }
  if (s.uuid == "unknown") {
    const std::string out = shell_capture(
        "nvidia-smi --query-gpu=pci.bus_id,gpu_uuid "
        "--format=csv,noheader 2>/dev/null",
        4096, 64);
    if (!out.empty() && s.bdf != "unknown") {
      std::string want = s.bdf;
      for (auto& c : want) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      for (const auto& line : split_lines_capped(out, 64)) {
        const auto comma = line.find(',');
        if (comma == std::string::npos) continue;
        std::string bus = trim(line.substr(0, comma));
        for (auto& c : bus) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (bus == want || ends_with(bus, want)) {
          const std::string id = trim(line.substr(comma + 1));
          if (!id.empty()) s.uuid = id;
          break;
        }
      }
    }
  }
  s.version_text = read_version_text();
  s.flavor = open_flavor_of(s.version_text);
  {
    const std::string mi = shell_capture(
        "modinfo -F version nvidia 2>/dev/null", 512, 4);
    const std::string first = first_non_empty_line(mi);
    s.modinfo = first.empty() ? "unknown" : first;
  }
  s.params = read_exposed_params();
  {
    const std::string uv = shell_capture(
        "nvidia-smi --query-gpu=driver_version --format=csv,noheader "
        "2>/dev/null",
        1024, 16);
    const std::string first = first_non_empty_line(uv);
    s.userspace = first.empty() ? "unknown" : first;
  }
  if (!s.nodes.empty()) {
    // O driver DRM e lido do symlink do primeiro no observado.
    s.drm_driver = drm_driver_view(s.nodes.front().name);
  }
  s.vulkan = vulkan_view();
  s.logs = filtered_log_view();
  s.compute_apps = compute_apps_view();
  s.holders = drm_holder_view(s.nodes);
  s.readiness = lab_readiness_from(collect_lab_status());
  return s;
}

std::string render_txt(const Snapshot& s) {
  std::ostringstream os;
  os << kTag << " baseline " << s.ts << " for " << s.bdf << "\n";
  os << "Meta:\n";
  os << "  tool: ga106-lab " << kVersion << " (Fase 0, somente leitura)\n";
  os << "  timestamp_utc: " << s.ts << "\n";
  os << "  hostname: " << s.hostname << "\n";
  os << "  kernel: " << s.kernel << "\n";
  os << "  gpu_count: " << s.gpu_count << "\n";
  os << "Identity:\n";
  os << "  BDF: " << s.bdf << "\n";
  os << "  Vendor: " << s.vendor << "\n";
  os << "  Device: " << s.device << "\n";
  os << "  Subsystem: " << s.subsys << "\n";
  os << "  Revision: " << s.revision << "\n";
  os << "  Arch: " << s.arch << "\n";
  os << "  Chip: " << s.chip << "\n";
  os << "  Model: " << s.model << "\n";
  os << "  VBIOS: " << s.vbios << "\n";
  os << "  UUID: " << s.uuid << "\n";
  os << "  Kernel driver: " << s.kernel_driver << "\n";
  os << "PCI:\n";
  if (s.bar_lines.empty()) {
    os << "  BARs: unknown\n";
  }
  for (const auto& ln : s.bar_lines) os << "  " << ln << "\n";
  os << "  Link current: width=" << s.link.cur_width
     << " speed=" << s.link.cur_speed << "\n";
  os << "  Link max: width=" << s.link.max_width
     << " speed=" << s.link.max_speed << "\n";
  if (s.resize.empty()) {
    os << "  Resizable: unknown\n";
  }
  for (const auto& r : s.resize) os << "  " << r.name << ": " << r.value << "\n";
  os << "  IOMMU group: " << s.iommu << "\n";
  os << "  NUMA node: " << s.numa << "\n";
  os << "  lspci -vv excerpt:\n";
  for (const auto& ln : split_lines_capped(s.lspci, 80)) os << "    " << ln << "\n";
  os << "Driver:\n";
  os << "  version_file:\n";
  for (const auto& ln : split_lines_capped(s.version_text, 6)) {
    os << "    " << ln << "\n";
  }
  os << "  flavor: " << s.flavor << " (\"open\" se version contem Open)\n";
  os << "  modinfo: " << s.modinfo << "\n";
  os << "  modules: " << s.modules << "\n";
  os << "  userspace (nvidia-smi): " << s.userspace << "\n";
  os << "  exposed params:\n";
  if (s.params.empty()) {
    os << "    unknown\n";
  }
  for (const auto& p : s.params) os << "    " << p << "\n";
  os << "DRM:\n";
  if (s.nodes.empty()) {
    os << "  nodes: unknown\n";
  }
  for (const auto& n : s.nodes) {
    os << "  " << n.name << ": " << n.dri_path;
    if (!n.dri_present) os << " (missing)";
    os << " (" << n.major_minor << ") slot=" << n.pci_slot << "\n";
  }
  os << "  drm driver: " << s.drm_driver << "\n";
  os << "  connectors:\n";
  if (s.connectors.empty()) {
    os << "    unknown\n";
  }
  for (const auto& c : s.connectors) {
    os << "    " << c.name << ": status=" << c.status << " modes=" << c.modes
       << "\n";
  }
  os << "Memory:\n";
  os << "  VRAM total: " << s.vram_total << "\n";
  os << "  VRAM used: " << s.vram_used << "\n";
  os << "  BAR1 aperture: " << s.bar1_size << "\n";
  os << "  Note: " << kBarNote << "\n";
  os << "GSP (passive, exposed only):\n";
  os << "  GPU Firmware: " << s.gpu_firmware << "\n";
  os << "Vulkan:\n";
  for (const auto& ln : split_lines_capped(s.vulkan, 80)) os << "  " << ln << "\n";
  os << "Logs (filtered kernel excerpt, max 100):\n";
  if (s.logs.empty()) {
    os << "  unknown (sem linhas filtradas ou journalctl ausente)\n";
  }
  for (const auto& ln : s.logs) os << "  " << ln << "\n";
  os << "In-use (passive view, no action taken):\n";
  os << "  compute-apps (nvidia-smi):\n";
  for (const auto& ln : split_lines_capped(s.compute_apps, 64)) {
    os << "    " << ln << "\n";
  }
  os << "  drm holders (via /proc fd):\n";
  if (s.holders.empty()) {
    os << "    unknown (nenhum holder visivel)\n";
  }
  for (const auto& h : s.holders) os << "    " << h << "\n";
  os << "LabReadiness (read-only, vide `ga106-lab lab-status`):\n";
  os << "  alternative_gpu_present: "
     << (s.readiness.alt_present ? "true" : "false") << "\n";
  os << "  desktop_gpu: "
     << (s.readiness.has_desktop_gpu ? s.readiness.desktop_gpu : "unknown")
     << "\n";
  os << "  target_gpu_used_by_desktop: "
     << (!s.readiness.used_known
             ? "unknown"
             : (s.readiness.target_used ? "true" : "false"))
     << "\n";
  os << "  target_gpu_active_connectors: "
     << (s.readiness.has_active ? std::to_string(s.readiness.active_count)
                                : "unknown")
     << "\n";
  os << "  status: " << s.readiness.status << "\n";
  os << "  desktop_independence: " << s.readiness.desktop_independence << "\n";
  os << "  mmio_readiness: " << s.readiness.mmio_readiness << "\n";
  os << "Provenance:\n";
  os << "  snapshot reproduzivel: reexecute `ga106-lab baseline [--out-dir "
        "DIR] [--stdout]` no mesmo host para comparar\n";
  os << "  outputs: baseline.txt (este arquivo) + baseline.json (mesmos "
        "campos, formato maquina)\n";
  return os.str();
}

void json_kv(std::ostringstream& os, const std::string& key,
             const std::string& val, bool comma) {
  os << "    \"" << json_escape(key) << "\": \"" << json_escape(val) << "\"";
  if (comma) os << ",";
  os << "\n";
}

std::string render_json(const Snapshot& s) {
  std::ostringstream os;
  os << "{\n";
  os << "  \"meta\": {\n";
  json_kv(os, "tool", std::string("ga106-lab ") + kVersion, true);
  json_kv(os, "timestamp_utc", s.ts, true);
  json_kv(os, "hostname", s.hostname, true);
  json_kv(os, "kernel", s.kernel, true);
  os << "    \"gpu_count\": " << s.gpu_count << "\n";
  os << "  },\n";
  os << "  \"identity\": {\n";
  json_kv(os, "bdf", s.bdf, true);
  json_kv(os, "vendor", s.vendor, true);
  json_kv(os, "device", s.device, true);
  json_kv(os, "subsystem", s.subsys, true);
  json_kv(os, "revision", s.revision, true);
  json_kv(os, "arch", s.arch, true);
  json_kv(os, "chip", s.chip, true);
  json_kv(os, "model", s.model, true);
  json_kv(os, "vbios", s.vbios, true);
  json_kv(os, "uuid", s.uuid, true);
  json_kv(os, "kernel_driver", s.kernel_driver, false);
  os << "  },\n";
  os << "  \"pci\": {\n";
  os << "    \"bars\": [";
  for (std::size_t i = 0; i < s.bar_lines.size(); ++i) {
    if (i) os << ", ";
    os << "\"" << json_escape(s.bar_lines[i]) << "\"";
  }
  os << "],\n";
  json_kv(os, "link_current_width", s.link.cur_width, true);
  json_kv(os, "link_current_speed", s.link.cur_speed, true);
  json_kv(os, "link_max_width", s.link.max_width, true);
  json_kv(os, "link_max_speed", s.link.max_speed, true);
  os << "    \"resizable\": [";
  for (std::size_t i = 0; i < s.resize.size(); ++i) {
    if (i) os << ", ";
    os << "\"" << json_escape(s.resize[i].name + "=" + s.resize[i].value)
       << "\"";
  }
  os << "],\n";
  json_kv(os, "iommu_group", s.iommu, true);
  json_kv(os, "numa_node", s.numa, true);
  json_kv(os, "lspci_excerpt", s.lspci, false);
  os << "  },\n";
  os << "  \"driver\": {\n";
  json_kv(os, "version_file", s.version_text, true);
  json_kv(os, "flavor", s.flavor, true);
  json_kv(os, "modinfo", s.modinfo, true);
  json_kv(os, "modules", s.modules, true);
  json_kv(os, "userspace", s.userspace, true);
  json_kv(os, "gpu_firmware", s.gpu_firmware, true);
  os << "    \"exposed_params\": [";
  for (std::size_t i = 0; i < s.params.size(); ++i) {
    if (i) os << ", ";
    os << "\"" << json_escape(s.params[i]) << "\"";
  }
  os << "]\n";
  os << "  },\n";
  os << "  \"drm\": {\n";
  os << "    \"nodes\": [";
  for (std::size_t i = 0; i < s.nodes.size(); ++i) {
    if (i) os << ", ";
    const auto& n = s.nodes[i];
    std::string desc = n.name + " " + n.dri_path + " " + n.major_minor +
                       " slot=" + n.pci_slot;
    if (!n.dri_present) desc += " (missing)";
    os << "\"" << json_escape(desc) << "\"";
  }
  os << "],\n";
  json_kv(os, "drm_driver", s.drm_driver, true);
  os << "    \"connectors\": [";
  for (std::size_t i = 0; i < s.connectors.size(); ++i) {
    if (i) os << ", ";
    const auto& c = s.connectors[i];
    os << "\""
       << json_escape(c.name + " status=" + c.status + " modes=" + c.modes)
       << "\"";
  }
  os << "]\n";
  os << "  },\n";
  os << "  \"memory\": {\n";
  json_kv(os, "vram_total", s.vram_total, true);
  json_kv(os, "vram_used", s.vram_used, true);
  json_kv(os, "bar1_aperture", s.bar1_size, true);
  json_kv(os, "note", kBarNote, false);
  os << "  },\n";
  os << "  \"gsp\": {\n";
  json_kv(os, "gpu_firmware", s.gpu_firmware, false);
  os << "  },\n";
  os << "  \"vulkan\": {\n";
  json_kv(os, "summary_excerpt", s.vulkan, false);
  os << "  },\n";
  os << "  \"logs\": [";
  for (std::size_t i = 0; i < s.logs.size(); ++i) {
    if (i) os << ", ";
    os << "\"" << json_escape(s.logs[i]) << "\"";
  }
  os << "],\n";
  os << "  \"in_use\": {\n";
  json_kv(os, "compute_apps", s.compute_apps, true);
  os << "    \"drm_holders\": [";
  for (std::size_t i = 0; i < s.holders.size(); ++i) {
    if (i) os << ", ";
    os << "\"" << json_escape(s.holders[i]) << "\"";
  }
  os << "]\n";
  os << "  },\n";
  os << "  \"lab_readiness\": {\n";
  os << "    \"alternative_gpu_present\": "
     << (s.readiness.alt_present ? "true" : "false") << ",\n";
  if (s.readiness.has_desktop_gpu) {
    os << "    \"desktop_gpu\": \"" << json_escape(s.readiness.desktop_gpu)
       << "\",\n";
  } else {
    os << "    \"desktop_gpu\": null,\n";
  }
  if (s.readiness.used_known) {
    os << "    \"target_gpu_used_by_desktop\": "
       << (s.readiness.target_used ? "true" : "false") << ",\n";
  } else {
    os << "    \"target_gpu_used_by_desktop\": null,\n";
  }
  if (s.readiness.has_active) {
    os << "    \"target_gpu_active_connectors\": " << s.readiness.active_count
       << ",\n";
  } else {
    os << "    \"target_gpu_active_connectors\": null,\n";
  }
  os << "    \"status\": \"" << json_escape(s.readiness.status) << "\",\n";
  os << "    \"desktop_independence\": \""
     << json_escape(s.readiness.desktop_independence) << "\",\n";
  os << "    \"mmio_readiness\": \"" << json_escape(s.readiness.mmio_readiness)
     << "\"\n";
  os << "  }\n";
  os << "}\n";
  return os.str();
}

int baseline_usage() {
  std::cerr << kTag << " uso: ga106-lab baseline [--out-dir DIR] [--stdout]\n";
  std::cerr << kTag
            << "   baseline gera snapshot reproduzivel em "
               "artifacts/baselines/<timestamp>/ (ou DIR)\n";
  return 2;
}

}  // namespace

int run_baseline(const std::vector<std::string>& args) {
  std::string out_dir;
  bool has_out_dir = false;
  bool to_stdout = false;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    if (a == "--stdout") {
      to_stdout = true;
    } else if (a == "--out-dir") {
      if (i + 1 >= args.size()) return baseline_usage();
      out_dir = args[++i];
      if (out_dir.empty()) return baseline_usage();
      has_out_dir = true;
    } else if (a == "-h" || a == "--help") {
      std::cout << "ga106-lab " << kVersion << " (Fase 0, somente leitura)\n";
      std::cout << kTag << " uso: ga106-lab baseline [--out-dir DIR] [--stdout]\n";
      return 0;
    } else {
      return baseline_usage();
    }
  }

  const Snapshot snap = collect_snapshot();
  const std::string txt = render_txt(snap);
  const std::string js = render_json(snap);

  std::string base;
  if (has_out_dir) {
    base = out_dir;
  } else {
    base = std::string("artifacts/baselines/") + snap.ts;
  }
  if (!ensure_dir(base)) {
    std::cerr << kTag << " falha ao criar diretorio de saida: " << base
              << "\n";
    return 1;
  }
  const std::string txt_path = base + "/baseline.txt";
  const std::string json_path = base + "/baseline.json";
  if (!write_text_file(txt_path, txt)) {
    std::cerr << kTag << " falha ao gravar " << txt_path << "\n";
    return 1;
  }
  if (!write_text_file(json_path, js)) {
    std::cerr << kTag << " falha ao gravar " << json_path << "\n";
    return 1;
  }
  std::cerr << kTag << " baseline para " << snap.bdf << " em " << base << "\n";
  if (to_stdout) std::cout << txt;
  return 0;
}

}  // namespace ga106lab
