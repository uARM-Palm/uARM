//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_32k_TMR_H_
#define _OMAP_32k_TMR_H_


#include <stdint.h>
#include "soc_IC.h"
#include "mem.h"

struct Omap32kTmr;


struct Omap32kTmr* omap32kTmrInit(struct ArmMem *physMem, struct SocIc *ic);

void omap32kTmrPeriodic(struct Omap32kTmr *tmr);


#endif
