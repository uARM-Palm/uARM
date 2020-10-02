//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_MCBSP_H_
#define _OMAP_MCBSP_H_


#include "soc_DMA.h"
#include "soc_IC.h"
#include "mem.h"
#include <stdbool.h>
#include <stdint.h>

struct OmapMcBsp;


struct OmapMcBsp* omapMcBspInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma, uint32_t base, uint8_t irqNoTx, uint8_t irqNoRx, uint8_t dmaNoTx, uint8_t dmaNoRx);
void omapMcBspPeriodic(struct OmapMcBsp *sp);





#endif