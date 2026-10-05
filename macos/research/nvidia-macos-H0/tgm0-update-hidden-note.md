# H0-15b — Atualização TGM0: HIDDEN != hardware unsupported (nota pronta, NÃO aplicada: repo read-only)

Acrescentar a `docs/macos/hardware/ga106-ioreg.md` §2 (ou como addendum) quando gravável:

```text
## Correção H0 (2026-09-05): HIDDEN != hardware unsupported

O estado HIDDEN da RTX 10de:2504 neste Tahoe significa EXCLUSIVAMENTE:

> a GPU foi detectada no PCI durante o boot (Found 10de:2504 + 10de:228e) e depois
> removida/ocultada por software (bridge 0:1:1 removing child, via Lilu a partir de
> -wegnoegpu + disable-gpu=true) antes do matching DriverKit.

Isso é DIFERENTE de "macOS cannot enumerate GA106":

- Enumerar: PROVADO que enumera (discovery PCI OK, class 030000, cmd 0003).
- Remover: PROVADO que remove (log + GPP0 filho vazio D004@ff + 0 nós 10de vivos).
- Suporte de hardware (BAR/MMIO/GSP/compute no macOS): segue UNKNOWN — nada sobre o
  silício foi provado nem refutado; o bloqueador atual é 100% configuração, não silício.

Consequência para A/B/C (internal-pcie-gap.md): todos seguem em aberto E intestáveis
até o boot BARE; o veredito INDETERMINATE do match-check reflete ausência de nó, não
rejeição de matching.
```

E corrigir o título do §6 do HANDOFF conforme `typo-fix-wegnoegpu.patch.md` (nesta pasta).
Referência de evidência: `artifacts-h0/log-pci-removal.txt` (6 linhas IOPCIFamily) +
`artifacts-h0/oc-config-plist-SANITIZED.txt` (DeviceProperties + NVRAM Add).
