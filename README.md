# nvidia-macos

Projeto experimental para estudar o uso de uma NVIDIA GeForce RTX 3060 LHR (GA106, `10de:2504`) no macOS. Reúne um laboratório Linux, protótipos de extensão de kernel para macOS e documentação dos experimentos.

## Estado atual

**Em desenvolvimento. Ainda não oferece um driver gráfico utilizável, aceleração de desktop ou suporte a Metal para a RTX 3060.**

O projeto já passou da fase exclusivamente documental no macOS. Os arquivos importados em 5 de outubro de 2026 incluem código do GA106Lab, ferramenta de comunicação com o kernel, testes offline e registros de leituras realizadas na bancada.

| Parte do projeto | Situação |
|---|---|
| Laboratório Linux | Implementado: identificação PCI/DRM, coleta de estado e comparação de resultados. |
| Extensão GA106Lab para macOS | Protótipo com várias revisões de fonte. Há registros de carregamento e comunicação com a extensão. |
| Leituras PCI e MMIO | Resultados de hardware registrados nos documentos do Mac. Não foram repetidos nesta importação. |
| Memória, canais e envio de comandos | Código experimental e modelos offline; isso não comprova execução real pela GPU. |
| Inicialização completa da GPU, GSP e DMA | Ainda não demonstrada pelo material consolidado. |
| Aceleração gráfica e Metal | Objetivo futuro, sem suporte funcional demonstrado. |

O [estado consolidado](docs/macos/STATUS-ATUAL.md) diferencia resultados registrados no hardware, testes offline e etapas pendentes. Arquivos como `CURRENT-STATE.md` dentro do arquivo de pesquisa são fotografias de fases anteriores, não uma descrição automática do estado atual da máquina.

## O que está implementado

### Laboratório Linux

O programa `ga106-lab` oferece:

- `info`: identifica a placa e lê informações de PCI e DRM.
- `baseline`: registra o estado observado em texto e JSON.
- `baseline --compare`: compara dois registros e destaca mudanças.
- `lab-status`: apresenta as condições da bancada e seus bloqueios.

A biblioteca em `common/` e os testes de lógica não dependem de uma GPU real.

### Protótipos macOS

A pasta [macos/research](macos/research) contém 25 revisões de fonte do GA106Lab, preservadas com seus nomes originais. Elas mostram a evolução da ligação ao dispositivo PCI até comunicação com o programa `ga106ctl`, leitura de configuração PCI, leitura de registradores e experimentos de memória.

Há também modelos e testes para canais, memória virtual, ciclo de vida dos objetos e envio de comandos. Cada revisão tem seu próprio escopo: a existência de uma operação no código não significa que ela foi validada no hardware.

## Tecnologias

- C++20 e CMake no laboratório Linux.
- C e C++ nos protótipos macOS e testes de modelos.
- IOKit e extensões de kernel no macOS.
- Python 3 para análise estática e ferramentas de pesquisa.
- Xcode e SDK de kernel para compilar os protótipos no macOS.

## Como executar

### Laboratório Linux

Requisitos: Linux, CMake 3.20 ou superior e compilador com suporte a C++20.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/linux/ga106-lab/ga106-lab info
./build/linux/ga106-lab/ga106-lab baseline --out-dir /tmp/ga106-baseline
```

Os testes podem rodar sem uma RTX 3060. Algumas leituras do sistema podem ficar indisponíveis sem as permissões ou ferramentas externas necessárias.

### Modelos offline do macOS

Sete suítes que usam apenas lógica e dados simulados podem ser executadas no Linux com um compilador C++17:

```bash
./scripts/test-macos-models.sh
```

Elas não carregam extensões e não acessam a GPU, a EFI ou o IOKit.

### Compilação dos protótipos no macOS

Cada revisão em `macos/research/GA106Lab-*-src/` possui seu próprio `build.sh`, quando disponível. Os scripts usam Xcode, SDK e cabeçalhos do kernel; o ambiente de referência está nos documentos de pesquisa.

A compilação completa dos KEXTs não foi verificada nesta sessão Linux. O repositório não fornece instalação automática nem um pacote pronto para uso. Os registros de ensaio não devem ser tratados como um guia genérico de instalação.

## Organização

| Pasta | Conteúdo |
|---|---|
| `common/` | Biblioteca de identificação e modelo PCI. |
| `linux/ga106-lab/` | Ferramentas de observação no Linux. |
| `macos/research/` | Revisões de fonte, testes e documentos importados do laboratório macOS. |
| `docs/macos/STATUS-ATUAL.md` | Resumo consolidado dos resultados e limitações. |
| `docs/macos/evidence/` | Registros históricos de testes e validação da importação. |
| `docs/` | Pesquisa, decisões e documentação histórica. |
| `scripts/` | Compilação, análise estática e execução de modelos offline. |
| `tests/` | Testes unitários do laboratório Linux. |

## Testes e evidências

Na verificação de 5 de outubro de 2026, passaram:

- Os três testes CTest: `common_smoke`, `compare_logic` e `chip_id`.
- Sete suítes de modelos offline: estados, geração de canais, sequências, conclusão de comandos, contratos, memória virtual e submissão simulada.

Os detalhes estão em [Validação da importação](docs/macos/evidence/IMPORT-VALIDATION-2026-10-05.md).

Os documentos do Mac também registram testes com AddressSanitizer, ThreadSanitizer e testes de mutação. Esses resultados são históricos e não foram todos reproduzidos aqui. Testes com dados simulados não comprovam funcionamento da GPU.

## Limitações e próximos passos

- O projeto está incompleto e não substitui um driver gráfico.
- O código e os registros se referem a uma bancada específica; não há compatibilidade demonstrada com outros computadores.
- Nem todas as revisões foram compiladas novamente ou validadas no hardware.
- Alguns documentos antigos descrevem fases já superadas. O estado consolidado indica essas diferenças.
- A leitura PCI registrada em uma das sessões tem um defeito conhecido na captura do código de saída; essa ressalva foi preservada.
- Os próximos experimentos dependem de confirmar o estado do hardware e os resultados das etapas anteriores.
- Inicialização de GSP, DMA, execução de comandos e integração gráfica continuam como trabalho experimental.

A cópia original foi preservada localmente. Backups completos de EFI, dumps brutos da máquina, binários compilados, credenciais e arquivos de conversas não fazem parte desta publicação. O [manifesto da importação](macos/research/IMPORT-MANIFEST.json) identifica os arquivos publicados e seus hashes.

## Licença

Não foi adicionada uma nova licença nesta importação. Arquivos e trechos de terceiros preservam os avisos de autoria e licença que já continham.
