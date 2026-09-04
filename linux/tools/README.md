# linux/tools/ — utilitários de inspeção (read-only)

Helpers futuros para observação passiva. Mesmas proibições de `../ga106-lab/`.

Ideias (todas read-only, a documentar antes de implementar):
- `lspci_parse.sh` — normaliza `lspci -nnv` para preencher `NvidiaPciDevice`.
- `sysfs_snapshot.sh` — copia `config`/`resource*` via `O_RDONLY` para `docs/ga106/`.
- `dmesg_nouveau.sh` — filtra `dmesg` por `nouveau|nvkm|GSP|ga106` para análise.

Nenhum script aqui pode conter `dd ... of=/sys/...`, `setpci -s ... {...}`,
`devmem`, `mmap(PROT_WRITE)`, `driver/unbind` ou equivalente. Se um helper
precisar de privilégio, deve falhar abertamente e explicar o motivo seguro.
