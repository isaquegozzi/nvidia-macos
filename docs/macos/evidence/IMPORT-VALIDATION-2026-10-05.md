# Validação da importação — 2026-10-05

## Cópia local

O pacote `NVIDIA-PROJETO-2026-10-05` foi copiado integralmente do pendrive para a pasta local Nvidia. Foram transferidos 14.955 arquivos regulares, com 405.436.030 bytes de conteúdo. A verificação `rsync --checksum --dry-run` terminou sem diferenças.

A cópia original e o projeto Linux existente foram preservados. Para a publicação, fontes e documentos foram selecionados e alguns dados da máquina foram removidos da documentação pública. O manifesto registra hashes de origem e publicação.

## Laboratório Linux

Ambiente de validação: GCC 16.2.1, CMake e Ninja, compilação `RelWithDebInfo`.

```text
common_smoke: PASS
compare_logic: PASS
chip_id: PASS
CTest: 3/3
```

A compilação incluiu `ga106-lab`. Os comandos de coleta de hardware não foram executados.

## Modelos offline importados do Mac

Execução: `scripts/test-macos-models.sh`, com compilador C++17, sem acesso a GPU ou IOKit.

```text
test-production-phase: PASS (5 grupos)
test-channel-generation-contract: PASS (6 grupos)
test-seq-fuzz-contract: PASS (4 grupos)
test-completion-chain-contract: PASS (7 grupos)
test-contract-fuzz: PASS (4 grupos)
test-vm: PASS (238 verificações, zero falhas)
test-submission: PASS (132 verificações, zero falhas)
Total: 7/7 suítes
```

## Limites desta validação

Não foram executados build de KEXT com Xcode, carga de extensão, alteração de EFI, reboot, comandos `ga106ctl`, leituras/escritas de GPU, DMA ou ensaios Metal. ASAN, TSAN e a matriz completa de mutações do Mac não foram repetidos nesta sessão.

Resultados de hardware e sanitizadores recebidos do Mac permanecem classificados como registros históricos.
