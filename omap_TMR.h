//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_TMR_H_
#define _OMAP_TMR_H_


#include <stdint.h>
#include "omap_Misc.h"
#include "soc_IC.h"
#include "mem.h"

struct OmapTmr;

#define OMAP_TMRS_BASE		0xFFFEC500UL
#define OMAP_TMRS_INCREMENT	0x00000100UL



struct OmapTmr* omapTmrInit(struct ArmMem *physMem, struct SocIc *ic, struct OmapMisc *misc, uint32_t base, int8_t irqNo);

void omapTmrPeriodic(struct OmapTmr *tmr);


#endif
