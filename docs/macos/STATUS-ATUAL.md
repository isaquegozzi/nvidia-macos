# Estado consolidado do laboratório macOS

Consolidação feita em 2026-10-05 a partir do pacote `NVIDIA-PROJETO-2026-10-05`.

Este documento resume o material recebido. Não afirma qual extensão está carregada agora no computador do autor e não substitui uma nova coleta de estado no macOS.

## Objetivo e limites

A pesquisa busca desenvolver os componentes necessários para utilizar a RTX 3060 GA106 no macOS. Metal e aceleração gráfica aparecem como objetivos de longo prazo nos documentos posteriores. Não há demonstração de suporte funcional a essas capacidades.

## Resultados registrados na bancada

O [contexto histórico do Mac](CONTEXTO-MACOS-HISTORICO.md), especialmente as seções 19, 20, 47, 52 e 55, reúne os registros de execução:

- A placa `10de:2504` ficou visível no PCI com o desktop mantido pela GPU AMD integrada.
- Foram registrados carregamento do GA106Lab, comunicação por UserClient e consulta de versão do protocolo.
- A consulta de identidade pelo selector 1 retornou dados derivados do dispositivo registrado no IOKit. Isso não é uma leitura direta de registrador da GPU.
- O selector 3 retornou um snapshot de configuração PCI. A sessão preserva `exit_code=` sem valor por um defeito no wrapper; o documento registra sucesso semântico e a limitação de instrumentação.
- O selector 5 leu `NV_PMC_BOOT_0` em `BAR0 + 0x0`, com valor registrado `0xb76000a1`, identificação de chip `0x176` e código de saída 0.
- Uma fase anterior registrou preparação de uma página de memória de sistema na revisão 1.5.3. Preparar memória não comprova DMA real, inicialização de firmware ou execução na GPU.

Esses resultados são registros recebidos do Mac, não ensaios repetidos durante a publicação no Linux.

## Fontes e revisões

As 25 árvores em [macos/research](../../macos/research) preservam a sequência GA106Lab inicial, KEXT1 a KEXT8 e suas correções. O nome da pasta, a versão de `Info.plist` e o estado de validação podem ser diferentes. Por exemplo, a árvore `GA106Lab-kext8-revF-r3probe-src` declara versão de bundle 1.8.1; não se deve inferir uma versão 1.8.6 apenas pelo nome revF.

A árvore `GA106Lab-kext8-production-architecture-src` também recebeu trabalho offline posterior. Por isso, resultados históricos de um binário 1.8.0 não comprovam automaticamente a validação de todas as fontes atuais dessa árvore.

## Trabalho offline

Os pacotes P77, P78 e P79 incluem modelos de canais, memória virtual e envio de comandos. Revisões posteriores incluem experimentos de pré-condições de memória, escritas controladas e verificações de estado, com bloqueios e simulações.

O [registro de testes de 2026-09-10](evidence/2026-09-10/6H-TEST-RESULTS.md) relata testes, sanitizadores e mutações feitos no Mac. Sete suítes portáveis foram reproduzidas nesta importação; a [validação atual](evidence/IMPORT-VALIDATION-2026-10-05.md) lista o escopo.

## O que continua pendente

- Revalidar a revisão e o estado real da máquina antes de novos ensaios.
- Validar as etapas que existem apenas em modelos ou candidatos offline.
- Demonstrar a cadeia real de firmware/GSP, DMA e execução de comandos.
- Construir e validar a integração com a pilha gráfica do macOS.
- Resolver os bloqueios e as questões abertas de cada pacote, sem transformar propostas em resultados.

Não há evidência suficiente neste pacote para anunciar aceleração da RTX, suporte a Metal ou um driver pronto para distribuição.

## Como ler os documentos antigos

`CURRENT-STATE.md`, `PROJECT-HANDOFF.md`, runbooks e READMEs das revisões são registros das suas respectivas fases. Eles podem dizer que uma etapa era proibida, planejada ou pendente antes de ela aparecer em registros posteriores. Os documentos foram preservados para rastreabilidade, com indicação de que são históricos.

A importação não executou comandos de GPU, instalação de KEXT, alteração de EFI, reboot, MMIO ou DMA.
