# P81 — Rollback & Recovery (documentação; NÃO executar neste turno)

```text
P81_EXECUTION_AUTHORIZATION = NONE / LIVE_EXECUTION_AUTHORIZED = NO
Este arquivo DESCREVE caminhos para uso futuro autorizado. Nada aqui foi executado.
Teste P81-D (proposto) faz zero writes => rollback = "nada a reverter" + verificação.
Matriz segue 26/26 NO + exceção recovery-only explícita autorizada por humano (ver §8).
```

## 1. EFI backup (princípio; para qualquer futuro B/C)
- Regra: nenhum futuro teste com boot-select/stage sem antes (a) backup completo da
  EFI/ESP para mídia externa + (b) `shasum` do backup + (c) foto da config ativa (E02 + metadados obrigatórios) +
  (d) anotação de qual config é known-good. Sem os 4 => ABORT antes de começar.
- Verificação read-only NO ALVO (futura, autorizada): identidade SEM montar
  (`diskutil info -plist` da ESP; comparar com known-good documentado). Hash de
  conteúdo da ESP (`shasum`) SÓ no ensaio Option 0 non-target — nunca no alvo.
  Nenhum write, nenhum mount (nem RO) no alvo. Fallback nunca escala para mount no alvo. (R2.)
- S12-identidade parcial do alvo é declarada explicitamente INSUFICIENTE para qualquer write futuro (EFI ou outro). (R2/F2.)
- O que NÃO fazer: editar `config.plist`/EFI "de cabeça"; montar ESP read-write
  "só para olhar" — nem read-only "só para o hash" no alvo; confiar em
  "eu lembro qual era".

## 2. Known-good config / KEXT
- Known-good hoje (declarado, não tocado): config que boota Tahoe com GA106Lab 1.5.3
  residente (live histórico). 1.8.0 NÃO é known-good (offline only).
- Regra: known-good nunca é sobrescrito; futuros testes usam entrada/config
  separada ou mídia separada; volta = reselecionar known-good, nunca "desfazer edit".

## 3. Boot recovery path (ordem de tentativa, futura; exceção recovery-only §8)
1. Re-selecionar known-good no picker (sem editar nada). Cada boot de recovery é
   evento numerado R-1/R-2 (hora + motivo + foto E02 + metadados); MÁXIMO 2 por
   janela abortada (G-M0244-10); estouro => ABORT + review, sem exceção.
2. Se sem picker/vídeo: desligar (procedimento do fabricante), esperar 30 s, ligar
   com picker forçado; fotografar cada tentativa (E02 + metadados obrigatórios) como
   evento numerado R-N.
3. Se ainda sem vídeo: Recovery do macOS (sem reinstalar nada; só para coletar logs
   se possível); documentar como evento numerado e parar.
4. Se ainda sem vídeo: parar tudo, fotos de LEDs/ventoinhas/cabos, pedir segunda
   pessoa (reviewer/alternativa NOMEADOS G-M0244-08); PROIBIDO abrir/ressentar HW ou "tentar outro cabo rapidinho" além do
   documentado — cada ação física extra entra como improvisação (ABORT cobre).

## 4. No-signal (detalhe)
- Tratar como ABORT-VIDEO/ABORT-HEALTH, não como "bug do teste" (o teste D não toca vídeo).
- Checklist: monitor ligado/entrada correta? (1 checagem, sem loop) → fotos (E02 + metadados) →
  recovery path acima (§8) → parar. Não entrar em loop de "trocar cabo/porta 5 vezes".

## 5. Pós-panic (detalhe; R4/R5)
- Não retentar automaticamente. Fotografar tela de panic se presente (E02 + metadados obrigatórios).
- Reboot SÓ pelo procedimento normal (exceção recovery-only §8); se hang total > 5 min, desligamento forçado
  pelo botão (fabricante). Primeiro boot pós-panic: estado pós-janela = DESCONHECIDO + HW possivelmente preso;
  coleta read-only bounded do novo panic log (evidência, sem diagnóstico) = NOVA janela com review (§ Review do FAIL em PASS-FAIL-ABORT),
  nunca continuação. Encerrar janela original como FAIL/ABORT. (R4.)
- VERIFY-pós-ABORT (IDENT-2 pelo comando-filtro G-M0244-04, 60 s, só se vídeo presente E após coleta mínima FOTO-abort + anotação) é a ÚNICA execução permitida pós-ABORT; atribuição de segundo panic é AMBÍGUA por escrito (G-M0244-11). (R5; AL-2.)
- PROIBIDO: deletar panic logs, "limpar NVRAM para ver se passa", reinstalar KEXT,
  rodar `kextcache`/`kmutil` write, ou qualquer "correção" no dia.

## 6. Logs pós-reboot (o que coletar, futuro autorizado, read-only; NOVA janela, R4)
- Novo panic log (se houver) + fotos LEDs + `log show` bounded da janela (predicado `eventMessage CONTAINS "panic"`, `--last 24h`, cap 200 linhas, 60 s) + mesmos IDENT-1..4
  para comparar antes/depois. Tudo com timestamps HH-MM-SS + INDEX com shasum. Nada além disso no dia.

## 7. O que NÃO retentar automaticamente (lista explícita)
- Stage/load/unload de qualquer KEXT; qualquer selector; qualquer MMIO/PCI/BME/
  DMA/GSP/firmware/VRAM/IRQ/reset/doorbell/PUT/command/IOGPU/IOAccelerator/Metal;
  qualquer EFI-write; qualquer "reboot de novo para confirmar"; qualquer diagnóstico
  de causa no dia. Cada item exige design + autorização + review próprios.

## 8. Exceção recovery-only explícita (R5/F8/F10)
- A matriz segue 26/26 NO para todas as classes de TESTE. Exceção única, adotada: ações
  físicas/humanas de RECOVERY — botão power (procedimento do fabricante), picker/reseleção
  de known-good, Recovery do macOS sem reinstalar — são autorizadas pelo OPERADOR HUMANO
  no dia SOMENTE em ABORT-HEALTH/ABORT-VIDEO; não são delegadas ao runbook nem contam como YES de teste.
- Alternativa documentada: se o operador preferir, tudo segue NO puro e exige-se YES humano
  dedicado e registrado antes de qualquer reboot/shutdown físico.
- Em ambos os casos: VERIFY-pós-ABORT (IDENT-2 pelo comando-filtro G-M0244-04, bound 60 s, só se vídeo presente E após coleta mínima) é a única
  execução permitida pós-ABORT; segundo panic pós-VERIFY tem atribuição AMBÍGUA declarada (G-M0244-11). Sem vídeo ou sem coleta mínima, nada se executa e o estado segue DESCONHECIDO.
- Contador recovery (G-M0244-10, DURA): MÁXIMO 2 recovery boots por janela abortada, eventos R-1/R-2 numerados; estouro = ABORT + review antes de qualquer nova janela. (AL-1.)
