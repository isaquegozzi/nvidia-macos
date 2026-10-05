// test-flush-prewrite.cpp — Gate A selector9 offline (TG-KEXT7, P68 §15).
// Zero HW: pure encoding/ABI + shared sysmem page template via MockOps
// (mesmo template da producao com estado dedicado em producao).
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "GA106LabProtocol.h"
#include "GA106LabFlushPrewriteFlow.hpp"
#include "GA106LabGspSysmemFlow.hpp"
#include "GA106LabMockOps.hpp"
#include "ga106ctl-validate.h"

static int gGroups = 0;
#define PASS(m) do { printf("%s\n", m); gGroups++; } while (0)

static void makeGood(GA106LabFlushPrewriteV1 &o) {
    memset(&o, 0, sizeof(o));
    o.size = 48; o.version = 1;
    o.status = kGA106LabFlushPrewriteStatus_PreconditionsReady;
    o.phase = kGA106LabFlushPrewritePhase_PreconditionsReady;
    o.providerReady = 1; o.ga106Exact = 1; o.bar0Ready = 1;
    o.mseEnabled = 1; o.gfwReady = 1; o.bmeEnabled = 0;
    o.pageReady = 1; o.segmentCount = 1; o.segmentLength = 4096;
    o.alignmentReady = 1; o.addressRepresentable = 1; o.encodingRoundtripReady = 1;
    o.flushRegistersState = kGA106LabFlushRegs_NotChecked;
    o.programmed = 0; o.readbackMatch = 0; o.writeCount = 0; o.readCount = 0;
}

int main(void) {
    // 1. ABI size/contract.
    { assert(sizeof(GA106LabFlushPrewriteV1) == 48);
      GA106LabFlushPrewriteV1 o; makeGood(o);
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) == 0);
      assert(ga106ctl_flush_prewrite_cli_exit(1, &o, sizeof(o)) == 0);
      assert(ga106ctl_flush_prewrite_cli_exit(0, &o, sizeof(o)) != 0); }
    PASS("ABI 48B contract + cli exit");
    // 2. Encoding boundaries (artificiais, sem HW).
    { uint64_t a = 0x00000000ABCDEF00ull;
      assert(GA106LabFlushAlignmentOk(a));
      assert(GA106LabFlushEncodeLo(a) == 0x00ABCDEFu);
      assert(GA106LabFlushEncodeHi(a) == 0u);
      assert(GA106LabFlushDecode(GA106LabFlushEncodeLo(a), GA106LabFlushEncodeHi(a)) == a);
      assert(GA106LabFlushRepresentable(a)); }
    { uint64_t a = 0x0000123456789A00ull;
      assert(GA106LabFlushEncodeLo(a) == 0x3456789Au);
      assert(GA106LabFlushEncodeHi(a) == 0x12u);
      assert(GA106LabFlushDecode(GA106LabFlushEncodeLo(a), GA106LabFlushEncodeHi(a)) == a);
      assert(GA106LabFlushRepresentable(a)); }
    { uint64_t mis = 0x12345678ull; // low 8 != 0
      assert(!GA106LabFlushAlignmentOk(mis));
      assert(!GA106LabFlushRepresentable(mis)); }
    { uint64_t mx = 0x00FFFFFFFFFF00ull;
      // low 8 zero, HI non-zero, sem overflow de pagina
      assert(GA106LabFlushRepresentable(mx));
      assert(GA106LabFlushEncodeHi(mx) == 0xFFu);
      assert(GA106LabFlushEncodeLo(mx) == 0xFFFFFFFFu); }
    PASS("encoding roundtrip/misalign/boundaries");
    // 3. ABI failures.
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.bmeEnabled = 1;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.mseEnabled = 0;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.gfwReady = 0;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.segmentCount = 2;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.segmentLength = 2048;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.segmentCount = 0;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.reserved[3] = 1;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.programmed = 1;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.writeCount = 2;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.flushRegistersState = 1;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.status = kGA106LabFlushPrewriteStatus_Failed;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    PASS("ABI failures BME/MSE/GFW/segments/reserved/programmed");
    // 4. Wrong device / provider.
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.ga106Exact = 0;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    { GA106LabFlushPrewriteV1 o; makeGood(o); o.providerReady = 0;
      assert(ga106ctl_check_flush_prewrite(&o, sizeof(o)) != 0); }
    PASS("wrong device/provider");
    // 5. Repeated idempotent observation.
    { GA106LabFlushPrewriteV1 a, b; makeGood(a); makeGood(b);
      b.status = kGA106LabFlushPrewriteStatus_AlreadyReady;
      assert(ga106ctl_check_flush_prewrite(&a, sizeof(a)) == 0);
      assert(ga106ctl_check_flush_prewrite(&b, sizeof(b)) == 0); }
    PASS("repeated AlreadyReady idempotent");
    // 6. Dedicated page via shared template (MockOps proxy): 1x4096, vivo, sem HW.
    { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
      GA106LabSysmemMockMakeLive(m); memset(&out, 0, sizeof(out));
      IOReturn kr = RunPrepareGspSysmemFlow(m, out);
      assert(kr == kIOReturnSuccess);
      assert(out.segmentCount == 1 && out.segmentLength == 4096);
      assert(m.mmioWriteCount == 0 && m.configWriteCount == 0);
      assert(m.bmeEnableCount == 0 && m.dmaTriggerCount == 0);
      assert(m.mmioReadCount == 0); }
    // 6b. alloc failure / multi-segment / short-segment via mock flags.
    { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
      GA106LabSysmemMockMakeLive(m); m.allocFail = true; memset(&out, 0, sizeof(out));
      assert(RunPrepareGspSysmemFlow(m, out) != kIOReturnSuccess); }
    { GA106LabSysmemMockOps m; GA106LabGspSysmemV1 out;
      GA106LabSysmemMockMakeLive(m); m.segMode = 2; memset(&out, 0, sizeof(out));
      IOReturn kr = RunPrepareGspSysmemFlow(m, out);
      assert(kr != kIOReturnSuccess || out.segmentCount != 1); }
    PASS("dedicated page alloc/multi fail, zero HW");
    printf("FLUSH_PREWRITE_GROUPS=%d PASS\n", gGroups);
    return 0;
}
