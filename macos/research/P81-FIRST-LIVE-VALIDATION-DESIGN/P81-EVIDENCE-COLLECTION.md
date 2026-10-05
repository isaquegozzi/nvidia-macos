# P81 — Evidence Collection (o quê, onde, como voltar ao estado anterior)

```text
P81_EXECUTION_AUTHORIZATION = NONE / LIVE_EXECUTION_AUTHORIZED = NO
```

## O que coletar (teste D proposto; tudo read-only)
| # | Artefato | Comando-fonte (a autorizar no futuro) | Bound quantificado (R6) |
|---|---|---|---|
| E01 | FOTO-0 (tela+torre/LEDs pré) | câmera | nítida (ver bound FOTO abaixo); E02 + metadados de câmera OBRIGATÓRIOS em TODAS as fotos |
| E02 | IDENT-1: data/hora + macOS + config OC ativa (leitura) | `date; sw_vers;` + leitura de config (sem editar) + clock-verify G-M0244-01-R1 (read-only, P81.1) | 60 s; E02 é a fonte de timestamp de todas as FOTOs; sem G-M0244-01-R1 PASS, janela NÃO abre |
| E03 | IDENT-2: kextstat filtrado | COMANDO-FILTRO EXATO G-M0244-04 (leitura; sem variação no dia) | 60 s; só presença — nada além disso até o método Q10 |
| E04 | IDENT-3: ioreg resumido | `ioreg` com profundidade ≤ 3 e cap de 300 linhas | 60 s |
| E05 | IDENT-4: nvram + identidade EFI SEM mount | `nvram -p`; comando EXATO `diskutil info -plist <device>` da ESP (sem montar; sem `shasum` no alvo; hash-ESP no ensaio Option 0; `diskutil mount*` PROIBIDO por prefixo no alvo, ver PROIBIDO) | 60 s; identidade parcial INSUFICIENTE para qualquer write futuro (R2) |
| E06 | LOGS-1: panic logs existentes | `log show --predicate 'eventMessage CONTAINS "panic"' --last 24h`, cap 200 linhas | 60 s |
| E07 | LOGS-2: gpuRestart history | fonte OS existente, cap 200 linhas | 60 s |
| E08 | FOTO-1 (tela pós) | câmera | idem E01 (E02 + metadados obrigatórios) |
| E09 | T+08: repetição IDENT-2 | idem E03 | 60 s; julgar pela ALLOWLIST (ver PASS-FAIL-ABORT) |
| E10 | Checklist CLOSE assinado | papel/segundo dispositivo | destino = mesma pasta datada (`<HH-MM-SS>_E10_CLOSE.md`/foto do papel); assinatura = nome do operador + data/hora + veredito PASS/FAIL/ABORT por extenso + carimbo PASS-OB + `GPU-STATE = UNKNOWN` (R1) |
| E11 | FOTO-abort + SAVE `ABORTED-/FAILED-` | se aplicável | destino = mesma pasta datada (`<HH-MM-SS>_E11_ABORT-*.jpg/md`); CLOSE parcial assinado com motivo do abort (mesmo formato de E10); coleta pós-reboot bounded = NOVA janela, nunca continuação (R4) |

## Mapa E## → passos T+ (M0242, P81-GLM-ABORT-03)
| Passo | Evidência |
|---|---|
| T+00 | E01 (FOTO-0) |
| T+01 | E02 (IDENT-1) |
| T+02 | E03 (IDENT-2) |
| T+03 | E04 (IDENT-3) |
| T+04 | E05 (IDENT-4, SEM mount) |
| T+05 | E06 (LOGS-1) |
| T+06 | E07 (LOGS-2) |
| T+07 | E08 (FOTO-1) |
| T+08 | E09 (repetição IDENT-2) |
| T+09 | SAVE de E01–E09 + `INDEX.md` |
| T+10 | E10 (CLOSE); E11 se ABORT/FAIL |

## Bound de "legível/nítida" para FOTOs (E01/E08/E11) (R1; G-M0244-03 DURA)
- Nítida = tela legível ao ampliar (texto do terminal/menus reconhecível),
  LEDs/estado do monitor distinguíveis, sem borrão de movimento.
- Bound objetivo adicional: resolução mínima 8 MP (ou 3264×2448), sem crop que
  remova tela/LEDs, sem zoom digital além de 2×; calibração com objeto de
  referência (régua/cartão ao lado do monitor) em FOTO-0 de cada janela.
- Operador NÃO auto-julga: foto duvidosa/borderline = FAIL-1 automático, sem
  "aceitar com nota". (FPP-3; M0244.)
- Timestamp da FOTO vem de E02/relógio do SO verificado por G-M0244-01-R1 (P81.1, read-only; `sntp -sS` PROIBIDO) + nome do
  arquivo `<HH-MM-SS>` + EXIF DateTimeOriginal da câmera (COMPARADOR ÚNICO M0248/F9: canônico `mdls -name kMDItemContentCreationDate <foto>`; fallback `sips -g dateTimeOriginal <foto>` SOMENTE se mdls ilegível, com nota em INDEX; formato canônico de comparação = `YYYY-MM-DDTHH:MM:SS` convertido para UTC contra C1; timezone da câmera registrado em INDEX; `stat` do arquivo = auxiliar, sem valor probatório) —
  OBRIGATÓRIOS em TODAS as FOTOs; foto sem E02 válido e sem EXIF = FAIL-1; `stat` citado/usado como B3 no lugar do EXIF = FAIL-1 explícito (M0248/F9).
  Relógio visível NA foto é desejável mas NÃO obrigatório (foto sem relógio =
  aceitar com nota, não FAIL-1 — ver PASS-2).

## Gates duros M0244 — clock, kextstat, INDEX (FPP-1/FPP-4, UA-2)
- G-M0244-01-R1 (clock-verify read-only, P81.1 — SUBSTITUI G-M0244-01; PRÉ-CONDICÃO DURA de abertura): G-M0244-01 original está RETIRADO (`sntp -sS` disciplina o relógio via `clock_settime`/`adjtime` = WRITE; contradiz ZERO writes). Procedimento novo, 100% observacional, sem sudo, sem daemon, sem timezone/NVRAM/prefs: (C1) `date -u +%Y-%m-%dT%H:%M:%SZ` antes e depois do gate (2 leituras, ≤ 60 s entre elas); (C2) `sysctl -n kern.boottime` (1 leitura); (C3) EXIF DateTimeOriginal da FOTO-0 (relógio externo independente; leitura nomeada get-only: `mdls -name kMDItemContentCreationDate <foto>` ou `sips -g dateTimeOriginal <foto>`; sem improvisar ferramenta). `stat` do arquivo da foto fica SOMENTE como metadado auxiliar, SEM valor probatório (mtime/ctime refletem o relógio do alvo na importação/cópia). Bounds: B1 monotonia C1b ≥ C1a e delta ≤ 60 s; B2 checagem de sanidade APENAS: uptime (C1 − boottime) > 0; a igualdade uptime_b − uptime_a = C1b − C1a é identidade algébrica (boottime constante no gate) e NÃO prova ausência de step — step-detection intra-gate = NÃO coberto (opção (b) Qwen P81-QWEN-CLKSEM-01); B3 |C1 − C3(EXIF)| ≤ 120 s é o ÚNICO detector de step (externo, frouxo; foto em instante único, não bracketeia o gate; step espontâneo do SO de 1–120 s durante o gate pode passar indetectado — limitação declarada; perda de precisão absoluta vs NTP-2 s declarada como residual aceito — timestamps servem a ordenação forense, não a sync absoluta). PASS = 3 fontes registradas em INDEX (fonte + drift observado + bound) com B1+B2+B3 OK; qualquer fonte ausente ou bound estourado = janela NÃO abre. E02 exige os mesmos metadados + verificação, não só presença. (FPP-1/UA-2; P81.1.)
- G-M0244-01-R1-PROIBIDO (qualquer um = violação, janela NÃO abre): `sntp -sS` / `sntp -s` / `sntp -S` / `sudo sntp` / `sudo date` com operando de set / `systemsetup -settimezone|-setnetworktimeserver` / escrita `nvram` / `sysctl -w` / iniciar daemon (`timed`, `sntpd`, `launchctl load`) / `diskutil mount*` por PREFIXO no alvo (`mount`, `mountDisk`, `mount readOnly`, ou qualquer subcomando iniciado por `mount`; forma admitida é SOMENTE `diskutil info -plist <device>`). NTP REMOVIDO do gate: `sntp -d` SEM `-sS`/`-r`/`-z` e SEM sudo é permitido SOMENTE como 4ª fonte opcional informativa (nunca exigida, nunca decide PASS) — prova de não-disciplina: sntp(1) atribui set EXCLUSIVAMENTE a `-s → adjtime(2)` e `-S → clock_settime(2)`; `-d` = só debug logging.
- G-M0244-01-R1-PROVA read-only (Tahoe atual, man pages embarcadas): `date`/`date -u +fmt` display = "read from the kernel clock" via gettimeofday(2)/clock_gettime(2) — date(1), SEE ALSO `clock_gettime(2)`/`gettimeofday(2)`; set exige operando `[[[mm]dd]HH]MM...` + "Only the superuser may set the date" — o procedimento passa SÓ `+fmt`, sem operando de set, sem sudo, logo set inalcançável. gettimeofday(2): obtido vs settimeofday() são syscalls distintas; set restrito a super-user — nunca invocado. adjtime(2): "restricted to the super-user", só alcançável via `sntp -s` — PROIBIDO. clock_settime(2): só via `sntp -S` (sntp(1): "-S Use clock_settime(2) to set the system clock...") — PROIBIDO. `sysctl -n kern.boottime`: tabela sysctl(8) marca `kern.boottime struct no` (coluna writable = no; `-W` = só writable) + sysctl(3) get (oldp) vs set (newp) — `sysctl -n` sem `-w` = get-only (newp=NULL). `stat`: stat(1) = lstat(2), leitura de metadados, sem write de conteúdo/mtime/ctime do alvo (read-only confirmado, mas SEM valor probatório para B3 — mtime/ctime refletem o relógio do alvo); EXIF DateTimeOriginal é escrito pela câmera externa, nunca pelo alvo (única âncora de B3). Se `sysctl kern.boottime` ilegível no alvo, gate = NÃO-ABRE (sem improvisar).
- G-M0244-01-R1-PROVA-COMPLEMENTO (M0248, fecha F4 — 1 linha por comando; man/syscall + daemon tocado + efeito rede/disco; o sem prova fica REBAIXADO, precedente S14/`log show`): `mdls -name kMDItemContentCreationDate` = get-only via mds/Spotlight IPC (mdls(1)); daemon tocado: mds; rede/disco: nenhum write direto pelo comando, mas IPC com subsistema indexador — REBAIXADO a "RO com IPC declarado, sem prova de zero-impacto no daemon". `sips -g dateTimeOriginal` = leitura de metadado da imagem (sips(1), sem flag de escrita na forma usada); daemon tocado: nenhum direto, mas cache de thumbnail/xattr do SO não excluído por prova — REBAIXADO, mesma classe. `df -h` = get-only via statfs(2) (df(1)); daemon: nenhum; rede/disco: nenhum — PROVEN read-only. `diskutil info -plist <device>` = probing get-only via DiskArbitration/IOService (diskutil(8), verbo `info`, sem verbo `mount`); daemon tocado: diskarbitrationd + IORegistry; rede: nenhuma; disco: nenhum mount/write pela forma `info -plist`, mas probing de dispositivo declarado — REBAIXADO a "RO com probing declarado, sem prova de zero-impacto" (erro de 1 palavra para `mount*` = violação, ver PROIBIDO). `nvram -p` = leitura de variáveis (nvram(8): forma `-p` imprime; set = `var=valor`, delete = `-d` — formas NÃO usadas); toca driver NVRAM via IORegistry; rede/disco: nenhum — RO-pela-forma com IPC de driver declarado, SEM prova de zero-impacto (REBAIXADO). `ioreg` (profundidade ≤ 3, cap 300 linhas) = leitura do IORegistry via IOKit (ioreg(8)); toca kernel registry; rede/disco: nenhum — RO-pela-forma com IPC declarado, SEM prova de zero-impacto (REBAIXADO). `log show --predicate ... --last 24h` (cap 200 linhas) = leitura via logd/IPC, subsistema sob suspeita (S14 PROVADA-CONDICIONAL, T7) — já REBAIXADO; fallback G-M0244-12 (pular LOGS se > 60 s) mantido. REGRA: só C1/C2/stat-forma são PROVEN zero-impacto; todos os REBAIXADOS acima são admitidos como evidência mas NÃO sustentam sozinhos a afirmação ZERO-writes — o gate de abertura continua sendo exclusivamente G-M0244-01-R1 (C1+C2+C3-EXIF).
- G-M0244-04 (comando-filtro kextstat EXATO, PRÉ-CONDICÃO DURA): o único comando
  admitido para E03/E09 é `kextstat -l | egrep -i 'ga106lab|com\.apple\.'`
  (forma exata publicada no runbook; sem `grep` livre, sem管道 alternativo, sem
  variação no dia). Qualquer saída fora dessa forma = FAIL-2 (comando fora do
  procedimento), sem improvisação. A cláusula "processo user-space efêmero" está
  REMOVIDA da allowlist kextstat: processos nunca aparecem em `kextstat`; a
  cláusula era vácua e ensinava o operador a desculpar linha extra. (FPP-4.)

## Formato de arquivo + INDEX com shasum (R6)
- Nomes: `<HH-MM-SS>_<E##>_<descrição>.txt/jpg/md` (HH-MM-SS = hora de coleta, relógio verificado por G-M0244-01-R1 em P3).
- `INDEX.md` por janela: operador, janela (início/fim HH-MM-SS), veredito PASS/FAIL/ABORT por extenso,
  reviewer + alternativa NOMEADOS (G-M0244-08; sem nomes, janela NÃO abre), + `shasum -a 256` de cada arquivo. INDEX inválido ou ausente = FAIL-1.
- HASH-DESTACADO (M0248/F9): `shasum -a 256 INDEX.md > INDEX.sha256` em arquivo separado, assinado (operador + data/hora) junto ao CLOSE; INDEX sem `INDEX.sha256` correspondente = FAIL-1.
- Bounds medidos no Option 0: duração real de cada comando anotada; se qualquer comando estourar 60 s no Option 0, D é reprovado antes de abrir.

## Onde salvar
- Destino primário: volume externo ou pasta datada fora da árvore do repo
  (ex.: `P81-EVIDENCE/<YYYY-MM-DD_HH-MM>/`), NUNCA dentro de `EFI/`, `Tools/`,
  árvores de KEXT, ou sobrescrevendo known-good.
- Threshold (G-M0244-09, PRÉ-CONDICÃO DURA de abertura): destino com ≥ 5 GB livres
  verificados em P3 (`df -h` anotado em INDEX); abaixo de 5 GB, janela NÃO abre
  (não só P3 informativo). Disco cheio mid-SAVE => ABORT-HEALTH, sem retry no dia. (R6/F9.)
- Bound SAVE-copy (G-M0244-09): SAVE T+09 limitado a 60 s e a 500 MB por janela;
  estouro => ABORT-HEALTH + SAVE parcial com regra abaixo. (RG-4.)
- Regra INDEX parcial (G-M0244-09): SAVE parcial gera `INDEX-PARTIAL.md` com
  `ABORTED-`/`FAILED-` por arquivo + `shasum` só dos arquivos completos; arquivo
  sem shasum válido NUNCA é lido como completo; parcial sem INDEX-PARTIAL = FAIL-1. (RG-4.)
- Cópia: 1 segunda cópia em MÍDIA EXTERNA IMEDIATA antes de encerrar cada
  micro-janela (G-M0244-06; fecha Q3); sem destino externo, janela NÃO abre.

## Como voltar ao estado anterior
- Caminho PASS limpo: por construção zero-write, estado anterior = estado posterior.
- Verificação objetiva: diff E03 vs E09 vazio pela ALLOWLIST (qualquer diff em linha de KEXT = FAIL-3) + CHECKLIST-KEXTSTAT linha-a-linha (M0248/F9): cada linha de E03 conferida contra E09 por 2 OLHOS (operador + segunda pessoa, ambos assinam) OU script de diff exato (`diff -u E03 E09` com saída arquivada no SAVE); sem checklist assinado ou sem saída de diff arquivada, PASS NÃO declarado + checklist E10.
- Se qualquer evidência sugerir mudança (diff fora da allowlist): tratar como FAIL-3,
  preservar tudo, NÃO "corrigir", reportar para review (reviewer + SLA, ver PASS-FAIL-ABORT).
- Caminho FAIL/ABORT-HEALTH: estado pós-janela DESCONHECIDO (R4); coleta pós-reboot bounded = NOVA janela com review.
- ESCOPO-ZERO-MOUNT (M0248, fecha F6): ZERO mounts / mount-table intocada = ESP/volumes DO ALVO; mídia externa de evidência É montada por desenho em T+09 (G-M0244-06) sem violar este item.
- Pipeline de volta: nada a desmontar (ESP do alvo nunca montada); remover mídia
  de evidência, confirmar boot segue no known-good sem alterações (só observação no
  próximo boot normal, sem ação extra no dia).

## O que NÃO coletar
- Dumps sem bound, `log stream` contínuo, sampling de kernel, sysdiagnose completo
  (pesado; observer effect), qualquer dump de VRAM/BAR/GSP, qualquer dado pessoal
  além do necessário (redigir seriais se publicar).
