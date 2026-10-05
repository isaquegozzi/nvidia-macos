> README anterior à importação de 2026-10-05. Algumas afirmações foram superadas pelos protótipos macOS. Consulte o README principal para o estado consolidado.

# nvidia-macos

Pesquisa de viabilidade para usar uma **NVIDIA GeForce RTX 3060 (GA106, Ampere)** no macOS. O trabalho é feito a partir de observação no Linux, e só com leitura de hardware.

**Isto não é um driver e não roda no macOS.** Nenhum código deste repositório compila ou executa em macOS. A pasta `macos/` é um placeholder vazio, por decisão de projeto.

## Estado atual

Fase 0 (laboratório de pesquisa no Linux), em andamento.

O que já existe:

- Quatro comandos de linha de comando prontos para coletar e comparar dados da placa de vídeo.
- Uma biblioteca C++20 portátil com o modelo do dispositivo PCI e a tabela de IDs do GA106.
- 44 documentos de pesquisa em `docs/`, incluindo o registro de segurança e duas ADRs (Architecture Decision Records, documentos que registram decisões e o motivo de cada uma).
- Dois "baseline" reais medidos em uma RTX 3060 de verdade, guardados em `artifacts/`.

O que ainda não existe:

- Qualquer código que escreva no hardware. A fase atual proíbe isso explicitamente.
- Qualquer código de macOS, kext ou dext.
- Cobertura de teste para a parte que coleta dados: os testes cobrem a biblioteca comum e a lógica de comparação, não a leitura de PCI/DRM.

Uma ressalva honesta: o `docs/SAFETY.md` e o índice `docs/README.md` ainda descrevem o estado mais antigo do projeto, quando os comandos de leitura ainda não existiam. A descrição acima vem de `linux/ga106-lab/README.md` e de `docs/macos/TGM0-HANDOFF.md`, que estão atualizados.

## Funcionalidades implementadas

### Completo

O executável `ga106-lab` (somente Linux) tem quatro comandos:

- **`info`** — relatório passivo da GPU lido de `/sys/bus/pci/devices`: identificadores, revisão, BARs (os blocos de memória e portos mapeados no barramento PCIe), grupo IOMMU, nó NUMA, nós DRM e VRAM.
- **`baseline`** — instantâneo reproduzível do estado da máquina, gravado em `baseline.txt` (leitura humana) e `baseline.json`. Cobre identidade, PCIe, driver, DRM, memória, GSP, Vulkan, logs do kernel e processos usando a GPU.
- **`baseline --compare`** — comparação determinística entre dois `baseline.json`. Separa o que é identidade, estático, semi-estático e dinâmico, para que uma mudança relevante não se perca no meio do ruído.
- **`lab-status`** — relatório de situação da bancada. Avalia se o desktop pode rodar sem a NVIDIA e, separadamente, se a placa está pronta para leitura de MMIO.

Também existe `scripts/tinygpu-match-check.py`, um avaliador em Python que só faz análise estática: ele lê um snapshot de IORegistry do macOS e diz se o TinyGPU (DriverKit) teria chance de casar com o dispositivo, retornando `LIKELY_MATCH`, `LIKELY_NO_MATCH` ou `INDETERMINATE`. Não compila nem carrega nada.

### Planejado, sem código

Três scripts de inspeção em `linux/tools/` estão apenas como ideia, e `pci_scan.cpp` e `bar_view.cpp` estão propostos para quando a fase de escrita for autorizada.

## Tecnologias

- **C++20** — padrão declarado no `CMakeLists.txt`. O código usa `std::optional`, `std::array`, `std::from_chars`, `std::filesystem`, `inline constexpr` e `[[nodiscard]]`. Não usa recursos exclusivos de C++20 como `std::format` ou conceitos.
- **CMake 3.20 ou superior** — `cmake_minimum_required(VERSION 3.20)`.
- **Sem dependências externas.** Não há biblioteca de terceiros, nem cabeçalhos de kernel, nem headers de DRM, nem VFIO. O JSON é escrito à mão justamente para evitar uma dependência.
- **Linux** — toda leitura vem de sysfs (`/sys/bus/pci`, `/sys/class/drm`, `/sys/class/graphics`) e de `/proc`. Ferramentas externas (`lspci`, `nvidia-smi`, `modinfo`, `vulkaninfo`, `journalctl`) são chamadas via `popen` e são opcionais: se faltarem, o campo correspondente sai como `unknown`.
- **Python 3** — só no `scripts/tinygpu-match-check.py`, apenas com a biblioteca padrão.

Bancada de referência registrada em `docs/macos/TGM0-HANDOFF.md` e nos baselines: CachyOS com `Linux 7.2.2-1-cachyos`, clang 22.1.8, RTX 3060 Laptop (`10de:2504`), Ryzen 5 5600G com Radeon Vega integrada (`1002:1638`). Alvo final: macOS Tahoe em Hackintosh Intel.

## Como executar

Requisitos: Linux, CMake 3.20 ou superior e um compilador com suporte a C++20 (GCC 11+ ou Clang 13+). Nada disso precisa de root para compilar, testar ou rodar.

```bash
# configurar, compilar e rodar os testes
./scripts/build.sh

# ou passo a passo
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

O script aceita `--clean`, `--no-test` e `--help`, e respeita as variáveis `BUILD_DIR` e `CMAKE_GENERATOR`.

Depois de compilar, o executável fica em `build/linux/ga106-lab/ga106-lab`.

### Hardware necessário

Não é obrigatório ter uma RTX 3060 para compilar ou rodar os testes. Mas, para que a saída signifique alguma coisa, é preciso ter a placa real: `lab-status` tem o endereço `0000:01:00.0` e o ID `0x10de:0x2504` fixos no código, porque a bancada foi montada em torno deles. Sem a placa, os comandos continuam rodando e imprimem `unknown`, mas o veredito final fica `unknown` em vez de `READY` ou `BLOCKED`.

## Como usar

```bash
# relatório rápido da placa
./build/linux/ga106-lab/ga106-lab info

# instantâneo completo, impresso na tela
./build/linux/ga106-lab/ga106-lab baseline --stdout

# instantâneo gravado em disco
./build/linux/ga106-lab/ga106-lab baseline --out-dir /tmp/ga106-baseline

# comparar dois estados
./build/linux/ga106-lab/ga106-lab baseline --compare \
  /tmp/ga106-antigo/baseline.json /tmp/ga106-novo/baseline.json

# falhar se identidade ou estado estático mudarem (útil em CI)
./build/linux/ga106-lab/ga106-lab baseline --compare \
  antigo.json novo.json --strict
echo "exit=$?"

# situação da bancada
./build/linux/ga106-lab/ga106-lab lab-status
```

O `baseline` grava por padrão em `artifacts/baselines/<UTC>/`. Esse diretório está no `.gitignore`: os instantâneos são reproduzíveis, então não fazem sentido no histórico do Git.

Para a análise estática do TinyGPU, é preciso um snapshot de IORegistry obtido no macOS:

```bash
python3 scripts/tinygpu-match-check.py snapshot.json
python3 scripts/tinygpu-match-check.py --self-test
```

O `runbook` para coletar esse snapshot no macOS está em `docs/macos/hardware/ga106-ioreg-runbook.md`.

## Organização do projeto

| Pasta | Responsabilidade |
|---|---|
| `common/` | Biblioteca estática portátil: modelo do dispositivo PCI e tabela de IDs de chip, sem dependência de sistema operacional. |
| `linux/ga106-lab/` | O executável de linha de comando, só para Linux. Todo o código de leitura do sistema real. |
| `linux/tools/` | Apenas um README com ideias de script. Sem código ainda. |
| `macos/` | Placeholder intencional, só um README. Nenhum código de driver. |
| `docs/` | A pesquisa: arquitetura Ampere, mapa do GA106, auditorias de drivers de terceiros, prontidão da bancada e a estratégia para macOS. |
| `scripts/` | `build.sh` e o avaliador estático em Python. |
| `tests/` | Três executáveis de teste registrados no CTest. |
| `artifacts/` | Instantâneos reais de baseline. Ignorado pelo Git. |

## Limitações e pendências

**Restrições da fase atual, por decisão de projeto:**

- Nada de escrita em MMIO, mudança de clock, tensão ou curva de ventoinha, reset de GPU, submissão de command buffer, troca de firmware, mudança de power state, unbind/bind de driver, abertura de `/dev/mem`, `mmap` de escrita em BAR ou contorno de IOMMU/Secure Boot.
- O prontidão para MMIO está travada em `BLOCKED` dentro do próprio código, e o checklist de segurança em `docs/SAFETY_MMIO_PREREQUISITES.md` está em 3 de 11 itens. Faltam, entre outros, acesso por SSH verificado e o desktop migrado para a GPU integrada.
- O parser de JSON aceita um subconjunto do formato, com profundidade máxima 64 e arquivo de até 8 MiB.
- As varreduras de processos são limitadas (até 4096 PIDs, 256 descritores por processo, 512 descritores de render).
- Sem root, `lspci -vv` mostra `Capabilities: <access denied>`.

**Problemas conhecidos, sem relação com a fase:**

- `README.md` e `docs/README.md` na raiz descrevem um estado anterior do projeto, quando os quatro comandos ainda não existiam.
- `docs/README.md` ainda marca `docs/ampere/`, `docs/ga106/`, `docs/nouveau/`, `docs/nvk/` e `docs/research/` como "placeholder", mas esses diretórios já têm conteúdo.
- `docs/README.md` classifica `why-usb4` como "C+D CONFIRMED", o que foi corrigido depois em `docs/tinygpu/why-usb4.md` para `C LIKELY` e `D UNKNOWN`.
- `docs/tinygpu/project-impact.md` pede um alinhamento de código que já foi feito em `common/include/nvidia/chip.hpp`.
- O teste de inicialização sem a NVIDIA (`docs/lab/nvidia-free-boot-test.md`) nunca foi executado, e o acesso por SSH nunca foi verificado.

**O que falta de verdade para uma fase futura**, segundo a análise de lacunas: as etapas de bring-up do TinyGPU marcadas como `NÃO IMPLEMENTADO` no projeto — BOOT0, boot do GSP, MMU, VRAM, FIFOs, GPFIFO, compute e DMA. Nada disso existe.

## Próximos passos

Todos os itens abaixo estão escritos nos documentos do próprio projeto.

1. **A missão imediata está no macOS, e ainda é só leitura.** Executar o runbook em `docs/macos/hardware/ga106-ioreg-runbook.md`, registrar o nó `10de:2504`, salvar um snapshot sanitizado, passar o `tinygpu-match-check.py` e recalcular o objetivo intermediário `MAC-COMPUTE-0`. O handoff está em `docs/macos/TGM0-HANDOFF.md`, com as proibições explicitadas.
2. **Desbloquear o checklist de segurança** — os itens 10 (SSH) e 11 (migrar o desktop para a GPU integrada) precisam estar operacionais antes de qualquer escrita ser sequer proposta.
3. **Confirmar em hardware passivo** os valores de `BOOT0` e da arquitetura citados em `docs/tinygrad-nv/ga106-gap-analysis.md`, sem escrita, mantendo a conclusão sobre o GA106 como `UNCERTAIN` até lá.
4. **Antes de qualquer código derivado do TinyGPU**, a ADR-0002 exige executar `ioreg -l -w0`, `systemextensionsctl list` e `log show --predicate tinygpu`, além de um plano de fork com matriz de testes.
5. **Backlogs de documentação**: `docs/ga106/bancada.md`, `docs/ga106/bars.md`, `docs/ga106/straps-bios.md` e as primeiras entradas de decisão em `docs/research/`.

Metal e aceleração de desktop estão fora de escopo, por decisão registrada no handoff. O teto realista registrado é um laboratório de PCI/GSP/compute.

## Testes

Existem três testes, executados pelo **CTest**, que é a ferramenta de teste embutida no CMake. Não há framework de testes externo.

```bash
ctest --test-dir build --output-on-failure
```

O `./scripts/build.sh` já chama o `ctest` no final.

| Teste | O que cobre |
|---|---|
| `common_smoke` | Invariantes do modelo PCI: BAR válido, BAR de memória, busca por índice, identificação NVIDIA, detecção de GA106. |
| `chip_id` | Regressão sobre a tabela de ID PCI para die, incluindo os casos que costumam dar erro (`0x2487` para GA104 e um ID sem mapeamento). |
| `compare_logic` | O parser de JSON (objetos, arrays, escapes, números, booleanos, `null`), rejeição de JSON malformado, classificação de chaves por classe e a ordenação fixa das seções na saída da comparação. |

Os três são testes unitários: não tocam em `/sys`, nem em GPU, e não precisam de hardware nem de root.

O que **não** tem teste: a coleta de dados em si (`pci_discovery`, `drm_discovery`, `lab_status` e a montagem do `baseline`) não tem cobertura de teste automatizada.

Há ainda um auto-teste fora do CTest, com casos embutidos no próprio script:

```bash
python3 scripts/tinygpu-match-check.py --self-test
```

Os documentos registram esse auto-teste como passando. Não é o mesmo que os testes do CTest, e nenhum log de execução está guardado no repositório.

## Documentos

Os documentos usam uma escala de confiança declarada em `docs/tinygpu/why-usb4.md`:

- `CONFIRMED` — afirmado ou provado no código-fonte ou no binário de terceiros.
- `LIKELY` — implicado por uma constante ou por código, sem afirmação direta.
- `UNCERTAIN` — evidência insuficiente.
- `FALSE` — contradito pelo código-fonte.
- `UNKNOWN` — nenhuma evidência encontrada.

Essa escala é uma convenção dos documentos em Markdown. O código C++ usa vocabulário diferente: o texto `"unknown"` para todo campo ilegível, e vereditos `READY`, `BLOCKED` ou `PARTIALLY_READY`.

Quando uma conclusão é superada por outra, o documento antigo recebe um aviso `[SUPERADO — ver Correção TGM0]` em vez de ser apagado, para que o histórico do raciocínio fique legível.