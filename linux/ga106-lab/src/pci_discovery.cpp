// ga106-lab: descoberta passiva de GPUs NVIDIA via sysfs (somente leitura).
// Contrato Fase 0: apenas varredura e impressao. Este arquivo abre arquivos
// com a flag de abertura read-only, usa readlink/readdir para metadados e
// nunca altera estado do dispositivo.

#include "pci_discovery.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace ga106lab {
namespace {

constexpr std::uint16_t kNvidiaVendor = 0x10DE;

// Bits de recurso do kernel usados apenas para decodificar leitura.
// Cobrem kernels antigos e novos: o nibble baixo preserva os bits de tipo
// do BAR PCI (bit3 = prefetchable, bits2:1 = 10 para 64-bit), e os bits
// altos trazem MEM_64 (estavel) e PREFETCH (0x2000 nos kernels recentes).
constexpr std::uint64_t kFlagIo = 0x100;
constexpr std::uint64_t kFlagMem = 0x200;
constexpr std::uint64_t kFlagBarPrefetch = 0x8;
constexpr std::uint64_t kFlagBarTypeMask = 0x6;
constexpr std::uint64_t kFlagBarType64 = 0x4;
constexpr std::uint64_t kFlagPrefetch = 0x2000;
constexpr std::uint64_t kFlagPrefetchLegacy = 0x800;
constexpr std::uint64_t kFlagMem64 = 0x100000;

std::string trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  std::size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
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

std::optional<unsigned long> parse_number(const std::string& text) {
  const std::string t = trim(text);
  if (t.empty()) return std::nullopt;
  unsigned long v = 0;
  const char* first = t.c_str();
  const char* last = first + t.size();
  int base = 10;
  const char* digits = first;
  if (t.size() > 1 && first[0] == '0' && (first[1] == 'x' || first[1] == 'X')) {
    base = 16;
    digits = first + 2;
  }
  auto res = std::from_chars(digits, last, v, base);
  if (res.ec != std::errc{} || res.ptr != last) return std::nullopt;
  return v;
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

}  // namespace

std::string hex_u64(std::uint64_t v) {
  std::ostringstream os;
  os << "0x" << std::hex << std::nouppercase << v;
  return os.str();
}

std::string hex_u16(std::uint16_t v) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string out = "0x0000";
  out[2] = kHex[(v >> 12) & 0xF];
  out[3] = kHex[(v >> 8) & 0xF];
  out[4] = kHex[(v >> 4) & 0xF];
  out[5] = kHex[v & 0xF];
  return out;
}

std::string bar_flags_text(std::uint64_t flags, bool is_io) {
  if (is_io) return "io";
  const bool is64 = (flags & kFlagMem64) != 0 ||
                    ((flags & kFlagBarTypeMask) == kFlagBarType64);
  const bool pref = (flags & (kFlagPrefetch | kFlagPrefetchLegacy |
                              kFlagBarPrefetch)) != 0;
  std::string s = "mem, ";
  s += is64 ? "64-bit" : "32-bit";
  s += pref ? ", prefetchable" : ", non-prefetchable";
  return s;
}

namespace {

// Decodifica as linhas de .../resource (start end flags por linha).
// Linhas 0..5 = BAR0..BAR5, linha 6 = janela ROM.
void parse_resource(const std::string& dev_dir, nvidia::NvidiaPciDevice* dev,
                    PciExtra* extra) {
  const int fd = ::open((dev_dir + "/resource").c_str(), O_RDONLY);
  if (fd < 0) return;
  std::string content;
  std::array<char, 4096> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    content.append(buf.data(), static_cast<std::size_t>(n));
  }
  ::close(fd);

  std::istringstream in(content);
  std::string line;
  int index = 0;
  while (std::getline(in, line) && index < 7) {
    line = trim(line);
    if (line.empty()) {
      ++index;
      continue;
    }
    std::istringstream cols(line);
    std::string c0, c1, c2;
    cols >> c0 >> c1 >> c2;
    const auto start = parse_number(c0);
    const auto end = parse_number(c1);
    const auto flags = parse_number(c2);
    if (!start || !end || !flags) {
      ++index;
      continue;
    }
    const auto s = static_cast<std::uint64_t>(*start);
    const auto e = static_cast<std::uint64_t>(*end);
    const auto f = static_cast<std::uint64_t>(*flags);
    if (index < 6) {
      if (s != 0 || e != 0) {
        nvidia::NvidiaPciBar bar{};
        bar.index = static_cast<std::uint8_t>(index);
        bar.base_address = s;
        bar.size = (e >= s) ? (e - s + 1) : 0;
        bar.is_io_space = (f & kFlagIo) != 0;
        bar.is_prefetchable = (f & (kFlagPrefetch | kFlagPrefetchLegacy |
                                    kFlagBarPrefetch)) != 0;
        bar.is_64bit = (f & kFlagMem64) != 0 ||
                       ((f & kFlagBarTypeMask) == kFlagBarType64);
        if (bar.is_io_space) {
          bar.is_64bit = false;
          bar.is_prefetchable = false;
        } else if ((f & kFlagMem) == 0) {
          // Sem bit MEM/IO: mantem como memoria 32-bit conservadora.
          bar.is_64bit = false;
        }
        dev->bars.push_back(bar);
        extra->bar_flags[index] = f;
      }
    } else if (index == 6) {
      if (s != 0 || e != 0) {
        extra->rom_present = true;
        extra->rom_start = s;
        extra->rom_end = e;
        extra->rom_flags = f;
      }
    }
    ++index;
  }
}

// Modulos relacionados vindos de /proc/modules (leitura). "unknown" se
// nenhum modulo correspondente estiver carregado ou o arquivo nao existir.
std::string related_modules() {
  const int fd = ::open("/proc/modules", O_RDONLY);
  if (fd < 0) return "unknown";
  std::string content;
  std::array<char, 8192> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    content.append(buf.data(), static_cast<std::size_t>(n));
    if (content.size() > (1U << 20)) break;  // limite anti-spam
  }
  ::close(fd);
  std::vector<std::string> hits;
  std::istringstream in(content);
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream words(trim(line));
    std::string name;
    words >> name;
    if (name.empty()) continue;
    if (name == "nouveau" || name.rfind("nvidia", 0) == 0) hits.push_back(name);
  }
  if (hits.empty()) return "unknown";
  std::string out;
  for (std::size_t i = 0; i < hits.size(); ++i) {
    if (i) out += ", ";
    out += hits[i];
  }
  return out;
}

std::string lowercase(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool ends_with(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

ArchInfo classify_arch(std::uint16_t vendor_id, std::uint16_t device_id) {
  if (vendor_id != kNvidiaVendor) return {"unknown", "unknown"};
  // Grupos de device-id segundo o banco pci-ids (pci.ids, fabricante 10de).
  // GA106 (desktop + mobile). GA104 listado separadamente para nao confundir.
  switch (device_id) {
    case 0x2501:
    case 0x2503:
    case 0x2504:
    case 0x2505:
    case 0x2509:
    case 0x2544:
    case 0x2520:
    case 0x2521:
      return {"Ampere", "GA106"};
    case 0x2484:
    case 0x2486:
    case 0x2487:
    case 0x2488:
    case 0x2489:
    case 0x249C:
    case 0x24A0:
      return {"Ampere", "GA104 (not GA106)"};
    default:
      break;
  }
  return {"NVIDIA GPU", "unknown"};
}

std::vector<ObservedGpu> discover_nvidia_gpus() {
  std::vector<ObservedGpu> out;
  const std::string root = "/sys/bus/pci/devices";
  std::error_code ec;
  std::filesystem::directory_iterator it(root, ec);
  if (ec) return out;
  std::vector<std::string> bdfs;
  for (const auto& entry : it) {
    bdfs.push_back(entry.path().filename().string());
  }
  std::sort(bdfs.begin(), bdfs.end());

  for (const auto& bdf : bdfs) {
    const std::string dir = root + "/" + bdf;
    const auto vendor = parse_number(read_first_line(dir + "/vendor"));
    const auto device = parse_number(read_first_line(dir + "/device"));
    if (!vendor || !device) continue;
    if (static_cast<std::uint16_t>(*vendor) != kNvidiaVendor) continue;

    ObservedGpu gpu{};
    gpu.pci.bdf = bdf;
    gpu.pci.vendor_id = static_cast<std::uint16_t>(*vendor);
    gpu.pci.device_id = static_cast<std::uint16_t>(*device);
    if (const auto sv = parse_number(read_first_line(dir + "/subsystem_vendor"))) {
      gpu.pci.subsystem_vendor = static_cast<std::uint16_t>(*sv);
    }
    if (const auto sd = parse_number(read_first_line(dir + "/subsystem_device"))) {
      gpu.pci.subsystem_device = static_cast<std::uint16_t>(*sd);
    }
    if (const auto rev = parse_number(read_first_line(dir + "/revision"))) {
      gpu.pci.revision_id = static_cast<std::uint8_t>(*rev & 0xFF);
    }
    if (const auto cls = parse_number(read_first_line(dir + "/class"))) {
      const unsigned long c = *cls;
      gpu.pci.pci_class = static_cast<std::uint8_t>((c >> 16) & 0xFF);
      gpu.pci.pci_subclass = static_cast<std::uint8_t>((c >> 8) & 0xFF);
      std::ostringstream os;
      os << "0x" << std::hex << std::nouppercase << std::setw(6)
         << std::setfill('0') << (c & 0xFFFFFFUL);
      gpu.extra.class_hex = os.str();
    }

    parse_resource(dir, &gpu.pci, &gpu.extra);

    const std::string drv = readlink_target(dir + "/driver");
    gpu.extra.driver = drv.empty() ? "unknown" : basename_of(drv);
    const std::string modalias = read_first_line(dir + "/modalias");
    gpu.extra.modalias = modalias.empty() ? "unknown" : modalias;
    const std::string iommu = readlink_target(dir + "/iommu_group");
    gpu.extra.iommu_group = iommu.empty() ? "unknown" : basename_of(iommu);
    const std::string numa = read_first_line(dir + "/numa_node");
    gpu.extra.numa_node = numa.empty() ? "unknown" : numa;
    out.push_back(std::move(gpu));
  }

  // Mesma fonte compartilhada por todos os GPUs: le uma vez.
  const std::string mods = related_modules();
  for (auto& g : out) g.extra.modules = mods;
  return out;
}

const ObservedGpu* pick_primary_gpu(const std::vector<ObservedGpu>& gpus) noexcept {
  if (gpus.empty()) return nullptr;
  for (const auto& g : gpus) {
    if (g.pci.pci_class == 0x03) return &g;  // controlador de video
  }
  return &gpus.front();
}

std::string query_vram_total(const std::string& bdf) {
  // Consulta opcional, somente-leitura, sem dependencia dura: se a
  // ferramenta auxiliar nao existir ou nao responder, retorna "unknown".
  std::array<char, 512> buf{};
  std::string result = "unknown";
  FILE* pipe = ::popen(
      "nvidia-smi --query-gpu=pci.bus_id,memory.total "
      "--format=csv,noheader,nounits 2>/dev/null",
      "r");
  if (pipe == nullptr) return result;
  const std::string want = lowercase(trim(bdf));
  int lines = 0;
  while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
    if (++lines > 64) break;
    std::string line(buf.data());
    line = trim(line);
    if (line.empty()) continue;
    const auto comma = line.find(',');
    if (comma == std::string::npos) continue;
    const std::string bus = lowercase(trim(line.substr(0, comma)));
    std::string mem = trim(line.substr(comma + 1));
    // Remove sufixo "MiB" quando presente para normalizar.
    if (ends_with(lowercase(mem), "mib")) mem = trim(mem.substr(0, mem.size() - 3));
    if (mem.empty()) continue;
    if (bus == want || ends_with(bus, want)) {
      result = mem + " MiB";
      break;
    }
  }
  ::pclose(pipe);
  return result;
}

std::string format_size(std::uint64_t bytes) {
  constexpr std::uint64_t kK = 1024ULL;
  constexpr std::uint64_t kM = 1024ULL * 1024ULL;
  constexpr std::uint64_t kG = 1024ULL * 1024ULL * 1024ULL;
  if (bytes >= kG && bytes % kG == 0) return std::to_string(bytes / kG) + "G";
  if (bytes >= kM && bytes % kM == 0) return std::to_string(bytes / kM) + "M";
  if (bytes >= kK && bytes % kK == 0) return std::to_string(bytes / kK) + "K";
  return std::to_string(bytes) + " B";
}

}  // namespace ga106lab
