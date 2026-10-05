# GATEB-UC-MAPPING-PROOF — writable UC BAR0 mapping (R2, offline)

## API usada

`IOPCIDevice::mapDeviceMemoryWithIndex(unsigned index, IOOptionBits options)`
com `options = kIOMapInhibitCache` (0x100), SEM `kIOMapReadOnly` (0x1000).
Callsite: `GA106LabGateBWriteRealOps::createUcWriteMap` (GA106Lab.cpp).
Descoberta do índice: estrita 16MiB + base == BAR0 atual via membro
`GA106LabRealOps` (mesmo match dos selectors 5/6); 0 ou >=2 matches => fail.

## Flags (SDK MacOSX, Kernel.framework/Headers/IOKit/IOMapTypes.h)

```text
kIOInhibitCache = 1  (IOTypes via IOMapTypes.h:39)
kIOMapInhibitCache = kIOInhibitCache << 8 = 0x100  (IOMapTypes.h:57)
kIOMapReadOnly = 0x1000                            (IOMapTypes.h:69)
kIOMapWriteCombineCache = 0x400                    (IOMapTypes.h:60)
kIOMapCacheMask = 0xf00                            (IOMapTypes.h:54)
```

## Validação exata (GA106LabCoreValidate.h: GA106LabMapOptionsValidWritableInhibit)

```text
(RO bit == 0) AND ((opts & CacheMask) == Inhibit)
```

## O que é provado

- Modo pedido == Inhibit (UC), sem ReadOnly => mapeamento gravável.
- Qualquer modo efetivo != writable+Inhibit (WC/Default/Copyback/RO/...) é
  rejeitado pelo flow => status UcSemanticsUnproven, zero stores.
- Não existe fallback WC: o único callsite de mapa do Gate B passa a constante
  acima; o teste mapMode=WC prova a rejeição (L3).

## O que permanece inferido

- Semântica x86 UC (stores fortemente ordenados) para mapeamentos Inhibit:
  padrão da plataforma/documentação Intel, não micro-testado neste driver
  (não há como observar ordenação sem HW live; o runbook R4 re-lê via readback
  aprovado separadamente — fora deste turno).
- WC é excluído por construção (nunca pedido) + por validação (rejeitado se
  efetivo); "nenhum WC" no path é provado por auditoria de fonte, não por
  observação de MMU.

## Por que WC está excluído

WC permitiria coalescing/reordenação visível (mimo-02) e quebraria a garantia
HI-antes-de-LO no device. UC/Inhibit é o mesmo modo dos mapas de leitura
existentes (RO+Inhibit), menos o bit ReadOnly.

## Adendo VA-wrap (Smoke NOTE-2, aceito como residual documentado)

`mapLen` é 64-bit end-to-end (sem truncamento de `getLength`). Não há guarda
`VA+offset wrap` em `writeFlushHi/Lo`: o VA vem de `IOMemoryMap` do kernel para
uma região de 16MiB — wrap exigiria VA > 2^64-0x100c44, impossível para um
mapeamento válido. Registrado como residual aceito, não como prova.
