//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _OMAP_USB_H_
#define _OMAP_USB_H_

#include "soc_DMA.h"
#include "soc_IC.h"
#include "mem.h"

struct OmapUsb;



struct OmapUsb* omapUsbInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma);



#endif
