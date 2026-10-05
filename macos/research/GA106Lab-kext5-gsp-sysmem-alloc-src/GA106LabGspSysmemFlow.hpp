// GA106LabGspSysmemFlow.hpp — ONE_SHARED_SYSMEM_FLOW (GSP-DMA1).
//
// Único fluxo de policy do selector 7 PREPARE_GSP_SYSMEM.
// Produção (SysmemRealOps) e testes (SysmemMockOps) instanciam O MESMO
// template RunPrepareGspSysmemFlow<Ops> + ReleaseGspSysmemFlow<Ops>.
// Static dispatch (sem virtual, sem callbacks runtime no ABI/UserClient).
//
// Escopo desta microfase: alocar 4 KiB, zerar, bindar IODMACommand
// (autoPrepare=false), prepare explícito 1x, resolver exatamente 1
// segmento DMA, validar largura/alinhamento, manter mapping vivo.
// Zero MMIO, zero PCI config, BME OFF, zero DMA trigger, zero firmware.
// Endereço DMA/bus é kernel-only e NUNCA entra na ABI.
//
// Ops deve prover (RealOps = só operações; MockOps = fixtures/counters):
//   uint32_t getState(); bool isBound(); bool isPrepared();
//   bool allocBuffer();      // withOptions(Out,4096,4096), cap>=4096
//   bool zeroBuffer();       // getBytesNoCopy != null, zera 4096
//   bool createCommand();    // withSpecification(host64,64,4096,kMapped,...)
//   bool bindDescriptor();   // setMemoryDescriptor(desc,false)
//   bool prepareDma();       // prepare(0,4096,true,true) exatamente 1x
//   bool genSegments(uint32_t &countOut, uint64_t &addrOut, uint64_t &lenOut);
//   bool validateAddressWidth(uint64_t addr); // hi/lo round-trip, sem overflow
//   void markAddressReady(uint64_t addr, uint32_t len);
//   void completeDma();      // complete(...) — SÓ se preparado
//   void clearDma();         // clearMemoryDescriptor(false) — SÓ se bound
//   void releaseCommand();   // no-op seguro se inexistente
//   void releaseDescriptor();// no-op seguro se inexistente
//   void resetState();       // addr=0,len=0,EMPTY,unbound,unprepared

#ifndef GA106LAB_GSP_SYSMEM_FLOW_HPP
#define GA106LAB_GSP_SYSMEM_FLOW_HPP

#include "GA106LabProtocol.h"

#include <IOKit/IOReturn.h>
#include <stdint.h>

// Teardown compartilhado: ordem única complete→clear→releases→reset,
// respeitando estado real (nunca double, nunca complete-sem-prepare).
template <typename Ops>
inline void ReleaseGspSysmemFlow(Ops &ops)
{
    if (ops.isPrepared()) {
        ops.completeDma();
    }
    if (ops.isBound()) {
        ops.clearDma();
    }
    ops.releaseCommand();
    ops.releaseDescriptor();
    ops.resetState();
}

template <typename Ops>
inline IOReturn RunPrepareGspSysmemFlow(Ops &ops, GA106LabGspSysmemV1 &out)
{
    uint32_t count = 0;
    uint64_t addr = 0;
    uint64_t len = 0;
    unsigned k = 0;

    out.size = 0;
    out.version = 0;
    out.status = kGA106LabGspSysmemStatus_Failed;
    out.flags = 0;
    out.bufferSize = 0;
    out.segmentCount = 0;
    out.segmentLength = 0;
    out.state = kGA106LabGspSysmemState_Empty;
    for (k = 0; k < 4; k++) {
        out.reserved[k] = 0;
    }

    // Repeat idempotente: mapping vivo => sem realocar, sem repreparar.
    if (ops.getState() == kGA106LabGspSysmemState_AddressReady) {
        out.size = (uint32_t)sizeof(out);
        out.version = GA106LAB_UC_PROTOCOL_VERSION;
        out.status = kGA106LabGspSysmemStatus_AlreadyPrepared;
        out.flags = kGA106LabGspSysmemFlag_AddressReady;
        out.bufferSize = kGA106LabGspSysmem_Size;
        out.segmentCount = 1;
        out.segmentLength = kGA106LabGspSysmem_Size;
        out.state = kGA106LabGspSysmemState_AddressReady;
        return kIOReturnSuccess;
    }
    if (ops.getState() != kGA106LabGspSysmemState_Empty) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnError; // estado transiente impossível via fluxo.
    }

    if (!ops.allocBuffer()) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnNoResources;
    }
    if (!ops.zeroBuffer()) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnNoMemory;
    }
    if (!ops.createCommand()) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnNoResources;
    }
    if (!ops.bindDescriptor()) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnError;
    }
    if (!ops.prepareDma()) {
        ReleaseGspSysmemFlow(ops); // bound, não preparado: clear sem complete.
        return kIOReturnError;
    }
    if (!ops.genSegments(count, addr, len)) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnNoMemory;
    }
    if (count != 1 || len != (uint64_t)kGA106LabGspSysmem_Size) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnNoMemory;
    }
    if ((addr % (uint64_t)kGA106LabGspSysmem_Alignment) != 0u) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnBadArgument;
    }
    if (!ops.validateAddressWidth(addr)) {
        ReleaseGspSysmemFlow(ops);
        return kIOReturnNoMemory;
    }

    ops.markAddressReady(addr, (uint32_t)len);

    out.size = (uint32_t)sizeof(out);
    out.version = GA106LAB_UC_PROTOCOL_VERSION;
    out.status = kGA106LabGspSysmemStatus_AddressReady;
    out.flags = kGA106LabGspSysmemFlag_AddressReady;
    out.bufferSize = kGA106LabGspSysmem_Size;
    out.segmentCount = 1;
    out.segmentLength = (uint32_t)len;
    out.state = kGA106LabGspSysmemState_AddressReady;
    return kIOReturnSuccess;
}

#endif /* GA106LAB_GSP_SYSMEM_FLOW_HPP */
