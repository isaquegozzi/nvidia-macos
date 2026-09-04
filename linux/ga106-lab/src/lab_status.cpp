// ga106-lab lab-status (somente leitura, Fase 0).
// Agrega ownership do alvo 0000:01:00.0 (driver via readlink, boot_vga,
// fb/fbcon, DRM + conectores status/enabled, processos GNOME via /proc,
// compute-apps opcional) e procura a alternativa 1002:1638 (Cezanne).
// Veredito conservador: BLOCKED salvo se iGPU presente E sessao grafica na
// iGPU E RTX sem conector ativo usado E sem clientes desktop no alvo.
// Nenhuma escrita, nenhum open alem de O_RDONLY, nenhum sinal a processos.

#include "lab_status.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include "drm_discovery.hpp"

namespace ga106lab {
namespace {

constexpr const char* kTag = "[ga106-lab]";

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

std::string lowercase_of(std::string s) {
  for (auto& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool is_all_digits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
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

// Roda uma linha de inspecao e captura a saida (leitura, com teto).
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

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) out.push_back(line);
  return out;
}

// Comunicadores/compositores de sessao grafica (comparacao por prefixo do
// comm, que o kernel trunca em 15 caracteres).
bool is_desktop_comm(const std::string& comm) {
  static const char* kNames[] = {"gnome-shell", "mutter",     "Xorg",
                                 "Xwayland",    "gnome-session", "gdm",
                                 "kwin",        "plasmashell", "weston",
                                 "sway",        "cage",       "cosmic-comp"};
  for (const char* n : kNames) {
    if (comm.compare(0, std::string(n).size(), n) == 0) return true;
  }
  return false;
}

// "pid comm" dos processos desktop visiveis via /proc (somente leitura).
std::vector<std::string> scan_desktop_procs() {
  std::vector<std::string> out;
  std::size_t scanned = 0;
  for (const auto& pid : list_dir_names("/proc")) {
    if (!is_all_digits(pid)) continue;
    if (++scanned > 4096) break;
    const std::string comm = read_first_line("/proc/" + pid + "/comm");
    if (comm.empty() || !is_desktop_comm(comm)) continue;
    out.push_back(pid + " " + comm);
    if (out.size() >= 64) break;
  }
  return out;
}

// Processos com fd aberto em algum /dev/dri/<node> do alvo (leitura).
std::vector<std::string> scan_target_holders(
    const std::vector<std::string>& node_names) {
  std::vector<std::string> out;
  if (node_names.empty()) return out;
  std::size_t scanned = 0;
  for (const auto& pid : list_dir_names("/proc")) {
    if (!is_all_digits(pid)) continue;
    if (++scanned > 4096) break;
    const std::string fddir = "/proc/" + pid + "/fd";
    std::error_code ec;
    std::filesystem::directory_iterator it(fddir, ec);
    if (ec) continue;
    std::size_t fds = 0;
    for (const auto& entry : it) {
      if (++fds > 256) break;
      const std::string link = readlink_target(entry.path().string());
      if (!starts_with(link, "/dev/dri/")) continue;
      const std::string base = basename_of(link);
      bool hit = false;
      for (const auto& w : node_names) {
        if (base == w) {
          hit = true;
          break;
        }
      }
      if (!hit) continue;
      const std::string comm = read_first_line("/proc/" + pid + "/comm");
      out.push_back(pid + " " + (comm.empty() ? "unknown" : comm) + " " +
                    link);
      break;
    }
    if (out.size() >= 64) return out;
  }
  return out;
}

// O no DRM (cardN) dono do prefixo de um conector "cardN-<saida>".
bool connector_owned_by(const std::string& name, const std::string& bdf,
                        const std::vector<std::string>& node_names) {
  const auto dash = name.find('-');
  if (dash != std::string::npos) {
    const std::string head = name.substr(0, dash);
    for (const auto& n : node_names) {
      if (head == n) return true;
    }
  }
  // Fallback: segue a cadeia de symlinks `device` (conector -> cardN ->
  // dispositivo PCI) e procura o BDF em cada nivel.
  std::string link = "/sys/class/drm/" + name;
  for (int hop = 0; hop < 4; ++hop) {
    const std::string target = readlink_target(link + "/device");
    if (target.empty()) break;
    if (target.find(bdf) != std::string::npos) return true;
    if (!starts_with(target, "..")) break;
    // Resolve relativo a /sys/class/drm/<atual>.
    const std::string cur = basename_of(link);
    std::string resolved = "/sys/class/drm/" + cur + "/" + target;
    // Normaliza "/./" e "/x/../" de forma minima (sem filesystem::canonical,
    // que pode falhar em sysfs).
    std::string norm;
    std::istringstream parts(resolved);
    std::string tok;
    std::vector<std::string> stack;
    while (std::getline(parts, tok, '/')) {
      if (tok.empty() || tok == ".") continue;
      if (tok == "..") {
        if (!stack.empty()) stack.pop_back();
        continue;
      }
      stack.push_back(tok);
    }
    for (const auto& p : stack) norm += "/" + p;
    link = norm;
  }
  // Ultimo recurso: PCI_SLOT_NAME no uevent do dispositivo.
  const int fd = ::open(
      ("/sys/class/drm/" + name + "/device/uevent").c_str(), O_RDONLY);
  if (fd < 0) return false;
  std::string content;
  std::array<char, 1024> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    content.append(buf.data(), static_cast<std::size_t>(n));
    if (content.size() > 65536) break;
  }
  ::close(fd);
  return content.find("PCI_SLOT_NAME=" + bdf) != std::string::npos;
}

// Conectores cardN-<saida> pertencentes ao BDF (status+enabled, leitura).
std::vector<ConnectorStatus> connectors_for_bdf(const std::string& bdf) {
  std::vector<ConnectorStatus> out;
  std::vector<std::string> node_names;
  for (const auto& n : discover_drm_nodes(bdf)) node_names.push_back(n.name);
  for (const auto& name : list_dir_names("/sys/class/drm")) {
    if (name.find('-') == std::string::npos) continue;
    if (!connector_owned_by(name, bdf, node_names)) continue;
    ConnectorStatus c;
    c.name = name;
    const std::string st =
        read_first_line("/sys/class/drm/" + name + "/status");
    if (!st.empty()) c.status = st;
    const std::string en =
        read_first_line("/sys/class/drm/" + name + "/enabled");
    if (!en.empty()) c.enabled = en;
    out.push_back(c);
    if (out.size() >= 32) break;
  }
  return out;
}

int count_active(const std::vector<ConnectorStatus>& cs) {
  int n = 0;
  for (const auto& c : cs) {
    if (c.active()) ++n;
  }
  return n;
}

std::string join_names(const std::vector<ConnectorStatus>& cs, bool active_only,
                       const std::string& empty_text) {
  std::string out;
  for (const auto& c : cs) {
    if (active_only && !c.active()) continue;
    if (!out.empty()) out += ", ";
    out += c.name;
  }
  return out.empty() ? empty_text : out;
}

}  // namespace

LabStatus collect_lab_status() {
  LabStatus st;
  const std::string tdir =
      std::string("/sys/bus/pci/devices/") + kLabTargetBdf;

  // --- alvo: identidade via sysfs ---
  {
    const std::string v = read_first_line(tdir + "/vendor");
    const std::string d = read_first_line(tdir + "/device");
    if (v.empty() || d.empty()) {
      st.target_present = false;
      st.verdict = "unknown";
      st.verdict_reasons.push_back(
          std::string("alvo ") + kLabTargetBdf +
          " nao detectavel em /sys/bus/pci/devices (sem inventar estado)");
      return st;  // sem alvo, o resto nao e avaliavel
    }
    st.target_present = true;
    st.vendor = v;
    st.device = d;
    const std::string sv = read_first_line(tdir + "/subsystem_vendor");
    const std::string sd = read_first_line(tdir + "/subsystem_device");
    if (!sv.empty() && !sd.empty()) st.subsys = sv + ":" + sd;
    const std::string rev = read_first_line(tdir + "/revision");
    if (!rev.empty()) st.revision = rev;
    st.is_expected_ga106 =
        (lowercase_of(v) == kLabTargetVendor &&
         lowercase_of(d) == kLabTargetDevice);
  }

  // --- ownership: driver, boot VGA ---
  {
    const std::string drv = readlink_target(tdir + "/driver");
    if (!drv.empty()) st.kernel_driver = basename_of(drv);
    const std::string bv = read_first_line(tdir + "/boot_vga");
    if (!bv.empty()) st.boot_vga = bv;
  }

  // --- fb/fbcon (leitura) ---
  {
    std::string fbs;
    for (const auto& e : list_dir_names("/sys/class/graphics")) {
      if (!starts_with(e, "fb")) continue;
      const std::string nm =
          read_first_line("/sys/class/graphics/" + e + "/name");
      if (nm.empty()) continue;
      if (e == "fbcon") continue;  // fbcon e no virtual; vai p/ hint abaixo
      if (!fbs.empty()) fbs += ", ";
      fbs += e + "=" + nm;
      if (st.fb_owner == "unknown") st.fb_owner = nm;
    }
    if (!fbs.empty() && st.fb_owner == "unknown") st.fb_owner = fbs;
    if (fbs.empty()) st.fb_owner = "unknown";
    const std::string fbcon =
        shell_capture("dmesg 2>/dev/null | grep -i -m 5 fbcon", 2048, 8);
    if (!fbcon.empty()) st.fbcon_hint = fbcon;
  }

  // --- DRM do alvo + holders ---
  std::vector<std::string> node_names;
  {
    const auto nodes = discover_drm_nodes(kLabTargetBdf);
    for (const auto& n : nodes) {
      node_names.push_back(n.name);
      st.drm_nodes.push_back(n.name + " " + n.dri_path + " " +
                             n.major_minor + " slot=" + n.pci_slot);
    }
    st.connectors = connectors_for_bdf(kLabTargetBdf);
    st.target_holders = scan_target_holders(node_names);
  }
  st.desktop_procs = scan_desktop_procs();

  // --- DRM primary: heuristica read-only via holders do compositor ---
  {
    std::string holder_card;
    std::string holder_who;
    int holder_cards = 0;
    for (const auto& h : st.target_holders) {
      // formato "pid comm /dev/dri/<node>"
      std::istringstream words(h);
      std::string pid, comm, link;
      words >> pid >> comm >> link;
      if (!is_desktop_comm(comm)) continue;
      const std::string card = basename_of(link);
      // Render nodes (renderD*) nunca tem master: so cards contam.
      if (!starts_with(card, "card")) continue;
      if (holder_card.empty()) {
        holder_card = card;
        holder_who = pid + " " + comm;
        holder_cards = 1;
      } else if (card != holder_card) {
        ++holder_cards;
      }
    }
    if (holder_cards == 1) {
      st.drm_primary = holder_card + " (via holder " + holder_who + ")";
      st.drm_primary_note =
          "heuristica read-only: unico card do alvo com fd do compositor; "
          "master DRM nao e exposto pelo sysfs";
    } else {
      st.drm_primary = "unknown";
      st.drm_primary_note =
          "master DRM nao e detectavel via sysfs sozinho "
          "(holders listados acima; debugfs nao consultado)";
    }
  }

  // --- compute-apps opcional (nvidia-smi, leitura) ---
  {
    const std::string out = shell_capture(
        "nvidia-smi --query-compute-apps=pid,process_name,used_memory "
        "--format=csv,noheader 2>/dev/null",
        8192, 64);
    if (!out.empty()) {
      st.compute_queried = true;
      st.compute_raw = out;
      for (const auto& ln : split_lines(out)) {
        const std::string low = lowercase_of(ln);
        // Nome do processo aparece antes da primeira virgula ou no texto.
        std::istringstream words(trim(ln));
        std::string tok;
        while (words >> tok) {
          std::string clean;
          for (char c : tok) {
            if (c == ',' || c == '/' || c == '(' || c == ')') {
              clean += ' ';
            } else {
              clean += c;
            }
          }
          std::istringstream parts(clean);
          std::string p;
          while (parts >> p) {
            if (is_desktop_comm(p) || is_desktop_comm(basename_of(p))) {
              st.compute_desktop.push_back(trim(ln));
              break;
            }
          }
          if (!st.compute_desktop.empty() &&
              st.compute_desktop.back() == trim(ln)) {
            break;
          }
        }
        (void)low;
      }
    }
  }

  // --- alternativa 1002:1638 (Cezanne) ---
  {
    std::string found;
    for (const auto& bdf : list_dir_names("/sys/bus/pci/devices")) {
      const std::string dir = "/sys/bus/pci/devices/" + bdf;
      const std::string v = lowercase_of(read_first_line(dir + "/vendor"));
      const std::string d = lowercase_of(read_first_line(dir + "/device"));
      if (v == kLabAltVendor && d == kLabAltDevice) {
        found = bdf;
        break;
      }
    }
    if (found.empty()) {
      st.alt.present = false;
    } else {
      st.alt.present = true;
      st.alt.bdf = found;
      const std::string dir = "/sys/bus/pci/devices/" + found;
      const std::string v = read_first_line(dir + "/vendor");
      const std::string d = read_first_line(dir + "/device");
      if (!v.empty()) st.alt.vendor = v;
      if (!d.empty()) st.alt.device = d;
      const std::string cl = read_first_line(dir + "/class");
      if (!cl.empty()) st.alt.pci_class = cl;
      const std::string drv = readlink_target(dir + "/driver");
      if (!drv.empty()) st.alt.driver = basename_of(drv);
      const std::string bv = read_first_line(dir + "/boot_vga");
      if (!bv.empty()) st.alt.boot_vga = bv;
      for (const auto& n : discover_drm_nodes(found)) {
        st.alt.drm_nodes.push_back(n.name + " " + n.dri_path);
      }
      st.alt.connectors = connectors_for_bdf(found);
    }
  }

  // --- sintese: qual GPU hospeda o desktop? ---
  const int target_active = count_active(st.connectors);
  const int alt_active =
      st.alt.present ? count_active(st.alt.connectors) : 0;
  const bool fb_on_target =
      st.fb_owner != "unknown" &&
      lowercase_of(st.fb_owner).find("nvidia") != std::string::npos;
  const bool fb_on_alt =
      st.fb_owner != "unknown" &&
      (lowercase_of(st.fb_owner).find("amd") != std::string::npos ||
       lowercase_of(st.fb_owner).find("amdgpu") != std::string::npos);
  bool compositor_on_target = false;
  bool compositor_on_alt = false;
  for (const auto& h : st.target_holders) {
    std::istringstream words(h);
    std::string pid, comm;
    words >> pid >> comm;
    if (is_desktop_comm(comm)) compositor_on_target = true;
  }
  if (st.alt.present) {
    std::vector<std::string> alt_nodes;
    for (const auto& n : discover_drm_nodes(st.alt.bdf)) {
      alt_nodes.push_back(n.name);
    }
    for (const auto& h : scan_target_holders(alt_nodes)) {
      std::istringstream words(h);
      std::string pid, comm;
      words >> pid >> comm;
      if (is_desktop_comm(comm)) compositor_on_alt = true;
    }
  }
  if (compositor_on_target) {
    st.desktop_gpu = kLabTargetBdf;
  } else if (fb_on_target) {
    st.desktop_gpu = kLabTargetBdf;
  } else if (target_active > 0) {
    st.desktop_gpu = kLabTargetBdf;
  } else if (st.alt.present &&
             (compositor_on_alt || fb_on_alt || alt_active > 0)) {
    st.desktop_gpu = st.alt.bdf;
  } else {
    st.desktop_gpu = "unknown";
  }

  st.target_used_known = true;
  st.target_used_by_desktop =
      target_active > 0 || fb_on_target || compositor_on_target ||
      st.desktop_gpu == kLabTargetBdf;

  st.session_on_alt =
      st.alt.present &&
      (compositor_on_alt || fb_on_alt || st.alt.boot_vga == "1" ||
       (alt_active > 0 && target_active == 0));

  // --- veredito (fail-closed) ---
  {
    auto& rs = st.verdict_reasons;
    rs.push_back(std::string("alvo ") + kLabTargetBdf +
                 (st.is_expected_ga106 ? " confere (10de:2504 GA106)"
                                       : " DIVERGE do esperado 10de:2504 (" +
                                             st.vendor + ":" + st.device +
                                             ")"));
    rs.push_back("kernel driver do alvo: " + st.kernel_driver);
    rs.push_back("boot_vga do alvo: " + st.boot_vga);
    rs.push_back("fb owner: " + st.fb_owner);
    rs.push_back("conectores ativos no alvo: " +
                 std::to_string(target_active) + " (" +
                 join_names(st.connectors, true, "nenhum") + ")");
    rs.push_back("processos desktop visiveis (/proc): " +
                 std::to_string(st.desktop_procs.size()));
    rs.push_back("holders desktop nos fds DRM do alvo: " +
                 std::string(compositor_on_target ? "sim" : "nao"));
    if (st.alt.present) {
      rs.push_back("alternativa 1002:1638 PRESENTE em " + st.alt.bdf +
                   " (driver=" + st.alt.driver +
                   ", boot_vga=" + st.alt.boot_vga + ", ativos=" +
                   std::to_string(alt_active) + ")");
    } else {
      rs.push_back("alternativa 1002:1638 NOT PRESENT "
                   "(nenhum 0x1002:0x1638 em /sys/bus/pci/devices)");
    }
    rs.push_back(std::string("sessao grafica na iGPU: ") +
                 (st.session_on_alt ? "sim" : "nao"));
    rs.push_back(std::string("clientes desktop no alvo: ") +
                 (st.target_used_by_desktop ? "sim" : "nao"));

    const bool compute_free =
        st.compute_queried && st.compute_desktop.empty();
    if (st.compute_queried) {
      rs.push_back("compute-apps nvidia-smi: " +
                   std::to_string(st.compute_desktop.size()) +
                   " processo(s) desktop");
    } else {
      rs.push_back("compute-apps nvidia-smi: nao consultavel "
                   "(ferramenta ausente ou sem saida)");
    }

    // READY exige TODAS; presenca da iGPU sozinha nunca basta.
    if (st.alt.present && st.session_on_alt && target_active == 0 &&
        !st.target_used_by_desktop && !compositor_on_target &&
        compute_free) {
      st.verdict = "READY_FOR_NEXT_PHASE";
      rs.push_back("veredito: todas as condicoes de liberacao valem");
    } else {
      st.verdict = "BLOCKED";
      if (!st.alt.present) {
        rs.push_back("veredito BLOCKED: sem iGPU alternativa "
                     "(e presenca isolada de outra GPU jamais implicaria "
                     "READY)");
      } else {
        rs.push_back("veredito BLOCKED: ao menos uma condicao de liberacao "
                     "falha (ver itens acima)");
      }
    }
  }
  return st;
}

LabReadiness lab_readiness_from(const LabStatus& st) {
  LabReadiness r;
  r.alt_present = st.alt.present;
  if (st.desktop_gpu != "unknown") {
    r.has_desktop_gpu = true;
    r.desktop_gpu = st.desktop_gpu;
  }
  r.used_known = st.target_present && st.target_used_known;
  r.target_used = st.target_used_by_desktop;
  if (st.target_present) {
    r.has_active = true;
    r.active_count = count_active(st.connectors);
  }
  r.status = st.verdict;
  return r;
}

std::string render_lab_status(const LabStatus& st) {
  std::ostringstream os;
  os << kTag << " lab-status for " << kLabTargetBdf << "\n";
  os << "Target GPU:\n";
  os << "  BDF: " << kLabTargetBdf
     << (st.target_present ? " (present)" : " (NOT PRESENT)") << "\n";
  if (!st.target_present) {
    os << "  detail: unknown (nao detectavel via sysfs; sem inventar)\n";
    os << "Verdict: " << st.verdict << "\n";
    for (const auto& r : st.verdict_reasons) os << "  - " << r << "\n";
    return os.str();
  }
  os << "  Vendor/Device: " << st.vendor << " / " << st.device
     << (st.is_expected_ga106 ? " (esperado 10de:2504 GA106: match)"
                              : " (DIVERGE do esperado 10de:2504)") << "\n";
  os << "  Subsystem: " << st.subsys << "\n";
  os << "  Revision: " << st.revision << "\n";
  os << "Ownership (read-only):\n";
  os << "  Kernel driver: " << st.kernel_driver << " (via driver readlink)\n";
  os << "  boot_vga: " << st.boot_vga << "\n";
  os << "  fb owner: " << st.fb_owner << " (via /sys/class/graphics/fb*/name)\n";
  os << "  fbcon hint: ";
  if (st.fbcon_hint == "unknown") {
    os << "unknown (dmesg indisponivel ou sem linhas fbcon)\n";
  } else {
    os << "(dmesg, somente leitura)\n";
    for (const auto& ln : split_lines(st.fbcon_hint)) os << "    " << ln << "\n";
  }
  os << "  DRM nodes:\n";
  if (st.drm_nodes.empty()) os << "    unknown (sem no associado ao BDF)\n";
  for (const auto& n : st.drm_nodes) os << "    " << n << "\n";
  os << "  DRM primary: " << st.drm_primary << "\n";
  os << "    note: " << st.drm_primary_note << "\n";
  os << "  Active connectors (status+enabled):\n";
  if (st.connectors.empty()) {
    os << "    unknown (sem conectores visiveis para o BDF)\n";
  }
  for (const auto& c : st.connectors) {
    os << "    " << c.name << ": status=" << c.status
       << " enabled=" << c.enabled << (c.active() ? " [ACTIVE]" : "") << "\n";
  }
  os << "  Desktop processes (via /proc scan, somente leitura):\n";
  if (st.desktop_procs.empty()) {
    os << "    none visible\n";
  }
  for (const auto& p : st.desktop_procs) os << "    " << p << "\n";
  os << "  Target DRM holders (via /proc fd, somente leitura):\n";
  if (st.target_holders.empty()) {
    os << "    none visible\n";
  }
  for (const auto& h : st.target_holders) os << "    " << h << "\n";
  os << "  Compute apps (nvidia-smi, opcional):\n";
  if (!st.compute_queried) {
    os << "    unknown (nvidia-smi ausente ou sem saida)\n";
  } else if (st.compute_raw.empty()) {
    os << "    none\n";
  } else {
    for (const auto& ln : split_lines(st.compute_raw)) os << "    " << ln << "\n";
  }
  os << "Alternative GPU (1002:1638 Cezanne):\n";
  if (!st.alt.present) {
    os << "  NOT PRESENT (nenhum 0x1002:0x1638 em /sys/bus/pci/devices)\n";
  } else {
    os << "  BDF: " << st.alt.bdf << "\n";
    os << "  Vendor/Device: " << st.alt.vendor << " / " << st.alt.device
       << "\n";
    os << "  Class: " << st.alt.pci_class << "\n";
    os << "  Driver: " << st.alt.driver << "\n";
    os << "  boot_vga: " << st.alt.boot_vga << "\n";
    os << "  DRM nodes:\n";
    if (st.alt.drm_nodes.empty()) os << "    unknown\n";
    for (const auto& n : st.alt.drm_nodes) os << "    " << n << "\n";
    os << "  Connectors:\n";
    if (st.alt.connectors.empty()) os << "    unknown\n";
    for (const auto& c : st.alt.connectors) {
      os << "    " << c.name << ": status=" << c.status
         << " enabled=" << c.enabled << (c.active() ? " [ACTIVE]" : "") << "\n";
    }
  }
  os << "Desktop GPU: " << st.desktop_gpu << "\n";
  os << "Verdict: " << st.verdict << "\n";
  for (const auto& r : st.verdict_reasons) os << "  - " << r << "\n";
  os << "  (READY_FOR_NEXT_PHASE exige iGPU presente E sessao na iGPU E "
        "RTX sem conector ativo usado E sem clientes desktop no alvo; "
        "outra GPU sozinha nunca implica READY)\n";
  return os.str();
}

int run_lab_status(const std::vector<std::string>& args) {
  for (const auto& a : args) {
    if (a == "-h" || a == "--help") {
      std::cout << "ga106-lab lab-status (Fase 0, somente leitura)\n";
      std::cout << kTag
                << " uso: ga106-lab lab-status\n";
      return 0;
    }
    std::cerr << kTag << " uso: ga106-lab lab-status\n";
    return 2;
  }
  const LabStatus st = collect_lab_status();
  std::cout << render_lab_status(st);
  return 0;
}

}  // namespace ga106lab
