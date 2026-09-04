#pragma once

// BAR PCI — visão somente leitura. Sem deps de OS.

#include <cstdint>

namespace nvidia {

struct NvidiaPciBar {
  std::uint8_t index = 0;          // 0..5
  std::uint64_t base_address = 0;  // bus address lido via sysfs (somente leitura)
  std::uint64_t size = 0;          // 0 = ausente / não mapeada
  bool is_64bit = false;
  bool is_prefetchable = false;
  bool is_io_space = false;

  [[nodiscard]] constexpr bool is_valid() const noexcept { return size != 0; }
  [[nodiscard]] constexpr bool is_memory() const noexcept { return !is_io_space; }
};

}  // namespace nvidia
