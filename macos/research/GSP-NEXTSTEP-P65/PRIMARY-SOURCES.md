# P65 PRIMARY SOURCES — GA106 GSP post-GFW next-step (offline)

Metodologia: repo + URL/origin + commit/revision + file + symbol/function + relevant lines + SHA-256 do snapshot local.
Nada de blogs/forum/comentarios internos como prova. Nouveau apenas como corroborador. OpenRM como corroboracao minima.

## 1. Nova-core (torvalds/linux, drivers/gpu/nova-core) — fonte primaria principal

Origin: https://github.com/torvalds/linux
Tree: master (snapshots via https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/nova-core/...)

Per-file last-commit (via GitHub API, coletado 2026-09-08):

- falcon.rs: 4c9ba407018e8deb06dbc643112bac8f40404f95 (2026-08-06)
- falcon/gsp.rs: 23d66dbab84e8518943563df2ced14aaab28b77a (2026-06-29)
- firmware/booter.rs: 91645a52ebf2e22f0bd37c142b79238617d38758 (2026-08-06)
- firmware/gsp.rs: 91645a52ebf2e22f0bd37c142b79238617d38758 (2026-08-06)
- gsp/boot.rs: ccdcefb71acd3664a0e38ebdb5fa7b56a1a546a4 (2026-08-05)
- gsp/commands.rs: 93b9511a3bba7f31d95502e5f912f0a476b0cf4a (2026-07-27)
- gsp/fw.rs: 91645a52ebf2e22f0bd37c142b79238617d38758 (2026-08-06)
- gsp/fw/commands.rs: 24d2581fd911d34f88153af59d3b0d6bc5f07adf (2026-07-01)
- gsp/hal.rs: 7923d0bc8ede8786f83b15bce5876cf112c347a6
- gsp/hal/tu102.rs: 91645a52ebf2e22f0bd37c142b79238617d38758 (2026-08-06)

Snapshots locais (UPSTREAM-EVIDENCE/, SHA-256):

- nova-core-falcon.rs — 9e7b3f8a80ecf630be0356d58f0e032eb06fda50c59fb09f4638ff3b9a9884ca
  file: drivers/gpu/nova-core/falcon.rs
  symbols: Falcon::dma_reset, Falcon::reset, Falcon::pio_load, Falcon::dma_wr, Falcon::dma_load,
    Falcon::wait_till_halted, Falcon::start, Falcon::write_mailboxes, Falcon::boot,
    Falcon::load, Falcon::write_os_version, Falcon::is_riscv_active,
    FalconFbifTarget::CoherentSysmem, FalconFbifMemType::Physical, DmaTrfCmdSize::Size256B
- nova-core-falcon-gsp.rs — 26abdeaadaea1ca13254b252bf016daff01ef2a038f0534a0c00f5999c882da8
  file: drivers/gpu/nova-core/falcon/gsp.rs
  symbols: Gsp (BASE 0x00110000 / 0x00111000), FalconEngine for Gsp,
    Falcon<Gsp>::clear_swgen0_intr, check_reload_completed (NV_PGC6_BSI_SECURE_SCRATCH_14.boot_stage_3_handoff),
    is_riscv_active, priv_target_mask_released
- nova-core-falcon-sec2.rs — f67755b7463d9d5cf6afe5d770192688be1169ae7fe203340e33ba572406c4b3
  file: drivers/gpu/nova-core/falcon/sec2.rs
- nova-core-falcon-hal.rs — 7091d8a0d2ca7cef9cd35f1b150d0c992a5b0e84c9af5b84fba47a370535402c
  file: drivers/gpu/nova-core/falcon/hal.rs
  symbols: FalconHal (select_core, signature_reg_fuse_version, program_brom, is_riscv_active,
    reset_wait_mem_scrubbing, reset_eng, load_method), LoadMethod::Pio/Dma, falcon_hal()
- nova-core-falcon-hal-ga102.rs — 5aae2f5a612c5701a4805a80589c0b1f07be5ccae4f4dc21b1d789d2dc031c3d
  file: drivers/gpu/nova-core/falcon/hal/ga102.rs — GA106 path (Ampere nao-GA100 => Ga102, LoadMethod::Dma)
- nova-core-falcon-hal-tu102.rs — c5f3bd7bd0ba7dc7136b9d8dd015d42b3f3deceffd9c4e2acc8547a41408ddaa
  file: drivers/gpu/nova-core/falcon/hal/tu102.rs — LoadMethod::Pio, referencia de reset/scrub 10ms
- nova-core-fb.rs — 8bb5bffd291605f8008de5eb626be61487b6cb02f95e7725f6b2e4d3f1f44f62
  file: drivers/gpu/nova-core/fb.rs
  symbols: SysmemFlush::register/drop, FbRanges::new, wpr2_range(), NV_PFB_NISO_FLUSH_SYSMEM_ADDR*
- nova-core-fb-regs.rs — de24633967a4fc182d80abbc6d522ca20f225cc88210e8669db7389d74555cf9
  file: drivers/gpu/nova-core/fb/regs.rs
  symbols: NV_PFB_NISO_FLUSH_SYSMEM_ADDR @0x00100c10 (adr_39_08),
    NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI @0x00100c40 (adr_63_40),
    NV_PFB_PRI_MMU_WPR2_ADDR_LO @0x001fa824, NV_PFB_PRI_MMU_WPR2_ADDR_HI @0x001fa828
- nova-core-fb-hal.rs — 7199a7b8c68b4e65a39f38d8702d1eb844047834e63f6f7fea7814fcea2f1c7f
  file: drivers/gpu/nova-core/fb/hal.rs — FbHal::read/write_sysmem_flush_page
- nova-core-fb-hal-tu102.rs — ca3f9192c93e64e4a7f82bd55fa7f9308e5279344aa9dc213e916959737334bf
  file: drivers/gpu/nova-core/fb/hal/tu102.rs — FLUSH_SYSMEM_ADDR_SHIFT=8, write LO only (<=40bit)
- nova-core-fb-hal-ga100.rs — b5295c7338d0fc2bad84ecd44ced229649fbeee1e8298526936f086f0e8228f9
  file: drivers/gpu/nova-core/fb/hal/ga100.rs — GA106/GA102 path: HI(shift 40) depois LO(shift 8)
- nova-core-fb-hal-ga102.rs — f7fd72128ea9accba983b7a40368e9f24fa91d8fdf2baf03a1ea561e39035c97
  file: drivers/gpu/nova-core/fb/hal/ga102.rs — delega para ga100 (prova do caminho GA106)
- nova-core-firmware-booter.rs — 2448040e8f91ec8c979bee4671dac1e15788719ec0007c0efc362dfc6d3ccb9f
  file: drivers/gpu/nova-core/firmware/booter.rs
  symbols: BooterFirmware::new (booter_load/booter_unload TLV), BooterKind::Loader/Unloader, run()
    (sec2 reset+load+boot com wpr_meta DMA addr nas mailboxes, mbox0==0)
- nova-core-firmware-fwsec.rs — d000900a7a9f229623a3f939e425b5721c166013a3bf656ed3ea48f1a60fc9b8
  file: drivers/gpu/nova-core/firmware/fwsec.rs
  symbols: FwsecFirmware::new (extrai do VBIOS), FwsecCommand::Frts{frts_addr,frts_size}, run() direto
- nova-core-firmware-fwsec-bootloader.rs — 240974d8e3f163a6bc18c4ded1bcfc849742beb53e50db2b7942e1864e0f7118
  file: drivers/gpu/nova-core/firmware/fwsec/bootloader.rs
- nova-core-firmware-gsp.rs — 8dfd0035013a41e55b1b40e8b701779e49ae946fa900391de299387454038822
  file: drivers/gpu/nova-core/firmware/gsp.rs
  symbols: GspFirmware::new (gsp Radix3 L0/L1/L2 SGTable + Coherent L0 + signatures + bootloader),
    radix3_dma_address(), map_into_lvl()
- nova-core-firmware-riscv.rs — 0b9504ffdcf42b8dc3ec59847708d8e5c99c736ddf42fdcaa7cde01804c84e04
  file: drivers/gpu/nova-core/firmware/riscv.rs — RiscvFirmware (bootloader ucode DMA)
- nova-core-firmware-tlv.rs — 348755920d04d6a452e4c62451f624c9b0958588e3a3571edef1475e342879bf
  file: drivers/gpu/nova-core/firmware/tlv.rs — request_tlv()
- nova-core-gpu.rs — 3f543a5a4250784ceb99c8612aa69844378a88523f59644c5299d2ff9ac18b1d
  file: drivers/gpu/nova-core/gpu.rs
  symbols: Chipset::GA106=0x176 (arch Ampere), Gpu::new order
    (dma_set_mask_and_coherent -> wait_gfw_boot_completion -> SysmemFlush::register -> GspResources),
    PinnedDrop unload
- nova-core-driver.rs — 3ee7edaee192671c0f62b748e2aa982c36eb7164b563a8b98efa961f20c60241
  file: drivers/gpu/nova-core/driver.rs
  symbols: NovaCoreDriver::probe (enable_device_mem + set_master ANTES de Gpu::new), BAR0_SIZE 16M
- nova-core-gsp-boot.rs — f02fee7becad837581bed6d54c9f766ab09ca2405b2328315f19fa405a5e4dd6
  file: drivers/gpu/nova-core/gsp/boot.rs
  symbols: Gsp::boot (hal.boot -> write_os_version -> poll RISC-V 10ms/5s ->
    SetSystemInfo + SetRegistry send_command_no_wait -> hal.post_boot -> wait_gsp_init_done),
    shutdown_gsp (UnloadingGuestDriver + mailbox bit31 10ms/5s)
- nova-core-gsp-commands.rs — 81e8f1db47340c7386f08224debfa5a1407ede35605403a031c9500a733ee8c2
  file: drivers/gpu/nova-core/gsp/commands.rs
  symbols: SetSystemInfo, SetRegistry (RMSecBusResetEnable/RMForcePcieConfigSave/RMDevidCheckIgnore),
    GspInitDone, wait_gsp_init_done()
- nova-core-gsp-cmdq.rs — b419c01c770b9f3cd32978bdc946de76bbb6f3a4aa70a0e01c1cfedf749bd426
  file: drivers/gpu/nova-core/gsp/cmdq.rs
  symbols: Cmdq::RECEIVE_TIMEOUT 5s, send_command_no_wait, receive_msg/wait_for_msg via
    read_poll_timeout (polling, sem IRQ)
- nova-core-gsp-fw.rs — 9a7b252ca522fc518257afbc181b52c564b86aef19732f3b563ad348b54c224d
  file: drivers/gpu/nova-core/gsp/fw.rs
  symbols: LibosParams::from_chipset (GA106 => LIBOS3), GspFwWprMeta::from_ranges
    (sysmemAddrOfRadix3Elf/Bootloader/Signature + ranges), GspDmaTarget, SeqBufOpcode
- nova-core-gsp-fw-commands.rs — d3c74214552ba488059d1d6b4ff5e4b12a6374caae0c40305152f645cbbe3134
  file: drivers/gpu/nova-core/gsp/fw/commands.rs — PackedRegistryTable, GspStaticConfigInfo, UnloadingGuestDriver
- nova-core-gsp-hal.rs — 131af9f863fb94546ad315e66150ec792952d5c1e8e864297b7fea05da17fb78
  file: drivers/gpu/nova-core/gsp/hal.rs
  symbols: GspHal::boot/post_boot, boot_firmware_files() (Ampere nao-GA100 =>
    ["booter_load.tlv","booter_unload.tlv"]), gsp_hal() (Ampere/Ada nao-GA100 => GA102_HAL)
- nova-core-gsp-hal-tu102.rs — c998a7a76e187ee08e4f8eb29adb3c2f7860d8c7b16092578360f7f467579695
  file: drivers/gpu/nova-core/gsp/hal/tu102.rs
  symbols: Tu102::run_fwsec_frts (WPR2 check/create via FWSEC-FRTS + FRTS_ERR poll),
    build_unload_bundle, Tu102::boot (FbRanges -> wpr_meta Coherent -> unload bundle ->
    FWSEC-FRTS (se frts nao vazio) -> gsp reset -> gsp.boot(libos DMA addr) -> Booter load via SEC2),
    post_boot (GspSequencer::run)
- nova-core-gsp-hal-ga102.rs — 65128110f01f2a679d696390e68342e8e8e2d5658a73444e269df175d725d1e8
  file: drivers/gpu/nova-core/gsp/hal/ga102.rs — GA102_HAL = Tu102{needs_fwsec_bootloader:false}
    (GA106 usa este HAL)
- nova-core-gsp-hal-gh100.rs — b0229e1b613f533b8a995a5bb12794f979003d51b393d6eb624258aa63aad074
  file: drivers/gpu/nova-core/gsp/hal/gh100.rs — contraste Hopper (FMC), nao-GA106
- nova-core-gsp-sequencer.rs — e2ad21fa5ebbc1116b2746a6fb79fb54b2f91c062dda9a3a556832e086f49db6
  file: drivers/gpu/nova-core/gsp/sequencer.rs
  symbols: GspSequence/GspSeqCmd (RegWrite/RegModify/RegPoll/DelayUs/RegStore/CoreReset/CoreStart/
    CoreWaitForHalt/CoreResume), GspSequencer::run (recebe seq via cmdq RECEIVE_TIMEOUT, executa
    comandos dirigidos por firmware; CoreResume = gsp reset + libos addr nas mailboxes + sec2 start
    + check_reload_completed 2s + mbox0==0 + write_os_version + is_riscv_active)
- nova-core-gsp-regs.rs — 736f133b2d9f9a648a5aa12ef0f18a0421c61a86ae8af771ddc373927b3bea59
  file: drivers/gpu/nova-core/gsp/regs.rs
- nova-core-regs.rs — 29df9fd7cad9d8af245a774503edcfe468c0bde97858ae6df65dd2cc64aede19
  file: drivers/gpu/nova-core/regs.rs — NV_PFALCON_FALCON_IRQSCLR.swgen0, DMATRF*, MAILBOX*, OS, etc.
- nova-core-vbios.rs — cb30683d661ac0937c7558eab6d17d580d4bb16ba5785815553f2ff5a2c14da3
  file: drivers/gpu/nova-core/vbios.rs — Vbios::new (fonte do FWSEC)
- nova-falcon-rst.txt — d8be54d5bbac1732fb78d4f8d738ddec9015331fae45ff66c7bf451d7027a1f6
  file: Documentation/gpu/nova/core/falcon.rst — Falcon/FBDMA doc (aperture, sysmem coherente)

## 2. Nouveau (corroborador — GA106/Ampere apenas)

Origin: https://github.com/torvalds/linux, drivers/gpu/drm/nouveau/
- drivers/gpu/drm/nouveau/nvkm/subdev/fb/gf100.c — gf100_fb_sysmem_flush_page_init():
  WARN_ON(addr > DMA_BIT_MASK(40)); nvkm_wr32(dev, 0x100c10, addr >> 8).
  Corrobora: LO=0x100c10, shift 8, pagina unica. Nao escreve HI (legado <=40bit).
- drivers/gpu/drm/nouveau/nvkm/subdev/fb/ga102.c — ga102_fb.sysmem.flush_page_init =
  gf100_fb_sysmem_flush_page_init; MODULE_FIRMWARE ga106/nvdec/scrubber.bin.
  Corrobora: GA106 reusa caminho flush GF100; scrubber e firmware SEC2/nvdec, nao primeiro boot GSP.
- include/nvkm/subdev/fb.h — struct sysmem { page *flush_page; dma_addr_t flush_page_addr; }

## 3. OpenRM (corroboracao minima)

Origin: https://github.com/NVIDIA/open-gpu-kernel-modules, branch main
- src/nvidia/src/kernel/gpu/gsp/kernel_gsp.c (snapshot textual /tmp, nao arquivado como prova primaria):
  sequencia WPR2-check -> FWSEC do VBIOS -> Booter Load/Unload em SYSMEM via SEC2 ->
  GspSetSystemInfo + SetRegistry como primeiros RPCs. Confere Nova.
- src/nvidia/src/kernel/gpu/gsp/kernel_gsp_booter.c: Booter via KernelSec2, alinhamento 256 p/ DMA.

## 4. Apple Kernel SDK (local, BME API)

- /Applications/Xcode.app/.../Kernel.framework/.../IOKit/pci/IOPCIDevice.h
  setMemoryEnable(bool) (:563), setBusMasterEnable(bool) (:574, deprecated, replacement
  setBusLeadEnable), setBusLeadEnable(bool) (:1095). Equivalente macOS de pci set_master().
  Sem snapshot (header do sistema); citar path+linha como acima.

## Manifest

Ver UPSTREAM-EVIDENCE/ (shasum -a 256). Manifest consolidado gerado na finalizacao do workspace.
Nota: arquivos /tmp/* citados como corroboracao OpenRM/Nouveau nao fazem parte do source set
primario arquivado; os snapshots arquivados sao os nova-core-* + nova-falcon-rst.txt acima.
