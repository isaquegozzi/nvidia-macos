// ga106ctl-validate.h — validação ABI pura (testável offline, sem IOKit).
#ifndef GA106CTL_VALIDATE_H
#define GA106CTL_VALIDATE_H

#include <stddef.h>

#include "GA106LabProtocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 0 = válido. Recusa size/version incompatíveis antes de interpretar. */
int ga106ctl_check_identity(const void * buf, size_t len);
int ga106ctl_check_status(const void * buf, size_t len);
/* Snapshot PCI: size+version+status coerentes (raw preservado como veio). */
int ga106ctl_check_pci_snapshot(const void * buf, size_t len);
/* BAR map probe: size+version+status MAP_RELEASED + RELEASE_CONFIRMED. */
int ga106ctl_check_bar_map_probe(const void * buf, size_t len);
/* First MMIO read: size+version+status MAP_RELEASED + RELEASE_CONFIRMED. */
int ga106ctl_check_mmio_read(const void * buf, size_t len);
/* CLI propagation real (mesmo fluxo do ga106ctl first-mmio-read):
   0 = CLI exit 0 (kr ok + ABI válida); !=0 = CLI exit !=0.
   krSuccess: 1 se IOConnectCallMethod retornou KERN_SUCCESS, 0 caso contrário.
   Usado tanto pelo CLI quanto pelos testes, para não testar só o validator. */
int ga106ctl_mmio_cli_exit(int krSuccess, const void * buf, size_t len);
/* Static identity (selector 6, single-read BOOT_42): size+version+status
   MAP_RELEASED + BOOT42/CHIP/COUNT + RELEASE_CONFIRMED + raw não-absent. */
int ga106ctl_check_static_identity(const void * buf, size_t len);
/* CLI propagation real do static-identity (mesmo fluxo do CLI). */
int ga106ctl_static_identity_cli_exit(int krSuccess, const void * buf,
                                      size_t len);
/* GSP sysmem (selector 7): size+version+status ADDRESS_READY/ALREADY +
   flags + 4096/1/4096 + state; sem endereços. */
int ga106ctl_check_gsp_sysmem(const void * buf, size_t len);
/* CLI propagation real do prepare-gsp-sysmem. */
int ga106ctl_gsp_sysmem_cli_exit(int krSuccess, const void * buf,
                                 size_t len);
/* GFW readiness (selector 8): 32B/v1 + status Completed/TimedOut coerente +
   reserved zero; sem endereços. */
int ga106ctl_check_gfw_readiness(const void * buf, size_t len);
/* CLI propagation real do read-gfw-boot-readiness. */
int ga106ctl_gfw_readiness_cli_exit(int krSuccess, const void * buf,
                                    size_t len);
/* Flush prewrite Gate A (selector 9): 48B/v1 + status PreconditionsReady/AlreadyReady +
   campos semanticos coerentes + reserved zero; sem enderecos. */
int ga106ctl_check_flush_prewrite(const void * buf, size_t len);
int ga106ctl_flush_prewrite_cli_exit(int krSuccess, const void * buf, size_t len);
/* Gate B program (selector 10): 48B/v1 + status Success/AlreadyProgrammed +
   latch coerente + writeCount 0|2 + recoveryRequired + reserved zero;
   sem endereços. Qualquer outro status => CLI exit != 0 (sem retry). */
int ga106ctl_check_gateb_program(const void * buf, size_t len);
int ga106ctl_gateb_program_cli_exit(int krSuccess, const void * buf, size_t len);
/* BME enable (selector 11): 48B/v1 + Success/AlreadyEnabled coerente +
   boot binding + readback + reserved zero; sem endereços. */
int ga106ctl_check_bme_enable(const void * buf, size_t len);
int ga106ctl_bme_enable_cli_exit(int krSuccess, const void * buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* GA106CTL_VALIDATE_H */
