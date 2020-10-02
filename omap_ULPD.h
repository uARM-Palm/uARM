//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_ULPD_H_
#define _OMAP_ULPD_H_

#include <stdint.h>
#include "soc_IC.h"
#include "mem.h"

struct OmapUlpd;



struct OmapUlpd* omapUlpdInit(struct ArmMem *physMem, struct SocIc *ic);

void omapUlpdPeriodic(struct OmapUlpd *ulpd);		//only needed if someone uses ULPD's clocks

#endif
