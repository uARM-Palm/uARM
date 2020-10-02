//(c) uARM project    https://github.com/uARM-Palm/uARM    uARM@dmitry.gr

#ifndef _S3C24XX_USB_H_
#define _S3C24XX_USB_H_

#include "mem.h"
#include "CPU.h"
#include "soc_IC.h"
#include "soc_DMA.h"

struct S3C24xxUsb;



struct S3C24xxUsb* s3c24xxUsbInit(struct ArmMem *physMem, struct SocIc *ic, struct SocDma *dma);


#endif

