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

#ifdef __cplusplus
}
#endif

#endif /* GA106CTL_VALIDATE_H */
