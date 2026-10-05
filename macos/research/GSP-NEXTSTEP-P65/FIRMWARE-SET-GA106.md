# P65 FIRMWARE SET — GA106 (Ampere nao-GA100)

## Resposta

```text
GA106_REQUIRED_FIRMWARE_SET =
  - booter_load.tlv (SEC2 Loader; via request_tlv(dev, chipset, "booter_load"))
  - booter_unload.tlv (SEC2 Unloader p/ teardown WPR2; via request_tlv "booter_unload")
  - bootloader ("gsp_bootloader" TLV -> RiscvFirmware; ucode DMA + code/data/manifest offsets)
  - gsp ("gsp" TLV -> Radix3 ELF FILE=nvidia/<chip>/gsp/<file> + SIZE + VERS + SIGN signatures)
  - FWSEC (extraido do VBIOS ROM via Vbios::new + FwsecFirmware::new, comando FRTS; nao e arquivo separado)
  - scrubber (nvidia/ga106/nvdec/scrubber.bin, Nouveau ga102.c) = SEC2/nvdec VPR scrub, NAO e do primeiro boot GSP
FIRST_FIRMWARE_STAGE = FWSEC-FRTS do VBIOS no falcon GSP (cria WPR2); depois Booter_load no SEC2 (carrega GSP-RM em WPR2); depois bootloader/GSP-RM executam.
```

## Prova

- `gsp/hal.rs:boot_firmware_files()`: Turing => [booter_load, booter_unload, gen_bootloader];
  Ampere GA100 => idem; **Ampere|Ade (outros, inclui GA106) => [booter_load.tlv, booter_unload.tlv]**;
  Hopper+ => [fmc.tlv]. Logo GA106 = 2 TLVs + "bootloader"+"gsp" (sempre exigidos, ver `GspFirmware::new`).
- `gsp/hal.rs:gsp_hal()`: Ampere nao-GA100 => `GA102_HAL` (sem bootloader FWSEC).
- `firmware/booter.rs:BooterFirmware::new`: `Loader=>"booter_load"`, `Unloader=>"booter_unload"`;
  parse TLV (VERS/DAOF/DASZ/CDOF/CDSZ/PLOC/FUSE/ENID/UCID/A0CO/A0CS/BLOB/SIGN), patch signature pelo
  fuse version (`signature_reg_fuse_version`), `run()` = sec2 reset+load+boot(wpr_meta addr), mbox0==0.
- `firmware/gsp.rs:GspFirmware::new`: `request_tlv("gsp")` -> VERS/SIZE/FILE/SIGN; `FILE` => blob
  `nvidia/{chip}/gsp/{file}`; L2/L1 SGTables + L0 Coherent + signatures Coherent + bootloader
  `request_tlv("gsp_bootloader")` -> `RiscvFirmware::new`.
- `firmware/fwsec.rs`: `FwsecFirmware::new(dev, falcon=GSP, bios, Frts{addr,size})` — FWSEC vem do BIOS,
  patch de comando FRTS (addr>>12/size>>12, type FB=2, cmd 0x15) + signature; `run()` direto em GA106
  (sem bootloader FWSEC). `gsp/hal/tu102.rs:run_fwsec_frts` confirma uso + `FRTS_ERR` + `wpr2_range` check.
- Nomes/caminhos/versionamento: TLV `VERS` logado (`loaded X firmware vY`); `FILE` dentro do TLV decide o
  blob; `FUSE/ENID/UCID` decidem assinatura. Nada baixado/executado neste prompt (so snapshots textuais).

## Obrigatoriedade primeiro boot vs lifecycle

- Mandatorios p/ primeiro boot: FWSEC-FRTS (WPR2) + booter_load + bootloader + gsp (+ signatures).
- Lifecycle/unload (nao primeiro boot, mas preparados em `build_unload_bundle`): FWSEC-SB + booter_unload
  (teardown WPR2 + reset GSP p/ pre-libos).
- Scrubber (`ga106/nvdec/scrubber.bin`): VPR scrub via nvdec falcon (`ga102_fb_oneinit`), posterior;
  nao confundir com Booter.
