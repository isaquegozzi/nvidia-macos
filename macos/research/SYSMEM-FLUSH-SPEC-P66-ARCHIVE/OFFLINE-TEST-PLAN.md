# P66 OFFLINE-TEST-PLAN — spec de testes futuros (§20, sem implementar agora)

Todos host-side + auditoria estática; nenhum live neste prompt.

## 1. Encoding (puros, sem HW)

- roundtrip: para addrs artificiais (incl. §3 Ex. A–C), `decode(encode(a))==a`.
- overflow: `HI>>24==0` sempre; `addr=0xFFFF...FF00` → HI==0xFFFFFF, LO==0xFFFFFFFF.
- alignment: `addr&0xFF!=0` → EINVAL; `addr&0xFFF==0` (4096) sempre PASS.
- wrong-shift mutations: `(addr>>4)`, `(addr>>12)`, `(addr>>32)` devem FALHAR no roundtrip.
- reserved: HI com bits 31:24 setados → rejeitar.

## 2. Mapping (mock, sem HW)

- one-segment: mock com 1×4096 → PASS; 2 segmentos / len!=4096 → FAIL.
- lifetime: prepare→complete balanceado; complete sem prepare → FAIL; leak check (descriptor/command release em todo caminho F6–F10).
- stop-before-commit: stop durante PREPARE → writeCount==0, sem stores (contador de MMIO mockado).
- stop-at-commit: stop injetado entre HI e LO (mock) → política "completa o par" verificada, writeCount==2, sem terceiro write.

## 3. MMIO (mock de BAR)

- exact-count: Gate B executa exatamente 2 stores (HI@0x100c40, LO@0x100c10) + 0 ou 2 loads.
- order: HI antes de LO; mutation reversed deve ser detectada pelo teste.
- partial-store: só HI / só LO → teste FAIL (detecta auditoria).
- extra-store: qualquer 3º store, PCI config write, BME enable, DMA trigger, Falcon reset, firmware action → FAIL.
- readback: match → readbackMatch=1; mismatch injetado → EIO + sem reescrita.

## 4. ABI

- sem exposição: scan de `PrewriteA_Out`/`GateB_Out` prova ausência de IOVA/VA/físico/HI/LO/MMIO addrs/ponteiros (static_assert de tamanho 48 + teste de zero em reserved).
- userspace não controla: fuzz de input (qualquer bytes) não altera HI/LO escritos (são derivados da página interna).

## 5. Concorrência (lições 1.6.2)

- close-during-commit: close marca, free só após drain; sem write-after-stop.
- double-call: 2ª chamada com HI/LO !=0 → EBUSY, sem segundo par de writes.
- selector7-call mutation: Gate A/B que chame selector7 internamente → FAIL em auditoria.

## 6. Auditoria estática (futura)

- `write_reg` count == 2, ambos com registradores fixos; `setBusLeadEnable/setBusMasterEnable` ausente; `dma_load`/`doorbell`/`Falcon::` ausentes em Gate B; `genIOVMSegments` só em PREPARE.

```text
OFFLINE_TEST_PLAN_READY = YES (spec pronta; implementação pendente de Astra + autorização)
```
