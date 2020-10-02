//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_CAMIF_H_
#define _OMAP_CAMIF_H_


#include "soc_DMA.h"
#include <stdbool.h>
#include <stdint.h>
#include "soc_IC.h"
#include "mem.h"
#include "vSD.h"


struct OmapCamera;


struct OmapCamera* omapCameraInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma* dma);


#endif
