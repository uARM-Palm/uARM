//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_MMC_H_
#define _OMAP_MMC_H_


#include "soc_DMA.h"
#include <stdbool.h>
#include <stdint.h>
#include "soc_IC.h"
#include "mem.h"
#include "vSD.h"


struct OmapMmc;


struct OmapMmc* omapMmcInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma* dma);

void omapMmcInsert(struct OmapMmc *mmc, struct VSD* vsd);	//NULL also acceptable


#endif
