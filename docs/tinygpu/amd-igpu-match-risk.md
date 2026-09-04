# TinyGPU — risco de match na iGPU AMD `1002:1638` (TGM0, análise estática em Linux)

> Fase TGM0. SOMENTE análise estática em Linux. Nenhum hardware tocado, nenhum driver instalado, nada executado, nada copiado para o repo além de `.md`.
> Zip pinado `c0d024f9ff0e1dc8fdf217f255da7101d91e8323` baixado para `/tmp/opencode/tgm0-app/TinyGPU.zip` e extraído em `/tmp/tgm0-extract` (`unzip -o -q`). Cache TG0 anterior (`/tmp/opencode/tg0-tinygpu`, `/tmp/tg0-tinygpu-extract`) não foi reutilizado como fonte — re-baixado e re-verificado abaixo.
> Ferramentas Linux: `unzip`, `file`, `llvm-objdump`, `strings`, `python3 plistlib` (parse do `LC_CODE_SIGNATURE`/plist embutido + `embedded.provisionprofile`). `codesign -d --entitlements -` NÃO existe no Linux — o que exigir macOS está marcado `REQUIRES_MACOS_INSPECTION`.
> Escala por afirmação: `CONFIRMED` / `LIKELY` / `UNCERTAIN` / `UNKNOWN`. Veredito de risco: `SAFE` / `LIKELY_SAFE` / `RISK` / `UNKNOWN` (sem prova → `UNKNOWN`/`RISK`).
> **NÃO instale o TinyGPU.app. Este documento NÃO recomenda instalar.**

## 0. Artefato e hashes (TGM0, re-verificados)

| Artefato | sha256 | bytes |
|---|---|---|
| `/tmp/opencode/tgm0-app/TinyGPU.zip` (`tinygpu_releases@c0d024f9ff0e1dc8fdf217f255da7101d91e8323`, URL `https://github.com/tinygrad/tinygpu_releases/raw/c0d024f9ff0e1dc8fdf217f255da7101d91e8323/TinyGPU.zip`) | `0c47285e2232643210555cf30ce08289b9e55da261c300e0c82e8448a359a21f` | `1634625` |
| `/tmp/tgm0-extract/TinyGPU.app/Contents/MacOS/TinyGPU` | `3ed8bbd9ec8e14e7cf0047fbe7fb6169242394e3e5e75f5428d3edcb70254409` | `328000` |
| `/tmp/tgm0-extract/.../org.tinygrad.tinygpu.driver2.dext/org.tinygrad.tinygpu.driver2` | `236035427b9b182ad5f9eb3c16d4a3e5804f84bb864a933d6c7aa8e9c6f3f198` | `230976` |

Idênticos a `binary-architecture.md` §0 (TG0) — `CONFIRMED` mesma build auditada.

## 1. Pergunta

Se ativássemos o `TinyGPU.app`, a dext tentaria match na iGPU AMD `1002:1638` (display principal neste lab — driver `amdgpu` no Linux / driver AMD no macOS)?

Resposta curta: **é elegível no matching (classe + entitlement passam); se anexaria ou venceria o driver de display é não-provado → veredito `RISK` (§6). NÃO instalar.**

## 2. Personality da dext (resumo — tabela completa no addendum de `binary-architecture.md`)

Fonte: `/tmp/tgm0-extract/.../org.tinygrad.tinygpu.driver2.dext/Info.plist` (bplist, `plistlib`), personality única `TinyGPUDriver`. `file` = `Apple binary property list`; app `Info.plist` = XML sem nenhuma chave `IOPCI*` (`CONFIRMED`).

| Chave pedida | Valor na personality `TinyGPUDriver` | Fonte |
|---|---|---|
| `IOProviderClass` | `IOPCIDevice` | `CONFIRMED` (decode) |
| `IOPCIClassMatch` | `0x03000000` | `CONFIRMED` (decode) |
| `IOPCIPrimaryMatch` | `ABSENT` | `CONFIRMED` (0 ocorrências `IOPCIPrimaryMatch` no plist; só no entitlement §3) |
| `IOPCIMatch` | `ABSENT` | `CONFIRMED` (0 ocorrências) |
| `IOPCISecondaryMatch` | `ABSENT` | `CONFIRMED` (0 ocorrências) |
| `IOPCITunnelCompatible` | `True` | `CONFIRMED` (decode) |
| `IOProbeScore` | `ABSENT` | `CONFIRMED` (0 ocorrências no plist e no binário) |
| `IOUserClass` | `TinyGPUDriver` | `CONFIRMED` |
| `IOUserServerName` | `org.tinygrad.tinygpu.Driver` | `CONFIRMED` |
| UserClient | `TinyGPUDriverUserClient` via `TinyGPUDriverUserClientProperties {IOClass=IOUserUserClient, IOUserClass=TinyGPUDriverUserClient}` | `CONFIRMED` |
| `IOMatchCategory` | `TinyGPUDriver` | `CONFIRMED` |
| Extras presentes (não-pedidos, registrados) | `IOClass=IOUserService`, `IOResourceMatch=IOKit`, `CFBundleIdentifier=org.tinygrad.tinygpu.driver2` | `CONFIRMED` |

Semântica `IOPCITunnelCompatible=True`: declara suporte a Thunderbolt (capacidade do driver), NÃO prova exclusão de não-tunneled — ver Correção TGM0 em `binary-architecture.md` §9 / `why-usb4.md` / `internal-pcie-gap.md`. Filtro tunneled-only = `UNKNOWN`.

## 3. Entitlements de PRODUÇÃO da dext assinada (extraíveis em Linux)

`codesign -d --entitlements -` não existe no Linux (`osslsigncode` também ausente — verificado via `which`). Extração Linux por duas fontes independentes e convergentes (`CONFIRMED`):

(a) Blob de assinatura embutido no Mach-O (busca por `<?xml version` no binário; plist XML dentro do `LC_CODE_SIGNATURE`/`SuperBlob` — idêntico nos 2 slices `x86_64`+`arm64`):
- App: `com.apple.security.app-sandbox=false`, `com.apple.security.files.user-selected.read-only=true`, `com.apple.developer.system-extension.install=true`, `com.apple.developer.driverkit.userclient-access=[org.tinygrad.tinygpu.driver2]`.
- Dext: `com.apple.application-identifier=9YG3G8543N.org.tinygrad.tinygpu.driver2`, `com.apple.developer.driverkit=true`, `com.apple.developer.driverkit.transport.pci=[{IOPCIPrimaryMatch: 0x000010de&0x0000FFFF}, {IOPCIPrimaryMatch: 0x00001002&0x0000FFFF}]`.

(b) `embedded.provisionprofile` (CMS, plist após offset 62; `CreationDate 2026-03-31`, `Expiration 2044-03-26`, `TeamName tinygrad, Corp.`, `Name driver_release_0431`, `UUID 80f84d4e-…`, `Name installer_release_0431` no app):
- Dext: mesmos `application-identifier`, `driverkit true`, `transport.pci` com os **mesmos dois `IOPCIPrimaryMatch`** acima, mais `team-identifier 9YG3G8543N`, `keychain-access-groups [9YG3G8543N.*]`, `Platform [OSX]`, `ProvisionsAllDevices True`, `IsXcodeManaged False`.
- App: `application-identifier 9YG3G8543N.org.tinygrad.tinygpu.installer`, `system-extension.install true`, `driverkit.userclient-access [org.tinygrad.tinygpu.driver2]`, idem time/keychain.

Leitura `IOPCIPrimaryMatch`: formato `0xDDDDVVVV & mask`; `0x000010de&0x0000FFFF` = vendor `10de` (NVIDIA) qualquer device; `0x00001002&0x0000FFFF` = vendor `1002` (AMD) qualquer device — allowlist por vendor, não por SKU (`CONFIRMED`).

### 3.1 Produção Tiny Corp vs dev Apple (não confundir)

| Variante | `transport.pci` | `application-identifier` / cert | O que é |
|---|---|---|---|
| Shipped (PRODUÇÃO, no binário+provision acima) | `[10de any, 1002 any]` | `9YG3G8543N.org.tinygrad.tinygpu.driver2`, `Developer ID Application: tinygrad, Corp. (9YG3G8543N)` (strings do dext) | `CONFIRMED` produção Tiny Corp — é o que o SO validaria na ativação |
| Source `TinyGPUDriver.Release.entitlements` | `[10de any, 1002 any]` (= shipped) | `9YG3G8543N.…` | `CONFIRMED` idêntico ao shipped |
| Source `TinyGPUDriver.NV.Release.entitlements` | `[10de any]` só | `9YG3G8543N.…` | `CONFIRMED` variante NÃO shipped (sem `1002`) |
| Source `TinyGPUDriver.entitlements` (dev) | `[0xFFFFFFFF&0x00000000]` wildcard | sem `application-identifier` | `CONFIRMED` desenvolvimento Apple (DTS wildcard `match any hardware`; guia DTS exige Xcode 16+, conta paga, sem distribuição) |
| Source `TinyGPUDriver.NoSIP.entitlements` (dev) | wildcard + `allow-any-userclient-access true` | sem `application-identifier` | `CONFIRMED` desenvolvimento NoSIP — NÃO shipped |
| Source `macOS.entitlements` (app) | — (sandbox false + install + userclient-access) | — | `CONFIRMED` = app shipped |

### 3.2 Conclusões permissão/restrição (uma confiança por linha)

| Conclusão | Nível | Base |
|---|---|---|
| Dext tem direito `driverkit` + transporte PCI para vendors `10de` e `1002` (qualquer device) | `CONFIRMED` | duas fontes convergentes (a)+(b), ambos slices |
| `1002:1638` PASSA no filtro de entitlement (vendor `1002` bate `0x00001002&0x0000FFFF`) | `CONFIRMED` | aritmética + valor extraído (não é SKU-deny) |
| Dext NÃO tem isenção `...builtin` (zero `builtin` no dext; 2x `builtin` no app são `__swift5_builtin__TEXT`, falso positivo) | `CONFIRMED` (ausência no dext) | `strings`/busca binária + provision sem a chave |
| Se o nó `1638` expuser `built-in` no macOS, o match falharia por `matchPropertyTable` | `LIKELY` | mecanismo `CONFIRMED` fora deste doc (código Apple `IOPCIDevice::matchPropertyTable` + DTS, ver `driver-architecture-gate.md` §3.1); aplicabilidade ao nó `1638` = `UNCERTAIN` até `ioreg` |
| Dext NÃO tem `get-task-allow` nem `allow-any-userclient-access` (0 ocorrências) | `CONFIRMED` | busca binária + entitlements decodificados |
| CDHash vs `CodeResources`, cadeia/notarização ao vivo, `spctl`/`stapler`, `IOServiceDEXTEntitlements` vivo | `REQUIRES_MACOS_INSPECTION` | sem `codesign`/`spctl` no Linux; `Contents/CodeResources` (app root) é CMS opaco |

## 4. A iGPU `1002:1638` neste lab (somente leitura, 2026-09-04)

- `08:00.0 VGA compatible controller [0300]: Cezanne [Radeon Vega] [1002:1638] rev c9` — `PRESENTE` (`lspci -nn`, sysfs `vendor=0x1002 device=0x1638 class=0x030000`) — `CONFIRMED`.
- `class=0x030000` = display controller — mesma classe do `IOPCIClassMatch=0x03000000` (§2) — `CONFIRMED`.
- Driver vivo no Linux: `.../08:00.0/driver → .../bus/pci/drivers/amdgpu` (`readlink` sysfs); `boot_vga=1` neste instante (display principal; antes NVIDIA — ver `lab/igpu-readiness.md` §6 e adendo) — `CONFIRMED` no Linux, analogia macOS abaixo.
- `01:00.0 [10de:2504] class=0x030000 driver=nvidia` coexiste — dois nós `0x030000` elegíveis por classe — `CONFIRMED`.
- No macOS o dono do display seria o driver AMD da Apple (kext/DriverKit gráfico), NÃO `amdgpu` Linux — a analogia "driver de display já anexado" é `LIKELY`; qual driver e com que probe/match exato no Tahoe é `UNKNOWN` sem `ioreg`.

## 5. Análise do match (camada por camada)

| Camada | `1002:1638` | Efeito |
|---|---|---|
| Classe (`IOPCIClassMatch=0x03000000` vs `class=0x030000`) | PASSA | `CONFIRMED` — display controller casa; sem `IOPCIMatch` por ID no plist, nada exclui a iGPU aqui |
| Entitlement (`0x00001002&0x0000FFFF`) | PASSA | `CONFIRMED` — vendor AMD explícito na allowlist de produção (§3); `1638` não é negado |
| `IOPCITunnelCompatible=True` (interna não-tunneled) | NÃO EXCLUI (provado) / NÃO SE SABE se exclui | filtro tunneled-only = `UNKNOWN` (Correção TGM0); presença declara suporte, não exclusão |
| `built-in` (iGPU integrada tende a ter, mas depende de ACPI/OpenCore) | ABERTO | mecanismo de exclusão `LIKELY` se presente; presença no nó `1638` no macOS = `UNKNOWN` sem `ioreg -l` (`built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`) |
| Driver existente + precedência (`IOMatchCategory=TinyGPUDriver`, `IOProbeScore` ABSENT) | ABERTO | sem `IOProbeScore` a dext não pede prioridade (`CONFIRMED` ausência); se categoria isola arbitragem ou se o driver AMD (match por ID, probe maior) vence = `UNKNOWN` sem teste vivo; categoria distinta NÃO prova coexistência segura |
| Comportamento vivo (`Start_Impl`/`Open`/`SetName("tinygpu")`, `systemextensionsctl`, `log show --predicate tinygpu`) | NÃO OBSERVADO | `REQUIRES_MACOS_INSPECTION` — TG0/TGM0 nunca ativaram |

## 6. Veredito: `RISK`

A dext é **elegível** para a iGPU `1002:1638`: classe `0x030000` casa (`§2+§4`) E vendor `1002` está na allowlist de produção (`§3`). Nenhuma camada extraível em Linux exclui a iGPU com prova (tunnel = `UNKNOWN`, `built-in` no nó macOS = `UNKNOWN`, precedência vs driver AMD de display = `UNKNOWN`). A iGPU é display principal (`boot_vga=1`, `amdgpu` anexado no Linux) — anexar ou disputar esse nó no macOS arrisca o display. Sem prova de exclusão, a regra do projeto manda `UNKNOWN`/`RISK`; pelo impacto em display, fixa-se **`RISK`**.

Duas evidências que sustentam o veredito:

1. `IOPCIClassMatch=0x03000000` (plist decodificado, `CONFIRMED`) casa `class=0x030000` da `08:00.0 [1002:1638]` (sysfs+lspci, `CONFIRMED`) — nenhum `IOPCIMatch`/`IOPCIPrimaryMatch`/`IOPCISecondaryMatch` no plist para restringir por ID.
2. Entitlement de produção contém `0x00001002&0x0000FFFF` (binário ambos slices + provision `driver_release_0431`, `CONFIRMED` convergente) — allowlist por vendor AMD cobre `1638`; variante só-NVIDIA (`TinyGPUDriver.NV.Release.entitlements`) NÃO é a shipped.

**NÃO ative o TinyGPU.app neste lab. NÃO instale a dext. Verificação viva obrigatória antes de qualquer conclusão diferente (não executada): `ioreg -l -w0` no nó `1638`/`2504` (`built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name`), `systemextensionsctl list`, `log show --predicate tinygpu` — tudo `REQUIRES_MACOS_INSPECTION`.**

## 7. `REQUIRES_MACOS_INSPECTION` (não afirmável em Linux)

`codesign -dvv --deep --strict`, `codesign -d --entitlements -`, `spctl -a -vv`, `stapler validate`, CDHash vs hash2, cadeia/notarização ao vivo, `systemextensionsctl list`, `ioreg -l`, `log show`, mapeamento BAR/DMA vivo, `ExternalMethod` ao vivo, tráfego `tinygpu.sock` real, desassembly além de `strings`, execução dos Mach-O.

## 8. Método (reprodutível, sem instalar)

```sh
# download pinado + hash
python3 -c "import urllib.request; urllib.request.urlretrieve('https://github.com/tinygrad/tinygpu_releases/raw/c0d024f9ff0e1dc8fdf217f255da7101d91e8323/TinyGPU.zip','/tmp/opencode/tgm0-app/TinyGPU.zip')"
sha256sum /tmp/opencode/tgm0-app/TinyGPU.zip  # 0c47285e…21f
rm -rf /tmp/tgm0-extract && mkdir -p /tmp/tgm0-extract && unzip -o -q /tmp/opencode/tgm0-app/TinyGPU.zip -d /tmp/tgm0-extract
python3 -c "import plistlib; print(plistlib.loads(open('/tmp/tgm0-extract/TinyGPU.app/Contents/Library/SystemExtensions/org.tinygrad.tinygpu.driver2.dext/Info.plist','rb').read())['IOKitPersonalities'])"
# entitlements: buscar <?xml no Mach-O + plist do provision após offset 62 (ver §3)
```
