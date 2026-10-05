# P66 FAILURE-MODES — tabela (§16)

Todos os modos retornam status semântico, sem retry automático, sem avançar para BME/DMA/firmware.
`PARTIAL_PROGRAMMING_POLICY` detalhada em HILO-COMMIT-ATOMICITY.md §5.

| # | Modo | Detecção (Gate) | Ação | Código sugerido |
|---|---|---|---|---|
| F1 | GFW != 0xFF | A (selector8) | abortar antes de alloc; sem writes | EAGAIN |
| F2 | MSE OFF | A (PCI read) | abortar; sem writes | EIO |
| F3 | BME estado inesperado (ON quando se esperava OFF) | A | registrar, abortar Gate B (BME ON não autoriza DMA; exige revisão) | EBUSY/EIO |
| F4 | Provider indisponível / retain falha | A | abortar | ENXIO |
| F5 | BAR0 ausente/curto (<0x100c40+4) | A | abortar | ENXIO |
| F6 | Flush page alloc falha | A | abortar, sem MMIO | ENOMEM |
| F7 | DMA mapping falha / genSegments falha | A | complete+release, abortar | EIO |
| F8 | segmentCount != 1 / length != 4096 | A | teardown mapping, abortar | EINVAL |
| F9 | IOVA misaligned (low 8 != 0) | A | teardown, abortar | EINVAL |
| F10 | IOVA fora da faixa representável / roundtrip fail / HI>>24 != 0 | A | teardown, abortar | ERANGE/EINVAL |
| F11 | HI/LO atuais != 0 (já programado) | A | abortar, sem reprogramar (ROLLBACK NOT_AUTHORIZED) | EBUSY |
| F12 | WPR2 presente (HI_WPR2 != 0) | A | abortar; GPU precisa reset (fora de escopo) | EBUSY |
| F13 | MMIO map falha (Gate B) | B PREPARE | abortar antes de COMMIT | EIO |
| F14 | readback mismatch (read != IOVA) | B após COMMIT | FAIL, manter página pinned, não reescrever, não prosseguir | EIO |
| F15 | stop/close durante PREPARE | A/B PREPARE | abortar limpo (só SW cleanup), writeCount=0 | ECANCELED |
| F16 | stop entre HI e LO / após HI antes de LO (crítico) | B COMMIT | completar o par (política), depois FAIL controlado se stop pendente; sem restore cego | ECANCELED (com writeCount=2 registrado) |
| F17 | stop após COMMIT (durante readback/CLEANUP) | B | terminar readback imediato, retornar, drenar | OK ou ECANCELED conforme ponto |
| F18 | Owner liberado / use-after-free tentado | qualquer | nunca tocar HW; FAIL; prova por retain/drain (lição 1.6.2) | ESHUTDOWN |

Caso crítico F16: ver COMMIT_POINT_MODEL. Não assumir restauração de valores antigos sem fonte.
