# Laboratório macOS

Esta pasta contém os protótipos GA106Lab e a pesquisa realizada no macOS. O projeto está em desenvolvimento e ainda não fornece um driver gráfico utilizável para a RTX 3060.

Comece pelo [estado consolidado](../docs/macos/STATUS-ATUAL.md). As [fontes e revisões](research) foram importadas do pacote de 2026-10-05, preservando a sequência de experimentos.

- `research/GA106Lab-*-src/`: revisões do protótipo, ferramenta `ga106ctl` e testes.
- `research/SHADOW-*/`: modelos offline com dados simulados.
- `research/P*-*/`: pesquisa e contratos das fases experimentais.
- `research/IMPORT-MANIFEST.json`: origem e hashes dos arquivos publicados.

As revisões têm diferentes estados de implementação e validação. Não trate o nome da pasta ou um resultado de teste offline como prova de compatibilidade ou de funcionamento no hardware.

O antigo documento que descrevia esta pasta como vazia está preservado como [registro da Fase 0](../docs/macos/PLACEHOLDER-FASE0-HISTORICO.md).
