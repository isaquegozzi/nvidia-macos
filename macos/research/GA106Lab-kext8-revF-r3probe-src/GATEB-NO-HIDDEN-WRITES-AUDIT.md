# GATEB-NO-HIDDEN-WRITES-AUDIT — fonte (R2, offline)

Regra: antes/durante/depois do Gate B, os únicos MMIO stores do path são os
2 intencionais (HI uma vez, LO uma vez). Auditoria reproduzível via build.sh
(bloco "Gate B presence audit") + resumo abaixo.

## Sites de store MMIO em produção (grep OSWriteLittleInt32)

```text
GA106LabRealOps.hpp / GA106Lab.cpp GateBWriteRealOps::writeFlushHi (1 callsite no flow)
GA106LabRealOps.hpp / GA106Lab.cpp GateBWriteRealOps::writeFlushLo (1 callsite no flow)
```

Fora disso: zero (auditoria falha senão).

## Vetores proibidos e onde morrem

```text
generic writer .....: GateBWriteRegister32 existe mas exige Ops::writeRegisterAny,
                     que só o Mock implementa; uso em produção = erro de compilação.
                     Teste afirma genericWrites==0.
teardown ...........: releaseUcMap só faz release+null (lido); destructor/stop/free/
                     clientClose não referenciam o latch nem o mapa (lido).
cleanup/rollback ...: nenhum zeroing/restore/rewrite em produção (mutações
                     ZEROING_STORE/ROLLBACK_WRITE/DTOR_STORE provam detecção).
PCI/BME ............: nenhum configWrite/setBus* em produção (grep falha senão);
                     BME nunca habilitado pelo gate (só lido).
destructor .........: sem stores (mutação DTOR_STORE vive só no Mock release).
```

## Contagem por caminho (flow)

```text
pré-store (qualquer falha) ............: writes = 0
sucesso ...............................: writes = exatamente 2 (HI, LO)
stop entre HI e LO ....................: writes = 1, latch UnknownPartial, reboot
pós-map sem status ....................: UnknownPartial defensivo (inacessível em teste;
                                         mantido como fail-closed)
```

## Evidência binária

Ver auditoria de build (nm/file/strings) no build.log do candidato + OBJECT_AUDIT
na revisão R4 (nm/otool no objeto compilado).
