// test-production-contracts.cpp — P80 §7/§10: contratos + oracles P77/P78/P79.
#include <cassert>
#include <cstdio>
#include "../SHADOW-P77-M10-CHANNEL-SIM/channel_sim.hpp"
#include "../SHADOW-P78-VM-MMU-SIM/vm_sim.hpp"
#include "../SHADOW-P79-COMMAND-SUBMISSION-SIM/submission_sim.hpp"
#include "GA106LabModelContracts.h"

static int g = 0;
#define PASS(m) do { printf("%s\n", m); g++; } while (0)

int main(void) {
    // Channel contract vs P77 oracle.
    {
        GA106LabChannelContract c{};
        assert(!GA106LabChannelContractValid(NULL));
        assert(!GA106LabChannelContractValid(&c)); // zeros: vasHandle 0
        c.chid = 2048; c.vasHandle = 1; c.gpfifoLength = 128;
        assert(!GA106LabChannelContractValid(&c));
        c.chid = 7; c.runq = 2;
        assert(!GA106LabChannelContractValid(&c));
        c.runq = 0; c.gpfifoLength = 100; // não múltiplo de 8
        assert(!GA106LabChannelContractValid(&c));
        c.gpfifoLength = 24; // 3 entries, não pot.2
        assert(!GA106LabChannelContractValid(&c));
        c.gpfifoLength = 128; c.engineKnown = 1; c.scheduled = 1;
        assert(GA106LabChannelContractValid(&c));
        assert(GA106LabChannelContractEntries(&c) == 16);
        // Oracle P77: mesmos inputs passam no Alloc até Scheduled.
        chsim::FifoModel fifo;
        fifo.NotifyInitDone(); fifo.NotifyStaticInfo();
        chsim::ChannelReq r;
        r.chid = 7; r.runq = 0; r.engine = chsim::Engine::kGr0; r.vasHandle = 1;
        r.gpfifoLength = 128; r.gpfifoOffset = 0;
        r.instSize = 4096; r.userdSize = 4096; r.mthdbufSize = 4096;
        r.instAddr = 0x10000; r.userdAddr = 0x20000; r.mthdbufAddr = 0x30000;
        assert(fifo.Alloc(r) == chsim::Result::kOk);
        assert(fifo.Bind(7) == chsim::Result::kOk);
        assert(fifo.Schedule(7, true) == chsim::Result::kOk);
        // Oracle P77 rejeita chid>=2048 e runq>=2 como o contrato.
        r.chid = 2048;
        assert(fifo.Alloc(r) == chsim::Result::kInvalidChid);
    }
    PASS("CHANNEL_CONTRACT_AND_P77_ORACLE");

    // VM contract vs P78 oracle.
    {
        GA106LabVmContract m{};
        assert(!GA106LabVmContractValid(NULL));
        assert(!GA106LabVmContractValid(&m)); // shift 0 inválido
        m.shift = 12; m.va = 0; m.size = 0x1000; m.aperture = 2; m.flushed = 1;
        assert(GA106LabVmContractValid(&m));
        m.aperture = 1;
        assert(!GA106LabVmContractValid(&m));
        m.aperture = 2; m.shift = 38;
        assert(!GA106LabVmContractValid(&m));
        m.shift = 12; m.va = 1;
        assert(!GA106LabVmContractValid(&m));
        // Oracle P78: aperture 1 rejeitada no Map; wrap detectado.
        vm78::Vm vm;
        vm78::VaspaceAllocParams ap;
        assert(vm.VaspaceAlloc(ap, false) == vm78::Fault::kNone);
        vm78::CopyPdesParams cp;
        cp.pageSize = vm78::kServerSize; cp.virtLo = vm78::kServerStart;
        cp.virtHi = vm78::kServerEnd - 1; cp.numLevels = 5; cp.rootPhys = 0x10000;
        assert(vm.CopyServerReservedPdes(cp) == vm78::Fault::kNone);
        uint64_t va = 0;
        assert(vm.VmGet(12, 0x1000, &va) == vm78::Fault::kNone);
        assert(vm.Map(va, 12, 0x10000, static_cast<vm78::Aperture>(1),
                      false, false, 0) == vm78::Fault::kApertureMismatch);
    }
    PASS("VM_CONTRACT_AND_P78_ORACLE");

    // Submission contract vs P79 oracle + cross-model.
    {
        GA106LabSubmissionContract s{};
        assert(!GA106LabSubmissionContractValid(NULL));
        assert(!GA106LabSubmissionContractValid(&s)); // entries 0
        s.entries = 16; s.put = 0; s.get = 0; s.tokenLive = 1;
        assert(GA106LabSubmissionContractValid(&s));
        s.put = 16;
        assert(!GA106LabSubmissionContractValid(&s));
        s.put = 0; s.entries = 24; // não pot.2
        assert(!GA106LabSubmissionContractValid(&s));
        s.entries = 16;
        GA106LabChannelContract c{};
        c.chid = 7; c.runq = 0; c.engineKnown = 1; c.vasHandle = 1;
        c.gpfifoLength = 128; c.scheduled = 1;
        GA106LabVmContract m{};
        m.shift = 12; m.va = 0x200000; m.size = 0x1000; m.aperture = 2; m.flushed = 1;
        assert(GA106LabCrossContractValid(&c, &m, &s, 0x200000, 64));
        assert(!GA106LabCrossContractValid(&c, &m, &s, 0x200000, 0));
        assert(!GA106LabCrossContractValid(&c, &m, &s, 0x300000, 64)); // fora do mapping
        c.scheduled = 0;
        assert(!GA106LabCrossContractValid(&c, &m, &s, 0x200000, 64));
        c.scheduled = 1; m.flushed = 0;
        assert(!GA106LabCrossContractValid(&c, &m, &s, 0x200000, 64));
        m.flushed = 1; s.tokenLive = 0;
        assert(!GA106LabCrossContractValid(&c, &m, &s, 0x200000, 64));
        s.tokenLive = 1; s.entries = 8; // diverge do channel (16)
        assert(!GA106LabCrossContractValid(&c, &m, &s, 0x200000, 64));
        // Oracle P79: token ausente bloqueia Append.
        sub79::SubmitChannel sub;
        sub.Configure(4, 0x100000, 0x200000);
        sub79::GpEntry e; e.get = 0; e.length = 64;
        sub.SetPbLive(0x200000, true);
        assert(sub.Append(e) == sub79::Fault::kTokenMissing);
    }
    PASS("SUBMISSION_CROSS_CONTRACT_AND_P79_ORACLE");

    printf("PRODUCTION_CONTRACT_TESTS = PASS (%d groups)\n", g);
    return 0;
}
