// gboot_sim.hpp — M15 GSP boot-sequencing host-only model (P76-DUAL-OVERNIGHT-V2).
// Isolated. Zero production wiring, zero real FW/HW. Semantics from local
// GSP-NEXTSTEP-P65/GSP-BOOT-SEQUENCE.md + GSP-BOOT-STATE-MACHINE.md (Nova
// gsp/boot.rs, hal/tu102+ga102, cmdq.rs, sequencer; OpenRM kernel_gsp.c).
// NO code copied. Stage order + exit criteria + timeouts mirror the docs:
// GfwReady -> FlushRegistered -> FwsecFrtsDone -> MboxBooted -> BooterLoadDone
// -> Executing -> RpcReady -> InitDone(+STATIC_INFO owned by caller).
// Budgets: RISC-V poll 10ms quantum / 5000ms (read_poll_timeout); cmdq receive
// 5s with ERANGE=>continue (each consumed message costs one 1ms fake quantum,
// mirroring the 1ms cmdq poll). Terminal Failed/Done: no auto-retry (house rule).
#pragma once
#include <cstdint>

namespace gboot {

inline constexpr uint64_t kRiscvQuantumMs = 10u;
inline constexpr uint64_t kRiscvBudgetMs = 5000u;
inline constexpr uint64_t kMsgBudgetMs = 5000u;
inline constexpr uint64_t kMsgQuantumMs = 1u;

enum class Result : int32_t {
  kOk = 0,
  kOutOfOrder = -1,      // stage precondition not met
  kAlreadyTerminal = -2, // call after Failed/InitDone (fresh object for retry)
  kFwsecError = -3,      // FRTS_ERR != 0
  kWprMismatch = -4,     // wpr2_range() != frts (start mismatch)
  kMboxError = -5,       // mailbox0 != 0 (boot / booter-load failed)
  kRiscvTimeout = -6,    // RISC-V not active within 5s
  kSeqTimeout = -7,      // sequencer GspSequence not received within 5s
  kInitTimeout = -8,     // GspInitDone not received within 5s
};

enum class Stage : uint8_t {
  kGfwReady = 0,
  kFlushRegistered = 1,
  kFwsecFrtsDone = 2,
  kMboxBooted = 3,
  kBooterLoadDone = 4,
  kExecuting = 5,
  kRpcReady = 6,
  kInitDone = 7,
  kFailed = 8,
};

enum class FailAt : uint8_t {
  kNone = 0,
  kFwsec = 1,
  kMbox = 2,
  kBooter = 3,
  kRiscv = 4,
  kSequencer = 5,
  kInitDone = 6,
};

struct FakeClock {
  uint64_t nowMs = 0u;
  void advance(uint64_t ms) { nowMs += ms; }
};

class Boot {
 public:
  Boot() = default;
  Stage stage() const { return stage_; }
  Result lastResult() const { return last_; }
  FailAt failedAt() const { return failedAt_; }

  // Phase 0 tail: sysmem flush page registered (HI+LO posted). Precondition for all.
  Result flushRegistered() {
    if (!pre(Stage::kGfwReady)) return last_;
    stage_ = Stage::kFlushRegistered;
    return setLast(Result::kOk);
  }

  // Phase 1: FWSEC-FRTS. errFlag ~ FRTS_ERR!=0; wprOk ~ wpr2_range()==frts.
  Result fwsecFrts(bool errFlag, bool wprOk) {
    if (!pre(Stage::kFlushRegistered)) return last_;
#ifdef MUT_FORCE_FRTS_ERR_OK
    (void)errFlag;
    (void)wprOk;
#else
    if (errFlag) return fail(FailAt::kFwsec, Result::kFwsecError);
    if (!wprOk) return fail(FailAt::kFwsec, Result::kWprMismatch);
#endif
    stage_ = Stage::kFwsecFrtsDone;
    return setLast(Result::kOk);
  }

  // Phase 2 head: GSP reset + boot(libos lo/hi), halted with mbox0 read back.
  Result mboxBoot(uint32_t mbox0) {
    if (!pre(Stage::kFwsecFrtsDone)) return last_;
#ifdef MUT_ACCEPT_MBOX_NONZERO
    (void)mbox0;
#else
    if (mbox0 != 0u) return fail(FailAt::kMbox, Result::kMboxError);
#endif
    stage_ = Stage::kMboxBooted;
    return setLast(Result::kOk);
  }

  // Phase 2 tail: Booter via SEC2 + boot(wpr_meta); SEC2 mbox0 must be 0.
  Result booterLoad(uint32_t sec2mbox0) {
    if (!pre(Stage::kMboxBooted)) return last_;
#ifdef MUT_ACCEPT_MBOX_NONZERO
    (void)sec2mbox0;
#else
    if (sec2mbox0 != 0u) return fail(FailAt::kBooter, Result::kMboxError);
#endif
    stage_ = Stage::kBooterLoadDone;
    return setLast(Result::kOk);
  }

  // Phase 3 head: write_os_version + RISC-V active poll (10ms/5s).
  Result riscvPoll(FakeClock &clk, uint64_t activeAfterMs) {
    if (!pre(Stage::kBooterLoadDone)) return last_;
    uint64_t spent = 0u;
    while (spent < activeAfterMs) {
      clk.advance(kRiscvQuantumMs);
      spent += kRiscvQuantumMs;
#ifdef MUT_IGNORE_RISCV_TIMEOUT
      (void)0;
#else
      if (spent >= kRiscvBudgetMs) return fail(FailAt::kRiscv, Result::kRiscvTimeout);
#endif
    }
    stage_ = Stage::kExecuting;
    return setLast(Result::kOk);
  }

  // Phase 3 mid: SetSystemInfo + SetRegistry (no-wait, modeled as instant) then
  // firmware-driven sequencer: eranges non-matching msgs consumed (1ms each),
  // then GspSequence; seqTimeout stalls past the 5s budget.
  Result rpcSequence(FakeClock &clk, uint64_t seqEranges, bool seqTimeout) {
    if (!pre(Stage::kExecuting)) return last_;
    uint64_t cost = seqEranges * kMsgQuantumMs;
    if (seqTimeout) cost = kMsgBudgetMs;
    if (cost >= kMsgBudgetMs) {
      clk.advance(kMsgBudgetMs);
      return fail(FailAt::kSequencer, Result::kSeqTimeout);
    }
    clk.advance(cost + kMsgQuantumMs);  // consumed msgs + the sequence itself
    stage_ = Stage::kRpcReady;
    return setLast(Result::kOk);
  }

  // Phase 3 tail: wait_gsp_init_done loop (ERANGE=>continue, 5s budget).
  Result initDone(FakeClock &clk, uint64_t eranges, bool timeout) {
    if (!pre(Stage::kRpcReady)) return last_;
    uint64_t cost = eranges * kMsgQuantumMs;
    if (timeout) cost = kMsgBudgetMs;
#ifdef MUT_SWALLOW_INIT_TIMEOUT
    if (timeout) {
      clk.advance(kMsgBudgetMs);
      stage_ = Stage::kInitDone;  // MUTATION TEST-ONLY: timeout reported as done
      return setLast(Result::kOk);
    }
#else
    if (cost >= kMsgBudgetMs) {
      clk.advance(kMsgBudgetMs);
      return fail(FailAt::kInitDone, Result::kInitTimeout);
    }
#endif
    clk.advance(cost + kMsgQuantumMs);
    stage_ = Stage::kInitDone;
    return setLast(Result::kOk);
  }

 private:
  Result setLast(Result r) {
    last_ = r;
    return r;
  }
  Result fail(FailAt at, Result r) {
    stage_ = Stage::kFailed;
    failedAt_ = at;
    return setLast(r);
  }
  // State gate: exact expected stage, else OutOfOrder (or AlreadyTerminal when
  // called on a terminal object). SKIP hook removes ordering entirely.
  bool pre(Stage want) {
#ifdef MUT_SKIP_ORDER
    (void)want;
    if (stage_ == Stage::kInitDone || stage_ == Stage::kFailed) {
      setLast(Result::kAlreadyTerminal);
      return false;
    }
    return true;
#else
    if (stage_ == Stage::kInitDone || stage_ == Stage::kFailed) {
      setLast(Result::kAlreadyTerminal);
      return false;
    }
    if (stage_ != want) {
      setLast(Result::kOutOfOrder);
      return false;
    }
    return true;
#endif
  }
  Stage stage_ = Stage::kGfwReady;
  Result last_ = Result::kOk;
  FailAt failedAt_ = FailAt::kNone;
};

}  // namespace gboot
