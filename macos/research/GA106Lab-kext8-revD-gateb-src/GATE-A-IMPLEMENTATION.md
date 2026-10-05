# GATE-A-IMPLEMENTATION — composicao read-only (P68 §§4-9)

REQUIRED implementados: provider alive, GA106 exact, BAR0 mapped/len>=0x100c44,
MSE ON, GFW Completed (via readGfwBootReadiness interno, sem invoke via
UserClient), BME==OFF, dedicated page 4096 (mesmo template RunPrepareGspSysmemFlow
com estado DISJUNTO fFlush*, sem reuso de selector7), 1x4096, alinh 4096,
IOVA 256-aligned, representavel 8..63, roundtrip HI/LO, lifetime healthy.

DIAGNOSTICOS OMITIDOS nesta revisao (P68 §5): HI/LO atuais, WPR2, BOOT0/42.
Motivo: sem path simples/claro que nao amplie escopo (WPR2 exigiria novo
mapeamento/leituras 0x1fa828 fora dos paths provados; HI/LO exigiriam novas
leituras 0x100c10/40). flushRegistersState = NotChecked; readCount = 0.
Revisao futura pode adiciona-los somente com justificativa + prova read-only.

GFW: reuso via chamada direta ao metodo provider (shared flow), nao via
UserClient; fresh bounded check por chamada (poll 1ms/4s ja definido), sem
retry alem do bound. BME: so leitura de Command via snapshot; nunca
setBusLeadEnable/setBusMasterEnable/configWrite. Segunda chamada: revalida
PCI/BAR/GFW e reusa mapping (AlreadyReady), sem realocar.
