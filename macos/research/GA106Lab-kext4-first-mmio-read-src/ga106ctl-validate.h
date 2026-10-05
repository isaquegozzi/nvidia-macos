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

#ifdef __cplusplus
}
#endif

#endif /* GA106CTL_VALIDATE_H */
