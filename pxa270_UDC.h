//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _PXA270_UDC_H_
#define _PXA270_UDC_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"
#include "pxa_DMA.h"
#include <stdio.h> 

struct Pxa270Udc;


struct Pxa270Udc* pxa270UdcInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma);



#endif

