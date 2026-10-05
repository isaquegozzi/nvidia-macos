> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# BARE-0 — execução 2026-09-05 ~20:05 (Tahoe, SEM reboot)

> Substitui o esquema TESTE 1 / TESTE 2 do plano H0 (`ga106-bare-boot-plan.md` §2):
> após análise do source upstream do Lilu (`DeviceInfo::create()` lê o switch-off global;
> `DeviceInfo::processSwitchOff()` tem dois caminhos independentes — global
> `requestedExternalSwitchOff` OU `disable-gpu` por-GPU), remover só `-wegnoegpu`
> provavelmente manteria a RTX HIDDEN. O primeiro teste BARE útil remove AMBOS os
> pedidos de desligamento. `disable-gpu=false/0` NÃO desarma (código testa PRESENÇA) —
> a propriedade foi REMOVIDA por completo.

## O que foi feito (2 mudanças, 1 lógica: REQUEST_DISABLE_RTX true → false)

- Mudança A — `NVRAM → Add → 7C436110-... → boot-args`: removido só o token `-wegnoegpu`.
  Antes: `-v debug=0x100 keepsyms=1 -amfipassbeta -wegnoegpu`
  Depois: `-v debug=0x100 keepsyms=1 -amfipassbeta`
- Mudança B — `DeviceProperties → Add → PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)`:
  propriedade `disable-gpu` REMOVIDA por completo (entrada PCI mantida como `<dict/>`
  vazio; era a única propriedade dela). NADA de `false/0/<00>`.

## Arquivos / hashes

- EFI usada: `/Volumes/MACOS PENDR/EFI/OC/config.plist`
  (pendrive disk6s1, único volume com `EFI/OC/`; OpenCore REL-108-2026-09-05 via NVRAM;
  disk3s4/s5/s6 verificadas e NÃO tocadas — sem `EFI/OC` nelas)
- Original SHA256: `dd431bdc7cebaee77bf45477d8c41135f472074259fcdf2991c91d60f4c9f74f`
- Backup: `/Volumes/MACOS PENDR/EFI/OC/config.plist.BEFORE-GA106-BARE-20260905-200520`
  (byte-for-byte, mesmo SHA do original)
- Novo SHA256: `3675d8f4270bb49260c254bcb2caaf53a9f96043ed2a5a89a5467b16387b670e`
- Diff (`artifacts-bare0/bare0-config-diff.txt`): SOMENTE os 2 hunks acima. Terceira
  mudança inexistente (tentativa via `plutil -remove/-replace` reformatou `<data>` e foi
  ABORTADA + restaurada byte-idêntica antes da edição cirúrgica final).
- `plutil -lint`: OK → `PLIST_VALID = YES`. `OCVALIDATE = NOT_AVAILABLE`
  (`EFI/OC/Tools/` vazio, nenhum ocvalidate no sistema; não usar versão incompatível).
- AMD: `AMD_CONFIG_CHANGED = NO` (NootedRed/layout-id/built-in/wegnoigpu idênticos;
  Kexts/ACPI intocados; zero `wegnoegpu`/`disable-gpu` residuais no config).

## Rollback (como voltar ao HIDDEN atual, sem ResetNvram)

De qualquer lugar que leia MSDOS (macOS que iniciar / Linux-Limine na disk3s4 / outro PC):

```sh
cp "/Volumes/MACOS PENDR/EFI/OC/config.plist.BEFORE-GA106-BARE-20260905-200520" "/Volumes/MACOS PENDR/EFI/OC/config.plist"
shasum -a 256 "/Volumes/MACOS PENDR/EFI/OC/config.plist"
# esperado: dd431bdc7cebaee77bf45477d8c41135f472074259fcdf2991c91d60f4c9f74f
```

Se falhar A (GUI degradada) / B (sem GUI) / C (panic/loop): ver `ga106-bare-boot-plan.md`
§4 no H0 (2 boots sem GUI = restaurar + parar; panic = foto + post-mortem).
NÃO usar `ResetNvramEntry` como recuperação (apagaria o boot-args inteiro).

## Pós-boot (quando o usuário reiniciar e chamar de volta — SOMENTE LEITURA)

```sh
ioreg -l -w0 -c IOPCIDevice | grep -i -B2 -A2 "10de\|2504" | head -n 30
system_profiler SPPCIDataType SPDisplaysDataType -detailLevel mini | head -n 60
ioreg -l -w0 -c IOPCIDevice | grep -B5 -A25 -i "0x2504" | head -n 60
ioreg -l -w0 -c IOPCIDevice | grep -i -E "built-in|IOPCITunnelled|AAPL,slot-name|IOServiceDEXTEntitlements" | grep -B8 -A2 "2504\|1638" | head -n 40
systemextensionsctl list; kextstat | grep -i -E "nvidia|amd|radeon|lilu|nootedred" | head
log show --last boot --style compact | grep "IOPCIFamily" | grep -i -E "10de|removing child" | head
nvram boot-args
```

Gabarito: `RTX_PRESENT=YES? / vendor=10de / device=2504 / class=030000 / driver=NONE? /
built-in=? / tunnelled=? / path=? / аудио 228e? / AMD_DESKTOP=WORKING?`
SUCESSO BARE = AMD ok + RTX presente. BARE-PARTIAL = AMD ok + RTX ainda ausente
(→ reauditar gatilhos). FAIL = GUI/panic/loop/AMD off (→ rollback acima).
Em qualquer caso: NADA de TinyGPU/MMIO.
