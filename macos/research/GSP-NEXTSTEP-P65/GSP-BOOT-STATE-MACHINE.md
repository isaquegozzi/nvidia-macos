# P65 GSP BOOT STATE MACHINE (nao tratar como equivalentes)

```text
GFW_READY
  (0x118234 progress==0xff apos gate 0x118128 bit0; so scratch completed)
→ SYSMEM_FLUSH_REGISTERED
  (HI 0x100c40 + LO 0x100c10 = DMA addr pagina Coherent; sysmembar armado)
→ FWSEC_FRTS_DONE
  (FWSEC do VBIOS via FBDMA no GSP; FRTS_ERR==0; WPR2 criado em frts.start)
→ GSP_MBOX_BOOTED
  (gsp reset + boot(mailbox=libos_dma lo/hi) + halt; mbox lidas)
→ BOOTER_LOAD_DONE
  (booter_load via SEC2 FBDMA + boot(wpr_meta addr); SEC2 mbox0==0; GSP-RM em WPR2)
→ GSP_EXECUTING
  (write_os_version + RISC-V active poll 10ms/5s == true)
→ RPC_READY
  (cmdq SetSystemInfo + SetRegistry enfileirados + sequencer firmware-driven executado,
   incl. CoreResume: gsp reset + libos mailboxes + sec2 start + reload_completed 2s +
   sec2 mbox0==0 + write_os_version + riscv active)
→ INIT_DONE
  (GSP->host GspInitDone recebido via cmdq polling 5s; so aqui "GSP booted" completo)
→ STATIC_INFO (get_static_info; fb regions; pronto p/ VRAM/MMU — fora P65)
```

Cada seta tem criterio de saida distinto (ver GSP-BOOT-SEQUENCE.md). "GFW 0xff" nao e "booter
finished", que nao e "bootloader running", que nao e "GSP-RM running", que nao e "RPC ready",
que nao e "INIT_DONE".
