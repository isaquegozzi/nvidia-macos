> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P80 — Open Questions

1. **Ordem estrita dos prepares**: milestones fora de ordem deixam a fase para trás
   (fail-closed). Se a produção futura exigir prepares fora de ordem, justificar e
   revisar a máquina — não relaxar silenciosamente.
2. **Selector 5 sem fase**: BOOT0 é corroboração; a fase avança no 6. Se um futuro
   gate depender de BOOT0 isolado, adicionar estado com justificativa.
3. **LiveBlocked só limpa em `start()`**: conservador de propósito; revisar quando
   houver fluxo de re-autorização (fora do P80).
4. **Contratos cobrem geometria, não semântica de engine**: `engineKnown` é token
   (herdado do P77); métodos de engine seguem no P79-allowlist, não no KEXT.
5. **Futuros handles DMA/GSP**: ownership reservado ao serviço (matriz no audit);
   bind real exigirá prova de lifetime antes de qualquer enable.
6. **TSAN no KEXT**: concorrência de kernel provada por disciplina + doubles com
   pthreads; TSAN direto no binário do KEXT fora de escopo offline.
