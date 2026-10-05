# P66 FLUSH-ADDRESS-ENCODING — formato exato (offline, endereços artificiais)

Fontes normativas: `nova-core-fb-regs.rs`, `nova-core-fb-hal-ga100.rs` (`SHIFT_HI=40`), `nova-core-fb-hal-tu102.rs` (`SHIFT=8`).

## 1. Definições

```text
FLUSH_ADDR_ALIGNMENT = 256 (2^8, low 8 bits ignorados pelo HW)
FLUSH_ADDR_MIN_BIT = 8
FLUSH_ADDR_MAX_BIT = 63

LO_VALUE_FORMULA = (dma_addr >> 8) as u32
LO_VALUE_MASK = 0xFFFFFFFF (campo 31:0 = dma[39:8]; upper bits de dma>40 são stripped pelo cast, vão para HI)

HI_VALUE_FORMULA = (dma_addr >> 40) & 0x00FFFFFF (campo 23:0 = dma[63:40], Bounded shr 40 + cast)
HI_VALUE_MASK = 0x00FFFFFF (apenas 24 bits baixos do reg HI são significativos; bits 31:24 reservados = 0)

READBACK_FORMULA = (u64(LO) << 8) | (u64(HI & 0x00FFFFFF) << 40)
```

Notas:
- LO é `adr_39_08` nominal, mas por ser `(addr>>8) as u32` ele carrega também bits >39 que depois são ignorados? Não: `as u32` trunca para 32 bits → LO contém dma[39:8] exatamente quando dma < 2^40; quando dma ≥ 2^40, upper bits vão para HI e LO mantém dma[39:8] (porque `(addr>>8) as u32` = low 32 bits de addr>>8 = addr[39:8]). Correto.
- HI `with_adr_63_40(Bounded::from(addr).shr::<40>().cast())` → HI = addr[63:40] truncado a 24 bits (sempre cabe, pois 64-40=24).
- Bits reservados: HI[31:24] devem ser 0 (zeroed() + with_ só preenche 23:0). LO não tem reservados (31:0 todo usado).
- Ambos são u32, MMIO 32-bit, little-endian, sem byte-enable parcial — stores integrais.

## 2. Validação / erros

- Se `dma_addr & 0xFF != 0` → EINVAL offline (misaligned). PAGE_SIZE alloc garante 0x000.
- Se `dma_addr >> 64 != 0` → impossível em u64; se `HI & ~0xFFFFFF != 0` após encode → EINVAL (nunca deve ocorrer com fórmula acima; indica bug).
- IOVA com mais de 48 bits? Registro suporta até 63:40, mas DMA mask do chipset limita na prática (ver DMA-IOVA-PREREQUISITES P65). Gate A deve checar `addr < (1<<dma_mask_bits)` + `encode→decode roundtrip == addr`.

## 3. Exemplos artificiais OFFLINE (NÃO são hardware real, NÃO usar como valor)

Todos com nota ARTIFICIAL. Cálculo manual para teste de encode/decode futuro:

Exemplo A (pequeno, <40bit):
```text
ARTIFICIAL_dma = 0x00000000ABCDEF00  (low 8 bits zero, artificial)
LO = ARTIFICIAL_dma >> 8 = 0x00ABCDEF
HI = ARTIFICIAL_dma >> 40 = 0x000000
readback = (0x00ABCDEF<<8)|(0<<40) = 0xABCDEF00 ✓
```

Exemplo B (requer HI, >40bit, artificial):
```text
ARTIFICIAL_dma = 0x0000123456789A00
LO = (dma>>8) as u32 = 0x56789A00
HI = dma>>40 = 0x12 & 0xFFFFFF = 0x000012
readback = (0x56789A00<<8)|(0x12<<40) = 0x123456789A00 ✓
```

Exemplo C (máximo de campo, artificial):
```text
ARTIFICIAL_dma = 0xFFFFFF_FFFFFFFF00 (todos bits representáveis 1, low 8 zero)
LO = 0xFFFFFFFF
HI = 0xFFFFFF
readback == dma ✓
```

Exemplo D (misaligned, deve FALHAR):
```text
ARTIFICIAL_dma = 0x0000000012345678 (low byte 0x78 != 0)
→ REJEITAR em Gate A (alignment), nunca escrever.
```

Exemplo E (truncamento LO-only legado — por que REJEITADO em GA106):
```text
ARTIFICIAL_dma = 0x0000123456789A00
LO-only (Nouveau gf100) escreveria só 0x56789A00, perderia HI=0x12 → readback daria 0x56789A00 ≠ dma.
→ Prova que LO-only é incorreto quando dma ≥ 2^40; como IOVA macOS pode exceder 40 bits, HI é obrigatório.
```

## 4. Regras para implementação futura

- Encode SEMPRE internamente no kernel a partir do IOVA obtido via IODMACommand; nunca aceitar HI/LO ou endereço de userspace.
- Mascarar HI com 0x00FFFFFF antes de comparar; garantir HI>>24==0.
- Teste de roundtrip obrigatório antes de qualquer write: `decode(encode(addr))==addr`.
