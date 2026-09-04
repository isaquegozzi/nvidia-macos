// test_compare_logic — selftest puro da comparacao de baselines.
// Sem hardware, sem IO alem de stdout/stderr: exercita o parser JSON manual
// (json_flat), a classificacao de chaves e a deteccao critica do compare.
// Falha via return != 0; sem dependencias externas.

#include <iostream>
#include <map>
#include <string>

#include "baseline_compare.hpp"
#include "json_flat.hpp"

namespace {

int failures = 0;

void check(bool cond, const char* name) {
  if (!cond) {
    ++failures;
    std::cerr << "FAIL: " << name << "\n";
  }
}

std::map<std::string, std::string> flat(const std::string& text) {
  std::map<std::string, std::string> m;
  std::string err;
  if (!ga106lab::parse_json_flat(text, &m, &err)) {
    std::cerr << "parse falhou: " << err << "\n";
    ++failures;
  }
  return m;
}

}  // namespace

int main() {
  using ga106lab::classify_compare_key;
  using ga106lab::CompareClass;

  // 1. Parser: objetos, arrays, escapes, numero, bool, null.
  {
    const auto m = flat(
        R"({"identity":{"bdf":"0000:01:00.0","uuid":"GPU-x"},"meta":{"gpu_count":1},"pci":{"bars":["A","B"],"link_max_speed":"16.0 GT/s PCIe"},"ok":true,"nada":null,"esc":"a\nb\"c"})");
    check(m.at("identity.bdf") == "0000:01:00.0", "parse obj aninhado");
    check(m.at("identity.uuid") == "GPU-x", "parse uuid");
    check(m.at("meta.gpu_count") == "1", "parse numero");
    check(m.at("pci.bars[0]") == "A", "parse array 0");
    check(m.at("pci.bars[1]") == "B", "parse array 1");
    check(m.at("ok") == "true", "parse bool");
    check(m.at("nada") == "null", "parse null");
    check(m.at("esc") == "a\nb\"c", "parse escapes");
  }

  // 2. Parser rejeita malformado e trailing garbage.
  {
    std::map<std::string, std::string> m;
    std::string err;
    check(!ga106lab::parse_json_flat("{", &m, &err), "rejeita '{'");
    check(!ga106lab::parse_json_flat("{\"a\":1} lixo", &m, &err),
          "rejeita trailing");
    check(!ga106lab::parse_json_flat("", &m, &err), "rejeita vazio");
  }

  // 3. Classificacao conforme o contrato do verbo.
  check(classify_compare_key("identity.bdf") == CompareClass::kIdentity,
        "classe identity.bdf");
  check(classify_compare_key("identity.uuid") == CompareClass::kIdentity,
        "classe identity.uuid");
  check(classify_compare_key("identity.vbios") != CompareClass::kIdentity,
        "vbios nao e identity");
  check(classify_compare_key("pci.bars[1]") == CompareClass::kStatic,
        "classe bars");
  check(classify_compare_key("pci.link_max_speed") == CompareClass::kStatic,
        "classe max link");
  check(classify_compare_key("pci.link_current_speed") ==
            CompareClass::kDynamic,
        "classe current link");
  check(classify_compare_key("memory.vram_used") == CompareClass::kDynamic,
        "classe vram used");
  check(classify_compare_key("drm.connectors[0]") == CompareClass::kDynamic,
        "classe connectors");
  check(classify_compare_key("in_use.compute_apps") == CompareClass::kDynamic,
        "classe compute-apps");
  check(classify_compare_key("driver.version_file") ==
            CompareClass::kSemiStatic,
        "classe driver");
  check(classify_compare_key("identity.vbios") == CompareClass::kSemiStatic,
        "classe vbios");
  check(classify_compare_key("meta.kernel") == CompareClass::kSemiStatic,
        "classe kernel");
  check(classify_compare_key("sensors.temp_gpu") == CompareClass::kDynamic,
        "classe temp futura");
  check(classify_compare_key("clocks.sm[0]") == CompareClass::kDynamic,
        "classe clocks futura");
  check(ga106lab::is_ignored_compare_key("meta.timestamp_utc"),
        "timestamp ignorado");

  // 4. Mapas identicos (exceto timestamp) => sem diffs.
  {
    const auto a = flat(R"({"meta":{"timestamp_utc":"t1"},"identity":{"bdf":"X"}})");
    const auto b = flat(R"({"meta":{"timestamp_utc":"t2"},"identity":{"bdf":"X"}})");
    const auto r = ga106lab::compare_flat_maps("a", a, "b", b);
    check(r.identity.empty() && r.statik.empty() && r.semi_static.empty() &&
              r.dynamic.empty() && r.unchanged == 1,
          "identicos exceto timestamp");
    check(!ga106lab::compare_report_has_critical(r), "sem critico");
  }

  // 5. Mudanca IDENTITY => critica; DYNAMIC sozinha => nao critica.
  {
    auto a = flat(R"({"identity":{"uuid":"U1"},"memory":{"vram_used":"10 MiB"}})");
    auto b = flat(R"({"identity":{"uuid":"U2"},"memory":{"vram_used":"99 MiB"}})");
    const auto r = ga106lab::compare_flat_maps("a", a, "b", b);
    check(r.identity.size() == 1, "identity diff detectado");
    check(r.dynamic.size() == 1, "dynamic diff detectado");
    check(ga106lab::compare_report_has_critical(r), "identity e critico");
  }
  {
    auto a = flat(R"({"memory":{"vram_used":"10 MiB"}})");
    auto b = flat(R"({"memory":{"vram_used":"99 MiB"}})");
    const auto r = ga106lab::compare_flat_maps("a", a, "b", b);
    check(!ga106lab::compare_report_has_critical(r),
          "dynamic sozinho nao e critico");
  }

  // 6. Mudanca STATIC => critica; SEMI_STATIC sozinha => nao critica.
  {
    auto a = flat(R"({"pci":{"link_max_speed":"16.0 GT/s PCIe"}})");
    auto b = flat(R"({"pci":{"link_max_speed":"8.0 GT/s PCIe"}})");
    const auto r = ga106lab::compare_flat_maps("a", a, "b", b);
    check(r.statik.size() == 1, "static diff detectado");
    check(ga106lab::compare_report_has_critical(r), "static e critico");
  }
  {
    auto a = flat(R"({"driver":{"modinfo":"610.57.04"}})");
    auto b = flat(R"({"driver":{"modinfo":"620.00.00"}})");
    const auto r = ga106lab::compare_flat_maps("a", a, "b", b);
    check(r.semi_static.size() == 1, "semi_static diff detectado");
    check(!ga106lab::compare_report_has_critical(r),
          "semi_static nao e critico");
  }

  // 7. Chave ausente de um lado vira diff com <missing>; ordem deterministica.
  {
    auto a = flat(R"({"z":{"k":"1"},"a":{"k":"1"}})");
    auto b = flat(R"({"a":{"k":"1"}})");
    const auto r = ga106lab::compare_flat_maps("a", a, "b", b);
    const std::string out = ga106lab::render_compare_report(r, false);
    check(out.find("z.k: 1 → <missing>") != std::string::npos,
          "missing renderizado");
    check(out.find("a.k") == std::string::npos, "igual nao listado");
    check(out.find("IDENTITY") < out.find("STATIC") &&
              out.find("STATIC") < out.find("SEMI_STATIC") &&
              out.find("SEMI_STATIC") < out.find("DYNAMIC"),
          "secoes em ordem fixa");
  }

  if (failures == 0) {
    std::cout << "compare_logic: OK\n";
    return 0;
  }
  std::cerr << "compare_logic: " << failures << " falha(s)\n";
  return 1;
}
