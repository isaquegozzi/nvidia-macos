# P82 — Q10 Resolution (identidade do 1.5.3 carregado vs binário em disco)

```text
SESSION_ID: FASTTRACK-DMINUS-D-P82-RELAY-V3
STATUS: DESIGN-ONLY. Pesquisa = leitura de man pages/documentação local
(`man kextstat`, `man kmutil`, `man codesign`). NENHUM kextstat/kmutil/kextcache/
kextload executado contra o kernel vivo neste turno (contadores live = 0).
```

## Q10 (enunciado P81-R3)
Existe método read-only para provar que o 1.5.3 carregado no kernel é o mesmo
binário em disco — sem unload/reload, sem side effects?

## Método pesquisado (só docs, sem tocar o kernel vivo)
1. `kextstat` (deprecated, ver `kmutil showloaded`): exibe por kext carregado —
   Index, Refs, Address, Size, Wired, [Arch], CFBundleIdentifier, CFBundleVersion,
   Linked-Against. NÃO exibe UUID, hash, assinatura ou conteúdo. Só presença/versão
   declarada. (Fonte: `man kextstat`, seções DESCRIPTION/OPTIONS.)
2. `kmutil showloaded`: equivalente suportado; mesmos campos (Index/Refs/Address/
   Size/Wired/Arch/Name/Version/Linked-Against) + `--show-mach-headers`
   (endereços/slide, não hash de TEXT). É por desenho uma consulta live ao kernel —
   e mesmo se consultada um dia, NÃO entrega artefato de hash comparável.
   (Fonte: `man kmutil`, subcomando `showloaded`.)
3. `kmutil inspect --show-kext-uuids`: exibe UUIDs dos kexts NA COLEÇÃO EM DISCO
   (inspect = conteúdo de coleção em disco, com `--show-mach-header`,
   `--show-fileset-entries`, `--show-prelink-info`). UUID-em-disco ≠ hash-da-RAM.
   Nada vincula bytes carregados aos bytes inspecionados. (Fonte: `man kmutil`,
   subcomando `inspect`.)
4. `kmutil check --load-info`: checagem de consistência interna (load-info do
   kernel espelha coleções em disco). É juízo interno da ferramenta, não artefato
   de hash portátil para INDEX; executá-la é query live, fora de escopo agora.
   (Fonte: `man kmutil`, subcomando `check`.)
5. `codesign -d/-v [path|pid]`: cria/checa/exibe assinaturas de objetos EM DISCO
   (ou status dinâmico de processos user-space por pid). Não existe modo
   documentado "hashear TEXT de kext carregado a partir de userspace". Valida o
   arquivo, não a RAM. (Fonte: `man codesign`, SYNOPSIS/DESCRIPTION.)
6. Version-string via selector do próprio KEXT: CIRCULAR — executa código do KEXT
   no kernel (S15), logo não é "sem side effects" e não prova identidade de bytes.

## Por que nada acima fecha Q10
- Address/Size iguais aos históricos não provam identidade (ASLR/slide, colisões,
  versões distintas com mesmo tamanho).
- CFBundleVersion é string declarada, não medida.
- UUID de coleção em disco não é medida de memória viva.
- Não há interface documentada userspace→kernel que exponha hash do segmento TEXT
  carregado de um kext sem executar código do próprio kext.
- Qualquer "leitura de prova" que execute o KEXT viola o "sem side effects" do
  enunciado.

## Resolução
```text
Q10 = BLOCKED (MANTIDO em M0255, SUBREVIEW FT-SMOKE-01)
```
Sem método confiável documentado. Sem improvisar (sem "address+size basta", sem
"version-string basta", sem hexdump via /dev/kmem — indisponível por desenho).
Consequência: opção A além de presença = NO-GO até fato novo
documentado (nova interface Apple ou método auditado + review). A fica A-OS
(só presença). Nenhum PASS futuro pode alegar "residente == binário histórico".

## Adendo M0255 — SUBREVIEW FT-SMOKE-01 (2b AMFI-overclaim corrigido, Q10=BLOCKED MANTIDO)
(a) AMFI/codesign no load-time [REBAIXADO M0255, substitui "GARANTIDO" de M0254]:
mecanismo de page-hash CodeDirectory descrito em docs (`man codesign`), NÃO verificado
neste alvo; SEM efeito pós-hoc (sem hash portátil da RAM para prová-lo depois);
SEM atenuação de Q10. Em alvo OpenCore/Hackintosh a garantia pode ser NULA
(boot-args que relaxam AMFI, SIP/SecureBootModel variáveis; TOCTOU, patch runtime
ou corrupção pós-map não cobertos por garantia no map). Fonte `man codesign` é
INSUFICIENTE para o loader do kernel (faltaria fonte do loader — xnu/AMFI ou doc
Apple de KEXT loading — não disponível neste turno, nada executado); logo o claim
load-time é REMOVIDO como garantia e mantido só como mecanismo não-verificado.
Q10 permanece BLOCKED sem reclassificação favorável.
(b) OSBundleUUID/LC_UUID via `ioreg` (IORegistry) comparado ao LC_UUID do Mach-O
em disco (`otool`): vínculo read-only DECLARADO adicional, NÃO enumerado nas
linhas 15-48. É string declarada do registro, não medição criptográfica da RAM;
INSUFICIENTE para desbloquear Q10. Documentado aqui como vínculo adicional, não
medição. Nenhum `ioreg` live executado neste turno (só `man ioreg`/docs).

## Fato novo que reabriria Q10 (não existe hoje)
Interface Apple documentada que exponha medida criptográfica do kext carregado, ou
método auditado com prova de ausência de side effects + review Gemini + autorização
humana dedicada. Reabertura = milestone próprio, nunca extensão silenciosa de P82.

## Pinning & expiry (2f)
P82-DESIGN-V3 (2026-09-09, FASTTRACK-DMINUS-D-P82-RELAY-V3). Escopo Q10: 1.5.3
residente vs binário em disco (sem vincular geração errada 1.6.2/1.7.x). Validade:
qualquer mudança macOS/EFI/HW/GPU/cabos/KEXT invalida e exige re-review antes de
qualquer janela. P82_LIVE_EXECUTION_AUTHORIZED = NO.
