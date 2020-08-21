#ifndef RGSM_DFU_H_INCLUDED
#define RGSM_DFU_H_INCLUDED

#include "chan_rgsm.h"
#include "ggw8_hal.h"

#define DFU_STAT_SUCCESS                0
#define DFU_STAT_INPROGRESS             1
#define DFU_STAT_BOOT_SWITCH_FAILURE    0x90
#define DFU_STAT_APP_SWITCH_FAILURE     0x91
#define DFU_STAT_COMMERROR              0xff

int rgsm_gw_dfu(struct gateway **gw, const char *pathto_manifest, int ast_clifd);

#endif // RGSM_DFU_H_INCLUDED
