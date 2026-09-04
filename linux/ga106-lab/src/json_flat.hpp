#pragma once

// ga106-lab: parser JSON manual minimo (somente leitura, sem dependencias).
// Converte um baseline.json (gerado pelo proprio ga106-lab, sem dependencia
// externa) em um mapa ordenado chave->valor com chaves pontilhadas:
// objetos viram "prefixo.chave", arrays viram "prefixo[i]", e folhas
// (string/numero/bool/null) viram texto. Strings sao unescaped;
// numeros/bools viram o literal; null vira "null".
// O mapa ordenado (std::map) garante comparacao deterministica.
// Nenhum estado do dispositivo e tocado; leitura de arquivo usa O_RDONLY.

#include <map>
#include <string>

namespace ga106lab {

// Faz parse do texto JSON em `out`. Retorna true em sucesso; em falha
// retorna false e descreve em `err` (pode ser nullptr).
bool parse_json_flat(const std::string& text,
                     std::map<std::string, std::string>* out,
                     std::string* err);

// Le o arquivo em O_RDONLY (limitado a 8 MiB) e faz parse.
// Retorna false + `err` se o arquivo nao existir ou o JSON for invalido.
bool load_json_flat(const std::string& path,
                    std::map<std::string, std::string>* out, std::string* err);

}  // namespace ga106lab
