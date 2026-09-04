#pragma once

// ga106-lab baseline --compare: comparacao deterministica entre dois
// baseline.json (somente leitura; nunca toca no hardware).
// Os dois arquivos sao achatados via json_flat (mapa ordenado => ordem
// deterministica) e cada chave e classificada em:
//   IDENTITY    BDF, vendor/device/subsys/rev, UUID (troca = outra placa?)
//   STATIC      layout de BARs, capacidade PCIe maxima (max_link_*,
//               resizable, bar1_aperture)
//   SEMI_STATIC versao de driver, firmware GSP exposto, VBIOS, kernel e
//               demais configuracoes (mudam com update/reboot, sem gating)
//   DYNAMIC     link atual, VRAM em uso, conectores, processos/holders,
//               logs, vulkan, lab_readiness (nunca erro critico)
// `meta.timestamp_utc` e ignorado (sempre difere; nota impressa no relatorio).

#include <map>
#include <string>
#include <vector>

namespace ga106lab {

enum class CompareClass { kIdentity, kStatic, kSemiStatic, kDynamic };

const char* compare_class_name(CompareClass c);

// Classifica uma chave achatada (ex. "identity.uuid", "pci.bars[1]").
// Chaves futuras desconhecidas com substratos de runtime (temp, clock,
// power, fan, util, process, holder, session, connector) caem em DYNAMIC;
// o restante desconhecido cai em SEMI_STATIC (conservador: nunca critico).
CompareClass classify_compare_key(const std::string& key);

// Chave ignorada na comparacao (nao e estado: sempre difere).
bool is_ignored_compare_key(const std::string& key);

struct KeyDiff {
  std::string key;
  std::string old_value;  // "<missing>" se ausente no old.json
  std::string new_value;  // "<missing>" se ausente no new.json
};

struct CompareReport {
  std::string old_path;
  std::string new_path;
  std::vector<KeyDiff> identity;
  std::vector<KeyDiff> statik;
  std::vector<KeyDiff> semi_static;
  std::vector<KeyDiff> dynamic;
  std::size_t unchanged = 0;
};

// Compara dois mapas achatados (ordem deterministica = ordem do std::map).
CompareReport compare_flat_maps(
    const std::string& old_path,
    const std::map<std::string, std::string>& old_map,
    const std::string& new_path,
    const std::map<std::string, std::string>& new_map);

// true se ha diff em IDENTITY ou STATIC (unico caso critico, vide --strict).
bool compare_report_has_critical(const CompareReport& report);

// Renderiza o relatorio em secoes IDENTITY/STATIC/SEMI_STATIC/DYNAMIC com
// "unchanged" ou diffs `a -> b` (seta UTF-8 no terminal).
std::string render_compare_report(const CompareReport& report, bool strict);

// Executa o verbo: baseline --compare OLD NEW [--strict].
// Retorna 0 em sucesso (diffs em modo normal nao sao erro); com --strict
// retorna 1 apenas se IDENTITY ou STATIC mudar (com mensagem explicando);
// retorna 1 em falha de IO/parse e 2 em uso invalido.
int run_baseline_compare(const std::vector<std::string>& args);

}  // namespace ga106lab
