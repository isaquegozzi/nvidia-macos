// ga106-lab: parser JSON manual minimo (somente leitura).
// Recursive-descent sobre o texto; sem alocacao alem de std::string/map,
// sem chamadas de sistema exceto open/read/close com O_RDONLY para carregar
// o arquivo. Suporta o subconjunto JSON emitido pelo baseline.json:
// objetos, arrays, strings (com escapes \" \\ \/ \b \f \n \r \t \uXXXX),
// numeros, true/false/null e espacos. Recusa trailing garbage e rejeita
// profundidade > 64 (anti-recurso).

#include "json_flat.hpp"

#include <array>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <unistd.h>

namespace ga106lab {
namespace {

constexpr std::size_t kMaxFileBytes = 8U * 1024U * 1024U;
constexpr int kMaxDepth = 64;

class Parser {
 public:
  explicit Parser(const std::string& text) : s_(text) {}

  bool run(std::map<std::string, std::string>* out, std::string* err) {
    out_ = out;
    err_ = err;
    skip_ws();
    if (!parse_value("", 0)) return fail("documento vazio ou invalido");
    skip_ws();
    if (pos_ != s_.size()) return fail("conteudo apos o fim do documento");
    return true;
  }

 private:
  const std::string& s_;
  std::size_t pos_ = 0;
  std::map<std::string, std::string>* out_ = nullptr;
  std::string* err_ = nullptr;

  bool fail(const std::string& msg) {
    if (err_ != nullptr) {
      *err_ = "JSON invalido na posicao " + std::to_string(pos_) + ": " + msg;
    }
    return false;
  }

  void skip_ws() {
    while (pos_ < s_.size()) {
      const char c = s_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++pos_;
      } else {
        break;
      }
    }
  }

  bool expect(char c) {
    if (pos_ < s_.size() && s_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool parse_value(const std::string& path, int depth) {
    if (depth > kMaxDepth) return fail("profundidade excessiva");
    skip_ws();
    if (pos_ >= s_.size()) return fail("valor esperado");
    const char c = s_[pos_];
    if (c == '{') return parse_object(path, depth);
    if (c == '[') return parse_array(path, depth);
    if (c == '"') {
      std::string v;
      if (!parse_string(&v)) return false;
      (*out_)[path] = v;
      return true;
    }
    if (c == 't' || c == 'f' || c == 'n') return parse_literal(path);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(path);
    return fail("caractere inesperado");
  }

  bool parse_object(const std::string& path, int depth) {
    ++pos_;  // '{'
    skip_ws();
    if (expect('}')) return true;  // objeto vazio nao gera folhas
    bool first = true;
    while (true) {
      skip_ws();
      if (!first) {
        if (!expect(',')) return fail("',' esperado no objeto");
        skip_ws();
      }
      first = false;
      if (pos_ >= s_.size() || s_[pos_] != '"') {
        return fail("chave string esperada no objeto");
      }
      std::string key;
      if (!parse_string(&key)) return false;
      skip_ws();
      if (!expect(':')) return fail("':' esperado no objeto");
      const std::string child = path.empty() ? key : path + "." + key;
      if (!parse_value(child, depth + 1)) return false;
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == '}') {
        ++pos_;
        return true;
      }
      if (pos_ >= s_.size()) return fail("objeto nao terminado");
      // senao, proxima iteracao consome a virgula.
    }
  }

  bool parse_array(const std::string& path, int depth) {
    ++pos_;  // '['
    skip_ws();
    if (expect(']')) {
      // Array vazio: registra marcador para distinguir de ausente.
      (*out_)[path] = "<empty>";
      return true;
    }
    std::size_t index = 0;
    while (true) {
      const std::string child =
          path + "[" + std::to_string(index) + "]";
      if (!parse_value(child, depth + 1)) return false;
      ++index;
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == ']') {
        ++pos_;
        return true;
      }
      if (pos_ >= s_.size() || s_[pos_] != ',') {
        return fail("array nao terminado");
      }
      ++pos_;
    }
  }

  bool parse_string(std::string* out) {
    ++pos_;  // '"'
    std::string v;
    while (pos_ < s_.size()) {
      const char c = s_[pos_++];
      if (c == '"') {
        *out = v;
        return true;
      }
      if (c == '\\') {
        if (pos_ >= s_.size()) return fail("escape incompleto");
        const char e = s_[pos_++];
        switch (e) {
          case '"':
            v += '"';
            break;
          case '\\':
            v += '\\';
            break;
          case '/':
            v += '/';
            break;
          case 'b':
            v += '\b';
            break;
          case 'f':
            v += '\f';
            break;
          case 'n':
            v += '\n';
            break;
          case 'r':
            v += '\r';
            break;
          case 't':
            v += '\t';
            break;
          case 'u': {
            // \uXXXX: converte BMP para UTF-8 (suficiente p/ baseline.json).
            if (pos_ + 4 > s_.size()) return fail("escape \\u incompleto");
            unsigned code = 0;
            for (int i = 0; i < 4; ++i) {
              const char h = s_[pos_++];
              code <<= 4;
              if (h >= '0' && h <= '9') {
                code += static_cast<unsigned>(h - '0');
              } else if (h >= 'a' && h <= 'f') {
                code += static_cast<unsigned>(h - 'a' + 10);
              } else if (h >= 'A' && h <= 'F') {
                code += static_cast<unsigned>(h - 'A' + 10);
              } else {
                return fail("digito hexadecimal invalido em \\u");
              }
            }
            if (code < 0x80) {
              v += static_cast<char>(code);
            } else if (code < 0x800) {
              v += static_cast<char>(0xC0 | (code >> 6));
              v += static_cast<char>(0x80 | (code & 0x3F));
            } else {
              v += static_cast<char>(0xE0 | (code >> 12));
              v += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
              v += static_cast<char>(0x80 | (code & 0x3F));
            }
            break;
          }
          default:
            return fail("escape desconhecido");
        }
      } else {
        v += c;
      }
    }
    return fail("string nao terminada");
  }

  bool parse_literal(const std::string& path) {
    for (const char* lit : {"true", "false", "null"}) {
      const std::string l(lit);
      if (s_.compare(pos_, l.size(), l) == 0) {
        pos_ += l.size();
        (*out_)[path] = l;
        return true;
      }
    }
    return fail("literal desconhecido");
  }

  bool parse_number(const std::string& path) {
    const std::size_t begin = pos_;
    if (pos_ < s_.size() && s_[pos_] == '-') ++pos_;
    while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    if (pos_ < s_.size() && s_[pos_] == '.') {
      ++pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
      ++pos_;
      if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
      while (pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9') ++pos_;
    }
    if (pos_ == begin) return fail("numero invalido");
    (*out_)[path] = s_.substr(begin, pos_ - begin);
    return true;
  }
};

}  // namespace

bool parse_json_flat(const std::string& text,
                     std::map<std::string, std::string>* out,
                     std::string* err) {
  if (out == nullptr) return false;
  out->clear();
  Parser p(text);
  return p.run(out, err);
}

bool load_json_flat(const std::string& path,
                    std::map<std::string, std::string>* out,
                    std::string* err) {
  if (out == nullptr) return false;
  out->clear();
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    if (err != nullptr) *err = "nao foi possivel abrir (O_RDONLY): " + path;
    return false;
  }
  std::string text;
  std::array<char, 4096> buf{};
  ssize_t n = 0;
  while ((n = ::read(fd, buf.data(), buf.size())) > 0) {
    text.append(buf.data(), static_cast<std::size_t>(n));
    if (text.size() > kMaxFileBytes) break;
  }
  ::close(fd);
  if (text.size() > kMaxFileBytes) {
    if (err != nullptr) *err = "arquivo excede 8 MiB: " + path;
    return false;
  }
  return parse_json_flat(text, out, err);
}

}  // namespace ga106lab
