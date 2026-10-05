# SELECTOR9-ABI — VERIFY_SYSMEM_FLUSH_PRECONDITIONS v1 48B

`kGA106LabSelector_VerifySysmemFlushPreconditions = 9`, Count = 10.
Zero-input (scalar 0, struct 0); output `GA106LabFlushPrewriteV1` 48B.

Layout: size@0, version@4(=1), status@8, phase@12,
providerReady@16, ga106Exact@17, bar0Ready@18, mseEnabled@19,
gfwReady@20, bmeEnabled@21(requerido 0), pageReady@22, segmentCount@23(=1),
segmentLength@24(=4096), alignmentReady@28, addressRepresentable@29,
encodingRoundtripReady@30, flushRegistersState@31(=NotChecked),
programmed@32(=0), readbackMatch@33(=0), writeCount@34(=0), readCount@35(=0),
reserved[12]@36=0. static_asserts em Protocol.h (C e C++).

Status: PreconditionsReady(1)/AlreadyReady(2) = pronto; Failed(3) = resposta
valida mas precondicao nao atendida. Transporte Success + Failed = semantica,
nao erro (padrao GFW); erros IOKit so para BadArgument/Busy/Aborted/recursos.
CLI `verify-sysmem-flush-preconditions` imprime so semantica, nunca IOVA/HI/LO.
