# TG-KEXT2-FIRST-PCI-SNAPSHOT result — FAIL (causa exata, sem repetição)

## Veredito

```text
MAC_GPU_PCI_READ_0 = FAIL
Causa: snapshot leu o dispositivo PCI ERRADO (host bridge 00:00.0),
       não a RTX 01:00.0. Nenhuma escrita ocorreu em lugar algum.
```

## O que aconteceu

`sudo ga106ctl pci` (1×, exit 0) retornou `vendor=1022 device=1630
class=060000` — AMD host bridge `00:00.0`, não `10de:2504`.
`PCI_IDENTITY_VALID = NO` → STOP, sem segunda leitura.

## Causa raiz (source Apple, prova)

`IOPCIDevice::configRead32(space, offset)` faz
`parent->configRead32(_space, offset)` — repassa a union VERBATIM; o bridge
gera o ciclo com bus/device/function DA UNION, não do provider.
Nossa union `.bits = 0` = b:d:f 0:0:0 → leu o host bridge.
(Evidência: `apple-configRead-forward-evidence.txt`, XNU/IOPCIFamily.)

Ou seja: a forma 2-arg (`space`, `offset`) endereça quem a union descreve;
para "ler o próprio dispositivo" o correto são os wrappers de 1 arg
(`configRead*(offset)` → via `space` interno do provider) — correção para
fase futura, NÃO aplicada aqui.

## Impacto de segurança

Leituras atingiram SOMENTE config space do host bridge (leitura pura, sem
efeito). Zero escritas (auditoria mantida). Sistema saudável após:
AMD Main+Online, 0 exits displaypolicyd, sem panic novo, NDRV intacto.
`COMMAND_REGISTER_UNCHANGED = n/a` (snapshot inválido; nada a comparar).
Estabilidade/repetição: NÃO testadas (STOP correto).

## Correção futura (uma linha de direção, sem implementar)

Preencher a union com o endereço do provider (ou usar os wrappers 1-arg do
próprio dispositivo) + teste offline de endereçamento + revalidação de que
`getProvider()`/BDF correlacionam antes de qualquer live read.
