# P81 — Authorization Matrix (tudo NO)

```text
SESSION_ID: P81-DESIGN-RELAY-V3
P81_EXECUTION_AUTHORIZATION = NONE
LIVE_EXECUTION_AUTHORIZED = NO
Este turno NÃO autoriza nem executa live. Qualquer YES futuro exige humano +
review dedicado por classe. "Observar" abaixo = só se um dia autorizado; hoje = NO.
```

| # | Classe de ação | P81 (este design) | Notas |
|---|---|---|---|
| 01 | READ_ONLY_DIAGNOSTIC (ioreg/kextstat/log/nvram/shasum RO) | NO | Proposta D descreve uso futuro; hoje nada executado |
| 02 | KEXT_STAGE | NO | — |
| 03 | KEXT_LOAD (incl. 1.8.0 e qualquer outro) | NO | C rejeitada em P81 |
| 04 | KEXT_UNLOAD | NO | Caminho de teardown temido (T3/T4) |
| 05 | SELECTOR_LIVE (qualquer selector, incl. read-only) | NO | Ver S15: read-only ≠ inofensivo |
| 06 | FIRST_MMIO_WRITE | NO | "Ainda não fazer write nenhum" |
| 07 | FIRST_MMIO_READ (nova, além do OS-level) | NO | Nenhuma leitura HW nova em D |
| 08 | PCI_CONFIG_WRITE | NO | — |
| 09 | PCI_CONFIG_READ (nova, além do OS-level) | NO | Só via ioreg existente em D |
| 10 | BME_ENABLE | NO | — |
| 11 | DMA_REAL | NO | — |
| 12 | GSP_LIVE | NO | — |
| 13 | FIRMWARE_UPLOAD | NO | — |
| 14 | VRAM_WRITE | NO | — |
| 15 | VRAM_READ (nova) | NO | — |
| 16 | INTERRUPTS (enable/config) | NO | — |
| 17 | RESET (GPU/function) | NO | — |
| 18 | DOORBELL | NO | — |
| 19 | PUT_ADVANCE | NO | — |
| 20 | COMMAND_SUBMISSION | NO | P79 segue offline/simulação |
| 21 | IOGPU | NO | — |
| 22 | IOACCELERATOR | NO | — |
| 23 | METAL / MTLDevice (criar/usar) | NO | — |
| 24 | EFI_WRITE / CONFIG_WRITE | NO | Só verificação RO futura |
| 25 | REBOOT / SHUTDOWN (iniciados pelo teste) | NO | B um dia exige; hoje NO; exceção recovery-only só-humana em § Exceção (abaixo), nunca YES de teste |
| 26 | NVRAM_WRITE / `kmutil`/`kextcache` write / panic-log delete | NO | Proibidos no dia (ver rollback §5) |

## Exceção recovery-only explícita (R5; matriz segue 26/26 NO para teste)
- Exceção única, autorizada pelo OPERADOR HUMANO no dia SOMENTE em ABORT-HEALTH/ABORT-VIDEO:
  botão power (procedimento do fabricante), picker/reseleção de known-good, Recovery do
  macOS sem reinstalar. Não delegada ao runbook; não conta como YES de teste.
  LIMITE NUMÉRICO (G-M0244-10, AL-1): MÁXIMO 2 recovery boots por janela abortada,
  cada boot registrado como evento numerado R-1/R-2 (hora + motivo + foto); estouro = ABORT + review.
- Alternativa documentada: tudo segue NO puro e exige-se YES humano dedicado e registrado
  antes de qualquer reboot/shutdown físico.
- VERIFY-pós-ABORT (IDENT-2 pelo comando-filtro G-M0244-04, bound 60 s, só se vídeo presente E após coleta mínima; atribuição de segundo panic AMBÍGUA por escrito G-M0244-11) é a única execução permitida
  pós-ABORT. Hard flags seguem todos NO: FIRST_MMIO_WRITE, PCI_CONFIG_WRITE, BME_ENABLE,
  DMA, INTERRUPTS, RESET, GSP, FIRMWARE, VRAM_WRITE, COMMAND_SUBMISSION (nenhuma frase
  deste pacote autoriza BME/MMIO/KEXT/comando/live).

```text
Contadores live deste turno: todos 0 (LIVE/REBOOT/SHUTDOWN/EFI/KEXT_LOAD/
SELECTORS_LIVE/MMIO/PCI/BME/DMA/GSP_LIVE/FIRMWARE/VRAM/INTERRUPTS/RESET/
DOORBELL/PUT/COMMAND_LIVE/IOGPU/IOAccelerator/MTLDevice/Metal = 0).
Verificação: nenhum comando live executado; só escrita de documentação P81.
```
