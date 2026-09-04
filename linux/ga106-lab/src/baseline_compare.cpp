// ga106-lab baseline --compare (somente leitura).
// Compara dois baseline.json de forma deterministica (chaves ordenadas via
// std::map) e imprime secoes IDENTITY/STATIC/SEMI_STATIC/DYNAMIC.
// DYNAMIC nunca e erro critico; --strict falha (exit != 0) apenas se
// IDENTITY ou STATIC mudar, com mensagem explicando o motivo.

#include "baseline_compare.hpp"

#include <iostream>
#include <sstream>

#include "json_flat.hpp"

namespace ga106lab {
namespace {

constexpr const char* kTag = "[ga106-lab]";
constexpr const char* kMissing = "<missing>";
constexpr const char* kIgnoredTs = "meta.timestamp_utc";

bool starts_with(const std::string& s, const std::string& prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

bool contains(const std::string& s, const std::string& sub) {
  return s.find(sub) != std::string::npos;
}

int compare_usage() {
  std::cerr << kTag
            << " uso: ga106-lab baseline --compare OLD.json NEW.json "
               "[--strict]\n";
  std::cerr << kTag
            << "   compara dois baseline.json; --strict falha (exit != 0) "
               "apenas se IDENTITY ou STATIC mudar\n";
  return 2;
}

}  // namespace

const char* compare_class_name(CompareClass c) {
  switch (c) {
    case CompareClass::kIdentity:
      return "IDENTITY";
    case CompareClass::kStatic:
      return "STATIC";
    case CompareClass::kSemiStatic:
      return "SEMI_STATIC";
    case CompareClass::kDynamic:
      return "DYNAMIC";
  }
  return "UNKNOWN";
}

bool is_ignored_compare_key(const std::string& key) {
  return key == kIgnoredTs;
}

CompareClass classify_compare_key(const std::string& key) {
  // IDENTITY: BDF, vendor/device/subsys/rev, UUID.
  if (key == "identity.bdf" || key == "identity.vendor" ||
      key == "identity.device" || key == "identity.subsystem" ||
      key == "identity.revision" || key == "identity.uuid") {
    return CompareClass::kIdentity;
  }
  // STATIC: layout de BARs + capacidade PCIe maxima.
  if (starts_with(key, "pci.bars") || key == "pci.link_max_width" ||
      key == "pci.link_max_speed" || starts_with(key, "pci.resizable") ||
      key == "memory.bar1_aperture") {
    return CompareClass::kStatic;
  }
  // DYNAMIC: runtime — link atual, VRAM em uso, conectores, processos,
  // logs, vulkan, prontidao do lab.
  if (key == "pci.link_current_width" || key == "pci.link_current_speed" ||
      key == "memory.vram_used" || starts_with(key, "drm.connectors") ||
      starts_with(key, "in_use") || starts_with(key, "logs") ||
      starts_with(key, "vulkan") || starts_with(key, "lab_readiness")) {
    return CompareClass::kDynamic;
  }
  // Heuristica futura: campos de telemetria que o baseline ainda nao
  // coleta (temp, clocks) sao runtime por definicao.
  if (contains(key, "temp") || contains(key, "clock") ||
      contains(key, "power") || contains(key, "fan") ||
      contains(key, "util") || contains(key, "process") ||
      contains(key, "holder") || contains(key, "session") ||
      contains(key, "connector")) {
    return CompareClass::kDynamic;
  }
  // Restante (driver, firmware exposto, VBIOS, kernel, DRM nodes,
  // VRAM total, lspci, meta): SEMI_STATIC, nunca critico.
  return CompareClass::kSemiStatic;
}

CompareReport compare_flat_maps(
    const std::string& old_path,
    const std::map<std::string, std::string>& old_map,
    const std::string& new_path,
    const std::map<std::string, std::string>& new_map) {
  CompareReport r;
  r.old_path = old_path;
  r.new_path = new_path;
  auto it_old = old_map.begin();
  auto it_new = new_map.begin();
  // Merge ordenado: deterministico por construcao (std::map).
  while (it_old != old_map.end() || it_new != new_map.end()) {
    bool take_old = false;
    bool take_new = false;
    std::string key;
    if (it_old != old_map.end() &&
        (it_new == new_map.end() || it_old->first < it_new->first)) {
      key = it_old->first;
      take_old = true;
    } else if (it_new != new_map.end() &&
               (it_old == old_map.end() || it_new->first < it_old->first)) {
      key = it_new->first;
      take_new = true;
    } else {
      key = it_old->first;  // chaves iguais: compara valores
      take_old = take_new = true;
    }
    const std::string* va =
        take_old ? &it_old->second : nullptr;
    const std::string* vb =
        take_new ? &it_new->second : nullptr;
    if (take_old) ++it_old;
    if (take_new) ++it_new;
    if (is_ignored_compare_key(key)) continue;
    if (va != nullptr && vb != nullptr && *va == *vb) {
      ++r.unchanged;
      continue;
    }
    KeyDiff d;
    d.key = key;
    d.old_value = (va != nullptr) ? *va : kMissing;
    d.new_value = (vb != nullptr) ? *vb : kMissing;
    switch (classify_compare_key(key)) {
      case CompareClass::kIdentity:
        r.identity.push_back(d);
        break;
      case CompareClass::kStatic:
        r.statik.push_back(d);
        break;
      case CompareClass::kSemiStatic:
        r.semi_static.push_back(d);
        break;
      case CompareClass::kDynamic:
        r.dynamic.push_back(d);
        break;
    }
  }
  return r;
}

bool compare_report_has_critical(const CompareReport& report) {
  return !report.identity.empty() || !report.statik.empty();
}

namespace {

void render_section(std::ostringstream& os, const char* name,
                    const std::vector<KeyDiff>& diffs) {
  os << name << " (" << diffs.size() << " diferenca(s)):\n";
  if (diffs.empty()) {
    os << "  unchanged\n";
    return;
  }
  for (const auto& d : diffs) {
    os << "  " << d.key << ": " << d.old_value << " → " << d.new_value
       << "\n";
  }
}

}  // namespace

std::string render_compare_report(const CompareReport& report, bool strict) {
  std::ostringstream os;
  os << kTag << " baseline compare\n";
  os << "  old: " << report.old_path << "\n";
  os << "  new: " << report.new_path << "\n";
  os << "  mode: " << (strict ? "strict" : "normal") << "\n";
  render_section(os, "IDENTITY", report.identity);
  render_section(os, "STATIC", report.statik);
  render_section(os, "SEMI_STATIC", report.semi_static);
  render_section(os, "DYNAMIC", report.dynamic);
  os << "Summary: unchanged=" << report.unchanged
     << " identity=" << report.identity.size()
     << " static=" << report.statik.size()
     << " semi_static=" << report.semi_static.size()
     << " dynamic=" << report.dynamic.size() << "\n";
  os << "Note: " << kIgnoredTs << " ignorado (sempre difere entre snapshots)\n";
  os << "Note: diferencas DYNAMIC nunca sao erro critico\n";
  if (strict) {
    if (compare_report_has_critical(report)) {
      os << "Result: STRICT FAIL — IDENTITY ou STATIC mudou "
            "(ver secoes acima)\n";
    } else {
      os << "Result: STRICT OK — sem mudanca em IDENTITY/STATIC\n";
    }
  } else if (report.identity.empty() && report.statik.empty() &&
             report.semi_static.empty() && report.dynamic.empty()) {
    os << "Result: no differences\n";
  } else {
    os << "Result: differences found (nao critico em modo normal; "
          "use --strict para barrar em IDENTITY/STATIC)\n";
  }
  return os.str();
}

int run_baseline_compare(const std::vector<std::string>& args) {
  std::string old_path;
  std::string new_path;
  bool strict = false;
  bool want_help = false;
  std::vector<std::string> positional;
  for (const auto& a : args) {
    if (a == "--strict") {
      strict = true;
    } else if (a == "-h" || a == "--help") {
      want_help = true;
    } else if (!a.empty() && a[0] == '-') {
      return compare_usage();
    } else {
      positional.push_back(a);
    }
  }
  if (want_help) {
    std::cout << "ga106-lab baseline --compare OLD.json NEW.json [--strict]\n";
    return 0;
  }
  if (positional.size() != 2) return compare_usage();
  old_path = positional[0];
  new_path = positional[1];

  std::map<std::string, std::string> old_map;
  std::map<std::string, std::string> new_map;
  std::string err;
  if (!load_json_flat(old_path, &old_map, &err)) {
    std::cerr << kTag << " baseline --compare: falha ao ler old.json: " << err
              << "\n";
    return 1;
  }
  if (!load_json_flat(new_path, &new_map, &err)) {
    std::cerr << kTag << " baseline --compare: falha ao ler new.json: " << err
              << "\n";
    return 1;
  }
  const CompareReport report =
      compare_flat_maps(old_path, old_map, new_path, new_map);
  std::cout << render_compare_report(report, strict);
  if (strict && compare_report_has_critical(report)) {
    std::cerr << kTag
              << " baseline --compare STRICT FAIL: IDENTITY ("
              << report.identity.size() << ") ou STATIC ("
              << report.statik.size()
              << ") mudou entre os snapshots; DYNAMIC nunca e critico e "
                 "SEMI_STATIC nao barra o modo strict\n";
    return 1;
  }
  return 0;
}

}  // namespace ga106lab
