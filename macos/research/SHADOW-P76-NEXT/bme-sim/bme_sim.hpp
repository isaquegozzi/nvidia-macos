// bme_sim.hpp — M6 BME policy model host-only (P76-DUAL-OVERNIGHT-V2).
// Mocked PCI backend only. Zero production wiring, zero real config writes.
// Spec: G0004 §3 + G0007 M6 direction.
// Command register (offset 0x04): bit0 IO, bit1 MSE, bit2 BME.
// Live baseline: 0x0003 (MSE=1,BME=0). Enabled: 0x0007 (MSE=1,BME=1).
// setBusMasterEnable is deprecated (macOS 10.0+ KPI) — model rejects it.
#pragma once
#include <cstdint>
#include <vector>

namespace bme {

inline constexpr uint16_t kCmdOff = 0x04u;
inline constexpr uint16_t kCmdBaseline = 0x0003u;  // MSE=1 BME=0
inline constexpr uint16_t kCmdEnabled = 0x0007u;   // MSE=1 BME=1
inline constexpr uint16_t kMseBit = 0x0002u;
inline constexpr uint16_t kBmeBit = 0x0004u;
inline constexpr uint16_t kKnownBits = 0x0007u;  // IO|MSE|BME; all else unknown

enum Result : int32_t {
  kOk = 0,
  kDenied = -1,      // preconditions unmet (fail-closed, no transition)
  kError = -2,       // unknown bits / backend fault (no transition)
  kAlreadyOn = -3,   // no-op, zero writes
  kAlreadyOff = -4,  // no-op, zero writes
  kInvalidArg = -5,  // deprecated API (never touches backend)
};

enum class FlushPhase : uint8_t { kUnregistered = 0, kRegistered = 1, kVerified = 2 };

struct Preconditions {
  bool gateAReady = false;      // Gate A PreconditionsReady observed
  bool gateAActive = false;     // Gate A op in flight -> BME must stay OFF
  bool flushRegistered = false; // dedicated flush page registered
  bool gfwReady = false;        // GFW 0xFF completion
  bool bar0Mapped = false;      // BAR0 mapped
  FlushPhase flushPhase = FlushPhase::kUnregistered;
};

// Mock PCI config space (256B). ONLY writeConfig performs writes.
struct MockPci {
  uint8_t cfg[256] = {};
  struct Write {
    uint16_t off;
    uint16_t val;
  };
  std::vector<Write> writes;
  bool failWrites = false;

  MockPci() {
    cfg[kCmdOff] = static_cast<uint8_t>(kCmdBaseline & 0xFFu);
    cfg[kCmdOff + 1] = static_cast<uint8_t>((kCmdBaseline >> 8u) & 0xFFu);
  }
  uint16_t readCommand() const {
    return static_cast<uint16_t>(cfg[kCmdOff]) |
           (static_cast<uint16_t>(cfg[kCmdOff + 1]) << 8u);
  }
  bool writeConfig(uint16_t off, uint16_t val) {
    if (failWrites) return false;
    if (off != kCmdOff) return false;  // model only owns Command
    writes.push_back({off, val});
    cfg[off] = static_cast<uint8_t>(val & 0xFFu);
    cfg[off + 1] = static_cast<uint8_t>((val >> 8u) & 0xFFu);
    return true;
  }
};

class BmePolicy {
 public:
  // Modern KPI model. Deprecated sibling always rejected (never touches HW).
  static Result setBusMasterEnable(bool) {
#ifdef MUT_ACCEPT_DEPRECATED_API
    return kOk;  // MUTATION TEST-ONLY: deprecated API accepted (must be CAUGHT)
#else
    return kInvalidArg;
#endif
  }

  static Result enable(MockPci &pci, const Preconditions &pre) {
#ifdef MUT_SKIP_PRECONDITIONS
    (void)pre;
#else
    if (!pre.gateAReady || !pre.flushRegistered || !pre.gfwReady || !pre.bar0Mapped)
      return kDenied;
    if (pre.flushPhase == FlushPhase::kUnregistered) return kDenied;
#endif
#ifdef MUT_ALLOW_BME_DURING_GATEA
    (void)0;
#else
    if (pre.gateAActive) return kDenied;  // BME stays OFF during Gate A
#endif
    const uint16_t cmd = pci.readCommand();
    if ((cmd & ~kKnownBits) != 0u) {
#ifdef MUT_IGNORE_UNKNOWN_BITS
      (void)0;
#else
      return kError;  // unknown bits: touch nothing
#endif
    }
    if ((cmd & kBmeBit) != 0u) return kAlreadyOn;  // no-op
#ifdef MUT_DOUBLE_ENABLE_FLIP
    (void)0;
#endif
    const uint16_t want = static_cast<uint16_t>(cmd | kBmeBit);
    if (!pci.writeConfig(kCmdOff, want)) return kError;
#ifdef MUT_DOUBLE_ENABLE_FLIP
    // MUTATION TEST-ONLY: second enable flips BME back off
    if (pci.readCommand() == kCmdEnabled) pci.writeConfig(kCmdOff, kCmdBaseline);
#endif
    return kOk;
  }

  static Result disable(MockPci &pci) {
    const uint16_t cmd = pci.readCommand();
    if ((cmd & ~kKnownBits) != 0u) return kError;
    if ((cmd & kBmeBit) == 0u) return kAlreadyOff;  // no-op
    if (!pci.writeConfig(kCmdOff, static_cast<uint16_t>(cmd & ~kBmeBit))) return kError;
    return kOk;
  }

 private:
  BmePolicy() = delete;
};

}  // namespace bme
