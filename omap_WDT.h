//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_WDT_H_
#define _OMAP_WDT_H_



#include <stdint.h>
#include "soc_IC.h"
#include "mem.h"

struct OmapWdt;



struct OmapWdt* omapWdtInit(struct ArmMem *physMem, struct SocIc *ic);

void omapWdtPeriodic(struct OmapWdt *wdt);	//1MHz


#endif
