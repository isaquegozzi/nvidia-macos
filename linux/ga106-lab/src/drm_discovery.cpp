// ga106-lab: descoberta passiva de nos DRM (somente leitura).
// Este arquivo apenas lista diretorios, resolve symlinks e le pequenos
// arquivos de texto com abertura read-only. Nenhum dispositivo e aberto.

#include "drm_discovery.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace ga106lab {
namespace {

std::string trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
  std::size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
  return s.substr(b, e - b);
}

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

bool starts_with(const std::string& s, const char* prefix) {
  const std::string p(prefix);
  return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

// No de dispositivo (cardN/renderDN) ou conector (cardN-...)?
bool is_device_node(const std::string& name) {
  if (name.find('-') != std::string::npos) return false;
  return starts_with(name, "card") || starts_with(name, "renderD");
}

// Extrai PCI_SLOT_NAME do uevent do dispositivo.
std::string pci_slot_from_uevent(const std::string& uevent_path) {
  const int fd = ::open(uevent_path.c_str(), O_RDONLY);
  if (fd < 0) return {};
  std::string content;
  std::array<char, 4096> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    content.append(buf.data(), static_cast<std::size_t>(n));
    if (content.size() > 65536) break;
  }
  ::close(fd);
  const std::string key = "PCI_SLOT_NAME=";
  auto pos = content.find(key);
  if (pos == std::string::npos) return {};
  pos += key.size();
  auto end = content.find('\n', pos);
  return trim(content.substr(pos, end == std::string::npos ? end : end - pos));
}

}  // namespace

std::vector<DrmNodeInfo> discover_drm_nodes(const std::string& bdf) {
  std::vector<DrmNodeInfo> out;
  const std::string root = "/sys/class/drm";
  std::error_code ec;
  std::filesystem::directory_iterator it(root, ec);
  if (ec) return out;

  std::vector<std::string> names;
  for (const auto& entry : it) names.push_back(entry.path().filename().string());
  std::sort(names.begin(), names.end());

  for (const auto& name : names) {
    if (!is_device_node(name)) continue;
    const std::string node_dir = root + "/" + name;
    // O symlink .../device aponta para o dispositivo PCI (ex.
    // "../../../0000:01:00.0"). Cruza pelo BDF observado no hardware.
    const std::string target = readlink_target(node_dir + "/device");
    if (target.empty() || target.find(bdf) == std::string::npos) continue;

    DrmNodeInfo node{};
    node.name = name;
    node.dri_path = "/dev/dri/" + name;
    const std::string dev = read_first_line(node_dir + "/dev");
    node.major_minor = dev.empty() ? "unknown" : dev;
    const std::string slot = pci_slot_from_uevent(node_dir + "/device/uevent");
    node.pci_slot = slot.empty() ? "unknown" : slot;
    std::error_code exists_ec;
    node.dri_present =
        std::filesystem::exists(node.dri_path, exists_ec) && !exists_ec;
    out.push_back(std::move(node));
  }
  return out;
}

}  // namespace ga106lab
